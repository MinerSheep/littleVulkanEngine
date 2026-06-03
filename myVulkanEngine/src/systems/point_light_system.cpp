#include "systems/point_light_system.hpp"

// libs
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

// std
#include <array>
#include <cassert>
#include <stdexcept>
#include <iostream>

namespace lve {

PointLightSystem::PointLightSystem(LveDevice& device, VkRenderPass renderPass, VkDescriptorSetLayout globalSetLayout)
    : lveDevice{device} {
  createPipelineLayout(globalSetLayout);

  // Good idea to check render pass compatibility here.  If its compatible then pipeline doesnt need
  // to be recreated Since it can use the same blueprint for submitted framebuffers and be just fine
  createPipeline(renderPass);
}

PointLightSystem::~PointLightSystem() {
  // Dont forget to destroy the pipeline layout in destructor, all vk objects needs to do this
  vkDestroyPipelineLayout(lveDevice.device(), pipelineLayout, nullptr);
}

void PointLightSystem::render(
    FrameInfo& frameInfo) {
  // std::cout << "Entering PointLightSystem::render\n";
  lvePipeline->bind(frameInfo.commandBuffer);

  // Every set overwritten must overwrite every set that comes after it
  // Bind it once, now ALL gameobjects can use it without need for rebinding
  vkCmdBindDescriptorSets(
      frameInfo.commandBuffer,
      VK_PIPELINE_BIND_POINT_GRAPHICS,
      pipelineLayout,
      0,
      1,
      &frameInfo.globalDescriptorSet,
      0, // dynamic offsets
      nullptr);

    // Just like drawing a triangle
    vkCmdDraw(frameInfo.commandBuffer, 6, 1, 0, 0);
}

void PointLightSystem::createPipelineLayout(VkDescriptorSetLayout globalSetLayout) {
  // This is us PREDEFINING our **push constant range**
  // stageFlags set to both shaders
  // size is set to a PREDEFINED struct of what we want our pushdata to contain
  // VkPushConstantRange pushConstantRange{};
  // pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
  // pushConstantRange.offset = 0;  // (offset only used if you separate vertex and frag data)
  // pushConstantRange.size = sizeof(SimplePushConstantData);

  std::vector<VkDescriptorSetLayout> descriptorSetLayouts{globalSetLayout};

  VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
  pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

  // LAYOUTS - this is where we tell it about the descriptor set layout
  // It can take multiple layouts in the descriptorSetLayouts vector, just needs to index into that
  pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(descriptorSetLayouts.size());
  pipelineLayoutInfo.pSetLayouts = descriptorSetLayouts.data();

  // Sends small data to shader programs
  pipelineLayoutInfo.pushConstantRangeCount = 0;
  pipelineLayoutInfo.pPushConstantRanges = nullptr;
  if (vkCreatePipelineLayout(lveDevice.device(), &pipelineLayoutInfo, nullptr, &pipelineLayout) !=
      VK_SUCCESS) {
    throw std::runtime_error("failed to create pipeline layout!");
  }
}

void PointLightSystem::createPipeline(VkRenderPass renderPass) {
  assert(pipelineLayout != nullptr && "Cannot create pipeline before pipeline layout");

  PipelineConfigInfo pipelineConfig{};
  LvePipeline::defaultPipelineConfigInfo(pipelineConfig);
  pipelineConfig.attributeDescriptions.clear();
  pipelineConfig.bindingDescriptions.clear();
  pipelineConfig.renderPass = renderPass;  // render pass describes structure and
                                           // format of our frame buffer objects
  pipelineConfig.pipelineLayout = pipelineLayout;
  lvePipeline = std::make_unique<LvePipeline>(
      lveDevice,
      "shaders/point_light.vert.spv",
      "shaders/point_light.frag.spv",
      pipelineConfig);
}

}  // namespace lve