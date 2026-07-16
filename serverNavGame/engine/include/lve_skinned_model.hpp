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
// Where LveModel produces a single vertex/index buffer with position/color/normal/uv
// LveSkinnedModel adds two per-vertex attributes -- joint indices + blend weights
// It also owns a bone matrix "palette" (an SSBO) that the skinning vertex shader reads via set = 1
class LveSkinnedModel {
 public:

  // Does not include color
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
  // made of primitives; we concatenate them all so a whole model is one bind
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

  // ---- Procedural posing ----
  // A per-frame pose pass looks like:
  //   resetPose();
  //   rotateJoint(findNode("head.x"), {1,0,0}, angle);   // as many as you like
  //   recomputePalette();
  // then, at draw time, uploadPose(frameIndex) + boneDescriptorSet(frameIndex)

  // First skeleton node whose name contains `substr`, or -1 if none.
  int findNode(const std::string& substr) const;

  // Restore every node to its authored T pose, always call once at the start of the pose
  void resetPose();

  // Post-rotate the node's local transform about axis by radians. Composes
  // across calls, and moves that node plus everything parented beneath it. No-op
  // for node < 0, so `rotateJoint(findNode(...), ...)` is safe when a name is absent
  void rotateJoint(int node, glm::vec3 axis, float radians);
  
  // Rebuild the bone matrix palette from the current node transforms (CPU only)
  void recomputePalette();
  
  // Copy the current palette into the frame at frameIndex's bone buffer. Call at draw
  // time (after that frame's fence has been waited on in beginFrame):
  // one buffer per in-flight frame means we never overwrite what gpu is reading
  void uploadPose(int frameIndex);

  // The set = 1 descriptor holding this model's bone palette for a given frame.
  VkDescriptorSet boneDescriptorSet(int frameIndex) const { return boneSets[frameIndex]; }
  uint32_t jointCount() const { return static_cast<uint32_t>(jointMatrices.size()); }

 private:
  void loadGLTF(const std::string& filepath);
  void createVertexBuffers(const std::vector<Vertex>& vertices);
  void createIndexBuffer(const std::vector<uint32_t>& indices);
  void createBoneBuffers();

  LveDevice& lveDevice;

  std::unique_ptr<LveBuffer> vertexBuffer;
  uint32_t vertexCount = 0;

  bool hasIndexBuffer = false;
  std::unique_ptr<LveBuffer> indexBuffer;
  uint32_t indexCount = 0;

  std::vector<Primitive> primitives;

  // ---- Retained skeleton (kept so joints can be re-posed after load) ----
  // Per node: name, authored (bind) local transform, current local transform, and
  // parent index (-1 for roots). `nodeOrder` lists nodes parent-before-child so
  // world transforms accumulate in a single pass; `nodeWorld` is scratch for that.
  std::vector<std::string> nodeName;
  std::vector<glm::mat4> nodeLocalBind;
  std::vector<glm::mat4> nodeLocal;
  std::vector<int> nodeParent;
  std::vector<int> nodeOrder;
  std::vector<glm::mat4> nodeWorld;
  // Per palette joint: which node it comes from, and its inverse bind matrix.
  std::vector<int> jointNode;
  std::vector<glm::mat4> inverseBind;

  // Bone matrix palette: one matrix per joint + a trailing identity slot used by
  // non-skinned primitives (their node transform is baked into their vertices).
  // At bind pose every joint matrix is ~identity (the authored rest pose).
  std::vector<glm::mat4> jointMatrices;

  // One bone buffer + descriptor set per frame-in-flight (see uploadPose).
  std::vector<std::unique_ptr<LveBuffer>> boneBuffers;
  std::vector<VkDescriptorSet> boneSets;
};

}  // namespace lve
