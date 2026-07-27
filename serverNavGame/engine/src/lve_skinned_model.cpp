/* lve_skinned_model.{hpp,cpp} — LveSkinnedModel: parses .glb via cgltf.h, 
concatenates all primitives into one vertex+index buffer, 
builds a bone-matrix palette (world(joint)·inverseBind, + a trailing identity slot for the file's 2 non-skinned meshes), 
and owns the palette as an SSBO - Shader Storage Buffer Object with its own descriptor set. */

#include "lve_skinned_model.hpp"

#include "lve_engine.hpp"
#include "lve_descriptors.hpp"
#include "lve_swap_chain.hpp"

// libs
/*
cgltf_parse_file cgltf_load_buffers
cgltf_node_transform_world - world transform
cgltf_accessor_read_float - from a cgltf_accessor

cgltf_uint j[4] = {0, 0, 0, 0}; jointAcc = cgltf_accessor; cgltf_size v;
cgltf_accessor_read_uint(jointAcc, v, j, 4);
*/
#define CGLTF_IMPLEMENTATION
#include <cgltf/cgltf.h>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp> // glm::quat, slerp, mat4_cast

#include <cmath> // std::fmod

// std
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace lve {

std::unique_ptr<LveSkinnedModel> LveSkinnedModel::createModelFromFile(
    const std::string& filepath) {
  return std::make_unique<LveSkinnedModel>(LveEngine::instance().getDevice(), filepath);
}

LveSkinnedModel::LveSkinnedModel(LveDevice& device, const std::string& filepath)
    : lveDevice(device) {
  // load mesh, skin, joints (fills the retained skeleton + jointMatrices sizing)
  loadGLTF(filepath);
  // fill jointMatrices for the authored bind pose, then upload to every frame's buffer
  recomputePalette();
  createBoneBuffers();
}

LveSkinnedModel::~LveSkinnedModel() {
  // Buffers free themselves via unique_ptr. The bone descriptor set is released
  // when the engine's bone pool is destroyed (the pool has no free-individual
  // flag), which matches this model's program-lifetime ownership.
}

void LveSkinnedModel::loadGLTF(const std::string& filepath) {
  cgltf_options options{};
  cgltf_data* data = nullptr;

  // parse and output into data and options
  if (cgltf_parse_file(&options, filepath.c_str(), &data) != cgltf_result_success) {
    throw std::runtime_error("failed to parse glTF: " + filepath);
  }
  if (cgltf_load_buffers(&options, data, filepath.c_str()) != cgltf_result_success) {
    cgltf_free(data);
    throw std::runtime_error("failed to load glTF buffers: " + filepath);
  }

  std::vector<Vertex> vertices;
  std::vector<uint32_t> indices;

  // --- Retain the whole node hierarchy so joints can be re-posed after load ----
  // Store each node's T pose local transform + parent, and a parent-before-child ordering
  // recomputePalette() later turns these into world transforms and then into
  // the bone matrix palette (jointMatrices[k] = world(joint[k]) * inverseBind[k]).
  const cgltf_size nodeCount = data->nodes_count;
  nodeName.resize(nodeCount);
  nodeLocalBind.resize(nodeCount);
  nodeParent.resize(nodeCount);
  nodeWorld.resize(nodeCount);
  nodeBindT.resize(nodeCount);
  nodeBindR.resize(nodeCount);
  nodeBindS.resize(nodeCount);
  for (cgltf_size ni = 0; ni < nodeCount; ++ni) {
    cgltf_node* n = &data->nodes[ni];
    nodeName[ni] = n->name ? n->name : "";
    cgltf_node_transform_local(n, glm::value_ptr(nodeLocalBind[ni]));
    nodeParent[ni] = n->parent ? static_cast<int>(n->parent - data->nodes) : -1;

    // Bind TranslationRotScale for animation seeding; initializes to identity

    // glTF stores the quaternion as (x,y,z,w); glm::quat takes (w,x,y,z)
    // This rig uses TRS nodes, instead of a raw matrix from the nodes
    nodeBindT[ni] = glm::vec3(n->translation[0], n->translation[1], n->translation[2]);
    nodeBindR[ni] = glm::quat(n->rotation[3], n->rotation[0], n->rotation[1], n->rotation[2]);
    nodeBindS[ni] = glm::vec3(n->scale[0], n->scale[1], n->scale[2]);
  }
  nodeLocal = nodeLocalBind;  // current pose starts at bind

  // Parent-before-child order via an iterative Depth First Search from the roots
  // (nodes with no parent)
  // Popping a parent before pushing its children guarantees the parent
  // lands in nodeOrder first, so world transforms accumulate in one forward pass
  nodeOrder.clear();
  nodeOrder.reserve(nodeCount);
  std::vector<char> visited(nodeCount, 0);
  std::vector<int> stack;
  for (cgltf_size ni = 0; ni < nodeCount; ++ni)
    if (!data->nodes[ni].parent) stack.push_back(static_cast<int>(ni));
  while (!stack.empty()) {
    int n = stack.back();
    stack.pop_back();
    if (visited[n]) continue;
    visited[n] = 1;
    nodeOrder.push_back(n);
    cgltf_node* nd = &data->nodes[n];
    for (cgltf_size c = 0; c < nd->children_count; ++c)
      stack.push_back(static_cast<int>(nd->children[c] - data->nodes));
  }

  // Any node not reachable from a root (shouldn't happen) still gets processed.
  for (cgltf_size ni = 0; ni < nodeCount; ++ni)
    if (!visited[ni]) nodeOrder.push_back(static_cast<int>(ni));

  // --- Skin: which node each palette joint maps to, plus its inverse bind matrix.
  // JOINTS_0 values in the mesh index directly into skin->joints
  // so palette index == joint index.
  cgltf_skin* skin = (data->skins_count > 0) ? &data->skins[0] : nullptr;
  jointNode.clear();
  inverseBind.clear();
  if (skin) {
    jointNode.resize(skin->joints_count);
    inverseBind.resize(skin->joints_count, glm::mat4(1.f));
    for (cgltf_size i = 0; i < skin->joints_count; ++i) {
      jointNode[i] = static_cast<int>(skin->joints[i] - data->nodes);
      if (skin->inverse_bind_matrices) {
        cgltf_accessor_read_float(
            skin->inverse_bind_matrices, i, glm::value_ptr(inverseBind[i]), 16);
      }
    }
  }

  // Palette = one slot per joint + a trailing identity slot for non-skinned prims.
  // recomputePalette() fills [0..jointCount-1]; the identity slot is set once here
  const uint32_t staticJointIndex = static_cast<uint32_t>(jointNode.size());
  jointMatrices.assign(jointNode.size() + 1, glm::mat4(1.f));

  // Set bounds to max for now as it will be resolved later
  aabbMin = glm::vec3(std::numeric_limits<float>::max());
  aabbMax = glm::vec3(std::numeric_limits<float>::lowest());
  
  // --- Walk every node that carries a mesh -------------------------------------
  for (cgltf_size ni = 0; ni < data->nodes_count; ++ni) {
    cgltf_node* node = &data->nodes[ni];
    if (!node->mesh) continue;

    const bool nodeSkinned = (node->skin != nullptr);

    // For a non-skinned mesh the vertices live in the node's local space, so we
    // bake the node's world transform into them and let them ride the identity joint
    // (Skinned meshes ignore their node transform per the glTF spec -- the joint matrices already place them.)
    glm::mat4 nodeWorld(1.f);
    cgltf_node_transform_world(node, glm::value_ptr(nodeWorld));

    // Compute normal
    glm::mat3 nodeNormalMat = glm::transpose(glm::inverse(glm::mat3(nodeWorld)));

    cgltf_mesh* mesh = node->mesh;
    for (cgltf_size pi = 0; pi < mesh->primitives_count; ++pi) {
      cgltf_primitive* prim = &mesh->primitives[pi];
      if (prim->type != cgltf_primitive_type_triangles) continue;

      cgltf_accessor* posAcc = nullptr;
      cgltf_accessor* norAcc = nullptr;
      cgltf_accessor* uvAcc = nullptr;
      cgltf_accessor* jointAcc = nullptr;
      cgltf_accessor* weightAcc = nullptr;

      // extract attributes
      for (cgltf_size ai = 0; ai < prim->attributes_count; ++ai) {
        cgltf_attribute* attr = &prim->attributes[ai];
        switch (attr->type) {
          case cgltf_attribute_type_position: posAcc = attr->data; break;
          case cgltf_attribute_type_normal:   norAcc = attr->data; break;
          case cgltf_attribute_type_texcoord: if (!uvAcc) uvAcc = attr->data; break;
          case cgltf_attribute_type_joints:   if (!jointAcc) jointAcc = attr->data; break;
          case cgltf_attribute_type_weights:  if (!weightAcc) weightAcc = attr->data; break;
          default: break;
        }
      }
      if (!posAcc) continue;

      // this bool checks if the prim is skinned, if so, we can store joints + weights
      // + raw vertex positions
      const bool primSkinned = nodeSkinned && jointAcc && weightAcc && skin;
      const cgltf_size vcount = posAcc->count;
      const uint32_t baseVertex = static_cast<uint32_t>(vertices.size());

      for (cgltf_size v = 0; v < vcount; ++v) {
        Vertex vert{};

        float p[3] = {0, 0, 0};
        cgltf_accessor_read_float(posAcc, v, p, 3);
        glm::vec3 pos(p[0], p[1], p[2]);

        glm::vec3 nor(0.f, 0.f, 1.f);
        if (norAcc) {
          float n[3] = {0, 0, 0};
          cgltf_accessor_read_float(norAcc, v, n, 3);
          nor = glm::vec3(n[0], n[1], n[2]);
        }

        if (uvAcc) {
          float t[2] = {0, 0};
          cgltf_accessor_read_float(uvAcc, v, t, 2);
          vert.uv = {t[0], t[1]};
        }

        if (primSkinned) {
          vert.position = pos;
          vert.normal = nor;

          cgltf_uint j[4] = {0, 0, 0, 0};
          cgltf_accessor_read_uint(jointAcc, v, j, 4);
          vert.joints = glm::uvec4(j[0], j[1], j[2], j[3]);

          float w[4] = {0, 0, 0, 0};
          cgltf_accessor_read_float(weightAcc, v, w, 4);
          vert.weights = glm::vec4(w[0], w[1], w[2], w[3]);
        } else {
          // Bake node transform into vertex positions
          // Assign identity joint index and ride it
          glm::vec4 wp = nodeWorld * glm::vec4(pos, 1.f);
          vert.position = glm::vec3(wp);
          vert.normal = glm::normalize(nodeNormalMat * nor);
          vert.joints = glm::uvec4(staticJointIndex, 0u, 0u, 0u);
          vert.weights = glm::vec4(1.f, 0.f, 0.f, 0.f);
        }

        // AABB - Tracks min/max vertex positions
        aabbMin = glm::min(aabbMin, vert.position);
        aabbMax = glm::max(aabbMax, vert.position);
        vertices.push_back(vert);
      }

      Primitive out{};
      out.vertexOffset = static_cast<int32_t>(baseVertex);
      out.firstIndex = static_cast<uint32_t>(indices.size());
      if (prim->indices) {
        const cgltf_size icount = prim->indices->count;
        for (cgltf_size ii = 0; ii < icount; ++ii) {
          // Indices are local to this primitive; vertexOffset is applied at draw
          indices.push_back(static_cast<uint32_t>(cgltf_accessor_read_index(prim->indices, ii)));
        }
        out.indexCount = static_cast<uint32_t>(icount);
      } else {
        for (cgltf_size ii = 0; ii < vcount; ++ii) indices.push_back(static_cast<uint32_t>(ii));
        out.indexCount = static_cast<uint32_t>(vcount);
      }
      primitives.push_back(out);
    }
  }

  // --- Animation clips ---------------------------------------------------------
  // Copy each clip's keyframe tracks into our own storage so they survive
  // cgltf_free. A channel targets ONE NODE's translation/rotation/scale and reads
  // from ONE SAMPLER; poseAnimation() samples these at runtime. Only LINEAR and
  // STEP interpolation are handled
  animations.clear();
  animations.resize(data->animations_count);

  for (cgltf_size ai = 0; ai < data->animations_count; ++ai) {
      cgltf_animation* src = &data->animations[ai];            // source animation from glTF data
      AnimClip& clip = animations[ai];                         // destination clip

      clip.name = src->name ? src->name : ("clip" + std::to_string(ai));

      clip.samplers.resize(src->samplers_count);

      for (cgltf_size si = 0; si < src->samplers_count; ++si) {
        cgltf_animation_sampler* ss = &src->samplers[si];      // source sampler from glTF data
        AnimSampler& out = clip.samplers[si];                  // destination sampler

        out.step = (ss->interpolation == cgltf_interpolation_type_step);

        const cgltf_size kcount = ss->input->count;            // number of keyframes

        // output track has 4 components for rotation (quat) and 3 for translation/scale
        // read up to 4 components and let the channel path decide how they are used
        const cgltf_size comps = cgltf_num_components(ss->output->type); // number of float components

        out.times.resize(kcount);
        out.values.resize(kcount, glm::vec4(0.f));

        for (cgltf_size k = 0; k < kcount; ++k) {
          cgltf_accessor_read_float(ss->input, k, &out.times[k], 1);      // read keyframe time
          float tmp[4] = {0.f, 0.f, 0.f, 0.f};                            // temp buffer for up to 4 floats
          cgltf_accessor_read_float(ss->output, k, tmp, comps);           // read keyframe value components
          out.values[k] = glm::vec4(tmp[0], tmp[1], tmp[2], tmp[3]);      // store as vec4
          if (out.times[k] > clip.duration) clip.duration = out.times[k]; // update clip duration
        }
      }

      clip.channels.resize(src->channels_count);

      for (cgltf_size ci = 0; ci < src->channels_count; ++ci) {
        cgltf_animation_channel* cc = &src->channels[ci];      // source channel from glTF data
        AnimChannel& out = clip.channels[ci];                  // destination channel
        out.node = cc->target_node ? static_cast<int>(cc->target_node - data->nodes) : -1; // target node index

        // map glTF path to engine path enum
        switch (cc->target_path) {
          case cgltf_animation_path_type_translation: out.path = 0; break;  // translation track
          case cgltf_animation_path_type_rotation:    out.path = 1; break;  // rotation track
          case cgltf_animation_path_type_scale:       out.path = 2; break;  // scale track
          default:                                    out.path = -1; break; // weights or unknown track
        }

        out.sampler = cc->sampler ? static_cast<int>(cc->sampler - src->samplers) : 0; // sampler index
      }
  }

  const cgltf_size skinJoints = skin ? skin->joints_count : 0;
  const cgltf_size animCount = data->animations_count;
  cgltf_free(data);

  std::cout << "[SkinnedModel] " << filepath << "  verts=" << vertices.size()
            << " indices=" << indices.size() << " prims=" << primitives.size()
            << " joints=" << skinJoints << " anims=" << animCount << "\n";
  for (const auto& clip : animations)
    std::cout << "[SkinnedModel]   clip '" << clip.name << "' "
              << clip.duration << "s, " << clip.channels.size() << " channels\n";
  std::cout << "[SkinnedModel] AABB min=(" << aabbMin.x << ", " << aabbMin.y << ", " << aabbMin.z
            << ")  max=(" << aabbMax.x << ", " << aabbMax.y << ", " << aabbMax.z << ")\n";

  if (vertices.empty() || indices.empty()) {
    throw std::runtime_error("glTF produced no drawable geometry: " + filepath);
  }

  createVertexBuffers(vertices);
  createIndexBuffer(indices);
}

void LveSkinnedModel::createVertexBuffers(const std::vector<Vertex>& vertices) {
  vertexCount = static_cast<uint32_t>(vertices.size());
  VkDeviceSize bufferSize = sizeof(vertices[0]) * vertexCount;
  uint32_t vertexSize = sizeof(vertices[0]);

  LveBuffer stagingBuffer{
      lveDevice,
      vertexSize,
      vertexCount,
      VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT};

  stagingBuffer.map();
  stagingBuffer.writeToBuffer((void*)vertices.data());

  vertexBuffer = std::make_unique<LveBuffer>(
      lveDevice,
      vertexSize,
      vertexCount,
      VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

  lveDevice.copyBuffer(stagingBuffer.getBuffer(), vertexBuffer->getBuffer(), bufferSize);
}

void LveSkinnedModel::createIndexBuffer(const std::vector<uint32_t>& indices) {
  indexCount = static_cast<uint32_t>(indices.size());
  hasIndexBuffer = indexCount > 0;
  if (!hasIndexBuffer) return;

  VkDeviceSize bufferSize = sizeof(indices[0]) * indexCount;
  uint32_t indexSize = sizeof(indices[0]);

  LveBuffer stagingBuffer{
      lveDevice,
      indexSize,
      indexCount,
      VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT};

  stagingBuffer.map();
  stagingBuffer.writeToBuffer((void*)indices.data());

  indexBuffer = std::make_unique<LveBuffer>(
      lveDevice,
      indexSize,
      indexCount,
      VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

  lveDevice.copyBuffer(stagingBuffer.getBuffer(), indexBuffer->getBuffer(), bufferSize);
}

void LveSkinnedModel::createBoneBuffers() {
  const uint32_t count = static_cast<uint32_t>(jointMatrices.size());
  const int frames = LveSwapChain::MAX_FRAMES_IN_FLIGHT;

  auto& layout = LveEngine::instance().getBoneSetLayout();
  auto& pool = LveEngine::instance().getBonePool();

  // One host-visible, persistently-mapped buffer + descriptor set per in-flight
  // frame, so uploadPose() can rewrite the palette each frame without ever
  // touching memory a still-in-flight frame is reading. Bound at offset 0, so
  // storage-buffer offset alignment does not matter here.
  boneBuffers.resize(frames);
  boneSets.resize(frames, VK_NULL_HANDLE);
  for (int i = 0; i < frames; ++i) {
    boneBuffers[i] = std::make_unique<LveBuffer>(
        lveDevice,
        sizeof(glm::mat4),
        count,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    boneBuffers[i]->map();
    boneBuffers[i]->writeToBuffer(jointMatrices.data());

    auto bufferInfo = boneBuffers[i]->descriptorInfo();
    if (!LveDescriptorWriter(layout, pool).writeBuffer(0, &bufferInfo).build(boneSets[i])) {
      throw std::runtime_error("failed to allocate bone descriptor set (pool exhausted?)");
    }
  }
}

int LveSkinnedModel::findNode(const std::string& substr) const {
  for (size_t i = 0; i < nodeName.size(); ++i)
    if (nodeName[i].find(substr) != std::string::npos) return static_cast<int>(i);
  return -1;
}

void LveSkinnedModel::resetPose() { nodeLocal = nodeLocalBind; }

void LveSkinnedModel::rotateJoint(int node, glm::vec3 axis, float radians) {
  if (node < 0 || node >= static_cast<int>(nodeLocal.size())) return;  // safe no-op
  if (glm::dot(axis, axis) < 1e-12f) return;
  nodeLocal[node] = nodeLocal[node] * glm::rotate(glm::mat4(1.f), radians, glm::normalize(axis));
}

void LveSkinnedModel::recomputePalette() {
  // World transforms in one forward pass (parents precede children in nodeOrder).
  for (int n : nodeOrder) {
    int p = nodeParent[n];
    nodeWorld[n] = (p >= 0) ? nodeWorld[p] * nodeLocal[n] : nodeLocal[n];
  }
  // Palette entry per joint; the trailing identity slot (jointMatrices.back()) is
  // left untouched so baked static primitives keep rendering correctly.
  for (size_t k = 0; k < jointNode.size(); ++k)
    jointMatrices[k] = nodeWorld[jointNode[k]] * inverseBind[k];
}

int LveSkinnedModel::findAnimation(const std::string& name) const {
  for (size_t i = 0; i < animations.size(); ++i)
    if (animations[i].name == name) return static_cast<int>(i);
  return -1;
}

const std::string& LveSkinnedModel::animationName(int i) const {
  static const std::string empty;
  if (i < 0 || i >= static_cast<int>(animations.size())) return empty;
  return animations[i].name;
}

namespace {

// Ugly ahh code but basically
// Locate the keyframe segment [k, k+1] bracketing `time`, and the blend fraction
// u in [0,1] within it. Then clamp to the ends (u=0 there, k pinned in range)
void locateSegment(const std::vector<float>& times, float time, int& k, float& u) {
  const int last = static_cast<int>(times.size()) - 1;
  if (last <= 0 || time <= times.front()) { k = 0; u = 0.f; return; }
  if (time >= times.back()) { k = last; u = 0.f; return; }
  int lo = 0, hi = last;
  while (lo + 1 < hi) {
    const int mid = (lo + hi) / 2;
    if (times[mid] <= time) lo = mid; else hi = mid;
  }
  k = lo;
  const float t0 = times[lo], t1 = times[hi];
  u = (t1 > t0) ? (time - t0) / (t1 - t0) : 0.f;
}
} // namespace

bool LveSkinnedModel::poseAnimation(const std::string& name, float timeSeconds, bool loop) {
  const int ci = findAnimation(name);
  if (ci < 0) return false;
  const AnimClip& clip = animations[ci];

  // Wrap (or clamp) the caller's clock into the clip's [0, duration]
  float t = timeSeconds;
  if (loop && clip.duration > 0.f) {
    t = std::fmod(t, clip.duration);
    if (t < 0.f) t += clip.duration;
  } else {
    t = t < 0.f ? 0.f : (t > clip.duration ? clip.duration : t);
  }

  // Seed every node's TRS from its bind pose, then let the clip's channels
  // overwrite the components they drive. Untouched nodes keep their bind local
  // transform (set below via nodeLocalBind), so partial clips still pose right
  std::vector<glm::vec3> T = nodeBindT;
  std::vector<glm::quat> R = nodeBindR;
  std::vector<glm::vec3> S = nodeBindS;
  std::vector<char> touched(nodeLocal.size(), 0);

  for (const AnimChannel& ch : clip.channels) {
    if (ch.node < 0 || ch.node >= static_cast<int>(nodeLocal.size()) || ch.path < 0) continue;

    const AnimSampler& s = clip.samplers[ch.sampler];
    if (s.times.empty()) continue;

    int k; float u;
    locateSegment(s.times, t, k, u);
    const bool haveNext = (k + 1) < static_cast<int>(s.values.size());

    if (ch.path == 1) 
    {
      // Rotation: quaternions stored as (x,y,z,w); glm::quat takes (w,x,y,z)
      const glm::vec4& a = s.values[k];
      glm::quat q0(a.w, a.x, a.y, a.z);
      glm::quat q = q0;
      if (!s.step && haveNext) {
        const glm::vec4& b = s.values[k + 1];
        q = glm::slerp(q0, glm::quat(b.w, b.x, b.y, b.z), u);
      }
      R[ch.node] = glm::normalize(q);
    } 
    else 
    {
      glm::vec3 v(s.values[k]);
      if (!s.step && haveNext) v = glm::mix(v, glm::vec3(s.values[k + 1]), u);
      if (ch.path == 0) T[ch.node] = v; else S[ch.node] = v;
    }
    touched[ch.node] = 1;
  }

  // Compose the animated nodes' local transforms; leave the rest at bind
  nodeLocal = nodeLocalBind;
  for (size_t n = 0; n < nodeLocal.size(); ++n) {
    if (!touched[n]) continue;
    nodeLocal[n] = glm::translate(glm::mat4(1.f), T[n]) *
                   glm::mat4_cast(glm::normalize(R[n])) *
                   glm::scale(glm::mat4(1.f), S[n]);
  }
  recomputePalette();
  return true;
}

void LveSkinnedModel::uploadPose(int frameIndex) {
  boneBuffers[frameIndex]->writeToBuffer(jointMatrices.data());
}

void LveSkinnedModel::bind(VkCommandBuffer commandBuffer) {
  VkBuffer buffers[] = {vertexBuffer->getBuffer()};
  VkDeviceSize offsets[] = {0};
  vkCmdBindVertexBuffers(commandBuffer, 0, 1, buffers, offsets);

  if (hasIndexBuffer) {
    vkCmdBindIndexBuffer(commandBuffer, indexBuffer->getBuffer(), 0, VK_INDEX_TYPE_UINT32);
  }
}

void LveSkinnedModel::draw(VkCommandBuffer commandBuffer) {
  for (const auto& prim : primitives) {
    vkCmdDrawIndexed(commandBuffer, prim.indexCount, 1, prim.firstIndex, prim.vertexOffset, 0);
  }
}

std::vector<VkVertexInputBindingDescription>
LveSkinnedModel::Vertex::getBindingDescriptions() {
  std::vector<VkVertexInputBindingDescription> bindingDescriptions(1);
  bindingDescriptions[0].binding = 0;
  bindingDescriptions[0].stride = sizeof(Vertex);
  bindingDescriptions[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
  return bindingDescriptions;
}

std::vector<VkVertexInputAttributeDescription>
LveSkinnedModel::Vertex::getAttributeDescriptions() {
  std::vector<VkVertexInputAttributeDescription> attributeDescriptions{};
  attributeDescriptions.push_back({0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, position)});
  attributeDescriptions.push_back({1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, normal)});
  attributeDescriptions.push_back({2, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex, uv)});
  // Joint indices are integers -- must use a _UINT format, not _SFLOAT.
  attributeDescriptions.push_back({3, 0, VK_FORMAT_R32G32B32A32_UINT, offsetof(Vertex, joints)});
  attributeDescriptions.push_back({4, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Vertex, weights)});
  return attributeDescriptions;
}

}  // namespace lve
