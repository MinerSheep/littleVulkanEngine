#pragma once

#include "lve_device.hpp"
#include "lve_game_object.hpp"
#include "lve_pipeline.hpp"

// std
#include <memory>
#include <vector>

namespace lve {
class SimpleRenderSystem {
 public:
  SimpleRenderSystem(LveDevice& device, VkRenderPass renderPass);
  ~SimpleRenderSystem();

  SimpleRenderSystem(const SimpleRenderSystem&) = delete;
  SimpleRenderSystem& operator=(const SimpleRenderSystem&) = delete;

  void renderGameObjects(VkCommandBuffer commandBuffer, std::vector<LveGameObject>& gameObjects);

 private:
  void createPipelineLayout();
  void createPipeline(VkRenderPass renderPass);

  LveDevice& lveDevice;

  std::unique_ptr<LvePipeline>
      lvePipeline;  // {lveDevice, "shaders/simple_shader.vert.spv",
                    // "shaders/simple_shader.frag.spv",
                    // LvePipeline::defaultPipelineConfigInfo(WIDTH, HEIGHT)};
  VkPipelineLayout pipelineLayout;
};
}  // namespace lve