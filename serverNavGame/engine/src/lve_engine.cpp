#include "lve_engine.hpp"

#include <chrono>

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

  // std::vector<std::unique_ptr<LveBuffer>> uboBuffers(LveSwapChain::MAX_FRAMES_IN_FLIGHT);
  uboBuffers.reserve(globalUniformBufferSize);
  for (int i = 0; i < globalUniformBufferSize; i++)
  {
    uboBuffers.push_back(std::make_unique<LveBuffer>(
      lveDevice,
      sizeof(GlobalUbo),
      1,  // 1 - how many frames can be submit for rendering simultaneously
      VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,  // host coherent is disabled for SELECTIVE FLUSHING
      lveDevice.properties.limits.minUniformBufferOffsetAlignment));
    uboBuffers[i]->map();
  }

  // this should create 2 instances, so for each frame, we can use the one thats not being rendered
  // globalUboBuffer = std::make_unique<LveBuffer>(
  //     lveDevice,
  //     sizeof(GlobalUbo),
  //     globalUniformBufferSize,  // 2 - how many frames can be submit for rendering simultaneously
  //     VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
  //     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,  // host coherent is disabled for SELECTIVE FLUSHING
  //     lveDevice.properties.limits.minUniformBufferOffsetAlignment);

  // globalUboBuffer->map();
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

  // resize, not reserve: the loop below writes globalDescriptorSets[i], and reserve
  // leaves size() at 0, so every one of those writes would be out of bounds
  globalDescriptorSets.resize(globalUniformBufferSize);
  for (int i = 0; i < globalUniformBufferSize; i++) {
    auto bufferInfo = uboBuffers[i]->descriptorInfo();

    // descriptor writer class handles moving uniform buffer info INTO the descriptor set
    LveDescriptorWriter(*globalSetLayout, *globalPool)
        .writeBuffer(0, &bufferInfo)
        .build(globalDescriptorSets[i]);
  }

  // --- Skinned model (set = 1) bone-matrix infrastructure --------------------
  // A single storage-buffer binding holding a model's joint matrix palette. One
  // descriptor set is allocated per skinned model (up to MAX_SKINNED_MODELS)
  boneSetLayout =
      LveDescriptorSetLayout::Builder(lveDevice)
          .addBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT)
          .build();

  // Each skinned model allocates one bone set per frame-in-flight
  // uploadPose in LveSkinnedModel is how it gets applied
  const uint32_t maxBoneSets = MAX_SKINNED_MODELS * LveSwapChain::MAX_FRAMES_IN_FLIGHT;
  bonePool =
      LveDescriptorPool::Builder(lveDevice)
          .setMaxSets(maxBoneSets)
          .addPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, maxBoneSets)
          .build();

  // --- Texture (set = 1) infrastructure -------------------------------------
  // One combined image sampler per picture, which the floor pipeline binds
  textureSetLayout =
      LveDescriptorSetLayout::Builder(lveDevice)
          .addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
          .build();

  texturePool =
      LveDescriptorPool::Builder(lveDevice)
          .setMaxSets(MAX_TEXTURES)
          .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, MAX_TEXTURES)
          .build();

  shots = std::make_unique<LveScreenshot>(lveDevice);
  post = std::make_unique<LvePost>(lveDevice, *textureSetLayout, *texturePool);

  simpleRenderSystem = std::make_unique<SimpleRenderSystem>(
      lveDevice,
      lveRenderer.getSwapChainRenderPass(),
      globalSetLayout->getDescriptorSetLayout(),
      textureSetLayout->getDescriptorSetLayout());

  pointLightSystem = std::make_unique<PointLightSystem>(
      lveDevice,
      lveRenderer.getSwapChainRenderPass(),
      globalSetLayout->getDescriptorSetLayout());

  skinnedRenderSystem = std::make_unique<SkinnedRenderSystem>(
      lveDevice,
      lveRenderer.getSwapChainRenderPass(),
      globalSetLayout->getDescriptorSetLayout(),
      boneSetLayout->getDescriptorSetLayout());
}

void LveEngine::render() {
  // Returns null if swap chain needs to be recreated!
  if (!running) return;
  if (activeScene == nullptr) return;

  LveScene& scene = *activeScene;

  // X04: the picture is rebuilt whenever the window is a different size
  if (post) {
    const VkExtent2D want = lveRenderer.getExtent();
    if (!post->ready() || post->size().width != want.width || post->size().height != want.height) {
      vkDeviceWaitIdle(lveDevice.device());
      post->rebuild(want, lveRenderer.getImageFormat(), lveRenderer.getDepthFormat(),
                    lveRenderer.getSwapChainRenderPass());
    }
  }

  if (auto commandBuffer = lveRenderer.beginFrame()) {
    int frameIndex = lveRenderer.getFrameIndex();
    FrameInfo frameInfo{
        frameIndex,
        commandBuffer,
        globalDescriptorSets[frameIndex],
        scene.renderItems,
        scene.UIrenderItems,
        scene.lightItems,
        scene.skinnedRenderItems,
        scene.backgroundItems};

    // update
    pointLightSystem->update(frameInfo, scene.ubo);
    uboBuffers[frameIndex]->writeToBuffer(&scene.ubo);
    uboBuffers[frameIndex]->flush();

    // Everything the room is, in the order it goes down
    auto drawEverything = [&]() {
      // Background first
      simpleRenderSystem->renderUI(frameInfo, frameInfo.backgroundItems);

      simpleRenderSystem->render(frameInfo);
      skinnedRenderSystem->render(frameInfo);
      pointLightSystem->render(frameInfo);

      // UI overlay
      simpleRenderSystem->renderUI(frameInfo, frameInfo.UIrenderItems);
    };

    // X04: the room is drawn into a picture, and the picture is put on the
    // screen through one shader. A held picture is simply not drawn again
    if (post && post->ready()) {
      // Wall clock, so the wobble crawls at the same speed however the frames go
      static const auto begun = std::chrono::steady_clock::now();
      postClock = std::chrono::duration<float>(std::chrono::steady_clock::now() - begun).count();

      if (!scene.post.hold) {
        post->beginPass(commandBuffer);
        drawEverything();
        post->endPass(commandBuffer);
      }

      lveRenderer.beginSwapChainRenderPass(commandBuffer);
      post->drawTo(commandBuffer, scene.post, postClock);
      lveRenderer.endSwapChainRenderPass(commandBuffer);
    } else {
      lveRenderer.beginSwapChainRenderPass(commandBuffer);
      drawEverything();
      lveRenderer.endSwapChainRenderPass(commandBuffer);
    }

    // X12: the finished frame is copied aside before it goes to the screen
    const bool kept = shots && shots->waiting() && lveRenderer.canCopyFrames();
    if (kept)
      shots->record(commandBuffer, lveRenderer.getCurrentImage(), lveRenderer.getImageFormat(),
                    lveRenderer.getExtent());

    lveRenderer.endFrame();

    // The copy is read on the CPU, so it has to have actually happened first
    if (kept) {
      vkDeviceWaitIdle(lveDevice.device());
      shots->arrived();
    }
  }
}

void LveEngine::grabFrame(int width, int height) {
  if (!shots || !lveRenderer.canCopyFrames()) return;
  shots->want(width, height);
}

bool LveEngine::grabbing() const { return shots && shots->waiting(); }

bool LveEngine::takeFrame(LveCanvas& out) { return shots && shots->take(out); }

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
