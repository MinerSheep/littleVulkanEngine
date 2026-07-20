#pragma once

#include "lve_device.hpp"
#include "lve_buffer.hpp"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp> // glm::quat, for animation keyframes

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

  // ---- glTF clip playback ----
  // Pose the skeleton to the animation clip, sampled at timeSeconds 
  // Rebuilds the palette (this fully replaces the
  // resetPose/rotateJoint/recomputePalette dance for a played clip). 
  // With loop true the time wraps over the clip length, so a caller can feed an
  // ever-increasing clock. Returns false and leaves the pose untouched when no
  // clip of that name exists. At draw time still follow with uploadPose(frame)
  bool poseAnimation(const std::string& name, float timeSeconds, bool loop = true);

  // Clips parsed from the file: how many, and the name of clip i ("" if out of
  // range). Lets a caller list/cycle the available animations by index
  int animationCount() const { return static_cast<int>(animations.size()); }
  const std::string& animationName(int i) const;

  // The set = 1 descriptor holding this model's bone palette for a given frame.
  VkDescriptorSet boneDescriptorSet(int frameIndex) const { return boneSets[frameIndex]; }
  uint32_t jointCount() const { return static_cast<uint32_t>(jointMatrices.size()); }

 private:
  void loadGLTF(const std::string& filepath);
  void createVertexBuffers(const std::vector<Vertex>& vertices);
  void createIndexBuffer(const std::vector<uint32_t>& indices);
  void createBoneBuffers();

  // Index of the clip named `name`, or -1 if absent
  int findAnimation(const std::string& name) const;

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

  // ---- Animation Logic ---------------------------------------------------

  // Authored bind‑pose Translation Rotation Scale per node so clips with 
  // partial channels still compose correctly against rest values
  std::vector<glm::vec3> nodeBindT;
  std::vector<glm::quat> nodeBindR;
  std::vector<glm::vec3> nodeBindS;

  // ---- glTF animation clips (parsed at load) ----
  // A sampler is one keyframe track: times[k] paired with values[k]
  // values hold xyz for translation/scale, or a quaternion xyzw for rotation
  // step = step interpolation, !step = linear interpolation
  struct AnimSampler {
    std::vector<float> times;
    std::vector<glm::vec4> values;
    bool step = false;
  };

  // A channel binds one sampler to a node's translation/rotation/scale
  struct AnimChannel {
    int node = -1;
    int path = 0;    // 0 = translation, 1 = rotation, 2 = scale, -1 = ignore
    int sampler = 0; // index into the owning clip's samplers
  };

  struct AnimClip {
    std::string name;
    float duration = 0.f; // seconds, = latest keyframe time across its samplers
    std::vector<AnimSampler> samplers;
    std::vector<AnimChannel> channels;
  };
  std::vector<AnimClip> animations;

  // ---- Bone Logic ---------------------------------------------------

  // Bone matrix palette: one matrix per joint + a trailing identity slot used by
  // non-skinned primitives (their node transform is baked into their vertices).
  // At bind pose every joint matrix is ~identity (the authored rest pose)
  std::vector<glm::mat4> jointMatrices;

  // One bone buffer + descriptor set per frame-in-flight (uploadPose)
  std::vector<std::unique_ptr<LveBuffer>> boneBuffers;
  std::vector<VkDescriptorSet> boneSets;
};

}  // namespace lve
