
#pragma once

struct WeatherData
{
    float temperature;
    float windSpeed;
    float windDir;
};

WeatherData fetchWeather(float lat, float lon);