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
constexpr int   kGridSize      = 10;
constexpr float kCellDistance  = 1;   // distance a singular cell measures in nautical miles
constexpr float kArrivalRadius = 0.5f; // distance at which a vessel "docks"

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

  float speed = 5.f;     // world units / second in clear weather
  float fuel = 1.f;      // current fuel
  float maxFuel = 1.f;   // full tank
  float burnRate = 0.f;  // fuel consumed per world unit travelled (0 = free)

  // Index into ServerNav::stations of the current destination, or -1.
  int targetIndex = -1;

  float getSpeedModifier(const WeatherCell& w) const {
    if (USING_RTS)
    {
        // Wind effect on travel speed. windDir is degrees in [0,360):
        // 0 = wind blowing due South, 90 = wind blowing due West. World/screen
        // axes match the map render: +x = East (right), +y = South (down), so
        // the unit wind vector for angle t is (-sin t, cos t).
        const float     t       = glm::radians(w.data.windDir);
        const glm::vec2 windVec = {-glm::sin(t), glm::cos(t)};

        // dir is the vessel's heading. Sailing with the wind (dot > 0) is a
        // tailwind and speeds it up; heading into it (dot < 0) is a headwind.
        const float alignment = glm::dot(dir, windVec);

        // windSpeed is km/h (open-meteo); scale it into a modest multiplier and
        // clamp so even a strong headwind can't stall or reverse the vessel.
        constexpr float kWindInfluence = 0.02f;
        const float     mod = 1.0f + kWindInfluence * w.data.windSpeed * alignment;
        return mod < 0.1f ? 0.1f : mod;
    }
    else return 1.0f / (1.0f + w.weight);  // heavier weather = slower
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
