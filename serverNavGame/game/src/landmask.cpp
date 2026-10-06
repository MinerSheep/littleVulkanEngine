#include "landmask.hpp"

#include "http_client.hpp"

#include <nlohmann/json.hpp>

#include <cmath> // std::isfinite
#include <string>
#include <iostream>

namespace
{
HttpClient& sharedElevationClient()
{
    static HttpClient client; // default 64 KB response cap
    return client;
}
} // namespace

bool isLand(float lat, float lon)
{
    return isLand(sharedElevationClient(), lat, lon);
}

bool isLand(HttpClient& http, float lat, float lon)
{
    // 1) Sanitize inputs: only ever request finite, in-range coordinates so a
    //    bad caller value can't produce "nan"/out-of-range URLs. On failure we
    //    fall through to "open water" (return false) so navigation is never
    //    blocked by a phantom obstacle.
    if (!std::isfinite(lat) || !std::isfinite(lon))
    {
        std::cout << "err non-finite coordinates\n";
        return false;
    }
    lat = lat < -90.f ? -90.f : (lat > 90.f ? 90.f : lat);
    lon = lon < -180.f ? -180.f : (lon > 180.f ? 180.f : lon);

    const std::string url =
        "https://api.open-meteo.com/v1/elevation?latitude=" + std::to_string(lat) +
        "&longitude=" + std::to_string(lon);

    // 2) The client owns all the transport concerns (timeouts, TLS, HTTPS-only,
    //    redirect and response-size limits, treating HTTP >= 400 as failure).
    const HttpClient::Response res = http.get(url);
    if (!res || res.body.empty())
    {
        std::cout << "err elevation request failed for " << url
                  << " (HTTP status " << std::to_string(res.status) + ")\n";
        return false;
    }

    // 3) Parse WITHOUT exceptions; bail to "open water" on any malformed JSON.
    const nlohmann::json json =
        nlohmann::json::parse(res.body, /*cb=*/nullptr, /*allow_exceptions=*/false);
    if (json.is_discarded() || !json.contains("elevation"))
    {
        std::cout << "err elevation field missing\n";
        return false;
    }

    // 4) open-meteo returns elevation as a one-element array
    //    ({"elevation":[123.0]}); tolerate a bare number too.
    const nlohmann::json& elev = json["elevation"];
    float meters = 0.f;
    if (elev.is_array() && !elev.empty() && elev.front().is_number())
        meters = elev.front().get<float>();
    else if (elev.is_number())
        meters = elev.get<float>();
    else
    {
        std::cout << "err elevation not a number\n";
        return false;
    }
    if (!std::isfinite(meters))
        return false;

    // Ocean cells report 0 m in the DEM; anything above sea level is land.
    return meters > 0.f;
}
