#include "servernav_sim.hpp"

#include <cmath>   // std::atan2, std::cos
#include <random>
#include <utility> // std::move
#include <vector>

#include <iostream>
#include <chrono> // clock

// Local clamp helpers. Deliberately hand-rolled rather than std::clamp: it
// keeps this TU from depending on <algorithm> and avoids ADL ambiguity with
// glm::clamp (glm.hpp is always in scope here).
namespace
{
inline int clampInt(int v, int lo, int hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}
inline float clampF(float v, float lo, float hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

// Bilinear blend of four WeatherData samples. Temperature is a plain scalar
// blend, but wind is interpolated as a *vector* (decompose by speed+direction,
// blend the components, recompose) so direction wraps correctly -- a naive lerp
// of 350 deg and 10 deg would wrongly sweep through 180. The decode/encode
// matches Vessel::speedKnots' convention: windVec = (-sin t, cos t) * speed.
// Weights: s00 at (1-fx,1-fy) ... s11 at (fx,fy).
WeatherData blendWeather(const WeatherData& s00, const WeatherData& s10,
                         const WeatherData& s01, const WeatherData& s11,
                         float fx, float fy)
{
    auto windVec = [](const WeatherData& d) {
        const float t = glm::radians(d.windDir);
        return glm::vec2(-glm::sin(t), glm::cos(t)) * d.windSpeed;
    };

    const float w00 = (1.f - fx) * (1.f - fy);
    const float w10 = fx * (1.f - fy);
    const float w01 = (1.f - fx) * fy;
    const float w11 = fx * fy;

    WeatherData out;
    out.temperature = w00 * s00.temperature + w10 * s10.temperature +
                      w01 * s01.temperature + w11 * s11.temperature;

    const glm::vec2 wv = w00 * windVec(s00) + w10 * windVec(s10) +
                         w01 * windVec(s01) + w11 * windVec(s11);
    out.windSpeed = glm::length(wv);
    // Inverse of windVec(): t = atan2(-x, y). Guard the zero-vector case.
    float dir = glm::degrees(std::atan2(-wv.x, wv.y));
    if (dir < 0.f)
        dir += 360.f;
    out.windDir = out.windSpeed > 1e-4f ? dir : 0.f;
    return out;
}
} // namespace

const WeatherCell& ServerNav::weatherAt(const glm::vec2& p) const
{
    int x = clampInt(static_cast<int>(p.x), 0, kGridSize - 1);
    int y = clampInt(static_cast<int>(p.y), 0, kGridSize - 1);
    return map[x][y];
}

int ServerNav::pickTarget(int vesselIndex) const
{
    int   best      = -1;
    float bestRatio = refuelThreshold; // only consider stations below threshold

    for (int i = 0; i < static_cast<int>(stations.size()); ++i)
    {
        const Station& s = stations[i];

        // Skip stations another vessel has already claimed.
        if (s.assignedVessel != -1 && s.assignedVessel != vesselIndex)
            continue;

        float r = s.fuelRatio();
        if (r < bestRatio)
        {
            bestRatio = r;
            best      = i;
        }
    }
    return best;
}

void ServerNav::update(float dt)
{
    // The sim runs on its own clock: simTimeStep scales real frame seconds
    // into sim seconds (1.0 = real time, larger = fast-forward). Everything
    // downstream — the clock, station drain, travel, burn — uses this.
    dt *= simTimeStep;
    if (dt <= 0.f)
        return;

    stats.simTime += dt;

    // 1) Stations drain over time.
    for (auto& s : stations)
    {
        s.fuel -= s.depleteRate * dt;
        if (s.fuel <= 0.f)
        {
            s.fuel = 0.f;
            stats.stationStarvedTime += dt; // count time spent empty
        }
    }

    // 2) Vessels navigate and refuel.
    long stranded = 0;
    for (int vi = 0; vi < static_cast<int>(vessels.size()); ++vi)
    {
        Vessel& v = vessels[vi];

        // Acquire a target if we don't already have a valid one.
        if (v.targetIndex < 0 || v.targetIndex >= static_cast<int>(stations.size()))
        {
            v.targetIndex = pickTarget(vi);
            if (v.targetIndex >= 0)
                stations[v.targetIndex].assignedVessel = vi;
        }
        if (v.targetIndex < 0)
            continue; // nothing needs fuel right now; idle

        Station&  tgt   = stations[v.targetIndex];
        glm::vec2 delta = tgt.pos - v.pos;
        float     dist  = glm::length(delta); // note: NOT vec2::length()

        // Docked? Top up the station and the vessel, then release the target.
        if (dist <= kArrivalRadius)
        {
            stats.totalFuelDelivered += (tgt.capacity - tgt.fuel);
            stats.refuelEvents += 1;

            tgt.fuel           = tgt.capacity;
            v.fuel             = v.maxFuel;
            tgt.assignedVessel = -1;
            v.targetIndex      = -1;
            continue;
        }

        // Out of fuel mid-transit: the vessel is stranded and can't move.
        if (v.burnRate > 0.f && v.fuel <= 0.f)
        {
            v.fuel = 0.f;
            ++stranded;
            continue;
        }

        // Storm-wrecked: a hull with no health left is dead in the water.
        if (v.health <= 0.f)
        {
            v.health = 0.f;
            ++stranded;
            continue;
        }

        // Move toward the target, slowed by local weather.
        glm::vec2          dir = delta / dist; // safe: dist > kArrivalRadius > 0
        v.dir                  = dir;          // remember heading for the wind calc
        const WeatherCell& w   = weatherAt(v.pos);

        // A stormy wind batters the hull as the vessel presses on through it.
        v.health -= v.stormDamagePerSec(w) * dt;
        if (v.health < 0.f)
            v.health = 0.f;

        // Speed over ground in knots -> world cells covered this sim-step.
        float knots  = v.speedKnots(w);
        float travel = (knots / (kCellDistanceNm * kSecondsPerHour)) * dt;

        if (travel > dist)
            travel = dist; // don't overshoot the station

        // Clamp travel to remaining fuel range.
        if (v.burnRate > 0.f)
        {
            float reach = v.fuel / v.burnRate;
            if (travel > reach)
                travel = reach;
        }

        v.pos += dir * travel;
        v.fuel -= travel * v.burnRate;
        if (v.fuel < 0.f)
            v.fuel = 0.f;

        // Keep the vessel inside the weather grid.
        v.pos.x = clampF(v.pos.x, 0.f, static_cast<float>(kGridSize - 1));
        v.pos.y = clampF(v.pos.y, 0.f, static_cast<float>(kGridSize - 1));
    }

    stats.strandedVessels = stranded;
}

float ServerNav::effectiveSpeedCells(const Vessel& v) const
{
    // No way when out of fuel or wrecked by a storm.
    if ((v.burnRate > 0.f && v.fuel <= 0.f) || v.health <= 0.f)
        return 0.f;
    return v.speedKnots(weatherAt(v.pos)) / (kCellDistanceNm * kSecondsPerHour);
}

float ServerNav::vesselSpeedKnots(const Vessel& v) const
{
    // Idle vessels (no target) aren't underway, so speed over ground is 0.
    if (v.targetIndex < 0 || v.targetIndex >= static_cast<int>(stations.size()))
        return 0.f;
    // cells/simsec -> nm/simsec (*kCellDistance) -> nm/hour (*3600) = knots.
    return effectiveSpeedCells(v) * kCellDistanceNm * kSecondsPerHour;
}

float ServerNav::vesselDistanceNm(const Vessel& v) const
{
    if (v.targetIndex < 0 || v.targetIndex >= static_cast<int>(stations.size()))
        return 0.f;
    const Station& tgt = stations[v.targetIndex];
    return glm::length(tgt.pos - v.pos) * kCellDistanceNm;
}

double ServerNav::vesselEtaSimTime(const Vessel& v) const
{
    if (v.targetIndex < 0 || v.targetIndex >= static_cast<int>(stations.size()))
        return -1.0; // no target: nowhere to arrive
    const float speedCells = effectiveSpeedCells(v);
    if (speedCells <= 0.f)
        return -1.0; // stranded / stalled: arrival time unknown
    const Station& tgt       = stations[v.targetIndex];
    const float    distCells = glm::length(tgt.pos - v.pos);
    return stats.simTime + distCells / speedCells; // absolute sim-clock seconds
}

void ServerNav::reset()
{
    stats = SimStats{};
    for (auto& s : stations)
        s.assignedVessel = -1;
    for (auto& v : vessels)
        v.targetIndex = -1;
}

ServerNav ServerNav::makeRandomScenario(int numStations, int numVessels, float timeStep, uint32_t seed)
{
    ServerNav sim;
    sim.simTimeStep = timeStep;

    std::mt19937                          rng(seed == 0 ? static_cast<unsigned>(time(nullptr)) : seed);
    std::uniform_real_distribution<float> posDist(0.f, static_cast<float>(kGridSize - 1));
    std::uniform_real_distribution<float> weatherDist(0.f, 2.f);
    std::uniform_real_distribution<float> fuelDist(0.2f, 1.0f);
    std::uniform_real_distribution<float> depleteDist(0.01f, 0.08f);

    // Buffer (in degrees) kept between the grid centre and the lat/long limits
    // We deliberately don't halve this, to leave a little extra slack on the limit
    float latitudeRange  = static_cast<float>(kGridSize) / 60.0f;
    float longitudeRange = static_cast<float>(kGridSize) / 60.0f;
    std::uniform_real_distribution<float> latitudeDist(-90.0f + latitudeRange, 90.f - latitudeRange);
    std::uniform_real_distribution<float> longitudeDist(-180.f + longitudeRange, 180.f - longitudeRange);

    auto currentTime = std::chrono::high_resolution_clock::now();
    double timeElapsed = 0;

    /*
    1 Degree of Latitude: Always equals 60 nautical miles.
    1 Degree of Longitude: Equals 60 nautical miles ONLY at the Equator. Otherwise 60 * cos(latitude).
    */
    // Weather field
    if (USING_RTS)
    {
        const float latitudeC  = latitudeDist(rng);
        const float longitudeC = longitudeDist(rng);

        // Half-span of the grid in degrees, about the centre. The grid is
        // kGridSize nm across; 1 deg latitude == 60 nm and 1 deg longitude ==
        // 60*cos(lat) nm, so the FULL spans are kGridSize/60 and
        // kGridSize/(60 cos lat) degrees -- halve them to reach +/- the centre.
        // (At kGridSize == 60 that's +/-0.5 deg latitude: a 1 deg total span.)
        const float latHalfDeg = (static_cast<float>(kGridSize) / 60.0f) * 0.5f;
        const float lonHalfDeg = (static_cast<float>(kGridSize) /
                                  (60.0f * std::cos(glm::radians(latitudeC)))) * 0.5f;

        // Grid cell -> geographic coordinate. Column x runs West->East
        // (longitude); row y runs North->South (latitude), matching the map
        // render (+x East, +y South). Clamp/wrap to valid ranges so an edge
        // cell near a pole can't hand fetchWeather an out-of-range request.
        auto cellLat = [&](int y) {
            float lat = latitudeC + latHalfDeg - (y / float(kGridSize - 1)) * (2.f * latHalfDeg);
            return clampF(lat, -90.f, 90.f);
        };
        auto cellLon = [&](int x) {
            float lon = longitudeC - lonHalfDeg + (x / float(kGridSize - 1)) * (2.f * lonHalfDeg);
            while (lon < -180.f) lon += 360.f;
            while (lon >= 180.f) lon -= 360.f;
            return lon;
        };

        // --- Coarse sampling + bilinear interpolation --------------------
        // Fetching real weather for every cell is kGridSize*kGridSize blocking
        // HTTP calls (~3600 at 60x60): slow, and open-meteo rate-limits the
        // burst into empty responses that crash the JSON parse. Instead fetch
        // only a coarse lattice (every kWeatherSampleStride cells, plus the far
        // edge) and bilinearly interpolate the gaps -- 49 calls at stride 10.
        constexpr int kWeatherSampleStride = 10;

        std::vector<int> sampleIdx;
        for (int i = 0; i < kGridSize; i += kWeatherSampleStride)
            sampleIdx.push_back(i);
        if (sampleIdx.back() != kGridSize - 1)
            sampleIdx.push_back(kGridSize - 1); // anchor the far edge

        // Fetch the real data only on the sample lattice.
        for (int sx : sampleIdx)
            for (int sy : sampleIdx)
            {
                auto newTime = std::chrono::high_resolution_clock::now();
                float dt = std::chrono::duration<float, std::chrono::seconds::period>(newTime - currentTime).count();
                currentTime = newTime;
                std::cout << "Processing weather data - " << (timeElapsed += dt) << " s elapsed\n";
                sim.map[sx][sy].data = fetchWeather(cellLat(sy), cellLon(sx));
            }

        // Bracket an index between the two sample lines around it, and give the
        // blend fraction. sampleIdx is sorted ascending.
        auto bracket = [&](int i, int& lo, int& hi, float& f) {
            lo = sampleIdx.front();
            hi = sampleIdx.back();
            for (std::size_t s = 0; s + 1 < sampleIdx.size(); ++s)
                if (i >= sampleIdx[s] && i <= sampleIdx[s + 1])
                {
                    lo = sampleIdx[s];
                    hi = sampleIdx[s + 1];
                    break;
                }
            f = (hi == lo) ? 0.f : float(i - lo) / float(hi - lo);
        };

        // Fill every cell by bilinear blend of its four surrounding samples.
        // Safe in place: the only cells ever read are sample-lattice cells, and
        // those reproduce themselves exactly, so they're never disturbed.
        for (int x = 0; x < kGridSize; ++x)
        {
            int x0, x1; float fx;
            bracket(x, x0, x1, fx);
            for (int y = 0; y < kGridSize; ++y)
            {
                int y0, y1; float fy;
                bracket(y, y0, y1, fy);
                sim.map[x][y].data = blendWeather(
                    sim.map[x0][y0].data, sim.map[x1][y0].data,
                    sim.map[x0][y1].data, sim.map[x1][y1].data,
                    fx, fy);
            }
        }
    }
    else
    {
        for (int x = 0; x < kGridSize; ++x)
            for (int y = 0; y < kGridSize; ++y)
                sim.map[x][y].weight = weatherDist(rng);
    }

    // Stations.
    sim.stations.reserve(numStations);
    for (int i = 0; i < numStations; ++i)
    {
        Station s;
        s.name        = "S" + std::to_string(i);
        s.pos         = glm::vec2(posDist(rng), posDist(rng));
        s.capacity    = 1.f;
        s.fuel        = fuelDist(rng) * s.capacity;
        s.depleteRate = depleteDist(rng);
        sim.stations.push_back(std::move(s));
    }

    // Vessels. maxFuel / burnRate chosen so a full tank easily crosses the
    // map (range = maxFuel / burnRate = 400 units vs. ~70 unit diagonal),
    // so vessels don't strand under the default scenario.
    sim.vessels.reserve(numVessels);
    for (int i = 0; i < numVessels; ++i)
    {
        Vessel v;
        v.id       = i;
        v.pos      = glm::vec2(posDist(rng), posDist(rng));
        // Reference barge: kReferenceThrust engine + kReferenceWeightLbs hull
        // => ~7 kn cruise (up to ~9 kn with a strong tailwind). Weight will
        // vary per vessel later; for now every vessel is the reference boat.
        v.speed    = kReferenceThrust;
        v.weight   = kReferenceWeightLbs;
        v.maxFuel  = 1.f;
        v.fuel     = v.maxFuel;
        v.burnRate = 0.0025f;
        sim.vessels.push_back(v);
    }

    return sim; // NRVO
}

// ServerNav ServerNav::makeStructuredScenario(WorldCoords start, WorldCoords dest, float timeStep) {
//     ServerNav sim;
//     sim.simTimeStep = timeStep;

//     // Buffer (in degrees) kept between the grid centre and the lat/long limits
//     // We deliberately don't halve this, to leave a little extra slack on the limit
//     float latitudeRange  = static_cast<float>(kGridSize) / 60.0f;
//     float longitudeRange = static_cast<float>(kGridSize) / 60.0f;
//     std::uniform_real_distribution<float> latitudeDist(-90.0f + latitudeRange, 90.f - latitudeRange);
//     std::uniform_real_distribution<float> longitudeDist(-180.f + longitudeRange, 180.f - longitudeRange);

//     auto currentTime = std::chrono::high_resolution_clock::now();
//     double timeElapsed = 0;

//     /*
//     1 Degree of Latitude: Always equals 60 nautical miles.
//     1 Degree of Longitude: Equals 60 nautical miles ONLY at the Equator. Otherwise 60 * cos(latitude).
//     */
//     // Weather field
//     if (USING_RTS)
//     {
//         const float latitudeC  = latitudeDist(rng);
//         const float longitudeC = longitudeDist(rng);

//         // Half-span of the grid in degrees, about the centre. The grid is
//         // kGridSize nm across; 1 deg latitude == 60 nm and 1 deg longitude ==
//         // 60*cos(lat) nm, so the FULL spans are kGridSize/60 and
//         // kGridSize/(60 cos lat) degrees -- halve them to reach +/- the centre.
//         // (At kGridSize == 60 that's +/-0.5 deg latitude: a 1 deg total span.)
//         const float latHalfDeg = (static_cast<float>(kGridSize) / 60.0f) * 0.5f;
//         const float lonHalfDeg = (static_cast<float>(kGridSize) /
//                                   (60.0f * std::cos(glm::radians(latitudeC)))) * 0.5f;

//         // Grid cell -> geographic coordinate. Column x runs West->East
//         // (longitude); row y runs North->South (latitude), matching the map
//         // render (+x East, +y South). Clamp/wrap to valid ranges so an edge
//         // cell near a pole can't hand fetchWeather an out-of-range request.
//         auto cellLat = [&](int y) {
//             float lat = latitudeC + latHalfDeg - (y / float(kGridSize - 1)) * (2.f * latHalfDeg);
//             return clampF(lat, -90.f, 90.f);
//         };
//         auto cellLon = [&](int x) {
//             float lon = longitudeC - lonHalfDeg + (x / float(kGridSize - 1)) * (2.f * lonHalfDeg);
//             while (lon < -180.f) lon += 360.f;
//             while (lon >= 180.f) lon -= 360.f;
//             return lon;
//         };

//         // --- Coarse sampling + bilinear interpolation --------------------
//         // Fetching real weather for every cell is kGridSize*kGridSize blocking
//         // HTTP calls (~3600 at 60x60): slow, and open-meteo rate-limits the
//         // burst into empty responses that crash the JSON parse. Instead fetch
//         // only a coarse lattice (every kWeatherSampleStride cells, plus the far
//         // edge) and bilinearly interpolate the gaps -- 49 calls at stride 10.
//         constexpr int kWeatherSampleStride = 10;

//         std::vector<int> sampleIdx;
//         for (int i = 0; i < kGridSize; i += kWeatherSampleStride)
//             sampleIdx.push_back(i);
//         if (sampleIdx.back() != kGridSize - 1)
//             sampleIdx.push_back(kGridSize - 1); // anchor the far edge

//         // Fetch the real data only on the sample lattice.
//         for (int sx : sampleIdx)
//             for (int sy : sampleIdx)
//             {
//                 auto newTime = std::chrono::high_resolution_clock::now();
//                 float dt = std::chrono::duration<float, std::chrono::seconds::period>(newTime - currentTime).count();
//                 currentTime = newTime;
//                 std::cout << "Processing weather data - " << (timeElapsed += dt) << " s elapsed\n";
//                 sim.map[sx][sy].data = fetchWeather(cellLat(sy), cellLon(sx));
//             }

//         // Bracket an index between the two sample lines around it, and give the
//         // blend fraction. sampleIdx is sorted ascending.
//         auto bracket = [&](int i, int& lo, int& hi, float& f) {
//             lo = sampleIdx.front();
//             hi = sampleIdx.back();
//             for (std::size_t s = 0; s + 1 < sampleIdx.size(); ++s)
//                 if (i >= sampleIdx[s] && i <= sampleIdx[s + 1])
//                 {
//                     lo = sampleIdx[s];
//                     hi = sampleIdx[s + 1];
//                     break;
//                 }
//             f = (hi == lo) ? 0.f : float(i - lo) / float(hi - lo);
//         };

//         // Fill every cell by bilinear blend of its four surrounding samples.
//         // Safe in place: the only cells ever read are sample-lattice cells, and
//         // those reproduce themselves exactly, so they're never disturbed.
//         for (int x = 0; x < kGridSize; ++x)
//         {
//             int x0, x1; float fx;
//             bracket(x, x0, x1, fx);
//             for (int y = 0; y < kGridSize; ++y)
//             {
//                 int y0, y1; float fy;
//                 bracket(y, y0, y1, fy);
//                 sim.map[x][y].data = blendWeather(
//                     sim.map[x0][y0].data, sim.map[x1][y0].data,
//                     sim.map[x0][y1].data, sim.map[x1][y1].data,
//                     fx, fy);
//             }
//         }
//     }

//     int numStations = 1, numVessels = 1;

//     // Stations.
//     sim.stations.reserve(numStations);
//     for (int i = 0; i < numStations; ++i)
//     {
//         Station s;
//         s.name        = "S" + std::to_string(i);
//         s.pos         = glm::vec2(posDist(rng), posDist(rng));
//         s.capacity    = 1.f;
//         s.fuel        = fuelDist(rng) * s.capacity;
//         s.depleteRate = depleteDist(rng);
//         sim.stations.push_back(std::move(s));
//     }

//     // Vessels. maxFuel / burnRate chosen so a full tank easily crosses the
//     // map (range = maxFuel / burnRate = 400 units vs. ~70 unit diagonal),
//     // so vessels don't strand under the default scenario.
//     sim.vessels.reserve(numVessels);
//     for (int i = 0; i < numVessels; ++i)
//     {
//         Vessel v;
//         v.id       = i;
//         v.pos      = glm::vec2(posDist(rng), posDist(rng));
//         // Reference barge: kReferenceThrust engine + kReferenceWeightLbs hull
//         // => ~7 kn cruise (up to ~9 kn with a strong tailwind). Weight will
//         // vary per vessel later; for now every vessel is the reference boat.
//         v.speed    = kReferenceThrust;
//         v.weight   = kReferenceWeightLbs;
//         v.maxFuel  = 1.f;
//         v.fuel     = v.maxFuel;
//         v.burnRate = 0.0025f;
//         sim.vessels.push_back(v);
//     }

//     return sim; // NRVO
// }
