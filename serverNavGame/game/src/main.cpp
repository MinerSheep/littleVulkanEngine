#include <lve_audio.hpp>
#include <lve_engine.hpp>
#include <lve_event_dispatcher.hpp>
#include <lve_scene_editor.hpp>
#include "scenes/levelscene.hpp"
#include "scenes/servernavscene.hpp"
#include "scenes/reforgescene.hpp"
#include "scenes/skinneddemoscene.hpp"
#include "petscop/room_scene.hpp"
#include "servernav_sim.hpp"
#include "fetch_weather.hpp"

// std
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <stdexcept>

// lib
#include "game_analytics_manager.hpp"


// Upper bound on per-frame dt, to stop a one-off multi-second stall (first-frame
// warmup, a hitch) from teleporting the camera. It must sit ABOVE the normal
// frame time or it throttles every frame into slow-motion: measured ~86 ms/frame
// (≈11 FPS) on the software renderer, so 0.2 s clears real frames yet still caps
// a stall to a bounded step. Raise it if your steady frame time is ever higher.
#define MAX_FRAME_TIME (1.0f / 5.0f)

// TEMP diagnostic: frames slower than this get a [stall] line naming the part
// LVE_STALL_MS overrides it at run time
#define STALL_MS 150.0f

// these are used to avoid external memory leak warnings (out of control)
// Leak checking is off unless you ask for it, which is what play.sh already does
// for a run through the script. Running ./game/game straight had it on, and under
// WSL the leak pass can fall over on its own at exit with nothing useful to say
//
// Turn it back on for a session with:  ASAN_OPTIONS=detect_leaks=1 ./game/game
extern "C" const char *__asan_default_options() { return "detect_leaks=0"; }

extern "C" const char *__lsan_default_suppressions() {
  return
    "leak:libvulkan\n"    "leak:libvulkan_lvp\n" "leak:swrast\n"
    "leak:libgallium\n"   "leak:libLLVM\n"       "leak:libdrm\n"
    "leak:libglapi\n"     "leak:libxcb\n"        "leak:libX11\n"
    "leak:libglfw\n"      "leak:libasan\n";
}

int main() {
    lve::LveEngine& engine = lve::LveEngine::instance();
    // lve::FirstApp app;
    // ServerNav navGame;
    
    // Once at startup, before your main loop:
    GameAnalyticsManager::Get().Initialize();
    
    engine.init();
    
    lve::LveAudio& audio = lve::LveAudio::instance();
    audio.init();
    // One-shots, decoded up front. loadFolder does not walk into subfolders, so
    // each one is asked for by name
    audio.loadFolder("sounds");
    audio.loadFolder("sounds/ui");
    audio.loadFolder("sounds/stingers");

    // Beds, streamed. These two are 34 MB and 200 MB on disk, which is why they
    // are not swept up with the rest -- see GDC2026SOUNDS.md
    audio.loadLoop("clock_tick", "sounds/ambience/clock_tick.wav");
    audio.loadLoop("wind", "sounds/ambience/wind.wav");
    audio.loadLoop("forest_bed", "sounds/ambience/forest.wav");

    try {
    
        static ServerNavScene snscene;
        snscene.loadModels();
        snscene.setupLights();

        // static ReforgeScene rscene;
        // rscene.loadModels();
        // rscene.setupLights();

        // static SkinnedDemoScene sdscene;
        // sdscene.loadModels();
        // sdscene.setupLights();

        // // A layout editor used to spawn/move/save quads; engine specific
        static lve::LveSceneEditor editorScene;
        editorScene.loadModels();
        editorScene.setupLights();

        // Rooms joined by doors, built from maps/petscop.map
        // The map is compiled from maps/petscop.mapsrc by tools/build_map.py
        static RoomScene roomScene;
        roomScene.loadModels();
        roomScene.setupLights();

        // A clock that only ever goes forwards, unlike the wall clock
        auto currentTime = std::chrono::steady_clock::now();

        GLFWwindow* window = engine.getGLFWWindow();
        engine.activeScene = &roomScene;

        // --- Event wiring --------------------------------------------------
        // The dispatcher is the single hop between "something happened" and the
        // code that reacts. Below, the game loop turns raw key polling into
        // events; these two listeners decide what to do with them.
        auto& events = lve::EventDispatcher::instance();

        // events are forwarded to the active scene
        events.subscribe([&](const lve::Event& e) {
            if (engine.activeScene) engine.activeScene->onEvent(e);
        });

        // swaps the active engine.activeScene
        events.subscribe([&](const lve::Event& e) {
            if (e.i == GLFW_KEY_0) engine.activeScene = &editorScene;
            else if (e.i == GLFW_KEY_1) engine.activeScene = &roomScene;
            else if (e.i == GLFW_KEY_2) engine.activeScene = &snscene;
        }, lve::EventType::KeyPressed);

        // Keys we lift into events, main reads GLFW
        // The rest of the code just consumes events
        // 4-9 belong to the skinned demo's clip picker, off limits
        const int watchedKeys[] = {
            GLFW_KEY_0, GLFW_KEY_1, GLFW_KEY_2, GLFW_KEY_3, GLFW_KEY_4,
            GLFW_KEY_5, GLFW_KEY_6, GLFW_KEY_7, GLFW_KEY_8, GLFW_KEY_9,
            GLFW_KEY_R
        };
        constexpr int watchedKeyCount = sizeof(watchedKeys) / sizeof(watchedKeys[0]);
        bool prevDown[watchedKeyCount] = {};

        // FPS readout (updates the window title once per second).
        float fpsAccum = 0.f;
        int fpsFrames = 0;

        while (!engine.shouldClose()) {
            auto frameStart = std::chrono::steady_clock::now();
            // this can cause glitchiness on ubuntu because it blocks
            glfwPollEvents();

            // TEMP: shuts the window the way closing it does, for testing shutdown
            {
                static const char* quitAfter = std::getenv("PETSCOP_QUIT_AFTER");
                static auto begun = std::chrono::steady_clock::now();
                if (quitAfter) {
                    const double up = std::chrono::duration<double>(
                        std::chrono::steady_clock::now() - begun).count();
                    if (up > std::atof(quitAfter)) glfwSetWindowShouldClose(window, GLFW_TRUE);
                }
            }

            // Key presses are turned into events
            for (int i = 0; i < watchedKeyCount; i++) {
                bool down = glfwGetKey(window, watchedKeys[i]) == GLFW_PRESS;

                if (down && !prevDown[i])
                    events.post(lve::makeKeyEvent(watchedKeys[i]));
                prevDown[i] = down;
            }

            // Take the time after the block
            auto newTime = std::chrono::steady_clock::now();
            // rawDt = true frame time (used for the FPS meter).
            float rawDt = std::chrono::duration<float, std::chrono::seconds::period>(newTime - currentTime).count();
            currentTime = newTime;
            // What the game walks, falls and counts down against
            // Never negative, and never a whole stall in one step
            float dt = glm::clamp(rawDt, 0.f, MAX_FRAME_TIME);

            // Report frames-per-second in the title bar once every second.
            fpsAccum += rawDt;
            fpsFrames++;
            if (fpsAccum >= 1.0f) {
                float fps = fpsFrames / fpsAccum;
                int msPerFrame = static_cast<int>(1000.0f * fpsAccum / fpsFrames + 0.5f);
                std::string label = "Hello Vulkan!  -  " + std::to_string(fps) + " FPS  (" +
                                    std::to_string(msPerFrame) + " ms/frame)";
                glfwSetWindowTitle(window, label.c_str());
                std::cout << "FPS: " << fps << "  (" << msPerFrame << " ms/frame)" << std::endl;
                fpsAccum = 0.f;
                fpsFrames = 0;
            }

            // TEMP diagnostic: LVE_AUDIO_SELFTEST=stone plays one clip and traces it
            if (const char* selfTest = std::getenv("LVE_AUDIO_SELFTEST")) {
                static float selfTestClock = 0.f;
                static bool selfTestFired = false;
                selfTestClock += dt;
                if (!selfTestFired && selfTestClock > 3.f) {
                    selfTestFired = true;
                    audio.play(selfTest);
                    std::cout << "[audiotrace] played '" << selfTest << "'" << std::endl;
                }
                if (selfTestFired) audio.traceClip(selfTest);
            }

            // TEMP diagnostic: names whichever part of a slow frame ate the time
            auto stallMark = newTime;
            auto sinceMark = [&stallMark]() {
                auto now = std::chrono::steady_clock::now();
                float took = std::chrono::duration<float, std::milli>(now - stallMark).count();
                stallMark = now;
                return took;
            };

            // Polling covers the GLFW pump and the key sweep above it
            const float pollMs = std::chrono::duration<float, std::milli>(newTime - frameStart).count();

            events.dispatch();
            const float eventMs = sinceMark();
            engine.activeScene->update(dt);
            const float updateMs = sinceMark();
            // navGame.update(dt);
            engine.render();
            const float renderMs = sinceMark();

            // A frame this long is what chops a sound into pieces and makes a
            // close look like it hung
            static const float stallBar = [] {
                const char* set = std::getenv("LVE_STALL_MS");
                return set ? static_cast<float>(atof(set)) : STALL_MS;
            }();

            if (pollMs + eventMs + updateMs + renderMs > stallBar) {
                std::cout << "[stall] frame " << static_cast<long>(pollMs + eventMs + updateMs + renderMs)
                          << " ms  poll " << static_cast<long>(pollMs)
                          << "  events " << static_cast<long>(eventMs)
                          << "  update " << static_cast<long>(updateMs)
                          << "  render " << static_cast<long>(renderMs) << std::endl;
            }
        }

        auto mark = [](const char* what, std::chrono::steady_clock::time_point& last) {
            auto now = std::chrono::steady_clock::now();
            std::cout << "[shutdown] " << what << " "
                      << std::chrono::duration<double, std::milli>(now - last).count() << " ms"
                      << std::endl;
            last = now;
        };
        auto stage = std::chrono::steady_clock::now();
        mark("loop exited (engine.cleanup ran inside shouldClose)", stage);

        roomScene.cleanup();
        mark("roomScene.cleanup", stage);
        editorScene.cleanup();
        mark("editorScene.cleanup", stage);
        snscene.cleanup();
        mark("snscene.cleanup", stage);
        std::cout << "[shutdown] main returning, static dtors + LSan next" << std::endl;
        // rscene.cleanup();
        // sdscene.cleanup();
        // app.run();
    } catch (const std::exception &e) {
        engine.cleanup();
        std::cerr << e.what() << '\n';
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}