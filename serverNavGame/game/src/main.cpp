#include <lve_engine.hpp>
#include <first_app.hpp>

// std
#include <cstdlib>
#include <iostream>
#include <stdexcept>

int main() {
    // LveEngine engine;
    lve::FirstApp app;

    try {
        // engine.init();
    
        // GameScene scene;
        // scene.loadModels();
        // scene.setupLights();
    
        // while (!engine.shouldClose()) {
        //     scene.update(dt);
        //     engine.render(scene);
        // }
        app.run();
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}