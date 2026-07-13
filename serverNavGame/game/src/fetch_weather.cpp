#include "fetch_weather.hpp"

#include "http_client.hpp"

#include <nlohmann/json.hpp>

#include <cmath>  // std::isfinite
#include <string>

namespace
{
// Neutral, calm-weather fallback returned whenever a fetch can't be trusted.
// windSpeed 0 => no wind bonus and no storm damage, so a failed fetch degrades
// safely instead of crashing or corrupting the sim. (WeatherData has no default
// member initializers, so every field must be set.)
WeatherData fallbackWeather()
{
    WeatherData d;
    d.temperature = 15.0f;
    d.windSpeed   = 0.0f;
    d.windDir     = 0.0f;
    return d;
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

HttpClient& sharedWeatherClient()
{
    static HttpClient client; // default 64 KB response cap
    return client;
}
} // namespace

WeatherData fetchWeather(float lat, float lon)
{
    return fetchWeather(sharedWeatherClient(), lat, lon);
}

WeatherData fetchWeather(HttpClient& http, float lat, float lon)
{
    // 1) Sanitize inputs: only ever request finite, in-range coordinates so a
    //    bad caller value can't produce "nan"/out-of-range URLs.
    if (!std::isfinite(lat) || !std::isfinite(lon))
        return fallbackWeather();
    lat = lat < -90.f ? -90.f : (lat > 90.f ? 90.f : lat);
    lon = lon < -180.f ? -180.f : (lon > 180.f ? 180.f : lon);

    const std::string url =
        "https://api.open-meteo.com/v1/forecast?latitude=" + std::to_string(lat) +
        "&longitude=" + std::to_string(lon) + "&current_weather=true";

    // 2) The client owns all the transport concerns (timeouts, TLS, HTTPS-only,
    //    redirect and response-size limits, treating HTTP >= 400 as failure).
    const HttpClient::Response res = http.get(url);
    if (!res || res.body.empty())
        return fallbackWeather();

    // 3) Parse WITHOUT exceptions; bail to the fallback on any malformed JSON.
    const nlohmann::json json =
        nlohmann::json::parse(res.body, /*cb=*/nullptr, /*allow_exceptions=*/false);
    if (json.is_discarded() || !json.contains("current_weather"))
        return fallbackWeather();

    const nlohmann::json& cw = json["current_weather"];
    if (!cw.is_object())
        return fallbackWeather();

    // 4) Validate each field's presence/type/finiteness before use.
    WeatherData data;
    data.temperature = readNumber(cw, "temperature", 15.0f);
    data.windSpeed   = readNumber(cw, "windspeed", 0.0f);
    data.windDir     = readNumber(cw, "winddirection", 0.0f);
    return data;
}
