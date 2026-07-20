#include "servernav_sim.hpp"

#include "landmask.hpp"    // isLand(lat, lon) -- geographic land lookup
#include "pathfinding.hpp" // pathfinding::findPath -- route around land

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

// Wind as a velocity vector. Interpolating wind by speed+direction directly is
// wrong: a naive lerp of 350 deg and 10 deg sweeps through 180. Instead we
// decompose to a vector, blend the components, then recompose (storeWind). The
// encoding windVec = (-sin t, cos t) * speed matches Vessel::speedKnots'.
inline glm::vec2 windVec(const WeatherData& d)
{
    const float t = glm::radians(d.windDir);
    return glm::vec2(-glm::sin(t), glm::cos(t)) * d.windSpeed;
}

// Store a blended wind velocity back onto a WeatherData as speed + direction.
// The angle is the inverse of windVec(): t = atan2(-x, y); the zero-vector case
// is pinned to a defined direction of 0.
inline void storeWind(WeatherData& out, const glm::vec2& wv)
{
    out.windSpeed = glm::length(wv);
    float dir = glm::degrees(std::atan2(-wv.x, wv.y));
    if (dir < 0.f)
        dir += 360.f;
    out.windDir = out.windSpeed > 1e-4f ? dir : 0.f;
}

// Bilinear blend of four WeatherData samples: temperature as a scalar, wind as
// a vector (see windVec). Weights: s00 at (1-fx,1-fy) ... s11 at (fx,fy).
WeatherData blendWeather(const WeatherData& s00, const WeatherData& s10,
                         const WeatherData& s01, const WeatherData& s11,
                         float fx, float fy)
{
    const float w00 = (1.f - fx) * (1.f - fy);
    const float w10 = fx * (1.f - fy);
    const float w01 = (1.f - fx) * fy;
    const float w11 = fx * fy;

    WeatherData out;
    out.temperature = w00 * s00.temperature + w10 * s10.temperature +
                      w01 * s01.temperature + w11 * s11.temperature;
    storeWind(out, w00 * windVec(s00) + w10 * windVec(s10) +
                       w01 * windVec(s01) + w11 * windVec(s11));
    return out;
}

// Linear blend of two WeatherData samples at t in [0,1] (0 => a, 1 => b), using
// the same scalar-temperature / vector-wind scheme as blendWeather.
WeatherData lerpWeather(const WeatherData& a, const WeatherData& b, float t)
{
    WeatherData out;
    out.temperature = (1.f - t) * a.temperature + t * b.temperature;
    storeWind(out, (1.f - t) * windVec(a) + t * windVec(b));
    return out;
}

// Drop a few round synthetic islands onto the map's land mask. Used for the
// non-geographic scenarios (which have no real coordinates to query the
// elevation service with) so the land-avoidance routing still has something to
// steer around. Islands are kept in the interior and small enough that open
// water stays a single connected region, so any two water cells remain
// reachable from each other.
void generateSyntheticLand(WeatherCell map[kGridSize][kGridSize], std::mt19937& rng)
{
    std::uniform_real_distribution<float> centreDist(kGridSize * 0.25f, kGridSize * 0.75f);
    std::uniform_real_distribution<float> radiusDist(kGridSize * 0.08f, kGridSize * 0.15f);
    std::uniform_int_distribution<int>    countDist(1, 3);

    const int islands = countDist(rng);
    for (int n = 0; n < islands; ++n)
    {
        const glm::vec2 centre(centreDist(rng), centreDist(rng));
        const float     radius  = radiusDist(rng);
        const float     radiusSq = radius * radius;
        for (int x = 0; x < kGridSize; ++x)
            for (int y = 0; y < kGridSize; ++y)
            {
                const glm::vec2 d = glm::vec2(x, y) - centre;
                if (glm::dot(d, d) <= radiusSq)
                    map[x][y].land = true;
            }
    }
}
} // namespace

const WeatherCell& ServerNav::weatherAt(const glm::vec2& p) const
{
    int x = clampInt(static_cast<int>(p.x), 0, kGridSize - 1);
    int y = clampInt(static_cast<int>(p.y), 0, kGridSize - 1);
    return map[x][y];
}

bool ServerNav::isBlockedCell(int x, int y) const
{
    // Off-grid counts as land so a route can never step outside the map.
    if (x < 0 || y < 0 || x >= kGridSize || y >= kGridSize)
        return true;
    return map[x][y].land;
}

bool ServerNav::isLandAt(const glm::vec2& p) const
{
    int x = clampInt(static_cast<int>(p.x), 0, kGridSize - 1);
    int y = clampInt(static_cast<int>(p.y), 0, kGridSize - 1);
    return map[x][y].land;
}

bool ServerNav::segmentHitsLand(const glm::vec2& a, const glm::vec2& b) const
{
    const glm::vec2 d   = b - a;
    const float     len = glm::length(d);
    if (len < 1e-4f)
        return isLandAt(a);

    // Walk the segment at roughly one sample per cell; enough to catch any land
    // cell the straight line would clip without an exact grid traversal.
    const int steps = static_cast<int>(len) + 1;
    for (int i = 0; i <= steps; ++i)
    {
        const float t = static_cast<float>(i) / static_cast<float>(steps);
        if (isLandAt(a + d * t))
            return true;
    }
    return false;
}

std::vector<glm::vec2> ServerNav::planRoute(const glm::vec2& from, const glm::vec2& to) const
{
    const glm::ivec2 start{clampInt(static_cast<int>(from.x), 0, kGridSize - 1),
                           clampInt(static_cast<int>(from.y), 0, kGridSize - 1)};
    const glm::ivec2 goal{clampInt(static_cast<int>(to.x), 0, kGridSize - 1),
                          clampInt(static_cast<int>(to.y), 0, kGridSize - 1)};

    const pathfinding::Path cells = pathfinding::findPath(
        kGridSize, start, goal,
        [this](int x, int y) { return isBlockedCell(x, y); });

    std::vector<glm::vec2> route;
    route.reserve(cells.size());
    for (const glm::ivec2& c : cells)
        route.push_back(glm::vec2(c)); // cell index == world position of that cell
    return route;
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
            v.path.clear(); // route consumed; the next target plans a fresh one
            v.pathCursor      = 0;
            v.pathTarget      = -1;
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

        // --- Route around land ----------------------------------------
        // Drop any stale route if the vessel just switched targets, then plan a
        // detour only when the direct line to the station is actually blocked by
        // land (open water keeps the original straight-line behaviour). A failed
        // plan leaves path empty, so the vessel falls back to steering straight.
        if (v.pathTarget != v.targetIndex)
        {
            v.path.clear();
            v.pathCursor = 0;
            v.pathTarget = v.targetIndex;
        }
        if (v.path.empty() && segmentHitsLand(v.pos, tgt.pos))
        {
            v.path       = planRoute(v.pos, tgt.pos);
            v.pathCursor = 0;
        }

        // Immediate steering goal: the next unreached waypoint, or the station
        // itself once the route is exhausted (or was never needed).
        glm::vec2 steerTo = tgt.pos;
        while (v.pathCursor < static_cast<int>(v.path.size()))
        {
            const glm::vec2& wp = v.path[v.pathCursor];
            if (glm::length(wp - v.pos) > kWaypointRadius)
            {
                steerTo = wp; // still making for this waypoint
                break;
            }
            ++v.pathCursor; // reached it; advance (falls through to station if last)
        }

        glm::vec2 stepDelta = steerTo - v.pos;
        float     stepDist  = glm::length(stepDelta);
        if (stepDist < 1e-5f)
            continue; // already sitting on the steer target this step

        // Move toward the steer target, slowed by local weather.
        glm::vec2          dir = stepDelta / stepDist;
        v.dir                  = dir;          // remember heading for the wind calc
        const WeatherCell& w   = weatherAt(v.pos);

        // A stormy wind batters the hull as the vessel presses on through it.
        v.health -= v.stormDamagePerSec(w) * dt;
        if (v.health < 0.f)
            v.health = 0.f;

        // Speed over ground in knots -> world cells covered this sim-step.
        float knots  = v.speedKnots(w);
        float travel = (knots / (cellDistanceNm * kSecondsPerHour)) * dt;

        if (travel > stepDist)
            travel = stepDist; // don't overshoot the waypoint / station

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
    return v.speedKnots(weatherAt(v.pos)) / (cellDistanceNm * kSecondsPerHour);
}

float ServerNav::vesselSpeedKnots(const Vessel& v) const
{
    // Idle vessels (no target) aren't underway, so speed over ground is 0.
    if (v.targetIndex < 0 || v.targetIndex >= static_cast<int>(stations.size()))
        return 0.f;
    // cells/simsec -> nm/simsec (*cellDistanceNm) -> nm/hour (*3600) = knots.
    return effectiveSpeedCells(v) * cellDistanceNm * kSecondsPerHour;
}

float ServerNav::vesselDistanceNm(const Vessel& v) const
{
    if (v.targetIndex < 0 || v.targetIndex >= static_cast<int>(stations.size()))
        return 0.f;
    const Station& tgt = stations[v.targetIndex];
    return glm::length(tgt.pos - v.pos) * cellDistanceNm;
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
    {
        v.targetIndex = -1;
        v.path.clear();
        v.pathCursor = 0;
        v.pathTarget = -1;
    }
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
                // Land mask, sampled on the same coarse lattice (an elevation
                // lookup is one HTTP call each, same rate limits as weather).
                sim.map[sx][sy].land = isLand(cellLat(sy), cellLon(sx));
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
                // Land is boolean, so blend by nearest sample (blocky coastline)
                // rather than interpolating. Reads only sample cells, so it is
                // safe in place alongside the weather fill above.
                const int nx = (fx < 0.5f) ? x0 : x1;
                const int ny = (fy < 0.5f) ? y0 : y1;
                sim.map[x][y].land = sim.map[nx][ny].land;
            }
        }
    }
    else
    {
        for (int x = 0; x < kGridSize; ++x)
            for (int y = 0; y < kGridSize; ++y)
                sim.map[x][y].weight = weatherDist(rng);

        // No real coordinates in this mode, so synthesize a few islands for the
        // land-avoidance routing to steer around.
        generateSyntheticLand(sim.map, rng);
    }

    // Draw a random position that isn't on land, so vessels and stations never
    // spawn inside an island (which would blockade their own pathfinding). Falls
    // back to the last draw after a bounded number of tries.
    auto waterPos = [&]() {
        glm::vec2 p(posDist(rng), posDist(rng));
        for (int tries = 0; tries < 64 && sim.isLandAt(p); ++tries)
            p = glm::vec2(posDist(rng), posDist(rng));
        return p;
    };

    // Stations.
    sim.stations.reserve(numStations);
    for (int i = 0; i < numStations; ++i)
    {
        Station s;
        s.name        = "S" + std::to_string(i);
        s.pos         = waterPos();
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
        v.pos      = waterPos();
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

ServerNav ServerNav::makeStructuredScenario(WorldCoords start, WorldCoords dest, float timeStep)
{
    ServerNav sim;
    sim.simTimeStep = timeStep;

    // The grid's near corner (0,0) maps to `start` -- where the vessel begins --
    // and its far corner (max,max) to `dest` -- where the station sits. So a
    // straight run to the station is the (0,0)->(max,max) diagonal.
    const int maxIdx = kGridSize - 1;

    /*
    1 Degree of Latitude: Always equals 60 nautical miles.
    1 Degree of Longitude: Equals 60 nautical miles ONLY at the Equator. Otherwise 60 * cos(latitude).
    */
    // Scale the grid to the real world. The distance between the two coordinates
    // is length(dest - start) degrees * 60 nm/deg (we treat a degree of
    // longitude as 60 nm too -- a simplification; it's really 60*cos(lat)). That
    // real distance is spread along the (0,0)->(max,max) diagonal, so one cell
    // measures routeNm / diagonalCells nautical miles.
    const glm::vec2 degDelta(dest.latitude - start.latitude,
                             dest.longitude - start.longitude);
    const float routeNm   = glm::length(degDelta) * 60.0f;
    const float diagCells = glm::length(glm::vec2(static_cast<float>(maxIdx)));
    if (diagCells > 0.f)
        sim.cellDistanceNm = routeNm / diagCells;

    // Weather field. Rather than fetch real weather per cell, sample it at a few
    // points of interest along the start->dest route and piecewise-lerp across
    // the grid: a cell's weather is the blend of the two nearest samples by how
    // far along the start->dest diagonal it lies (0 at the start corner, 1 at
    // the dest corner). For now the samples are start, midpoint, and dest.
    if (USING_RTS)
    {
        // Midpoint (and, later, other waypoints) between the endpoints.
        const WorldCoords mid{(start.latitude + dest.latitude) * 0.5f,
                              (start.longitude + dest.longitude) * 0.5f};

        // Ordered start -> ... -> dest. Add more waypoints here to sharpen the
        // field; the piecewise lerp below adapts to however many there are.
        const std::vector<WeatherData> samples = {
            fetchWeather(start.latitude, start.longitude),
            fetchWeather(mid.latitude, mid.longitude),
            fetchWeather(dest.latitude, dest.longitude),
        };
        const int segCount = static_cast<int>(samples.size()) - 1; // >= 1

        for (int x = 0; x < kGridSize; ++x)
            for (int y = 0; y < kGridSize; ++y)
            {
                // Diagonal progress: the projection of (x,y) onto the (1,1)
                // diagonal, normalized so start corner = 0, dest corner = 1.
                const float t = maxIdx > 0 ? float(x + y) / float(2 * maxIdx) : 0.f;

                // Map t onto the piecewise path: which segment it falls in, and
                // the local blend fraction within that segment.
                const float scaled = t * static_cast<float>(segCount); // [0, segCount]
                int         seg    = static_cast<int>(scaled);
                if (seg >= segCount)
                    seg = segCount - 1; // pin the t == 1 endpoint into the last segment
                const float localT = scaled - static_cast<float>(seg);

                sim.map[x][y].data = lerpWeather(samples[seg], samples[seg + 1], localT);
            }
    }
    else
    {
        for (int x = 0; x < kGridSize; ++x)
            for (int y = 0; y < kGridSize; ++y)
                sim.map[x][y].weight = 0.f; // calm water
    }

    // One station at the far (dest) corner, kept low on fuel so the vessel is
    // dispatched to it immediately (pickTarget only chases stations whose fuel
    // ratio is below refuelThreshold).
    Station s;
    s.name        = "S0";
    s.pos         = glm::vec2(static_cast<float>(maxIdx));
    s.capacity    = 1.f;
    s.fuel        = 0.1f;
    s.depleteRate = 0.02f;
    sim.stations.push_back(std::move(s));

    // One vessel at the near (start) corner. maxFuel / burnRate give ~400 cells
    // of range -- far more than the ~85-cell grid diagonal -- so it won't strand
    // crossing to the station. Reference barge: ~7 kn cruise in calm air.
    Vessel v;
    v.id       = 0;
    v.pos      = glm::vec2(0.f);
    v.speed    = kReferenceThrust;
    v.weight   = kReferenceWeightLbs;
    v.maxFuel  = 1.f;
    v.fuel     = v.maxFuel;
    v.burnRate = 0.0025f;
    sim.vessels.push_back(v);

    return sim; // NRVO
}
