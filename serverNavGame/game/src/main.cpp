#include <lve_engine.hpp>
#include "levelscene.hpp"
// #include <first_app.hpp>
#include "servernav_sim.hpp"

// std
#include <cstdlib>
#include <iostream>
#include <stdexcept>

#include <chrono>

#define MAX_FRAME_TIME 1.0f


int main() {
    lve::LveEngine& engine = lve::LveEngine::instance();
    // lve::FirstApp app;
    // ServerNav navGame;

    engine.init();
    try {
    
        LevelScene scene;
        scene.loadModels();
        scene.setupLights();

        auto currentTime = std::chrono::high_resolution_clock::now();
    
        while (!engine.shouldClose()) {
            // this causes glitchiness on ubuntu because it blocks
            glfwPollEvents();

            // Take the time after the block
            auto newTime = std::chrono::high_resolution_clock::now();
            float dt = std::chrono::duration<float, std::chrono::seconds::period>(newTime - currentTime).count();
            currentTime = newTime;
            dt = glm::min(dt, MAX_FRAME_TIME);

            scene.update(dt);
            // navGame.update(dt);
            engine.render(scene);
        }

        scene.cleanup();
        // app.run();
    } catch (const std::exception &e) {
        engine.cleanup();
        std::cerr << e.what() << '\n';
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}