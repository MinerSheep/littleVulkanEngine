#pragma once

#include "lve_device.hpp"
#include "lve_pipeline.hpp"
#include "lve_camera.hpp"
#include "lve_frame_info.hpp"

// std
#include <memory>
#include <vector>

namespace lve {
class SimpleRenderSystem {
 public:

  SimpleRenderSystem(LveDevice& device, VkRenderPass renderPass, VkDescriptorSetLayout globalSetLayout);
  ~SimpleRenderSystem();

  SimpleRenderSystem(const SimpleRenderSystem&) = delete;
  SimpleRenderSystem& operator=(const SimpleRenderSystem&) = delete;

  void render(FrameInfo& frameInfo);

  // Called twice a frame: the background first, then the UI overlay
  void renderUI(FrameInfo& frameInfo, const std::vector<UIRenderItem>& items);

 private:
  void createPipelineLayout(VkDescriptorSetLayout globalSetLayout);
  void createPipeline(VkRenderPass renderPass);

  LveDevice& lveDevice;

  std::unique_ptr<LvePipeline>
      lvePipeline;  // {lveDevice, "shaders/simple_shader.vert.spv",
                    // "shaders/simple_shader.frag.spv",
                    // LvePipeline::defaultPipelineConfigInfo(WIDTH, HEIGHT)};
  std::unique_ptr<LvePipeline>
      UIPipeline;  // {lveDevice, "shaders/simple_shader.vert.spv",
                    // "shaders/simple_shader.frag.spv",
                    // LvePipeline::defaultPipelineConfigInfo(WIDTH, HEIGHT)};

  // Same shaders as lvePipeline, but it blends and leaves the depth buffer alone
  // Anything with an alpha below 1 is drawn with this one, after the solid pass
  std::unique_ptr<LvePipeline> transparentPipeline;

  VkPipelineLayout pipelineLayout;
};
}  // namespace lve