#pragma once

#include "lve_window.hpp"
#include "lve_game_object.hpp"
#include "lve_renderer.hpp"
#include "lve_descriptors.hpp"

#include <memory>
#include <vector>

// FIRST APP implementation has been split into lve_renderer and simple_render_system
namespace lve {
class FirstApp {
 public:
  static constexpr int WIDTH = 800;
  static constexpr int HEIGHT = 600;

  FirstApp();
  ~FirstApp();

  FirstApp(FirstApp const&) = delete;
  FirstApp &operator=(FirstApp const&) = delete;

  void run();

 private:
  void loadGameObjects();

  // ORDER MATTERS!
  // Initialize from top to bottom, DESTROY from bottom to top
  LveWindow lveWindow{WIDTH, HEIGHT, "Hello Vulkan!"};
  LveDevice lveDevice{lveWindow};
  LveRenderer lveRenderer{lveWindow, lveDevice};

  std::unique_ptr<LveDescriptorPool> globalPool{};
  std::vector<LveGameObject> gameObjects;
};
}  // namespace lve
