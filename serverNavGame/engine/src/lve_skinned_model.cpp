/* lve_skinned_model.{hpp,cpp} — LveSkinnedModel: parses .glb via cgltf.h, 
concatenates all primitives into one vertex+index buffer, 
builds a bone-matrix palette (world(joint)·inverseBind, + a trailing identity slot for the file's 2 non-skinned meshes), 
and owns the palette as an SSBO - Shader Storage Buffer Object with its own descriptor set. */

#include "lve_skinned_model.hpp"

#include "lve_engine.hpp"
#include "lve_descriptors.hpp"

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
  // load mesh, skin, joints
  loadGLTF(filepath);
  createBoneBuffer();
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

  // --- Build the bone matrix palette from the (first) skin ---------------------
  // Each bone gets a worldTransform multiplied by its inverseBindMatrix and stored in k
  // jointMatrices[k] = worldTransform(joint[k]) * inverseBindMatrix[k]

  // JOINTS_0 values in the mesh index directly into skin->joints, so palette
  // index == joint index. At bind pose each of these is ~identity
  cgltf_skin* skin = (data->skins_count > 0) ? &data->skins[0] : nullptr;
  jointMatrices.clear();
  if (skin) {
    jointMatrices.resize(skin->joints_count);
    for (cgltf_size i = 0; i < skin->joints_count; ++i) {
      glm::mat4 world(1.f);

      // compute world transform
      cgltf_node_transform_world(skin->joints[i], glm::value_ptr(world));

      glm::mat4 ibm(1.f);
      if (skin->inverse_bind_matrices) {
        cgltf_accessor_read_float(skin->inverse_bind_matrices, i, glm::value_ptr(ibm), 16);
      }
      jointMatrices[i] = world * ibm;
    }
  }

  // Trailing identity slot: lets static meshes ride a fake bone
  // used by non-skinned primitives
  const uint32_t staticJointIndex = static_cast<uint32_t>(jointMatrices.size());
  jointMatrices.push_back(glm::mat4(1.f));

  // --- Walk every node that carries a mesh -------------------------------------
  glm::vec3 aabbMin(std::numeric_limits<float>::max());
  glm::vec3 aabbMax(std::numeric_limits<float>::lowest());

  for (cgltf_size ni = 0; ni < data->nodes_count; ++ni) {
    cgltf_node* node = &data->nodes[ni];
    if (!node->mesh) continue;

    const bool nodeSkinned = (node->skin != nullptr);

    // For a non-skinned mesh the vertices live in the node's local space, so we
    // bake the node's world transform into them and let them ride the identity
    // joint
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

  const cgltf_size skinJoints = skin ? skin->joints_count : 0;
  cgltf_free(data);

  std::cout << "[SkinnedModel] " << filepath << "  verts=" << vertices.size()
            << " indices=" << indices.size() << " prims=" << primitives.size()
            << " joints=" << skinJoints << "\n";
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

void LveSkinnedModel::createBoneBuffer() {
  const uint32_t count = static_cast<uint32_t>(jointMatrices.size());

  // Host-visible + coherent -> ALLOWS writing to the palette directly. Bound at
  // offset 0, so storage-buffer offset alignment is irrelevant here
  boneBuffer = std::make_unique<LveBuffer>(
      lveDevice,
      sizeof(glm::mat4),
      count,
      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

  boneBuffer->map();
  boneBuffer->writeToBuffer(jointMatrices.data());

  auto& layout = LveEngine::instance().getBoneSetLayout();
  auto& pool = LveEngine::instance().getBonePool();
  auto bufferInfo = boneBuffer->descriptorInfo();
  if (!LveDescriptorWriter(layout, pool).writeBuffer(0, &bufferInfo).build(boneSet)) {
    throw std::runtime_error("failed to allocate bone descriptor set (pool exhausted?)");
  }
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
