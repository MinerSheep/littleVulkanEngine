#pragma once

#include "lve_camera.hpp"
#include "lve_game_object.hpp"

// lib
#include <vulkan/vulkan.h>

namespace lve {

  #define MAX_LIGHTS 10

  // iterate through each point light and accumulate its effects on the model
  struct PointLight {
    glm::vec4 position{}; // ignore w
    glm::vec4 color{}; // w is intensity
  };

  struct GlobalUbo {
    glm::mat4 projection{1.f};
    glm::mat4 view{1.f}; // world -> cam space
    glm::mat4 inverseView{1.f}; // cam position can be extracted from last col - cam -> world space
    glm::vec4 ambientLightColor{1.f,1.f,1.f,0.02f};
    PointLight pointLights[MAX_LIGHTS];
    int numLights;
  };

struct FrameInfo {
  int frameIndex;
  float frameTime;
  VkCommandBuffer commandBuffer;
  LveCamera &camera;
  VkDescriptorSet globalDescriptorSet;

  LveGameObject::Map &gameObjects;
};
}  // namespace lve