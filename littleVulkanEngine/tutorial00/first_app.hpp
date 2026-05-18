#pragma once

#include "lve_window.hpp"

namespace lve {
    class FirstApp {
        public:
            static constexpr int WIDTH = 800;
            static constexpr int HEIGHT = 600;

            void run() 
            {
                // while window doesn't want to close
                    // glfwPollEvents
            }
        private:
            LveWindow lveWindow{WIDTH,HEIGHT,"Hello Vulkan"};
    }
}