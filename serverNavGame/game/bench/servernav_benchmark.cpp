// Standalone benchmark + smoke test for the ServerNav simulation.
//
// It has its own main(), so it builds into a *separate* executable from the
// game. It lives in game/bench/ (NOT game/src/) on purpose: the game Makefile
// globs every .cpp under src/, so keeping it here avoids a duplicate-main()
// link error in the game build. It needs neither the engine, GLFW, nor Vulkan
// libs — only glm headers.
//
// Build (from game/). On this machine glm is a system include (/usr/include),
// so no -I for glm is needed; add -I"$VULKAN_SDK_PATH/include" if yours isn't:
//
//   g++ -std=c++17 -O2 -Isrc bench/servernav_benchmark.cpp src/servernav_sim.cpp -o servernav_bench
//   ./servernav_bench
//
// Exit code is 0 if every correctness check passes, 1 otherwise.

#include "servernav_sim.hpp"

#include <chrono>
#include <cstdint>
#include <cstdio>

using Clock = std::chrono::steady_clock;

// -------- tiny test harness (works regardless of NDEBUG) --------
static int g_failures = 0;
#define EXPECT(cond, msg)                        \
    do                                           \
    {                                            \
        if (!(cond))                             \
        {                                        \
            std::printf("  [FAIL] %s\n", (msg)); \
            ++g_failures;                        \
        }                                        \
    } while (0)

// -------- invariants that must hold at all times --------
static void checkInvariants(const ServerNav& sim)
{
    for (const auto& s : sim.stations)
    {
        EXPECT(s.fuel >= 0.f, "station fuel went negative");
        EXPECT(s.fuel <= s.capacity + 1e-3f, "station fuel exceeded capacity");
    }
    for (const auto& v : sim.vessels)
    {
        EXPECT(v.fuel >= 0.f, "vessel fuel went negative");
        EXPECT(v.fuel <= v.maxFuel + 1e-3f, "vessel fuel exceeded maxFuel");
        EXPECT(v.pos.x >= 0.f && v.pos.x <= kGridSize, "vessel left the grid in x");
        EXPECT(v.pos.y >= 0.f && v.pos.y <= kGridSize, "vessel left the grid in y");
    }
}

// -------- functional tests --------
static void functionalTests()
{
    std::printf("functional tests\n");

    // A vessel placed away from a nearly-empty station should reach it and
    // refuel it. This is also the regression guard for the classic
    // glm::length() vs vec2::length() bug: with the bug, the vessel never
    // docks and refuelEvents stays 0.
    {
        ServerNav sim;
        sim.simTimeStep = 3600.f; // 1 sim-hour per step-second so the realistic
                                  // ~7 kn vessel still crosses the map in-test

        Station s;
        s.name        = "depot";
        s.pos         = glm::vec2(10.f, 10.f);
        s.capacity    = 1.f;
        s.fuel        = 0.1f;
        s.depleteRate = 0.f; // hold it steady so the test is deterministic
        sim.stations.push_back(s);

        Vessel v;
        v.pos      = glm::vec2(0.f, 0.f);
        v.speed    = 5.f;
        v.maxFuel  = 1.f;
        v.fuel     = 1.f;
        v.burnRate = 0.f;
        sim.vessels.push_back(v);

        for (int i = 0; i < 2000 && sim.stats.refuelEvents == 0; ++i)
            sim.update(0.05f);

        EXPECT(sim.stats.refuelEvents > 0,
               "vessel never docked (check glm::length vs vec2::length)");
        EXPECT(sim.stats.totalFuelDelivered > 0.8,
               "expected ~0.9 fuel delivered to the empty station");
        checkInvariants(sim);
    }

    // With no vessels, stations must drain monotonically and clamp at 0.
    {
        ServerNav sim;
        Station   s;
        s.pos         = glm::vec2(5.f, 5.f);
        s.fuel        = 1.f;
        s.capacity    = 1.f;
        s.depleteRate = 0.5f;
        sim.stations.push_back(s);

        for (int i = 0; i < 100; ++i)
            sim.update(0.1f);

        EXPECT(sim.stations[0].fuel == 0.f, "unserviced station should drain to exactly 0");
        EXPECT(sim.stats.stationStarvedTime > 0.0, "starved time should accumulate");
    }

    // A vessel with too little fuel to reach a distant target should strand
    // rather than teleport, cheat, or crash.
    {
        ServerNav sim;
        sim.simTimeStep = 3600.f; // fast-forward so a realistic-speed vessel
                                  // exhausts its short range within the loop
        Station   s;
        s.pos         = glm::vec2(40.f, 40.f);
        s.fuel        = 0.f;
        s.capacity    = 1.f;
        s.depleteRate = 0.f;
        sim.stations.push_back(s);

        Vessel v;
        v.pos      = glm::vec2(0.f, 0.f);
        v.speed    = 5.f;
        v.maxFuel  = 1.f;
        v.fuel     = 0.02f;   // enough for ~8 units, target is ~56 away
        v.burnRate = 0.0025f;
        sim.vessels.push_back(v);

        for (int i = 0; i < 500; ++i)
            sim.update(0.05f);

        EXPECT(sim.stats.refuelEvents == 0, "stranded vessel should not have docked");
        EXPECT(sim.vessels[0].fuel == 0.f, "stranded vessel should be empty");
        checkInvariants(sim);
    }

    std::printf("  %s\n\n", g_failures == 0 ? "all functional tests passed" : "SOME TESTS FAILED");
}

// -------- throughput benchmark --------
struct BenchResult
{
    int      stations;
    int      vessels;
    long     steps;
    double   seconds;
    SimStats stats;
};

static BenchResult runBench(int numStations, int numVessels, long steps, float dt, uint32_t seed)
{
    ServerNav sim = ServerNav::makeRandomScenario(numStations, numVessels, seed);

    auto t0 = Clock::now();
    for (long i = 0; i < steps; ++i)
        sim.update(dt);
    auto t1 = Clock::now();

    checkInvariants(sim); // benchmark output is meaningless if state is corrupt

    BenchResult r;
    r.stations = numStations;
    r.vessels  = numVessels;
    r.steps    = steps;
    r.seconds  = std::chrono::duration<double>(t1 - t0).count();
    r.stats    = sim.stats;
    return r;
}

static void benchmark()
{
    std::printf("throughput benchmark (dt = 1/60 s)\n");

    struct Size
    {
        int stations;
        int vessels;
    };
    const Size  sizes[] = {{16, 2}, {64, 8}, {256, 32}, {1024, 128}, {4096, 512}};
    const long  steps   = 5000;
    const float dt      = 1.f / 60.f;

    std::printf("  %-9s %-8s %-7s %-11s %-13s %-13s %-9s\n",
                "stations", "vessels", "steps", "time(ms)", "steps/s", "ns/entity/step", "refuels");

    for (const auto& sz : sizes)
    {
        BenchResult r = runBench(sz.stations, sz.vessels, steps, dt, 1234u);

        double ms          = r.seconds * 1e3;
        double stepsPerSec  = r.steps / r.seconds;
        double nsPerEntity  = (r.seconds * 1e9) / static_cast<double>(r.steps) /
                             static_cast<double>(sz.stations + sz.vessels);

        std::printf("  %-9d %-8d %-7ld %-11.2f %-13.0f %-13.1f %-9ld\n",
                    r.stations, r.vessels, r.steps, ms, stepsPerSec, nsPerEntity, r.stats.refuelEvents);
    }
    std::printf("\n");
}

int main()
{
    std::printf("=== ServerNav simulation benchmark ===\n\n");

    functionalTests();
    benchmark();

    if (g_failures == 0)
        std::printf("RESULT: OK\n");
    else
        std::printf("RESULT: %d CHECK(S) FAILED\n", g_failures);

    return g_failures == 0 ? 0 : 1;
}
