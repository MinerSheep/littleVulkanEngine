#include "http_client.hpp"

#if _WIN32
#include <windows.h>
#include <winhttp.h>

#include <memory>
#else
#include <curl/curl.h>

#include <mutex> // std::once_flag / std::call_once
#endif
#include <utility>

namespace
{
// Network calls must never freeze the sim: bound both the connect phase and the
// whole transfer (seconds). Shared by every request the client makes.
constexpr long kConnectTimeoutSec  = 5;
constexpr long kTransferTimeoutSec = 10;

#if !_WIN32
// libcurl wants curl_global_init() called exactly once, before any handle is
// created, and (strictly) before other threads exist. curl_easy_init() would do
// it implicitly, but that path is not thread-safe, so we force it here. We
// intentionally skip curl_global_cleanup(): it would have to run after every
// handle is destroyed, and letting the OS reclaim the one-time allocation at
// exit is the common, safe trade-off.
void ensureGlobalInit()
{
    static std::once_flag once;
    std::call_once(once, [] { curl_global_init(CURL_GLOBAL_DEFAULT); });
}
#endif

// Passed through CURLOPT_WRITEDATA so the write callback can both append to the
// caller's buffer and enforce the per-request byte cap.
struct WriteCtx
{
    std::string* body = nullptr;
    std::size_t  cap  = 0; // 0 == unlimited
};

std::size_t writeCallback(void* contents, std::size_t size, std::size_t nmemb, void* userp)
{
    auto* ctx = static_cast<WriteCtx*>(userp);
    const std::size_t n = size * nmemb;

    // Enforce the cap without risking size_t underflow (body->size() stays <= cap
    // by this same guard). Returning a short count aborts the transfer.
    if (ctx->cap != 0 && n > ctx->cap - ctx->body->size())
        return 0;

    ctx->body->append(static_cast<char*>(contents), n);
    return n;
}

#if _WIN32
// Closes a WinHTTP handle when it goes out of scope
using WinHandle = std::unique_ptr<void, decltype(&WinHttpCloseHandle)>;

// Turns UTF-8 text into the wide text WinHTTP takes
std::wstring widen(const std::string& s)
{
    const int len = static_cast<int>(s.size());
    std::wstring out(MultiByteToWideChar(CP_UTF8, 0, s.data(), len, nullptr, 0), L'\0');
    if (!out.empty())
        MultiByteToWideChar(CP_UTF8, 0, s.data(), len, &out[0], static_cast<int>(out.size()));
    return out;
}

// Sends one HTTPS request on the session and reads the reply
// Fails like curl does: non-HTTPS urls, broken transfers, status >= 400, bodies over the cap
HttpClient::Response sendRequest(void* session, const std::string& url, const wchar_t* verb,
                                 const std::string& body, const std::string& contentType,
                                 std::size_t cap)
{
    HttpClient::Response res;

    const std::wstring wideUrl = widen(url);
    URL_COMPONENTS parts{};
    parts.dwStructSize      = sizeof(parts);
    parts.dwHostNameLength  = static_cast<DWORD>(-1);
    parts.dwUrlPathLength   = static_cast<DWORD>(-1);
    parts.dwExtraInfoLength = static_cast<DWORD>(-1);
    if (!WinHttpCrackUrl(wideUrl.c_str(), 0, 0, &parts) || parts.nScheme != INTERNET_SCHEME_HTTPS)
        return res;

    const std::wstring host(parts.lpszHostName, parts.dwHostNameLength);
    std::wstring path(parts.lpszUrlPath, parts.dwUrlPathLength);
    if (path.empty())
        path = L"/";
    path.append(parts.lpszExtraInfo, parts.dwExtraInfoLength);

    WinHandle connection(WinHttpConnect(session, host.c_str(), parts.nPort, 0), WinHttpCloseHandle);
    if (!connection)
        return res;
    WinHandle request(WinHttpOpenRequest(connection.get(), verb, path.c_str(), nullptr,
                                         WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                         WINHTTP_FLAG_SECURE),
                      WinHttpCloseHandle);
    if (!request)
        return res;

    // Follows up to 3 redirects and never drops from HTTPS to plain HTTP
    DWORD policy       = WINHTTP_OPTION_REDIRECT_POLICY_DISALLOW_HTTPS_TO_HTTP;
    DWORD maxRedirects = 3;
    WinHttpSetOption(request.get(), WINHTTP_OPTION_REDIRECT_POLICY, &policy, sizeof(policy));
    WinHttpSetOption(request.get(), WINHTTP_OPTION_MAX_HTTP_AUTOMATIC_REDIRECTS, &maxRedirects,
                     sizeof(maxRedirects));

    std::wstring headers;
    if (!contentType.empty())
        headers = L"Content-Type: " + widen(contentType);

    const DWORD bodySize = static_cast<DWORD>(body.size());
    if (!WinHttpSendRequest(request.get(), headers.empty() ? WINHTTP_NO_ADDITIONAL_HEADERS : headers.c_str(),
                            static_cast<DWORD>(headers.size()), const_cast<char*>(body.data()),
                            bodySize, bodySize, 0) ||
        !WinHttpReceiveResponse(request.get(), nullptr))
        return res;

    DWORD status     = 0;
    DWORD statusSize = sizeof(status);
    WinHttpQueryHeaders(request.get(), WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                        WINHTTP_HEADER_NAME_BY_INDEX, &status, &statusSize, WINHTTP_NO_HEADER_INDEX);
    res.status = static_cast<long>(status);
    if (status >= 400)
        return res;

    std::string received;
    received.reserve(4096);
    WriteCtx ctx{&received, cap};
    char  chunk[8192];
    DWORD got = 0;
    do
    {
        if (!WinHttpReadData(request.get(), chunk, sizeof(chunk), &got) ||
            writeCallback(chunk, 1, got, &ctx) != got)
            return res;
    } while (got > 0);

    res.ok   = true;
    res.body = std::move(received);
    return res;
}
#endif

void closeHandle(void* handle)
{
#if _WIN32
    WinHttpCloseHandle(handle);
#else
    curl_easy_cleanup(static_cast<CURL*>(handle));
#endif
}
} // namespace

HttpClient::HttpClient(std::size_t maxResponseBytes)
    : maxResponseBytes_(maxResponseBytes)
{
#if _WIN32
    handle_ = WinHttpOpen(L"serverNavGame", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                          WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
#else
    ensureGlobalInit();
    handle_ = curl_easy_init();
#endif
    if (handle_)
        applyCommonOptions();
}

HttpClient::~HttpClient()
{
    if (handle_)
        closeHandle(handle_);
}

HttpClient::HttpClient(HttpClient&& other) noexcept
    : handle_(other.handle_), maxResponseBytes_(other.maxResponseBytes_)
{
    other.handle_ = nullptr;
}

HttpClient& HttpClient::operator=(HttpClient&& other) noexcept
{
    if (this != &other)
    {
        if (handle_)
            closeHandle(handle_);
        handle_           = other.handle_;
        maxResponseBytes_ = other.maxResponseBytes_;
        other.handle_     = nullptr;
    }
    return *this;
}

// Set once at construction: everything that is identical for every request this
// client makes. Per-request state (URL, body, GET-vs-POST) is set in the call
// methods below.
void HttpClient::applyCommonOptions()
{
#if _WIN32
    // Lookup and connect get the connect limit
    // Sending and receiving get the transfer limit
    const int connectMs  = static_cast<int>(kConnectTimeoutSec * 1000);
    const int transferMs = static_cast<int>(kTransferTimeoutSec * 1000);
    WinHttpSetTimeouts(handle_, connectMs, connectMs, transferMs, transferMs);
#else
    CURL* curl = static_cast<CURL*>(handle_);

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);

    // Bounded time so a stalled endpoint can't hang the sim.
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, kConnectTimeoutSec);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, kTransferTimeoutSec);

    // Keep TLS verification on (defaults, made explicit) and restrict to HTTPS
    // even across redirects -- no downgrade to http/file/etc.
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
    curl_easy_setopt(curl, CURLOPT_PROTOCOLS, CURLPROTO_HTTPS);
    curl_easy_setopt(curl, CURLOPT_REDIR_PROTOCOLS, CURLPROTO_HTTPS);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 3L);

    // Treat HTTP >= 400 (e.g. 429 rate-limit) as a transfer failure so error
    // bodies never reach a JSON parser; the status code is still readable via
    // CURLINFO_RESPONSE_CODE in perform().
    curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
#endif
}

#if !_WIN32
HttpClient::Response HttpClient::perform(const std::string& url)
{
    Response res;
    if (!handle_)
        return res;

    CURL* curl = static_cast<CURL*>(handle_);

    std::string body;
    body.reserve(4096);
    WriteCtx ctx{&body, maxResponseBytes_};

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &ctx);

    const CURLcode rc = curl_easy_perform(curl);

    long status = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    res.status = status;

    if (rc == CURLE_OK)
    {
        res.ok   = true;
        res.body = std::move(body);
    }
    return res;
}
#endif

HttpClient::Response HttpClient::get(const std::string& url)
{
    if (!handle_)
        return {};

#if _WIN32
    return sendRequest(handle_, url, L"GET", "", "", maxResponseBytes_);
#else
    CURL* curl = static_cast<CURL*>(handle_);
    // Reset any POST state a previous post() may have left on the reused handle
    // so this transfer is a clean GET with no stray body or headers.
    curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, nullptr);
    return perform(url);
#endif
}

HttpClient::Response HttpClient::post(const std::string& url,
                                      const std::string& body,
                                      const std::string& contentType)
{
    if (!handle_)
        return {};

#if _WIN32
    return sendRequest(handle_, url, L"POST", body, contentType, maxResponseBytes_);
#else
    CURL* curl = static_cast<CURL*>(handle_);

    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    // CURLOPT_POSTFIELDS does not copy the buffer, so `body` must outlive the
    // transfer -- it does, as a const& alive for the whole call.
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(body.size()));

    curl_slist* headers = nullptr;
    if (!contentType.empty())
    {
        const std::string header = "Content-Type: " + contentType;
        headers = curl_slist_append(headers, header.c_str());
    }
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    Response res = perform(url);

    // The header list must outlive the transfer; detach it from the handle and
    // free it now that perform() has returned.
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, nullptr);
    if (headers)
        curl_slist_free_all(headers);

    return res;
#endif
}
