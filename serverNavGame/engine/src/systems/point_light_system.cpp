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
#include <map>

namespace lve {
  // we could render multiple lights through simple indexing with one draw call
  // however multiple draw calls w push constant makes easier to add modifications to each light (+ shader code simpler)
  struct PointLightPushConstant {
    glm::vec4 position{};
    glm::vec4 color{};
    float radius;
  };

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

void PointLightSystem::update(FrameInfo& frameInfo, GlobalUbo& ubo) {

  auto rotateLight = glm::rotate(glm::mat4(1.f), 0.5f * frameInfo.frameTime, {0.f, -1.f, 0.f});

  int lightIndex = 0;
  for (auto& kv : frameInfo.gameObjects)
  {
    auto& obj = kv.second;
    // no point light component = not point light
    if (obj.pointLight == nullptr) continue;

    assert(lightIndex < MAX_LIGHTS && "Point lights exceed maximum specified");

    // update light position
    obj.transform.translation = glm::vec3(rotateLight * glm::vec4(obj.transform.translation, 1.f));

    // copy light to the ubo
    ubo.pointLights[lightIndex].position = glm::vec4(obj.transform.translation, 1.0f);
    ubo.pointLights[lightIndex].color = glm::vec4(obj.color, obj.pointLight->lightIntensity);
    lightIndex++;
  }

  ubo.numLights = lightIndex;
}

void PointLightSystem::render(FrameInfo& frameInfo) {
  // we need to sort lights by distance to camera
  std::map<float, LveGameObject::id_t> sorted;
  for (auto& kv : frameInfo.gameObjects)
    {
      auto& obj = kv.second;
      // no point light component = not point light
      if (obj.pointLight == nullptr) continue;

      // calc distance w length squared
      auto offset = frameInfo.camera.getPosition() - obj.transform.translation;
      float disSquared = glm::dot(offset, offset);
      sorted[disSquared] = obj.getId();
    }

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

  // iterate through map in reverse
  for (auto it = sorted.rbegin(); it != sorted.rend(); it++) {
    auto& obj = frameInfo.gameObjects.at(it->second);

    // inefficient method of looping through lights?
    PointLightPushConstant push{};
    push.position = glm::vec4(obj.transform.translation, 1.0f);
    push.color = glm::vec4(obj.color, obj.pointLight->lightIntensity);
    push.radius = obj.transform.scale.x;

    // push the constants
    vkCmdPushConstants(
        frameInfo.commandBuffer,
        pipelineLayout,
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        0,
        sizeof(PointLightPushConstant),
        &push);

    // Just like drawing a triangle, previously we used ubo info to draw it
    // now we will use the push constant info to draw each light
    vkCmdDraw(frameInfo.commandBuffer, 6, 1, 0, 0);
  }
}

void PointLightSystem::createPipelineLayout(VkDescriptorSetLayout globalSetLayout) {
  // This is us PREDEFINING our **push constant range**
  // stageFlags set to both shaders
  // size is set to a PREDEFINED struct of what we want our pushdata to contain
  VkPushConstantRange pushConstantRange{};
  pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
  pushConstantRange.offset = 0;  // (offset only used if you separate vertex and frag data)
  pushConstantRange.size = sizeof(PointLightPushConstant);

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

void PointLightSystem::createPipeline(VkRenderPass renderPass) {
  assert(pipelineLayout != nullptr && "Cannot create pipeline before pipeline layout");

  PipelineConfigInfo pipelineConfig{};
  LvePipeline::defaultPipelineConfigInfo(pipelineConfig);
  LvePipeline::enableAlphaBlending(pipelineConfig);
  
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