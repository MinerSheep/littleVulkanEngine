#include "servernav_sim.hpp"

#include <random>
#include <utility> // std::move

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

        // Move toward the target, slowed by local weather.
        glm::vec2 dir    = delta / dist; // safe: dist > kArrivalRadius > 0
        float     mod    = v.getSpeedModifier(weatherAt(v.pos));
        float     travel = v.speed * mod * dt;

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

void ServerNav::reset()
{
    stats = SimStats{};
    for (auto& s : stations)
        s.assignedVessel = -1;
    for (auto& v : vessels)
        v.targetIndex = -1;
}

ServerNav ServerNav::makeRandomScenario(int numStations, int numVessels, uint32_t seed)
{
    ServerNav sim;

    std::mt19937                          rng(seed == 0 ? static_cast<unsigned>(time(nullptr)) : seed);
    std::uniform_real_distribution<float> posDist(0.f, static_cast<float>(kGridSize - 1));
    std::uniform_real_distribution<float> weatherDist(0.f, 2.f);
    std::uniform_real_distribution<float> fuelDist(0.2f, 1.0f);
    std::uniform_real_distribution<float> depleteDist(0.01f, 0.08f);
    std::uniform_real_distribution<float> speedDist(3.f, 8.f);

    float latitudeRange = 2.0f, longitudeRange = 1.0f;
    std::uniform_real_distribution<float> latitudeDist(-90.0f + latitudeRange, 90.f - latitudeRange);
    std::uniform_real_distribution<float> longitudeDist(-180.f + longitudeRange, 180.f - longitudeRange);


    // Weather field.
    if (USING_RTS)
    {
        float latitudeC = latitudeDist(rng);
        float longitudeC = longitudeDist(rng);

        for (int x = 0; x < kGridSize; ++x)
        {
            float longitude = longitudeC - longitudeRange + (x / float(kGridSize - 1)) * longitudeRange * 2.0f;
            for (int y = 0; y < kGridSize; ++y)
            {
                float latitude = latitudeC - 2 + (x / float(kGridSize - 1)) * latitudeRange * 2.0f;
                sim.map[x][y].data = fetchWeather(latitude, longitude);
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
        v.speed    = speedDist(rng);
        v.maxFuel  = 1.f;
        v.fuel     = v.maxFuel;
        v.burnRate = 0.0025f;
        sim.vessels.push_back(v);
    }

    return sim; // NRVO
}
