#include "fetch_weather.hpp"

#include "http_client.hpp"

#include <nlohmann/json.hpp>

#include <cmath>  // std::isfinite
#include <string> // std::to_string, std::stof
#include <iostream>

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

// Same, but for providers (like wttr.in) that encode their numbers as JSON
// *strings* ("15", "270"). Parses the string; returns the fallback if the key is
// missing, isn't a string, doesn't parse, or is NaN/inf. Never throws.
float readNumberString(const nlohmann::json& obj, const char* key, float fallback)
{
    const auto it = obj.find(key);
    if (it == obj.end() || !it->is_string())
        return fallback;
    try
    {
        const float f = std::stof(it->get<std::string>());
        return std::isfinite(f) ? f : fallback;
    }
    catch (...)
    {
        return fallback; // stof throws on empty/garbage input
    }
}

HttpClient& sharedWeatherClient()
{
    // Cap raised from the 64 KB default: the wttr.in provider (?format=j1) returns
    // a much larger document than open-meteo's current_weather block, and a
    // truncated body would fail the JSON parse. open-meteo replies stay tiny, so
    // the bigger cap costs nothing when it's the active provider.
    static HttpClient client(512 * 1024);
    return client;
}

// --- Provider: Open-Meteo -------------------------------------------------
// current_weather block reports temperature in degrees C, windspeed in km/h and
// winddirection in meteorological degrees (the direction the wind blows FROM), so
// every field maps onto WeatherData with no unit conversion. Assumes lat/lon have
// already been sanitized by fetchWeather().
WeatherData fetchOpenMeteo(HttpClient& http, float lat, float lon)
{
    const std::string url =
        "https://api.open-meteo.com/v1/forecast?latitude=" + std::to_string(lat) +
        "&longitude=" + std::to_string(lon) + "&current_weather=true";

    // The client owns all the transport concerns (timeouts, TLS, HTTPS-only,
    // redirect and response-size limits, treating HTTP >= 400 as failure).
    const HttpClient::Response res = http.get(url);
    if (!res || res.body.empty())
    {
        std::cout << "err request failed for " << url << " (HTTP status " << std::to_string(res.status) + ")\n";
        return fallbackWeather();
    }

    // Parse WITHOUT exceptions; bail to the fallback on any malformed JSON.
    const nlohmann::json json =
        nlohmann::json::parse(res.body, /*cb=*/nullptr, /*allow_exceptions=*/false);
    if (json.is_discarded() || !json.contains("current_weather"))
    {
        std::cout << "err current_weather is not an object\n";
        return fallbackWeather();
    }

    const nlohmann::json& cw = json["current_weather"];
    if (!cw.is_object())
    {
        std::cout << "err current_weather is not an object\n";
        return fallbackWeather();
    }

    // Validate each field's presence/type/finiteness before use.
    WeatherData data;
    data.temperature = readNumber(cw, "temperature", 15.0f);
    data.windSpeed   = readNumber(cw, "windspeed", 0.0f);
    data.windDir     = readNumber(cw, "winddirection", 0.0f);
    return data;
}

// --- Provider: wttr.in ----------------------------------------------------
// current_condition[0] reports temp_C (degrees C), windspeedKmph (km/h) and
// winddirDegree (also meteorological FROM-direction), so the units already match
// WeatherData -- no conversion, only a string->float parse (wttr encodes every
// value as a JSON string, hence readNumberString). ?format=j1 asks for the JSON
// document; a browser-style request would get HTML/ANSI art instead. Assumes
// lat/lon already sanitized. Needs the raised response cap (see sharedWeatherClient).
WeatherData fetchWttrIn(HttpClient& http, float lat, float lon)
{
    const std::string url =
        "https://wttr.in/" + std::to_string(lat) + "," + std::to_string(lon) + "?format=j1";

    const HttpClient::Response res = http.get(url);
    if (!res || res.body.empty())
    {
        std::cout << "err request failed for " << url << " (HTTP status " << std::to_string(res.status) + ")\n";
        return fallbackWeather();
    }

    const nlohmann::json json =
        nlohmann::json::parse(res.body, /*cb=*/nullptr, /*allow_exceptions=*/false);
    if (json.is_discarded() || !json.contains("current_condition"))
    {
        std::cout << "err current_condition missing\n";
        return fallbackWeather();
    }

    const nlohmann::json& cc = json["current_condition"];
    if (!cc.is_array() || cc.empty() || !cc[0].is_object())
    {
        std::cout << "err current_condition is not a non-empty array\n";
        return fallbackWeather();
    }

    const nlohmann::json& c0 = cc[0];
    WeatherData data;
    data.temperature = readNumberString(c0, "temp_C", 15.0f);
    data.windSpeed   = readNumberString(c0, "windspeedKmph", 0.0f);
    data.windDir     = readNumberString(c0, "winddirDegree", 0.0f);
    return data;
}
} // namespace

WeatherData fetchWeather(float lat, float lon)
{
    return fetchWeather(sharedWeatherClient(), lat, lon);
}

WeatherData fetchWeather(HttpClient& http, float lat, float lon)
{
    // Announce the active backend once so the "time elapsed" prints in the
    // scenario builders can be attributed to the right API when A/B testing.
    static bool announced = false;
    if (!announced)
    {
        announced = true;
        std::cout << "[fetchWeather] provider = "
                  << (kWeatherProvider == WeatherProvider::WttrIn ? "wttr.in" : "open-meteo")
                  << "\n";
    }

    // Sanitize inputs: only ever request finite, in-range coordinates so a bad
    // caller value can't produce "nan"/out-of-range URLs. Shared by all providers.
    if (!std::isfinite(lat) || !std::isfinite(lon))
    {
        std::cout << "err non-finite coordinates\n";
        return fallbackWeather();
    }
    lat = lat < -90.f ? -90.f : (lat > 90.f ? 90.f : lat);
    lon = lon < -180.f ? -180.f : (lon > 180.f ? 180.f : lon);

    // Dispatch to the selected backend. Both are referenced here, so both stay
    // compiled and type-checked whichever one kWeatherProvider picks.
    switch (kWeatherProvider)
    {
    case WeatherProvider::WttrIn:
        return fetchWttrIn(http, lat, lon);
    case WeatherProvider::OpenMeteo:
    default:
        return fetchOpenMeteo(http, lat, lon);
    }
}
