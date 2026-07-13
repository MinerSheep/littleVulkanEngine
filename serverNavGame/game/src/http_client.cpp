#include "http_client.hpp"

#include <curl/curl.h>

#include <mutex> // std::once_flag / std::call_once
#include <utility>

namespace
{
// Network calls must never freeze the sim: bound both the connect phase and the
// whole transfer (seconds). Shared by every request the client makes.
constexpr long kConnectTimeoutSec  = 5;
constexpr long kTransferTimeoutSec = 10;

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
} // namespace

HttpClient::HttpClient(std::size_t maxResponseBytes)
    : maxResponseBytes_(maxResponseBytes)
{
    ensureGlobalInit();
    handle_ = curl_easy_init();
    if (handle_)
        applyCommonOptions();
}

HttpClient::~HttpClient()
{
    if (handle_)
        curl_easy_cleanup(static_cast<CURL*>(handle_));
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
            curl_easy_cleanup(static_cast<CURL*>(handle_));
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
}

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

HttpClient::Response HttpClient::get(const std::string& url)
{
    if (!handle_)
        return {};

    CURL* curl = static_cast<CURL*>(handle_);
    // Reset any POST state a previous post() may have left on the reused handle
    // so this transfer is a clean GET with no stray body or headers.
    curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, nullptr);
    return perform(url);
}

HttpClient::Response HttpClient::post(const std::string& url,
                                      const std::string& body,
                                      const std::string& contentType)
{
    if (!handle_)
        return {};

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
}
