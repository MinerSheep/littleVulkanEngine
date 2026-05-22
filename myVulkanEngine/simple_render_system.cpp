#include "simple_render_system.hpp"

// libs
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

// std
#include <array>
#include <cassert>
#include <stdexcept>

namespace lve {

// alignas is NECESSARY here!
// Push constants have alignment requirements.  Every entry is aligned to 16 bytes
// / marks 16 bytes
// Incorrect: x y r g / b - - -      Correct: x y - - / r g b -
struct SimplePushConstantData {
  glm::mat4 transform{1.f};     // IDENTITY matrix
  // alignas(16) glm::vec3 color;  // bad because 12 bytes upscales to 16 bytes
  glm::mat4 modelMatrix{1.f};
};

SimpleRenderSystem::SimpleRenderSystem(LveDevice& device, VkRenderPass renderPass)
    : lveDevice{device} {
  createPipelineLayout();

  // Good idea to check render pass compatibility here.  If its compatible then pipeline doesnt need
  // to be recreated Since it can use the same blueprint for submitted framebuffers and be just fine
  createPipeline(renderPass);
}

SimpleRenderSystem::~SimpleRenderSystem() {
  // Dont forget to destroy the pipeline layout in destructor, all vk objects needs to do this
  vkDestroyPipelineLayout(lveDevice.device(), pipelineLayout, nullptr);
}

void SimpleRenderSystem::createPipelineLayout() {
  // This is us PREDEFINING our **push constant range**
  // stageFlags set to both shaders
  // size is set to a PREDEFINED struct of what we want our pushdata to contain
  VkPushConstantRange pushConstantRange{};
  pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
  pushConstantRange.offset = 0;  // (offset only used if you separate vertex and frag data)
  pushConstantRange.size = sizeof(SimplePushConstantData);

  VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
  pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

  // Layouts are responsible for putting textures in
  pipelineLayoutInfo.setLayoutCount = 0;
  pipelineLayoutInfo.pSetLayouts = nullptr;

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
  lvePipeline = std::make_unique<LvePipeline>(
      lveDevice,
      "shaders/simple_shader.vert.spv",
      "shaders/simple_shader.frag.spv",
      pipelineConfig);
}

void SimpleRenderSystem::renderGameObjects(
    VkCommandBuffer commandBuffer, std::vector<LveGameObject>& gameObjects, const LveCamera& camera) {
  lvePipeline->bind(commandBuffer);

  auto projectionView = camera.getProjection() * camera.getView();

  for (auto& obj : gameObjects) {

    SimplePushConstantData push{};
    auto modelMatrix = obj.transform.mat4();

    // We are now calculating on the GPU, not the CPU
    push.transform = projectionView * modelMatrix;
    push.modelMatrix = modelMatrix;

    // RECORD our push constant data
    vkCmdPushConstants(
        commandBuffer,
        pipelineLayout,
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        0,
        sizeof(SimplePushConstantData),
        &push);
    obj.model->bind(commandBuffer);
    obj.model->draw(commandBuffer);
  }
}

}  // namespace lve