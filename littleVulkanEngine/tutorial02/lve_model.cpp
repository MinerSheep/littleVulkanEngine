#include "lve_model.hpp"
#include <cstring>

lve::LveModel::LveModel(LveDevice& device, const std::vector<Vertex>& vertices) {
    createVertexBuffers(vertices);
}

lve::LveModel::~LveModel() {
    vkDestroyBuffer(lveDevice.device(), vertexBuffer, nullptr);
    vkFreeMemory(lveDevice.device(), vertexBufferMemory, nullptr);
}

void lve::LveModel::bind(VkCommandBuffer commandBuffer) {
    // This is responsible for putting multiple shader variable offsets into a single vertex buffers I think
    vkCmdDraw(commandBuffer, vertexCount, 1, 0, 0);
}

void lve::LveModel::draw(VkCommandBuffer commandBuffer) {
    VkBuffer buffers[] = {vertexBuffer};
    VkDeviceSize offsets[] = {0};

    // Bind one vertex buffer starting at binding 0  with a {0} offset
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, buffers, offsets);
}

void lve::LveModel::createVertexBuffers(const std::vector<Vertex>& vertices) {
    vertexCount = static_cast<uint32_t>(vertices.size());
    assert(vertexCount >= 3 && "Vertex count must be at least 3");

    VkDeviceSize bufferSize = sizeof(vertices[0]) * vertexCount;

    // VISIBLE BIT = memory is accessible from its host CPU (this is essentially an automatic flush)
    lveDevice.createBuffer(bufferSize, 
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 
        vertexBuffer, vertexBufferMemory);

    // 0 offset, 0 flags 
    void* data;
    // This makes it so we have a ptr to vertexData on our Host CPU and we can write to it, which flushes to the Device GPU 
    vkMapMemory(lveDevice.device(), vertexBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, vertices.data(), static_cast<size_t>(bufferSize));
    vkUnmapMemory(lveDevice.device(), vertexBufferMemory);
}
