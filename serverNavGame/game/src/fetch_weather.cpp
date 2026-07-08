// #include "fetch_weather.hpp"

// #include <curl/curl.h>
// #include <nlohmann/json.hpp>

// static size_t writeCallback(void* contents, size_t size, size_t nmemb, std::string* out) {
//     out->append((char*)contents, size * nmemb);
//     return size * nmemb;
// }

// WeatherData fetchWeather(float lat, float lon) {
//     std::string url = "https://api.open-meteo.com/v1/forecast?latitude=" +
//                       std::to_string(lat) +
//                       "&longitude=" + std::to_string(lon) +
//                       "&current_weather=true";

//     CURL* curl = curl_easy_init();
//     std::string response;

//     curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
//     curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
//     curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
//     curl_easy_perform(curl);
//     curl_easy_cleanup(curl);

//     auto json = nlohmann::json::parse(response);
//     auto cw = json["current_weather"];

//     WeatherData data;
//     data.temperature = cw["temperature"];
//     data.windSpeed   = cw["windspeed"];
//     data.windDir     = cw["winddirection"];
//     return data;
// }
