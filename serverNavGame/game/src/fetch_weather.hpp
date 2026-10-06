
#pragma once

class HttpClient; // defined in http_client.hpp; only referenced by ref here

struct WeatherData
{
    float temperature;
    float windSpeed;
    float windDir;
};

// --- Weather backend switch ----------------------------------------------
// fetchWeather() can query one of two public, key-free APIs. Flip kWeatherProvider
// and rebuild to A/B their latency via the "time elapsed" prints in the scenario
// builders (servernav_sim.cpp). Both translate their reply into the same
// WeatherData: temperature in degrees C, windSpeed in km/h, windDir in
// meteorological degrees (0 = wind blowing FROM the north, increasing clockwise).
//
//   OpenMeteo - api.open-meteo.com, fast and burst-friendly (the default)
//   WttrIn    - wttr.in, community-run; heavier reply and stricter rate limits,
//               so expect it to be slower (which is the point of the comparison)
enum class WeatherProvider { OpenMeteo, WttrIn };
constexpr WeatherProvider kWeatherProvider = WeatherProvider::OpenMeteo;

// Fetch current weather for a coordinate. Uses a process-wide shared HttpClient
// (created on first call) so the curl handle is configured once and its
// connection to the weather API is reused across calls.
WeatherData fetchWeather(float lat, float lon);

// Same, but against a caller-owned HttpClient. Create one at startup and pass it
// in when you want to control its lifetime or reuse the same client for other
// requests (e.g. vessel-data sync) too.
WeatherData fetchWeather(HttpClient& http, float lat, float lon);
