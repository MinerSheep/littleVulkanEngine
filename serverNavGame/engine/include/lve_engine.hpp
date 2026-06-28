
#pragma once

#include "lve_window.hpp"
#include "lve_device.hpp"
#include "lve_scene.hpp"
#include "lve_buffer.hpp"
#include "lve_renderer.hpp"
#include "lve_frame_info.hpp"
#include "systems/simple_render_system.hpp"
#include "systems/point_light_system.hpp"
#include <glm/gtc/matrix_transform.hpp>


#include "lve_descriptors.hpp"
#include <vector>


namespace lve
{
    
    class LveEngine {
        static constexpr int WIDTH = 800;
        static constexpr int HEIGHT = 600;

    public:
        static LveEngine* instance;

        LveEngine();
        ~LveEngine();


        void init();
        void update();
        void render(LveScene& scene);
        float getAspectRatio() const;
        VkRenderPass getRenderPass() const;

        GLFWwindow* getGLFWWindow();
        float getAspectRatio();
        bool shouldClose();

        LveWindow& getWindow() { return lveWindow; }
        LveDevice& getDevice() { return lveDevice; }
        LveRenderer& getRenderer() { return lveRenderer; }
    
    private:
        const int globalUniformBufferSize = LveSwapChain::MAX_FRAMES_IN_FLIGHT;
        std::vector<VkDescriptorSet> globalDescriptorSets;
        
        LveBuffer* globalUboBuffer = nullptr;
        SimpleRenderSystem* simpleRenderSystem = nullptr;
        PointLightSystem* pointLightSystem = nullptr;

        // ORDER MATTERS!
        // Initialize from top to bottom, DESTROY from bottom to top
        LveWindow lveWindow{WIDTH, HEIGHT, "Hello Vulkan!"};
        LveDevice lveDevice{lveWindow};
        LveRenderer lveRenderer{lveWindow, lveDevice};

        std::unique_ptr<LveDescriptorPool> globalPool{};
    };
} // namespace lve