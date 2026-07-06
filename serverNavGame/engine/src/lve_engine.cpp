#include "lve_engine.hpp"

namespace lve {
LveEngine::LveEngine() {
  // since the fns return a reference, we can chain initialization here
  globalPool =
      LveDescriptorPool::Builder(lveDevice)
          // we can create 2 SETS
          .setMaxSets(LveSwapChain::MAX_FRAMES_IN_FLIGHT)
          // we can create 2 UNIFORM BUFFER DESCRIPTORS to store in sets
          .addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, LveSwapChain::MAX_FRAMES_IN_FLIGHT)
          .build();

  // this should create 2 instances, so for each frame, we can use the one thats not being rendered
  globalUboBuffer = std::make_unique<LveBuffer>(
      lveDevice,
      sizeof(GlobalUbo),
      globalUniformBufferSize,  // 2 - how many frames can be submit for rendering simultaneously
      VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,  // host coherent is disabled for SELECTIVE FLUSHING
      lveDevice.properties.limits.minUniformBufferOffsetAlignment);

  globalUboBuffer->map();
}

LveEngine::~LveEngine() {
  cleanup();
}

void LveEngine::init() {
  auto globalSetLayout =
      LveDescriptorSetLayout::Builder(lveDevice)
          .addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_ALL_GRAPHICS)
          // .addBinding(1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT)
          .build();

  // this is currently taking 2 uniform buffers, I only have one...
  globalDescriptorSets.reserve(globalUniformBufferSize);
  for (int i = 0; i < globalUniformBufferSize; i++) {
    auto bufferInfo = globalUboBuffer->descriptorInfoForIndex(i);

    // descriptor writer class handles moving uniform buffer info INTO the descriptor set
    LveDescriptorWriter(*globalSetLayout, *globalPool)
        .writeBuffer(0, &bufferInfo)
        .build(globalDescriptorSets[i]);
  }

  simpleRenderSystem = std::make_unique<SimpleRenderSystem>(
      lveDevice,
      lveRenderer.getSwapChainRenderPass(),
      globalSetLayout->getDescriptorSetLayout());

  pointLightSystem = std::make_unique<PointLightSystem>(
      lveDevice,
      lveRenderer.getSwapChainRenderPass(),
      globalSetLayout->getDescriptorSetLayout());
}

void LveEngine::render(LveScene& scene) {
  // Returns null if swap chain needs to be recreated!
  if (!running) return;
  if (auto commandBuffer = lveRenderer.beginFrame()) {
    int frameIndex = lveRenderer.getFrameIndex();
    FrameInfo frameInfo{
        frameIndex,
        commandBuffer,
        globalDescriptorSets[frameIndex],
        scene.renderItems,
        scene.UIrenderItems,
        scene.lightItems};

    // update
    pointLightSystem->update(frameInfo, scene.ubo);
    globalUboBuffer->writeToIndex(&scene.ubo, frameIndex);
    globalUboBuffer->flushIndex(frameIndex);

    // render
    // Being able to control when the render pass begins and ends is helpful for post processing
    // effects
    lveRenderer.beginSwapChainRenderPass(commandBuffer);

    simpleRenderSystem->render(frameInfo);
    pointLightSystem->render(frameInfo);
    // simpleRenderSystem->renderUI(frameInfo);

    lveRenderer.endSwapChainRenderPass(commandBuffer);
    lveRenderer.endFrame();
  }
}

void LveEngine::cleanup() 
{
  /*
  Destroy pipelines
  Destroy descriptor sets/pools
  Destroy framebuffers
  Destroy render passes
  Destroy images + image views
  Destroy buffers + memory
  Destroy command buffers
  Destroy command pools
  Destroy swapchain
  Destroy semaphores + fences
  Destroy device
  Destroy instance
  */
  vkDeviceWaitIdle(lveDevice.device());
  
  // if (simpleRenderSystem != nullptr) {
  //   delete simpleRenderSystem;
  //   simpleRenderSystem = nullptr;
  // }
  // if (pointLightSystem != nullptr) {
  //   delete pointLightSystem;
  //   pointLightSystem = nullptr;
  // }
  
  // if (globalUboBuffer != nullptr) {
  //   delete globalUboBuffer;
  //   globalUboBuffer = nullptr;
  // }
}

float LveEngine::getAspectRatio() const { return 0.0f; }

VkRenderPass LveEngine::getRenderPass() const { return VkRenderPass(); }

GLFWwindow* LveEngine::getGLFWWindow() { return lveWindow.getGLFWWindow(); }

float LveEngine::getAspectRatio() { return lveRenderer.getAspectRatio(); }

bool LveEngine::shouldClose() 
{
  if (lveWindow.shouldClose())
  {
    running = false; // tells engine to finish GPU work
    // calls vkDeviceWaitIdle()
    cleanup();       // destroys Vulkan objects
  } 
  return lveWindow.shouldClose();
}
}  // namespace lve
