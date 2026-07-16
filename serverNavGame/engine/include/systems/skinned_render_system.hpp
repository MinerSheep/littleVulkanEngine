#pragma once

#include "lve_device.hpp"
#include "lve_pipeline.hpp"
#include "lve_frame_info.hpp"

// std
#include <memory>

namespace lve {

// Renders skinned (skeletal) meshes. Mirrors SimpleRenderSystem, but its pipeline
// uses the extended LveSkinnedModel::Vertex layout (position/normal/uv/joints/
// weights) and a second descriptor set (set = 1) that supplies each model's bone
// matrix palette to the skinning vertex shader.
class SkinnedRenderSystem {
 public:
  SkinnedRenderSystem(
      LveDevice& device,
      VkRenderPass renderPass,
      VkDescriptorSetLayout globalSetLayout,
      VkDescriptorSetLayout boneSetLayout);
  ~SkinnedRenderSystem();

  SkinnedRenderSystem(const SkinnedRenderSystem&) = delete;
  SkinnedRenderSystem& operator=(const SkinnedRenderSystem&) = delete;

  void render(FrameInfo& frameInfo);

 private:
  void createPipelineLayout(
      VkDescriptorSetLayout globalSetLayout, VkDescriptorSetLayout boneSetLayout);
  void createPipeline(VkRenderPass renderPass);

  LveDevice& lveDevice;

  std::unique_ptr<LvePipeline> lvePipeline;
  VkPipelineLayout pipelineLayout;
};

}  // namespace lve
