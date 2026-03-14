#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <string>

namespace lve
{
    class LveWindow {
        public:
            LveWindow(int w, int h, std::string name);
            ~LveWindow();

        private:
        GLFWwindow* window;

        const int width, height;

        std::string windowName;
        void initWindow();
    };
}