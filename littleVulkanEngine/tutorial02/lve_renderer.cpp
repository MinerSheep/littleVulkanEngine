#include "lve_renderer.hpp"

// std
#include <array>
#include <cassert>
#include <stdexcept>

namespace lve {

LveRenderer::LveRenderer(LveWindow& window, LveDevice& device)
    : lveWindow{window}, lveDevice{device} {
  recreateSwapChain();
  createCommandBuffers();
}

// We free command buffers here because application can continue w/o the renderer
LveRenderer::~LveRenderer() { freeCommandBuffers(); }

void LveRenderer::recreateSwapChain() {
  auto extent = lveWindow.getExtent();

  // Minimization check, if the window has no size, wait for it to have size
  while (extent.width == 0 || extent.height == 0) {
    extent = lveWindow.getExtent();
    glfwWaitEvents();
  }
  vkDeviceWaitIdle(lveDevice.device());

  // std::move sets old value to nullptr to prevent memory issues since we no longer need it
  if (lveSwapChain == nullptr) {
    lveSwapChain = std::make_unique<LveSwapChain>(lveDevice, extent);
  } else {
    std::shared_ptr<LveSwapChain> oldSwapChain = std::move(lveSwapChain);
    
    lveSwapChain = std::make_unique<LveSwapChain>(lveDevice, extent, oldSwapChain);

    if (!oldSwapChain->compareSwapFormats(*lveSwapChain.get())) {
      // Might be better to throw a callback error message here instead
      throw std::runtime_error("Swap chain image(or depth) format has changed!");
    }
  }
}

void LveRenderer::createCommandBuffers() {
  // It is simplest to give each frameBuffer its own commandBuffer to record with, since it needs to
  // be done every frame
  commandBuffers.resize(LveSwapChain::MAX_FRAMES_IN_FLIGHT);

  VkCommandBufferAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;

  allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;  // primary can submit to a queue for
                                                      // execution, CANNOT be called by others
  // secondary CANNOT be submitted to queue, so must be called by a PRIMARY command buffer

  allocInfo.commandPool = lveDevice.getCommandPool(); // commandBuffers need a pool so that memory allocation is done ONCE
  allocInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers.size());

  if (vkAllocateCommandBuffers(lveDevice.device(), &allocInfo, commandBuffers.data()) !=
      VK_SUCCESS) {
    throw std::runtime_error("failed to allocate command buffers!");
  }
}

void LveRenderer::freeCommandBuffers() {
  vkFreeCommandBuffers(
      lveDevice.device(),
      lveDevice.getCommandPool(),
      static_cast<uint32_t>(commandBuffers.size()),
      commandBuffers.data());
  commandBuffers.clear();
}

VkCommandBuffer LveRenderer::beginFrame() {
  assert(!isFrameStarted && "Can't call beginFrame while already in progress");

  auto result = lveSwapChain->acquireNextImage(&currentImageIndex);

  if (result == VK_ERROR_OUT_OF_DATE_KHR) {
    recreateSwapChain();
    return nullptr;
  }

  if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
    throw std::runtime_error("failed to acquire swap chain image!");
  }

  isFrameStarted = true;

  // Record our draw commands to each buffer
  auto commandBuffer = getCurrentCommandBuffer();
  VkCommandBufferBeginInfo beginInfo{};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

  if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
    throw std::runtime_error("failed to begin recording command buffer!");
  }
  return commandBuffer;
}

void LveRenderer::endFrame() {
  assert(isFrameStarted && "Can't call endFrame while frame is not in progress");

  auto commandBuffer = getCurrentCommandBuffer();
  if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
    throw std::runtime_error("failed to record command buffer!");
  }

  // DRAW FRAME implementation
  auto result = lveSwapChain->submitCommandBuffers(&commandBuffer, &currentImageIndex);
  // We need to check if this next frame is valid  (hasn't been RESIZED)
  // suboptimal means that it can still present but it is out of date
  if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR ||
      lveWindow.wasWindowResized()) {
    lveWindow.resetWindowResizedFlag();
    recreateSwapChain();
  } else if (result != VK_SUCCESS) {
    throw std::runtime_error("failed to present swap chain image!");
  }

  isFrameStarted = false;
  currentFrameIndex = (currentFrameIndex + 1) % LveSwapChain::MAX_FRAMES_IN_FLIGHT;
}

void LveRenderer::beginSwapChainRenderPass(VkCommandBuffer commandBuffer) {
  assert(isFrameStarted && "Can't call beginSwapChainRenderPass if frame is not in progress");
  assert(commandBuffer == getCurrentCommandBuffer() &&
      "Can't begin render pass on command buffer from a different frame");

  VkRenderPassBeginInfo renderPassInfo{};
  renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  renderPassInfo.renderPass = lveSwapChain->getRenderPass();
  // which frameBuffer is this renderPass writing?
  renderPassInfo.framebuffer = lveSwapChain->getFrameBuffer(currentImageIndex);

  // Shader loads and stores will take place in renderArea
  renderPassInfo.renderArea.offset = {0, 0};
  renderPassInfo.renderArea.extent = lveSwapChain->getSwapChainExtent();

  // Clear values - what we want initial value of A FRAME BUFFER to be after it is cleared
  std::array<VkClearValue, 2> clearValues{};
  clearValues[0].color = {0.01f, 0.01f, 0.01f, 1.0f};
  clearValues[1].depthStencil = {1.0f, 0};
  renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
  renderPassInfo.pClearValues = clearValues.data();

  // Inline here means that renderPass will be embedded in the commandBuffers
  vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

  // Dynamically recompute viewport BEFORE submitting the command buffer
  VkViewport viewport{};
  viewport.x = 0.0f;
  viewport.y = 0.0f;
  viewport.width = static_cast<float>(lveSwapChain->getSwapChainExtent().width);
  viewport.height = static_cast<float>(lveSwapChain->getSwapChainExtent().height);
  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;
  VkRect2D scissor{{0, 0}, lveSwapChain->getSwapChainExtent()};
  vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
  vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
}

void LveRenderer::endSwapChainRenderPass(VkCommandBuffer commandBuffer) {
  assert(isFrameStarted && "Can't call endSwapChainRenderPass if frame is not in progress");
  assert(commandBuffer == getCurrentCommandBuffer() &&
      "Can't end render pass on command buffer from a different frame");

  // end recording
  vkCmdEndRenderPass(commandBuffer);
}

}  // namespace lve