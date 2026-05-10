#include "first_app.hpp"

#include <stdexcept>

namespace lve {
FirstApp::FirstApp() {
  loadModels();
  createPipelineLayout();
  recreateSwapChain();
  createCommandBuffers();
}

FirstApp::~FirstApp() {
  // Dont forget to destroy the pipeline layout in destructor, all vk objects needs to do this
  vkDestroyPipelineLayout(lveDevice.device(), pipelineLayout, nullptr);
}

void FirstApp::run() {
  while (!lveWindow.shouldClose()) {
    // this causes glitchiness on ubuntu because it blocks
    glfwPollEvents();
    drawFrame();
  }

  // If you get many messages on close, it's likely because a command buffer was still being
  // executed This is the fix
  vkDeviceWaitIdle(lveDevice.device());
}
void sierpinski(
    std::vector<LveModel::Vertex>& vertices,
    int depth,
    glm::vec2 left,
    glm::vec2 right,
    glm::vec2 top) {
  if (depth <= 0) {
    vertices.push_back({top, {1.0f, 0.0f, 0.0f}});
    vertices.push_back({right, {1.0f, 0.0f, 0.0f}});
    vertices.push_back({left, {1.0f, 0.0f, 0.0f}});
  } else {
    auto leftTop = 0.5f * (left + top);
    auto rightTop = 0.5f * (right + top);
    auto leftRight = 0.5f * (left + right);
    sierpinski(vertices, depth - 1, left, leftRight, leftTop);
    sierpinski(vertices, depth - 1, leftRight, right, rightTop);
    sierpinski(vertices, depth - 1, leftTop, rightTop, top);
  }
}
void FirstApp::loadModels() {
  std::vector<LveModel::Vertex> vertices{
      {{0.0f, -0.5f}, {1.0f, 0.0f, 0.0f}},
      {{0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}},
      {{-0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}}};

  // sierpinski(vertices, 5, {-0.5f, 0.5f}, {0.5f, 0.5f}, {0.0f, -0.5f});

  lveModel = std::make_unique<LveModel>(lveDevice, vertices);
}
void FirstApp::createPipelineLayout() {
  VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
  pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

  // Layouts are responsible for putting textures in
  pipelineLayoutInfo.setLayoutCount = 0;
  pipelineLayoutInfo.pSetLayouts = nullptr;
  // Sends small data to shader programs
  pipelineLayoutInfo.pushConstantRangeCount = 0;
  pipelineLayoutInfo.pPushConstantRanges = nullptr;

  if (vkCreatePipelineLayout(lveDevice.device(), &pipelineLayoutInfo, nullptr, &pipelineLayout) !=
      VK_SUCCESS) {
    throw std::runtime_error("Failed to create pipeline layout");
  }
}
void FirstApp::createPipeline() {
  auto pipelineConfig =
      LvePipeline::defaultPipelineConfigInfo(lveSwapChain->width(), lveSwapChain->height());
  pipelineConfig.renderPass = lveSwapChain->getRenderPass();  // render pass describes structure and
                                                              // format of our frame buffer objects
  pipelineConfig.pipelineLayout = pipelineLayout;
  lvePipeline = std::make_unique<LvePipeline>(
      lveDevice,
      "shaders/simple_shader.vert.spv",
      "shaders/simple_shader.frag.spv",
      pipelineConfig);
}

void FirstApp::createCommandBuffers() {
  // It is simplest to give each frameBuffer its own commandBuffer to record with, since it needs to
  // be done every frame
  commandBuffers.resize(lveSwapChain->imageCount());

  VkCommandBufferAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;

  allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;  // primary can submit to a queue for
                                                      // execution, CANNOT be called by others
  // secondary CANNOT be submitted to queue, so must be called by a PRIMARY command buffer

  allocInfo.commandPool =
      lveDevice
          .getCommandPool();  // commandBuffers need a pool so that memory allocation is done ONCE
  allocInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers.size());

  if (vkAllocateCommandBuffers(lveDevice.device(), &allocInfo, commandBuffers.data()) !=
      VK_SUCCESS) {
    throw std::runtime_error("Failed to allocate command buffers");
  }
}
void FirstApp::drawFrame() {
  uint32_t imageIndex;
  // Fetch the next index in renderFrame
  auto result = lveSwapChain->acquireNextImage(&imageIndex);

  // We need to check if this next frame is valid  (hasn't been RESIZED)
  if (result == VK_ERROR_OUT_OF_DATE_KHR) {
    recreateSwapChain();
    return;
  }

  if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
    throw std::runtime_error("Failed to acquire swapChain image!");

  recordCommandBuffer(imageIndex);
  result = lveSwapChain->submitCommandBuffers(&commandBuffers[imageIndex], &imageIndex);
  // suboptimal means that it can still present but it is out of date
  if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR ||
      lveWindow.wasWindowResized()) {
    lveWindow.resetWindowResizedFlag();
    recreateSwapChain();
    return;
  }

  if (result != VK_SUCCESS) throw std::runtime_error("Failed to draw frame!");
}
void FirstApp::recreateSwapChain() {
  auto extent = lveWindow.getExtent();

  // Minimization check, if the window has no size, wait for it to have size
  while (extent.width == 0 || extent.height == 0) {
    extent = lveWindow.getExtent();
    glfwWaitEvents();
  }

  vkDeviceWaitIdle(lveDevice.device());
  lveSwapChain = std::make_unique<LveSwapChain>(lveDevice, extent);
  createPipeline();
}
void FirstApp::recordCommandBuffer(int imageIndex) {
  // Record our draw commands to each buffer
  VkCommandBufferBeginInfo beginInfo{};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

  if (vkBeginCommandBuffer(commandBuffers[imageIndex], &beginInfo) != VK_SUCCESS)
    throw std::runtime_error("Failed to begin recording command buffer");

  VkRenderPassBeginInfo renderPassInfo{};
  renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  renderPassInfo.renderPass = lveSwapChain->getRenderPass();
  // which frameBuffer is this renderPass writing?
  renderPassInfo.framebuffer = lveSwapChain->getFrameBuffer(imageIndex);

  // Shader loads and stores will take place in renderArea
  renderPassInfo.renderArea.offset = {0, 0};
  renderPassInfo.renderArea.extent = lveSwapChain->getSwapChainExtent();

  // Clear values - what we want initial value of A FRAME BUFFER to be after it is cleared
  std::array<VkClearValue, 2> clearValues{};
  clearValues[0].color = {0.1f, 0.1f, 0.1f, 1.0f};  // first element
  clearValues[1].depthStencil = {1.0f, 0};          // 2nd element
  renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
  renderPassInfo.pClearValues = clearValues.data();

  // Inline here means that renderPass will be embedded in the commandBuffers
  vkCmdBeginRenderPass(commandBuffers[imageIndex], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

  lvePipeline->bind(commandBuffers[imageIndex]);
  // 3 vertices, 1 instance, firstIndex, firstInstance
  // vkCmdDraw(commandBuffers[imageIndex], 3, 1, 0, 0);
  lveModel->bind(commandBuffers[imageIndex]);
  lveModel->draw(commandBuffers[imageIndex]);

  // end recording
  vkCmdEndRenderPass(commandBuffers[imageIndex]);
  if (vkEndCommandBuffer(commandBuffers[imageIndex]) != VK_SUCCESS)
    throw std::runtime_error("Failed to record command buffer");
}
}  // namespace lve