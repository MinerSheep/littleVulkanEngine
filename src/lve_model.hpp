#pragma once

#include "lve_device.hpp"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE // necessary for vulkan, glfw uses -1 to 1 depth by default
#include <glm/glm.hpp>

#include <vector>

namespace lve
{
    // take vertex data created or read by a file
    // allocate the memory and copy data over to device for rendering
    class LveModel {
    public:

        struct Vertex {
            glm::vec3 position;
            glm::vec3 color;

            static std::vector<VkVertexInputBindingDescription> getBindingDescriptions();
            static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions();
        };
        LveModel(LveDevice& device, const std::vector<Vertex>& vertices);
        ~LveModel();

        LveModel(LveModel const&) = delete;
        LveModel &operator=(LveModel const&) = delete;

        void bind(VkCommandBuffer commandBuffer);
        void draw(VkCommandBuffer commandBuffer);

    private:
        void createVertexBuffers(const std::vector<Vertex> & vertices);

        LveDevice& lveDevice;
        VkBuffer vertexBuffer;             // vertexBuffer is separate from its memory
        VkDeviceMemory vertexBufferMemory;
        uint32_t vertexCount; 
    };
} // namespace lve
