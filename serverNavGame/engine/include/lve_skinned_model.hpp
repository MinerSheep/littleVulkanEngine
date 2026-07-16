#pragma once

#include "lve_device.hpp"
#include "lve_buffer.hpp"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

// std
#include <memory>
#include <string>
#include <vector>

namespace lve {

// A skinned (skeletal) mesh loaded from a glTF / .glb file.
//
// This is intentionally SEPARATE from LveModel (which loads static .obj files)
// so the existing static render path stays untouched. Where LveModel produces a
// single vertex/index buffer with position/color/normal/uv, LveSkinnedModel adds
// two per-vertex attributes -- joint indices + blend weights -- and owns a bone
// matrix "palette" (an SSBO) that the skinning vertex shader reads via set = 1.
class LveSkinnedModel {
 public:
  struct Vertex {
    glm::vec3 position{};
    glm::vec3 normal{};
    glm::vec2 uv{};
    glm::uvec4 joints{0u};    // up to 4 bone indices into the palette
    glm::vec4 weights{0.f};   // matching blend weights (sum to 1)

    static std::vector<VkVertexInputBindingDescription> getBindingDescriptions();
    static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions();
  };

  // One drawable range inside the shared vertex/index buffers. glTF meshes are
  // made of primitives; we concatenate them all so a whole model is one bind.
  struct Primitive {
    uint32_t firstIndex;
    uint32_t indexCount;
    int32_t vertexOffset;
  };

  LveSkinnedModel(LveDevice& device, const std::string& filepath);
  ~LveSkinnedModel();

  LveSkinnedModel(const LveSkinnedModel&) = delete;
  LveSkinnedModel& operator=(const LveSkinnedModel&) = delete;

  static std::unique_ptr<LveSkinnedModel> createModelFromFile(const std::string& filepath);

  void bind(VkCommandBuffer commandBuffer);
  void draw(VkCommandBuffer commandBuffer);

  // The set = 1 descriptor holding this model's bone matrix palette.
  VkDescriptorSet boneDescriptorSet() const { return boneSet; }
  uint32_t jointCount() const { return static_cast<uint32_t>(jointMatrices.size()); }

 private:
  void loadGLTF(const std::string& filepath);
  void createVertexBuffers(const std::vector<Vertex>& vertices);
  void createIndexBuffer(const std::vector<uint32_t>& indices);
  void createBoneBuffer();

  LveDevice& lveDevice;

  std::unique_ptr<LveBuffer> vertexBuffer;
  uint32_t vertexCount = 0;

  bool hasIndexBuffer = false;
  std::unique_ptr<LveBuffer> indexBuffer;
  uint32_t indexCount = 0;

  std::vector<Primitive> primitives;

  // Bone matrix palette: one matrix per joint, plus a trailing identity slot
  // (the last index) used by any non-skinned primitives whose node transform we
  // baked into their vertices at load time. At bind pose every joint matrix is
  // ~identity, so with no animation the mesh renders in its authored rest pose.
  std::vector<glm::mat4> jointMatrices;
  std::unique_ptr<LveBuffer> boneBuffer;
  VkDescriptorSet boneSet = VK_NULL_HANDLE;
};

}  // namespace lve
