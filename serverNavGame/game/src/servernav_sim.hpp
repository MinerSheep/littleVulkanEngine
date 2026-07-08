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
#if USING_RTS
#include "fetch_weather.hpp"
#endif


#include <cstdint>
#include <string>
#include <vector>

#include <glm/glm.hpp> // glm::vec2 + glm::length / glm::distance

// World is a fixed kGridSize x kGridSize field of weather cells. Positions are
// expressed in the same units, i.e. in the range [0, kGridSize).
constexpr int   kGridSize      = 50;
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

  float speed = 5.f;     // world units / second in clear weather
  float fuel = 1.f;      // current fuel
  float maxFuel = 1.f;   // full tank
  float burnRate = 0.f;  // fuel consumed per world unit travelled (0 = free)

  // Index into ServerNav::stations of the current destination, or -1.
  int targetIndex = -1;

  float getSpeedModifier(const WeatherCell& w) const {
    if (USING_RTS)
    {

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

    // Build a randomized (but seeded / reproducible) scenario for testing and
    // benchmarking. Vessels are given enough range not to strand under the
    // default parameters.
    static ServerNav makeRandomScenario(int numStations, int numVessels, uint32_t seed = 0);

private:
    // Weather at a position, clamped to the grid so out-of-bounds is impossible.
    const WeatherCell& weatherAt(const glm::vec2& p) const;

    // Index of the neediest unclaimed station below refuelThreshold, or -1.
    int pickTarget(int vesselIndex) const;
};
