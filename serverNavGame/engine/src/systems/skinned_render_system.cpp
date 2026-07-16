#include "systems/skinned_render_system.hpp"

#include "lve_skinned_model.hpp"

// libs
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

// std
#include <array>
#include <cassert>
#include <stdexcept>

namespace lve {

// Same push layout as SimpleRenderSystem: places/orients the whole character.
struct SkinnedPushConstantData {
  glm::mat4 modelMatrix{1.f};
  glm::mat4 normalMatrix{1.f};
};

SkinnedRenderSystem::SkinnedRenderSystem(
    LveDevice& device,
    VkRenderPass renderPass,
    VkDescriptorSetLayout globalSetLayout,
    VkDescriptorSetLayout boneSetLayout)
    : lveDevice{device} {
  createPipelineLayout(globalSetLayout, boneSetLayout);
  createPipeline(renderPass);
}

SkinnedRenderSystem::~SkinnedRenderSystem() {
  vkDestroyPipelineLayout(lveDevice.device(), pipelineLayout, nullptr);
}

void SkinnedRenderSystem::render(FrameInfo& frameInfo) {
  if (frameInfo.skinnedRenderItems.empty()) return;

  lvePipeline->bind(frameInfo.commandBuffer);

  // set = 0 (global UBO) is the same for every model; bind it once.
  vkCmdBindDescriptorSets(
      frameInfo.commandBuffer,
      VK_PIPELINE_BIND_POINT_GRAPHICS,
      pipelineLayout,
      0,
      1,
      &frameInfo.globalDescriptorSet,
      0,
      nullptr);

  for (const auto& item : frameInfo.skinnedRenderItems) {
    if (!item.model) continue;

    SkinnedPushConstantData push{};
    push.modelMatrix = item.modelMatrix;
    push.normalMatrix = item.normalMatrix;
    vkCmdPushConstants(
        frameInfo.commandBuffer,
        pipelineLayout,
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        0,
        sizeof(SkinnedPushConstantData),
        &push);

    // set = 1 (this model's bone matrix palette).
    VkDescriptorSet boneSet = item.model->boneDescriptorSet();
    vkCmdBindDescriptorSets(
        frameInfo.commandBuffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        pipelineLayout,
        1,
        1,
        &boneSet,
        0,
        nullptr);

    item.model->bind(frameInfo.commandBuffer);
    item.model->draw(frameInfo.commandBuffer);
  }
}

void SkinnedRenderSystem::createPipelineLayout(
    VkDescriptorSetLayout globalSetLayout, VkDescriptorSetLayout boneSetLayout) {
  VkPushConstantRange pushConstantRange{};
  pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
  pushConstantRange.offset = 0;
  pushConstantRange.size = sizeof(SkinnedPushConstantData);

  // set = 0 global UBO, set = 1 bone palette SSBO.
  std::vector<VkDescriptorSetLayout> descriptorSetLayouts{globalSetLayout, boneSetLayout};

  VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
  pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(descriptorSetLayouts.size());
  pipelineLayoutInfo.pSetLayouts = descriptorSetLayouts.data();
  pipelineLayoutInfo.pushConstantRangeCount = 1;
  pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

  if (vkCreatePipelineLayout(lveDevice.device(), &pipelineLayoutInfo, nullptr, &pipelineLayout) !=
      VK_SUCCESS) {
    throw std::runtime_error("failed to create skinned pipeline layout!");
  }
}

void SkinnedRenderSystem::createPipeline(VkRenderPass renderPass) {
  assert(pipelineLayout != nullptr && "Cannot create pipeline before pipeline layout");

  PipelineConfigInfo pipelineConfig{};
  LvePipeline::defaultPipelineConfigInfo(pipelineConfig);
  pipelineConfig.renderPass = renderPass;
  pipelineConfig.pipelineLayout = pipelineLayout;

  // Replace the default (static LveModel) vertex layout with the skinned one.
  pipelineConfig.bindingDescriptions = LveSkinnedModel::Vertex::getBindingDescriptions();
  pipelineConfig.attributeDescriptions = LveSkinnedModel::Vertex::getAttributeDescriptions();

  std::string exeDir = getExecutableDir();
  lvePipeline = std::make_unique<LvePipeline>(
      lveDevice,
      exeDir + "/../shaders/skinned_shader.vert.spv",
      exeDir + "/../shaders/skinned_shader.frag.spv",
      pipelineConfig);
}

}  // namespace lve
