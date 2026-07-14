#pragma once

// ServerNav simulation
// --------------------
// A fleet of vessels ("refuellers") roams a weather-covered grid, topping up
// stations whose fuel is draining over time. A vessel picks the neediest
// station, travels toward it (slowed by local weather, burning its own fuel),
// and on arrival tops up both the station and itself.
//
// This header is intentionally self-contained: it only depends on glm (for
// vec2) and the standard library, so it can be driven by a tiny main() or by
// the accompanying benchmark without pulling in the rest of the engine.

#define USING_RTS 1
#include "fetch_weather.hpp"

#include <cstdint>
#include <string>
#include <vector>

#include <glm/glm.hpp> // glm::vec2 + glm::length / glm::distance

// World is a fixed kGridSize x kGridSize field of weather cells. Positions are
// expressed in the same units, i.e. in the range [0, kGridSize).
constexpr int   kGridSize      = 60;
constexpr float kCellDistanceNm  = 1;   // distance a singular cell measures in nautical miles
constexpr float kArrivalRadius = 0.5f; // distance at which a vessel "docks"

struct WorldCoords
{
    float latitude;
    float longitude;
};

// --- Vessel speed model ---------------------------------------------------
// A reference vessel (kReferenceThrust engine power, kReferenceWeightLbs hull
// weight) cruises at kReferenceCruiseKnots in clear water. Speed scales up
// with thrust and down with weight, both linear from the reference vessel,
// so a heavier hull is slower. Wind then adds/removes up to
// kMaxWindBonusKnots on top. These numbers are tuned so the default 10,000 lb
// barge makes ~7 kn in calm air and ~9 kn with a strong tailwind.
constexpr float kReferenceCruiseKnots = 7.0f;    // clear-water cruise, reference barge vessel
constexpr float kReferenceWeightLbs   = 10000.f; // barge vessel w hull weight 10000 lbs

constexpr float kReferenceThrust      = 5.0f;    // engine power that yields the above
constexpr float kMaxWindBonusKnots    = 2.0f;    // 7->9 kn tailwind, 7->5 kn headwind
constexpr float kReferenceWindKmh     = 40.f;    // wind speed that gives the full bonus
constexpr float kMinKnots             = 0.5f;    // floor so a headwind can't stall a vessel

constexpr float kSecondsPerHour       = 3600.f;  // sim-seconds per sim-hour (knots <-> cells)

// --- Storm damage model ---------------------------------------------------
// Wind at or above kStormWindKmh (km/h, ~23 kn) is "stormy" and batters a
// hull. A vessel pressing on through it loses health in proportion to how far
// the wind exceeds the threshold; when health hits zero the vessel is dead in
// the water and stops moving. windSpeed comes from real weather, so this only
// bites in USING_RTS mode (non-RTS maps carry no wind speed).
constexpr float kStormWindKmh      = 30.f;   // stormy-level wind speed
constexpr float kStormDamagePerKmh = 0.01f;  // health/sim-sec lost per km/h over the threshold

struct Station
{
    std::string name;
    glm::vec2   pos{0.f};

    float fuel        = 1.f;   // current fuel, in [0, capacity]
    float capacity    = 1.f;   // full tank
    float depleteRate = 0.05f; // fuel lost per second

    // Index of the vessel currently en route to this station, or -1 if none.
    // Prevents several vessels from piling onto the same station.
    int assignedVessel = -1;

    float fuelRatio() const { return capacity > 0.f ? fuel / capacity : 1.f; }
    bool  isEmpty() const { return fuel <= 0.f; }
};

struct WeatherCell
{
    float weight = 0.f; // 0 = clear; larger = heavier weather = slower travel

    WeatherData data;
};

struct Vessel
{
  using id_t = unsigned int;

  id_t id;
  glm::vec2 pos{0.f};
  glm::vec2 dir{0.f};    // current heading, a unit vector; refreshed while travelling

  float speed = kReferenceThrust; // engine power/thrust; with weight sets cruise
  float weight = kReferenceWeightLbs; // hull weight in lbs; heavier = slower
  float fuel = 1.f;      // current fuel
  float maxFuel = 1.f;   // full tank
  float burnRate = 0.f;  // fuel consumed per world unit travelled (0 = free)
  float health = 100.f;    // structural integrity; storms chip it away
  float maxHealth = 100.f; // full hull

  // Index into ServerNav::stations of the current destination, or -1.
  int targetIndex = -1;

  // Clear-water cruise speed in knots. More thrust speeds the vessel up; more
  // hull weight slows it down (both linear about the reference vessel). Weight
  // is constant for now but is meant to vary per vessel later.
  float cruiseKnots() const {
    return kReferenceCruiseKnots
           * (speed / kReferenceThrust)
           * (kReferenceWeightLbs / weight);
  }

  // Speed over ground in knots given the local weather. A tailwind adds up to
  // kMaxWindBonusKnots (scaled by how strong it is), a headwind subtracts as
  // much; the result is floored at kMinKnots so weather can slow but never
  // stall or reverse the vessel.
  float speedKnots(const WeatherCell& w) const {
    float knots = cruiseKnots();
    if (USING_RTS)
    {
        // windDir is degrees in [0,360): 0 = wind blowing due South,
        // 90 = wind blowing due West. World/screen axes match the map render:
        // +x = East (right), +y = South (down), so the unit wind vector for
        // angle t is (-sin t, cos t).
        const float     t       = glm::radians(w.data.windDir);
        const glm::vec2 windVec = {-glm::sin(t), glm::cos(t)};

        // dir is the vessel's heading. Sailing with the wind (dot > 0) is a
        // tailwind and speeds it up; heading into it (dot < 0) is a headwind.
        const float alignment = glm::dot(dir, windVec); // [-1, 1]

        // windSpeed is km/h (open-meteo). Scale into a knot bonus and clamp so
        // even a gale only nudges the vessel by +/- kMaxWindBonusKnots.
        float bonus = kMaxWindBonusKnots * alignment * (w.data.windSpeed / kReferenceWindKmh);
        if (bonus >  kMaxWindBonusKnots) bonus =  kMaxWindBonusKnots;
        if (bonus < -kMaxWindBonusKnots) bonus = -kMaxWindBonusKnots;
        knots += bonus;
    }
    else
    {
        knots *= 1.0f / (1.0f + w.weight); // heavier weather = slower
    }
    return knots < kMinKnots ? kMinKnots : knots;
  }

  // Health lost per sim-second in the given weather. Only stormy wind
  // (windSpeed above kStormWindKmh) does damage, scaled by how far over the
  // threshold it blows; calmer weather does none.
  float stormDamagePerSec(const WeatherCell& w) const {
    const float over = w.data.windSpeed - kStormWindKmh;
    return over > 0.f ? over * kStormDamagePerKmh : 0.f;
  }
};

// Rolling statistics, accumulated across update() calls. Cheap to copy.
struct SimStats
{
    double simTime            = 0.0; // total simulated seconds
    long   refuelEvents       = 0;   // number of successful dockings
    double totalFuelDelivered = 0.0; // sum of fuel pumped into stations
    double stationStarvedTime = 0.0; // station-seconds spent at empty
    long   strandedVessels    = 0;   // vessels out of fuel *this* step
};

class ServerNav
{
public:
    std::vector<Station> stations;
    std::vector<Vessel>  vessels;
    WeatherCell          map[kGridSize][kGridSize];

    // Tuning: a vessel only chases stations whose fuel ratio is below this.
    // Above it, everything is "healthy enough" and idle vessels wait.
    float refuelThreshold = 0.6f;

    SimStats stats;

    // Advance the simulation by dt seconds.
    void update(float dt);

    // Clear accumulated stats and release all in-flight targets/claims.
    void reset();

    // --- Navigation readouts (for the on-screen HUD) --------------------
    // Computed on demand from the current state; none of these mutate the
    // sim. They assume kCellDistance nautical miles per world cell and a
    // 3600 sim-second == 1 hour clock (so stats.simTime is in sim seconds).

    // Speed over ground in knots (nm/hour), after local weather. 0 when the
    // vessel is idle (no target) or stranded (out of fuel).
    float vesselSpeedKnots(const Vessel& v) const;

    // Straight-line distance from the vessel to its target station, in
    // nautical miles. 0 when the vessel has no target.
    float vesselDistanceNm(const Vessel& v) const;

    // Absolute sim-clock time (in sim seconds) at which the vessel is expected
    // to dock: simTime + remaining travel time. Negative when there is no
    // target or the vessel can't make way (ETA unknown).
    double vesselEtaSimTime(const Vessel& v) const;

    // Build a randomized (but seeded / reproducible) scenario for testing and
    // benchmarking. Vessels are given enough range not to strand under the
    // default parameters.
    static ServerNav makeRandomScenario(int numStations, int numVessels, float timeStep = 1.0f, uint32_t seed = 0);

    static ServerNav makeStructuredScenario(WorldCoords start, WorldCoords dest, float timeStep = 1.0f);

    float simTimeStep = 1.0f;  // timestep is a modifer on the dt
private:
    // Weather at a position, clamped to the grid so out-of-bounds is impossible.
    const WeatherCell& weatherAt(const glm::vec2& p) const;

    // Index of the neediest unclaimed station below refuelThreshold, or -1.
    int pickTarget(int vesselIndex) const;

    // Speed over ground in world cells per sim-second, after weather; 0 when
    // the vessel is stranded. Shared by the knots / ETA readouts.
    float effectiveSpeedCells(const Vessel& v) const;
};
