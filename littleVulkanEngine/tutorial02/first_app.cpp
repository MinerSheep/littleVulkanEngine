#include "first_app.hpp"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE // necessary for vulkan, glfw uses -1 to 1 depth by default
#include <glm/glm.hpp>

#include <stdexcept>

namespace lve {

// alignas is NECESSARY here!
// Push constants have alignment requirements.  Every entry is aligned to 16 bytes
// / marks 16 bytes
// Incorrect: x y r g / b - - -      Correct: x y - - / r g b - 
struct SimplePushConstantData {
  glm::mat2 transform{1.f};  // IDENTITY matrix
  glm::vec2 offset; // 8 bytes - divisible by 4, fine!
  alignas(16) glm::vec3 color;  // bad because 12 bytes upscales to 16 bytes
};

FirstApp::FirstApp() {
  loadGameObjects();
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
void FirstApp::loadGameObjects() {
  std::vector<LveModel::Vertex> vertices{
      {{0.0f, -0.5f}, {1.0f, 0.0f, 0.0f}},
      {{0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}},
      {{-0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}}};

  // sierpinski(vertices, 5, {-0.5f, 0.5f}, {0.5f, 0.5f}, {0.0f, -0.5f});

  auto lveModel = std::make_shared<LveModel>(lveDevice, vertices);

  auto triangle = LveGameObject::createGameObject();
  triangle.model = lveModel;
  triangle.color = {.1f,.8f,.1f};
  triangle.transform2d.translation.x = .2f;

  gameObjects.push_back(std::move(triangle));
}
void FirstApp::createPipelineLayout() {

  // This is us PREDEFINING our **push constant range**
  // stageFlags set to both shaders
  // size is set to a PREDEFINED struct of what we want our pushdata to contain
  VkPushConstantRange pushConstantRange{};
  pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
  pushConstantRange.offset = 0; // (offset only used if you separate vertex and frag data)
  pushConstantRange.size = sizeof(SimplePushConstantData);

  VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
  pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

  // Layouts are responsible for putting textures in
  pipelineLayoutInfo.setLayoutCount = 0;
  pipelineLayoutInfo.pSetLayouts = nullptr;
  // Sends small data to shader programs
  pipelineLayoutInfo.pushConstantRangeCount = 1;
  pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

  if (vkCreatePipelineLayout(lveDevice.device(), &pipelineLayoutInfo, nullptr, &pipelineLayout) !=
      VK_SUCCESS) {
    throw std::runtime_error("Failed to create pipeline layout");
  }
}
void FirstApp::createPipeline() {
  assert(lveSwapChain != nullptr && "Cannot create pipeline before swap chain");
  assert(pipelineLayout != nullptr && "Cannot create pipeline before pipeline layout");

  PipelineConfigInfo pipelineConfig{};
  LvePipeline::defaultPipelineConfigInfo(pipelineConfig);
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

  allocInfo.commandPool = lveDevice.getCommandPool();  // commandBuffers need a pool so that memory allocation is done ONCE
  allocInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers.size());

  if (vkAllocateCommandBuffers(lveDevice.device(), &allocInfo, commandBuffers.data()) !=
      VK_SUCCESS) {
    throw std::runtime_error("Failed to allocate command buffers");
  }
}
void FirstApp::freeCommandBuffers() {
  vkFreeCommandBuffers(lveDevice.device(), lveDevice.getCommandPool(), static_cast<float>(commandBuffers.size()), commandBuffers.data());
  commandBuffers.clear();
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

  // std::move sets old value to nullptr to prevent memory issues since we no longer need it
  if (lveSwapChain == nullptr)
    lveSwapChain = std::make_unique<LveSwapChain>(lveDevice, extent);
  else
  {
    lveSwapChain = std::make_unique<LveSwapChain>(lveDevice, extent, std::move(lveSwapChain));
    if (lveSwapChain->imageCount() != commandBuffers.size())
    {
      freeCommandBuffers();
      createCommandBuffers();
    }
  }

  // Good idea to check render pass compatibility here.  If its compatible then pipeline doesnt need to be recreated
  // Since it can use the same blueprint for submitted framebuffers and be just fine
  createPipeline();
}
void FirstApp::recordCommandBuffer(int imageIndex) {
  static int frame = 0;
  frame = (frame + 1) % 100;
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
  clearValues[0].color = {0.01f, 0.01f, 0.01f, 1.0f};  // first element
  clearValues[1].depthStencil = {1.0f, 0};          // 2nd element
  renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
  renderPassInfo.pClearValues = clearValues.data();

  // Inline here means that renderPass will be embedded in the commandBuffers
  vkCmdBeginRenderPass(commandBuffers[imageIndex], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

  // Dynamically recompute viewport BEFORE submitting the command buffer
  VkViewport viewport{};
  viewport.x = 0.0f;
  viewport.y = 0.0f;
  viewport.width = static_cast<float>(lveSwapChain->getSwapChainExtent().width);
  viewport.height = static_cast<float>(lveSwapChain->getSwapChainExtent().height);
  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;
  VkRect2D scissor{{0, 0}, lveSwapChain->getSwapChainExtent()};
  vkCmdSetViewport(commandBuffers[imageIndex], 0, 1, &viewport);
  vkCmdSetScissor(commandBuffers[imageIndex], 0, 1, &scissor);

  lvePipeline->bind(commandBuffers[imageIndex]);
  // 3 vertices, 1 instance, firstIndex, firstInstance
  // vkCmdDraw(commandBuffers[imageIndex], 3, 1, 0, 0);
  lveModel->bind(commandBuffers[imageIndex]);

  // This
  for (int j = 0; j < 4; j++) {
    SimplePushConstantData push{};
    push.offset = {-0.5f + frame * 0.02f, -0.4f + j * 0.25f};
    push.color = {0.0f, 0.0f, 0.2f + 0.2f * j};

    // RECORD our push constant data
    vkCmdPushConstants(
        commandBuffers[imageIndex],
        pipelineLayout,
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        0,
        sizeof(SimplePushConstantData),
        &push);
    
    // draws 4 copies of model
    lveModel->draw(commandBuffers[imageIndex]);
  }

  // end recording
  vkCmdEndRenderPass(commandBuffers[imageIndex]);
  if (vkEndCommandBuffer(commandBuffers[imageIndex]) != VK_SUCCESS)
    throw std::runtime_error("Failed to record command buffer");
}
}  // namespace lve