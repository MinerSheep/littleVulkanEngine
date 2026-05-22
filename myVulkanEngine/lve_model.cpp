#include "lve_model.hpp"

#include "lve_utils.hpp"

//libs
#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader/tiny_obj_loader.h"
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>

#include <cstring>
#include <iostream>
#include <unordered_map>

namespace std {
    template <>
    struct hash<lve::LveModel::Vertex> {
        size_t operator()(lve::LveModel::Vertex const & vertex) const {
            size_t seed = 0;

            // hash an instance of vertex to a hash value of size_t
            lve::hashCombine(seed, vertex.position, vertex.color, vertex.normal, vertex.uv);
            return seed;
        }
    };
}

lve::LveModel::LveModel(LveDevice& device, const LveModel::Builder& builder)
    : lveDevice(device) {
    createVertexBuffers(builder.vertices);
    createIndexBuffer(builder.indices);
}

lve::LveModel::~LveModel() {
    vkDestroyBuffer(lveDevice.device(), vertexBuffer, nullptr);
    vkFreeMemory(lveDevice.device(), vertexBufferMemory, nullptr);

    if (hasIndexBuffer)
    {
        vkDestroyBuffer(lveDevice.device(), indexBuffer, nullptr);
        vkFreeMemory(lveDevice.device(), indexBufferMemory, nullptr);
    }
}

std::unique_ptr<lve::LveModel> lve::LveModel::createModelFromFile(
    LveDevice& device, const std::string& filepath) {
  Builder builder{};
  builder.loadModel(filepath);

  std::cout << "Vertex count: " << builder.vertices.size() << "\n";

  return std::make_unique<LveModel>(device, builder);
}

void lve::LveModel::bind(VkCommandBuffer commandBuffer) {
  VkBuffer buffers[] = {vertexBuffer};
  VkDeviceSize offsets[] = {0};

  // Bind one vertex buffer starting at binding 0  with a {0} offset
  vkCmdBindVertexBuffers(commandBuffer, 0, 1, buffers, offsets);

  if (hasIndexBuffer) {
    // can upgrade to uint64 for WAY complex models
    vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);
  }
}

void lve::LveModel::draw(VkCommandBuffer commandBuffer) {
    if (hasIndexBuffer) {
        vkCmdDrawIndexed(commandBuffer, indexCount, 1,0,0,0);
    }
    else
    // This is responsible for putting multiple shader variable offsets into a single vertex buffers I think
    vkCmdDraw(commandBuffer, vertexCount, 1, 0, 0);
}

void lve::LveModel::createVertexBuffers(const std::vector<Vertex>& vertices) {
    vertexCount = static_cast<uint32_t>(vertices.size());
    assert(vertexCount >= 3 && "Vertex count must be at least 3");

    VkDeviceSize bufferSize = sizeof(vertices[0]) * vertexCount;

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;

    // VISIBLE BIT = memory is accessible from its host CPU (this is essentially an automatic flush)
    lveDevice.createBuffer(bufferSize, 
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,  // tell vulkan that buffer is used just as source location for memory transfer 
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 
        stagingBuffer, stagingBufferMemory);

    // 0 offset, 0 flags 
    void* data;
    // This makes it so we have a ptr to vertexData on our Host CPU and we can write to it, which flushes to the Device GPU 
    vkMapMemory(lveDevice.device(), stagingBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, vertices.data(), static_cast<size_t>(bufferSize));  // will copy the new color data as well
    vkUnmapMemory(lveDevice.device(), stagingBufferMemory);

    lveDevice.createBuffer(bufferSize, 
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 
        vertexBuffer, vertexBufferMemory);

    lveDevice.copyBuffer(stagingBuffer, vertexBuffer, bufferSize);

    vkDestroyBuffer(lveDevice.device(), stagingBuffer, nullptr);
    vkFreeMemory(lveDevice.device(), stagingBufferMemory, nullptr);
}

void lve::LveModel::createIndexBuffer(const std::vector<uint32_t>& indices) {
    indexCount = static_cast<uint32_t>(indices.size());
    hasIndexBuffer = indexCount > 0;

    if (!hasIndexBuffer)
        return;

    VkDeviceSize bufferSize = sizeof(indices[0]) * indexCount;

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;

    // VISIBLE BIT = memory is accessible from its host CPU (this is essentially an automatic flush)
    lveDevice.createBuffer(bufferSize, 
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,  // tell vulkan that buffer is used just as source location for memory transfer 
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 
        stagingBuffer, stagingBufferMemory);

    // 0 offset, 0 flags 
    void* data;
    // This makes it so we have a ptr to vertexData on our Host CPU and we can write to it, which flushes to the Device GPU 
    vkMapMemory(lveDevice.device(), stagingBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, indices.data(), static_cast<size_t>(bufferSize));  // will copy the new color data as well
    vkUnmapMemory(lveDevice.device(), stagingBufferMemory);

    lveDevice.createBuffer(bufferSize, 
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 
        indexBuffer, indexBufferMemory);

    lveDevice.copyBuffer(stagingBuffer, indexBuffer, bufferSize);

    vkDestroyBuffer(lveDevice.device(), stagingBuffer, nullptr);
    vkFreeMemory(lveDevice.device(), stagingBufferMemory, nullptr);
}

std::vector<VkVertexInputBindingDescription> lve::LveModel::Vertex::getBindingDescriptions()
{
    std::vector<VkVertexInputBindingDescription> bindingDescriptions(1);
    bindingDescriptions[0].binding = 0;
    bindingDescriptions[0].stride = sizeof(Vertex);  // stride is adjusted to match any changes to a Vertex's description
    bindingDescriptions[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    return bindingDescriptions;
}
std::vector<VkVertexInputAttributeDescription> lve::LveModel::Vertex::getAttributeDescriptions()
{
    std::vector<VkVertexInputAttributeDescription> attributeDescriptions(2);

    attributeDescriptions[0].binding = 0;
    attributeDescriptions[0].location = 0;
    attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[0].offset = offsetof(Vertex, position);

    attributeDescriptions[1].binding = 0;
    attributeDescriptions[1].location = 1; // match location used in vertex shader
    attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[1].offset = offsetof(Vertex, color);

    return attributeDescriptions;

}

void lve::LveModel::Builder::loadModel(const std::string& filename) {

    // Reading a wavefront obj file using tinyobj
    tinyobj::attrib_t attrib;                   // position + color + normal + texture coordinate
    std::vector<tinyobj::shape_t> shapes;       // index values for each face element
    std::vector<tinyobj::material_t> materials; // ignore
    std::string warn, err;

    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, filename.c_str())) {
        throw std::runtime_error(warn + err);
    }

    vertices.clear();
    indices.clear();

    // keep track of repeat vertices + what position they were originally added
    std::unordered_map<Vertex, uint32_t> uniqueVertices{};

    for (const auto &shape : shapes)
    {
        // each face in the model, get the index values
        for (const auto& index : shape.mesh.indices)
        {
            Vertex vertex{};

            // -1 indicates that no index value was provided
            if (index.vertex_index >= 0) {
                vertex.position = { 
                    attrib.vertices[3 * index.vertex_index + 0], 
                    attrib.vertices[3 * index.vertex_index + 1], 
                    attrib.vertices[3 * index.vertex_index + 2]
                 };

                 // wavefront objs don't have color...  need an extension for it

                 // luckily we have a workaround method
                 
                 auto colorIndex = 3 * index.vertex_index + 2;

                 if (colorIndex < attrib.colors.size()) {
                    vertex.color = {
                        attrib.colors[colorIndex - 2],
                        attrib.colors[colorIndex - 1],
                        attrib.colors[colorIndex - 0]
                    };
                 } else {
                    vertex.color = {1.f,1.f,1.f};
                 }
            }

            if (index.normal_index >= 0) {
                vertex.normal = { 
                    attrib.normals[3 * index.normal_index + 0], 
                    attrib.normals[3 * index.normal_index + 1], 
                    attrib.normals[3 * index.normal_index + 2]
                 };
            }

            if (index.texcoord_index >= 0) {
                vertex.uv = { 
                    attrib.texcoords[2 * index.texcoord_index + 0], 
                    attrib.texcoords[2 * index.texcoord_index + 1]
                 };
            }

            if (uniqueVertices.count(vertex) == 0)
            {
                // vertex is new, add it to map + index for index buffer!
                uniqueVertices[vertex] = static_cast<uint32_t>(vertices.size());
                vertices.push_back(vertex);
            }

            indices.push_back(uniqueVertices[vertex]);
        }
    }
}
