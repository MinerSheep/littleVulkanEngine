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

#define MAX_FRAME_TIME 1.0f

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
            float dt = std::chrono::duration<float, std::chrono::seconds::period>(newTime - currentTime).count();
            currentTime = newTime;
            dt = glm::min(dt, MAX_FRAME_TIME);

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