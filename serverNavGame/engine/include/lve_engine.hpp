
#pragma once

#include "lve_window.hpp"
#include "lve_device.hpp"
#include "lve_scene.hpp"
#include "lve_buffer.hpp"
#include "lve_renderer.hpp"
#include "lve_frame_info.hpp"
#include "systems/simple_render_system.hpp"
#include "systems/point_light_system.hpp"
#include "systems/skinned_render_system.hpp"
#include <glm/gtc/matrix_transform.hpp>


#include "lve_descriptors.hpp"
#include <vector>


namespace lve
{
    class LveEngine {
        static constexpr int WIDTH = 800;
        static constexpr int HEIGHT = 600;

    public:
        static LveEngine& instance() {
            static LveEngine inst;
            return inst;
        }

        LveEngine();
        ~LveEngine();


        void init();
        void update();
        void render();
        void cleanup();
        VkRenderPass getRenderPass() const;
        
        GLFWwindow* getGLFWWindow();
        float getAspectRatio();
        bool shouldClose();
        
        // Self managed variable, optional to set or not
        LveScene* activeScene;

        LveWindow& getWindow() { return lveWindow; }
        LveDevice& getDevice() { return lveDevice; }
        LveRenderer& getRenderer() { return lveRenderer; }

        // Shared by every LveSkinnedModel: they allocate their per-model bone
        // descriptor set (set = 1) from this pool/layout in createModelFromFile.
        LveDescriptorSetLayout& getBoneSetLayout() { return *boneSetLayout; }
        LveDescriptorPool& getBonePool() { return *bonePool; }

        // Every LveTexture takes one set (set = 1) out of these, the same way a
        // skinned model takes a bone set
        LveDescriptorSetLayout& getTextureSetLayout() { return *textureSetLayout; }
        LveDescriptorPool& getTexturePool() { return *texturePool; }

    private:
        bool running = true;

        // Max distinct skinned models that can own a bone descriptor set at once.
        static constexpr uint32_t MAX_SKINNED_MODELS = 16;

        // How many pictures may be loaded at once, one set each
        static constexpr uint32_t MAX_TEXTURES = 16;

        const uint32_t globalUniformBufferSize = LveSwapChain::MAX_FRAMES_IN_FLIGHT;
        
        // ORDER MATTERS!
        // Initialize from top to bottom, DESTROY from bottom to top
        LveWindow lveWindow{WIDTH, HEIGHT, "Hello Vulkan!"};
        LveDevice lveDevice{lveWindow};
        LveRenderer lveRenderer{lveWindow, lveDevice};
        
        std::unique_ptr<LveDescriptorPool> globalPool{};
        std::vector<VkDescriptorSet> globalDescriptorSets;

        // Bone-matrix (set = 1) infrastructure for skinned models. These must
        // outlive init() because skinned models allocate from them later, during
        // the scene's loadModels().
        std::unique_ptr<LveDescriptorSetLayout> boneSetLayout{};
        std::unique_ptr<LveDescriptorPool> bonePool{};

        // Where a texture's sampler binding is allocated from, and it must outlive
        // every LveTexture the scenes load
        std::unique_ptr<LveDescriptorSetLayout> textureSetLayout{};
        std::unique_ptr<LveDescriptorPool> texturePool{};

        // std::unique_ptr<LveBuffer> globalUboBuffer {};
        std::vector<std::unique_ptr<LveBuffer>> uboBuffers;
        std::unique_ptr<SimpleRenderSystem> simpleRenderSystem {};
        std::unique_ptr<PointLightSystem> pointLightSystem {};
        std::unique_ptr<SkinnedRenderSystem> skinnedRenderSystem {};

    };
} // namespace lve