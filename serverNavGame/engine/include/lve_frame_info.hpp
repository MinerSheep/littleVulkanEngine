#pragma once

#include "lve_camera.hpp"
#include "lve_model.hpp"
// #include "lve_game_object.hpp"

// lib
#include <vulkan/vulkan.h>

#include <string>

std::string getExecutableDir();   // declaration only

namespace lve {

  // Defined in lve_skinned_model.hpp; only referenced by pointer here.
  class LveSkinnedModel;

  // Defined in lve_texture.hpp, and only ever pointed at from here
  class LveTexture;

  #define MAX_LIGHTS 10

  // iterate through each point light and accumulate its effects on the model
  struct PointLight {
    glm::vec4 position{}; // ignore w
    glm::vec4 color{}; // w is intensity
  };

  struct LightRenderItem {
    glm::vec3 position;
    glm::vec3 color;
    float intensity;
    float radius;
    float distanceToCamera;
};
  struct RenderItem {
    glm::mat4 modelMatrix;
    glm::mat4 normalMatrix;
    LveModel* model;

    // this needs to be initialized otherwise its corrupt data
    float alpha = 1.f;

    // A picture painted across the top of the mesh, or null for a flat colour
    // Textured items are drawn by their own pipeline, after the plain ones
    LveTexture* texture = nullptr;

    // How many times the picture repeats over a metre of world
    glm::vec2 uvScale{0.5f};

    // Where the floor of the room is, which is where a picture on a wall starts
    // climbing from rather than halfway up a board
    float groundY = 0.f;
  };
  struct UIRenderItem {
    glm::mat2 transform;
    glm::vec2 offset;
    glm::vec3 color;
    float alpha;
    LveModel* model;
  };
  struct SkinnedRenderItem {
    glm::mat4 modelMatrix;
    glm::mat4 normalMatrix;
    LveSkinnedModel* model;
    // Flat color the whole mesh draws in, 0.85 grey is the untinted look
    glm::vec3 tint{0.85f};
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
  VkCommandBuffer commandBuffer;
  VkDescriptorSet globalDescriptorSet;

  std::vector<RenderItem> renderItems;
  std::vector<UIRenderItem> UIrenderItems;
  std::vector<LightRenderItem> lightItems;
  std::vector<SkinnedRenderItem> skinnedRenderItems;

  // The backdrop, drawn first
  std::vector<UIRenderItem> backgroundItems;
};
}  // namespace lve