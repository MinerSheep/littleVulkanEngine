#include "lve_window.hpp"

#include <stdexcept>
#include <iostream>

namespace lve {

LveWindow::LveWindow(int w, int h, std::string name) : width{w}, height{h}, windowName{name} {
  initWindow();
}

LveWindow::~LveWindow() {
  glfwDestroyWindow(window);
  glfwTerminate();
}

void LveWindow::createWindowSurface(VkInstance instance, VkSurfaceKHR* surface) 
{
  if (glfwCreateWindowSurface(instance, window, nullptr, surface) != VK_SUCCESS)
    throw std::runtime_error("Failed to create window surface");
}

void LveWindow::frameBufferResizeCallback(GLFWwindow* window, int width, int height) {
  auto lveWindow = reinterpret_cast<LveWindow*>(glfwGetWindowUserPointer(window));
  lveWindow->frameBufferResized = true;
  lveWindow->width = width;
  lveWindow->height = height;
}

void LveWindow::initWindow() {
  glfwSetErrorCallback([](int error, const char* desc)
  {
      std::cerr << "GLFW Error " << error << ": " << desc << '\n';
  });

  if (!glfwInit())
  {
      std::cerr << "glfwInit failed\n";
      return;
  }

  std::cout << "glfwInit succeeded\n";
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

  window = glfwCreateWindow(width, height, windowName.c_str(), nullptr, nullptr);

  if (!window)
  {
      std::cerr << "Failed to create GLFW window\n";
      return;
  }

  std::cout << "Window created successfully\n";

  // GLFW Window can be paired with an arbitrary pointer for later + callbacks
  glfwSetWindowUserPointer(window, this);
  glfwSetWindowSizeCallback(window, frameBufferResizeCallback);

  std::cout << "window ptr = " << window << '\n';

  int w, h;
  glfwGetWindowSize(window, &w, &h);

  std::cout << "size = "
            << w << " x "
            << h << '\n';
}
}  // namespace lve