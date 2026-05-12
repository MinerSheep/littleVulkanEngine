#pragma once

#include "lve_pipeline.hpp"
#include "lve_window.hpp"
#include "lve_game_object.hpp"
#include "lve_swap_chain.hpp"

#include <memory>
#include <vector>

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
  void createPipelineLayout();
  void createPipeline();
  void createCommandBuffers();
  void freeCommandBuffers();
  void drawFrame();
  void recreateSwapChain();
  void recordCommandBuffer(int imageIndex);
  void renderGameObjects(VkCommandBuffer commandBuffer);

  // ORDER MATTERS!
  // Initialize from top to bottom, DESTROY from bottom to top
  LveWindow lveWindow{WIDTH, HEIGHT, "Hello Vulkan!"};
  LveDevice lveDevice{lveWindow};
  std::unique_ptr<LveSwapChain> lveSwapChain; // {lveDevice, lveWindow.getExtent()};
  std::unique_ptr<LvePipeline> lvePipeline; // {lveDevice, "shaders/simple_shader.vert.spv", "shaders/simple_shader.frag.spv", LvePipeline::defaultPipelineConfigInfo(WIDTH, HEIGHT)};
  VkPipelineLayout pipelineLayout;
  std::vector<VkCommandBuffer> commandBuffers;
  std::vector<LveGameObject> gameObjects;
};
}  // namespace lve
