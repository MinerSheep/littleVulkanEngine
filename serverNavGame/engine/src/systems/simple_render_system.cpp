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

// 128 max bytes, the normalMatrix is a mat3 but it passes in alpha as [3][3]
struct SimplePushConstantData {
  glm::mat4 modelMatrix{1.f};  // IDENTITY matrix
  // alignas(16) glm::vec3 color;  // bad because 12 bytes upscales to 16 bytes
  glm::mat4 normalMatrix{1.f};  // [3][3] carries the alpha
};

static_assert(sizeof(SimplePushConstantData) <= 128,
              "push constants must fit in the 128 bytes Vulkan guarantees");

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
  // Every set overwritten must overwrite every set that comes after it
  // Bind it once, now ALL gameobjects can use it without need for rebinding
  // Both pipelines share this layout, so swapping pipeline keeps the binding
  vkCmdBindDescriptorSets(
      frameInfo.commandBuffer,
      VK_PIPELINE_BIND_POINT_GRAPHICS,
      pipelineLayout,
      0,
      1,
      &frameInfo.globalDescriptorSet,
      0, // dynamic offsets
      nullptr);

  auto drawItem = [&](const RenderItem& item) {
    SimplePushConstantData push{};

    // We are now calculating on the GPU, not the CPU
    push.modelMatrix = item.modelMatrix;
    push.normalMatrix = item.normalMatrix;

    // Alpha travels in the spare corner, see the struct above
    push.normalMatrix[3][3] = item.alpha;

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
  };

  // Solids, transparent objects will blend with these
  lvePipeline->bind(frameInfo.commandBuffer);
  for (const auto& item : frameInfo.renderItems) {
    if (item.alpha >= 1.f) drawItem(item);
  }

  // Transparent, blends and doesn't write depth
  //
  // Small issue is that depth ordering doesn't happen
  // So farther away transparent objects should be drawn first
  transparentPipeline->bind(frameInfo.commandBuffer);
  for (const auto& item : frameInfo.renderItems) {
    if (item.alpha < 1.f) drawItem(item);
  }
}

void SimpleRenderSystem::renderUI(
    FrameInfo& frameInfo,
    const std::vector<UIRenderItem>& items)
{
  // Nothing to draw means no pipeline swap and no descriptor bind
  // This is what keeps the background free for every scene that does not want one
  if (items.empty()) return;

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

  for (const auto& item : items) {
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

  // See-through geometry: the same shaders, but it blends and leaves the depth
  // buffer alone
  //
  // Depth writing has to be off. A ghosted wall stands nearer the camera than
  // everything else in the room, so if it wrote depth it would reject the floor
  // and the props behind it and you would see the background through the room
  // Depth TESTING stays on, so a see-through thing still hides behind solid ones
  PipelineConfigInfo transparentConfig{};
  LvePipeline::defaultPipelineConfigInfo(transparentConfig);
  transparentConfig.renderPass = renderPass;
  transparentConfig.pipelineLayout = pipelineLayout;
  transparentConfig.depthStencilInfo.depthWriteEnable = VK_FALSE;
  LvePipeline::enableAlphaBlending(transparentConfig);

  transparentPipeline = std::make_unique<LvePipeline>(
      lveDevice,
      exeDir + "/../shaders/simple_shader.vert.spv",
      exeDir + "/../shaders/simple_shader.frag.spv",
      transparentConfig);

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