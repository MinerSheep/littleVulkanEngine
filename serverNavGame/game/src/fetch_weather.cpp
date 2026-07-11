#include "fetch_weather.hpp"

#include <curl/curl.h>
#include <nlohmann/json.hpp>

#include <cmath>   // std::isfinite
#include <cstddef> // std::size_t
#include <string>

namespace
{
// Cap the body we're willing to buffer so a malformed or hostile reply can't
// balloon memory. open-meteo's current_weather payload is well under 4 KB.
constexpr std::size_t kMaxResponseBytes = 64 * 1024;

// Network calls must never freeze the sim: bound both the connect phase and
// the whole transfer (seconds).
constexpr long kConnectTimeoutSec  = 5;
constexpr long kTransferTimeoutSec = 10;

// Neutral, calm-weather fallback returned whenever a fetch can't be trusted.
// windSpeed 0 => no wind bonus and no storm damage, so a failed fetch degrades
// safely instead of crashing or corrupting the sim. (WeatherData has no default
// member initializers, so every field must be set.)

// Fallback to normal weather
WeatherData fallbackWeather()
{
    WeatherData d;
    d.temperature = 15.0f;
    d.windSpeed   = 0.0f;
    d.windDir     = 0.0f;
    return d;
}

std::size_t writeCallback(void* contents, std::size_t size, std::size_t nmemb, std::string* out)
{
    const std::size_t n = size * nmemb;
    // Enforce the cap without risking size_t underflow (out->size() stays <= cap
    // by this same guard). Returning a short count aborts the transfer.
    if (n > kMaxResponseBytes - out->size())
        return 0;
    out->append(static_cast<char*>(contents), n);
    return n;
}

// Pull a finite float out of an object field; returns the fallback if the key
// is missing, isn't a number, or is NaN/inf. Never throws.
float readNumber(const nlohmann::json& obj, const char* key, float fallback)
{
    const auto it = obj.find(key);
    if (it == obj.end() || !it->is_number())
        return fallback;
    const float f = it->get<float>();
    return std::isfinite(f) ? f : fallback;
}
} // namespace

WeatherData fetchWeather(float lat, float lon)
{
    // 1) Sanitize inputs: only ever request finite, in-range coordinates so a
    //    bad caller value can't produce "nan"/out-of-range URLs.
    if (!std::isfinite(lat) || !std::isfinite(lon))
        return fallbackWeather();
    lat = lat < -90.f ? -90.f : (lat > 90.f ? 90.f : lat);
    lon = lon < -180.f ? -180.f : (lon > 180.f ? 180.f : lon);

    CURL* curl = curl_easy_init();
    if (!curl) // 2) init can fail; don't dereference a null handle
        return fallbackWeather();

    const std::string url =
        "https://api.open-meteo.com/v1/forecast?latitude=" + std::to_string(lat) +
        "&longitude=" + std::to_string(lon) + "&current_weather=true";

    std::string response;
    response.reserve(4096);

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    // 3) Bounded time so a stalled endpoint can't hang the sim.
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, kConnectTimeoutSec);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, kTransferTimeoutSec);

    // 4) Keep TLS verification on (defaults, made explicit) and restrict to
    //    HTTPS even across redirects -- no downgrade to http/file/etc.
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
    curl_easy_setopt(curl, CURLOPT_PROTOCOLS, CURLPROTO_HTTPS);
    curl_easy_setopt(curl, CURLOPT_REDIR_PROTOCOLS, CURLPROTO_HTTPS);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 3L);

    // 5) Treat HTTP >= 400 (e.g. 429 rate-limit) as a failure so error bodies
    //    never reach the JSON parser.
    curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);

    const CURLcode rc = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if (rc != CURLE_OK || response.empty())
        return fallbackWeather();

    // 6) Parse WITHOUT exceptions; bail to the fallback on any malformed JSON.
    const nlohmann::json json =
        nlohmann::json::parse(response, /*cb=*/nullptr, /*allow_exceptions=*/false);
    if (json.is_discarded() || !json.contains("current_weather"))
        return fallbackWeather();

    const nlohmann::json& cw = json["current_weather"];
    if (!cw.is_object())
        return fallbackWeather();

    // 7) Validate each field's presence/type/finiteness before use.
    WeatherData data;
    data.temperature = readNumber(cw, "temperature", 15.0f);
    data.windSpeed   = readNumber(cw, "windspeed", 0.0f);
    data.windDir     = readNumber(cw, "winddirection", 0.0f);
    return data;
}
