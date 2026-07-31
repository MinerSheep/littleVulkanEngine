#include "systems/simple_render_system.hpp"

// libs
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

// std
#include <iostream>
#include <array>
#include <cassert>
#include <stdexcept>

namespace lve {

// alignas is NECESSARY here!
// Push constants have alignment requirements.  Every entry is aligned to 16 bytes
// / marks 16 bytes
// Incorrect: x y r g / b - - -      Correct: x y - - / r g b -
struct SimplePushConstantData {
  glm::mat4 modelMatrix{1.f};  // IDENTITY matrix
  // alignas(16) glm::vec3 color;  // bad because 12 bytes upscales to 16 bytes
  glm::mat4 normalMatrix{1.f};
};

struct UIPushConstantData {
  glm::mat2 transform{1.f};
  glm::vec2 offset;
  alignas(16) glm::vec3 color;  // bad because 12 bytes upscales to 16 bytes
  float alpha;
};

SimpleRenderSystem::SimpleRenderSystem(LveDevice& device, VkRenderPass renderPass, VkDescriptorSetLayout globalSetLayout)
    : lveDevice{device} {
  createPipelineLayout(globalSetLayout);

  // Good idea to check render pass compatibility here.  If its compatible then pipeline doesnt need
  // to be recreated Since it can use the same blueprint for submitted framebuffers and be just fine
  createPipeline(renderPass);
}

SimpleRenderSystem::~SimpleRenderSystem() {
  // Dont forget to destroy the pipeline layout in destructor, all vk objects needs to do this
  vkDestroyPipelineLayout(lveDevice.device(), pipelineLayout, nullptr);
}

void SimpleRenderSystem::render(
    FrameInfo& frameInfo) {
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

  for (const auto& item : frameInfo.renderItems) {
    SimplePushConstantData push{};

    // We are now calculating on the GPU, not the CPU
    push.modelMatrix = item.modelMatrix;
    push.normalMatrix = item.normalMatrix;

    // RECORD our push constant data
    vkCmdPushConstants(
        frameInfo.commandBuffer,
        pipelineLayout,
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        0,
        sizeof(SimplePushConstantData),
        &push);
    item.model->bind(frameInfo.commandBuffer);
    item.model->draw(frameInfo.commandBuffer);
  }
}

void SimpleRenderSystem::renderUI(
    FrameInfo& frameInfo)
{
  UIPipeline->bind(frameInfo.commandBuffer);

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

  for (const auto& item : frameInfo.UIrenderItems) {
      UIPushConstantData push{};

      // We are now calculating on the GPU, not the CPU
      push.transform = item.transform;
      push.offset = item.offset;
      push.color = item.color;
      push.alpha = item.alpha;

      // RECORD our push constant data
      vkCmdPushConstants(
          frameInfo.commandBuffer,
          pipelineLayout,
          VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
          0,
          sizeof(UIPushConstantData),
          &push);

      if (item.model)
      {
        item.model->bind(frameInfo.commandBuffer);
        item.model->draw(frameInfo.commandBuffer);
      }
      else
      {
        std::cout << "no model";
        vkCmdDraw(frameInfo.commandBuffer, 3, 1, 0, 0);
      }
  }
} 

void SimpleRenderSystem::createPipelineLayout(VkDescriptorSetLayout globalSetLayout) {
  // This is us PREDEFINING our **push constant range**
  // stageFlags set to both shaders
  // size is set to a PREDEFINED struct of what we want our pushdata to contain
  VkPushConstantRange pushConstantRange{};
  pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
  pushConstantRange.offset = 0;  // (offset only used if you separate vertex and frag data)
  pushConstantRange.size = sizeof(SimplePushConstantData);

  std::vector<VkDescriptorSetLayout> descriptorSetLayouts{globalSetLayout};

  VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
  pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

  // LAYOUTS - this is where we tell it about the descriptor set layout
  // It can take multiple layouts in the descriptorSetLayouts vector, just needs to index into that
  pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(descriptorSetLayouts.size());
  pipelineLayoutInfo.pSetLayouts = descriptorSetLayouts.data();

  // Sends small data to shader programs
  pipelineLayoutInfo.pushConstantRangeCount = 1;
  pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;
  if (vkCreatePipelineLayout(lveDevice.device(), &pipelineLayoutInfo, nullptr, &pipelineLayout) !=
      VK_SUCCESS) {
    throw std::runtime_error("failed to create pipeline layout!");
  }
}

void SimpleRenderSystem::createPipeline(VkRenderPass renderPass) {
  assert(pipelineLayout != nullptr && "Cannot create pipeline before pipeline layout");

  PipelineConfigInfo pipelineConfig{};
  LvePipeline::defaultPipelineConfigInfo(pipelineConfig);
  pipelineConfig.renderPass = renderPass;  // render pass describes structure and
                                           // format of our frame buffer objects
  pipelineConfig.pipelineLayout = pipelineLayout;

  std::string exeDir = getExecutableDir();
  lvePipeline = std::make_unique<LvePipeline>(
      lveDevice,
      exeDir + "/../shaders/simple_shader.vert.spv",
      exeDir + "/../shaders/simple_shader.frag.spv",
      pipelineConfig);

  // UI is a 2D overlay drawn after the 3D pass. Draw it in submission order
  // (painter's algorithm) rather than depth-testing: every UI quad sits at z=0, so
  // with depth-test LESS two overlapping UI quads reject each other and the later one
  // vanishes (e.g. text drawn over the weather arrows would be partially eaten).
  // Disabling depth test/write lets later UI items paint cleanly on top.
  PipelineConfigInfo uiConfig{};
  LvePipeline::defaultPipelineConfigInfo(uiConfig);
  uiConfig.renderPass = renderPass;
  uiConfig.pipelineLayout = pipelineLayout;
  uiConfig.depthStencilInfo.depthTestEnable = VK_FALSE;
  uiConfig.depthStencilInfo.depthWriteEnable = VK_FALSE;

  // Blend on alpha, so a UI quad can be see-through. UIRenderItem carries an alpha
  // and the shader reads it, but with blending off it painted solid no matter what
  // it was set to. A fade to black is one black quad over the whole screen
  LvePipeline::enableAlphaBlending(uiConfig);

  UIPipeline = std::make_unique<LvePipeline>(
      lveDevice,
      exeDir + "/../shaders/ui_shader.vert.spv",
      exeDir + "/../shaders/ui_shader.frag.spv",
      uiConfig);
}

}  // namespace lve