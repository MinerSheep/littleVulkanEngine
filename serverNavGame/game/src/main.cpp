#include <lve_engine.hpp>
#include "scenes/levelscene.hpp"
#include "scenes/servernavscene.hpp"
#include "scenes/reforgescene.hpp"
#include "scenes/skinneddemoscene.hpp"
#include "servernav_sim.hpp"
#include "fetch_weather.hpp"

// std
#include <cstdlib>
#include <iostream>
#include <stdexcept>

#include <chrono>

// Cap per-frame dt. At low/erratic FPS (e.g. software rendering) a large dt makes
// dt-scaled camera movement lurch in big discrete steps ("jumping"). Clamping to
// ~1/30s keeps motion smooth (it just goes slow-motion when the FPS is low).
#define MAX_FRAME_TIME (1.0f / 30.0f)

// these are used to avoid external memory leak warnings (out of control)
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

    engine.init();
    try {
    
        static ServerNavScene snscene;
        snscene.loadModels();
        snscene.setupLights();

        static ReforgeScene rscene;
        rscene.loadModels();
        rscene.setupLights();

        static SkinnedDemoScene sdscene;
        sdscene.loadModels();
        sdscene.setupLights();

        auto currentTime = std::chrono::high_resolution_clock::now();

        GLFWwindow* window = engine.getGLFWWindow();
        lve::LveScene *scene = &sdscene;

        // FPS readout (updates the window title once per second).
        float fpsAccum = 0.f;
        int fpsFrames = 0;

        while (!engine.shouldClose()) {
            // this causes glitchiness on ubuntu because it blocks
            glfwPollEvents();

            // Scene swap
            if (glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS)
                scene = &snscene;
            if (glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS)
                scene = &rscene;
            if (glfwGetKey(window, GLFW_KEY_3) == GLFW_PRESS)
                scene = &sdscene;

            // Take the time after the block
            auto newTime = std::chrono::high_resolution_clock::now();
            // rawDt = true wall-clock frame time (used for the FPS meter).
            float rawDt = std::chrono::duration<float, std::chrono::seconds::period>(newTime - currentTime).count();
            currentTime = newTime;
            // dt (clamped) is what the game/camera integrate against, so a slow
            // frame can't produce a huge movement step.
            float dt = glm::min(rawDt, MAX_FRAME_TIME);

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

            scene->update(dt);
            // navGame.update(dt);
            engine.render(*scene);
        }

        snscene.cleanup();
        rscene.cleanup();
        sdscene.cleanup();
        // app.run();
    } catch (const std::exception &e) {
        engine.cleanup();
        std::cerr << e.what() << '\n';
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}