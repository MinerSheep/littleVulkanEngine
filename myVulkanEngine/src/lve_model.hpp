#pragma once

#include "lve_device.hpp"
#include "lve_buffer.hpp"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE // necessary for vulkan, glfw uses -1 to 1 depth by default
#include <glm/glm.hpp>

//std
#include <memory>
#include <vector>

namespace lve
{
    // take vertex data created or read by a file
    // allocate the memory and copy data over to device for rendering
    class LveModel {
    public:

        struct Vertex {
            glm::vec3 position{};
            glm::vec3 color{};
            glm::vec3 normal{};
            glm::vec2 uv{};

            static std::vector<VkVertexInputBindingDescription> getBindingDescriptions();
            static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions();

            bool operator==(const Vertex& other) const {
                return position == other.position && color == other.color && normal == other.normal && uv == other.uv;
            }
        };

        struct Builder {
            std::vector<Vertex> vertices{};
            std::vector<uint32_t> indices{};

            void loadModel(const std::string & filename);
        };

        LveModel(LveDevice& device, const LveModel::Builder & builder);
        ~LveModel();

        LveModel(LveModel const&) = delete;
        LveModel &operator=(LveModel const&) = delete;

        static std::unique_ptr<LveModel> createModelFromFile(LveDevice& device, const std::string& filepath);

        void bind(VkCommandBuffer commandBuffer);
        void draw(VkCommandBuffer commandBuffer);

    private:
        void createVertexBuffers(const std::vector<Vertex> & vertices);
        void createIndexBuffer(const std::vector<uint32_t> &indices);

        LveDevice& lveDevice;

        std::unique_ptr<LveBuffer> vertexBuffer;
        uint32_t vertexCount; 

        bool hasIndexBuffer = false;
        std::unique_ptr<LveBuffer> indexBuffer;
        uint32_t indexCount; 
    };
} // namespace lve
