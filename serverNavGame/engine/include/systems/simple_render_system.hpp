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

  SimpleRenderSystem(LveDevice& device, VkRenderPass renderPass,
                     VkDescriptorSetLayout globalSetLayout,
                     VkDescriptorSetLayout textureSetLayout);
  ~SimpleRenderSystem();

  SimpleRenderSystem(const SimpleRenderSystem&) = delete;
  SimpleRenderSystem& operator=(const SimpleRenderSystem&) = delete;

  void render(FrameInfo& frameInfo);

  // Called twice a frame: the background first, then the UI overlay
  void renderUI(FrameInfo& frameInfo, const std::vector<UIRenderItem>& items);

 private:
  void createPipelineLayout(VkDescriptorSetLayout globalSetLayout);

  // The same layout with a picture's sampler on set 1, for anything textured
  void createTexturedPipelineLayout(VkDescriptorSetLayout globalSetLayout,
                                    VkDescriptorSetLayout textureSetLayout);
  void createPipeline(VkRenderPass renderPass);

  // Everything in the frame that carries a picture
  void drawTextured(FrameInfo& frameInfo);

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

  // Reads a picture off set 1 and lays it over the mesh by world position
  std::unique_ptr<LvePipeline> texturedPipeline;

  // The same, blending and leaving the depth buffer alone
  // A textured wall turned toward the camera has to ghost like a plain one
  std::unique_ptr<LvePipeline> texturedGhostPipeline;

  VkPipelineLayout pipelineLayout;
  VkPipelineLayout texturedPipelineLayout;
};
}  // namespace lve