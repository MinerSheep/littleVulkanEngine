
#pragma once

class HttpClient; // defined in http_client.hpp; only referenced by ref here

struct WeatherData
{
    float temperature;
    float windSpeed;
    float windDir;
};

// Fetch current weather for a coordinate. Uses a process-wide shared HttpClient
// (created on first call) so the curl handle is configured once and its
// connection to the weather API is reused across calls.
WeatherData fetchWeather(float lat, float lon);

// Same, but against a caller-owned HttpClient. Create one at startup and pass it
// in when you want to control its lifetime or reuse the same client for other
// requests (e.g. vessel-data sync) too.
WeatherData fetchWeather(HttpClient& http, float lat, float lon);
