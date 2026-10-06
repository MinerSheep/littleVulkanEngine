#pragma once

#include <cstddef> // std::size_t
#include <string>

// Thin RAII wrapper around a *reusable* libcurl easy handle.
//
// Construct one HttpClient at startup and reuse it for every request. The handle
// is created once and the common transfer options -- timeouts, TLS
// verification, HTTPS-only redirect policy, and the response-size cap -- are
// applied a single time in the constructor instead of being re-set on every
// call. Reusing the handle also lets curl keep the underlying connection alive
// (keep-alive / TLS session reuse) between requests to the same host, which is a
// real win when we fire dozens of weather lookups in a row.
//
// The header deliberately does not include <curl/curl.h>: the handle is kept as
// a void* so consumers of this class don't drag in curl's headers.
//
// NOT thread-safe. A single HttpClient (and its handle) must be used from one
// thread at a time; create separate clients for separate threads.
class HttpClient
{
public:
    // Result of a single request. `ok` is true only when the transfer completed
    // AND the HTTP status was < 400 (see CURLOPT_FAILONERROR in the .cpp), so a
    // 404/429/500 reports ok == false with the status still filled in.
    struct Response
    {
        bool        ok     = false; // transport succeeded and HTTP status < 400
        long        status = 0;     // HTTP status code (0 if never completed)
        std::string body;           // response body (empty on failure)

        explicit operator bool() const { return ok; }
    };

    // maxResponseBytes caps how much body we're willing to buffer, so a
    // malformed or hostile reply can't balloon memory. 0 disables the cap.
    explicit HttpClient(std::size_t maxResponseBytes = 64 * 1024);
    ~HttpClient();

    // Owns a raw handle: non-copyable, but movable so it can live in containers.
    HttpClient(const HttpClient&)            = delete;
    HttpClient& operator=(const HttpClient&) = delete;
    HttpClient(HttpClient&& other) noexcept;
    HttpClient& operator=(HttpClient&& other) noexcept;

    // False if the handle failed to initialize; every request then returns a
    // failed Response instead of crashing.
    bool valid() const { return handle_ != nullptr; }

    // HTTP GET.
    Response get(const std::string& url);

    // HTTP POST with a request body. `contentType` sets the Content-Type header
    // (default JSON); pass an empty string to send none. This is what the future
    // vessel-data upload will call.
    Response post(const std::string& url,
                  const std::string& body,
                  const std::string& contentType = "application/json");

private:
    void     applyCommonOptions();
    Response perform(const std::string& url);

    void*       handle_           = nullptr; // CURL* (opaque, keeps curl.h out of this header)
    std::size_t maxResponseBytes_ = 0;
};
