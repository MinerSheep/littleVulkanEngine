#include "first_app.hpp"
#include <stdexcept>

namespace lve {
FirstApp::FirstApp() {
  createPipelineLayout();
  createPipeline();
  createCommandBuffers();
}

FirstApp::~FirstApp() {
  // Dont forget to destroy the pipeline layout in destructor, all vk objects needs to do this
  vkDestroyPipelineLayout(lveDevice.device(), pipelineLayout, nullptr);
}

void FirstApp::run() {
  while (!lveWindow.shouldClose()) {
    glfwPollEvents();
  }
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

  if (vkCreatePipelineLayout(lveDevice.device(), &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS)
  {
    throw std::runtime_error("Failed to create pipeline layout");
  }
}
void FirstApp::createPipeline() {
  auto pipelineConfig = LvePipeline::defaultPipelineConfigInfo(lveSwapChain.width(), lveSwapChain.height());
  pipelineConfig.renderPass = lveSwapChain.getRenderPass(); // render pass describes structure and format of our frame buffer objects
  pipelineConfig.pipelineLayout = pipelineLayout;
  lvePipeline = std::make_unique<LvePipeline>(
    lveDevice, 
    "shaders/simple_shader.vert.spv", 
    "shaders/simple_shader.frag.spv", 
    pipelineConfig);
}
void FirstApp::createCommandBuffers() {
  // It is simplest to give each frameBuffer its own commandBuffer to record with, since it needs to be done every frame
  commandBuffers.resize(lveSwapChain.imageCount());

  VkCommandBufferAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;

  allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; // primary can submit to a queue for execution, CANNOT be called by others
  // secondary CANNOT be submitted to queue, so must be called by a PRIMARY command buffer

  allocInfo.commandPool = lveDevice.getCommandPool(); // commandBuffers need a pool so that memory allocation is done ONCE
  allocInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers.size());

  if (vkAllocateCommandBuffers(lveDevice.device(), &allocInfo, commandBuffers.data()) != VK_SUCCESS)
  {
    throw std::runtime_error("Failed to allocate command buffers");
  }

  // Record our draw commands to each buffer
  for (int i = 0; i < commandBuffers.size(); i++)
  {
    VkCommandBufferBeginInfo beginInfo {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    if (vkBeginCommandBuffer(commandBuffers[i], &beginInfo) != VK_SUCCESS)
      throw std::runtime_error("Failed to begin recording command buffer");

    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = lveSwapChain.getRenderPass();
    // which frameBuffer is this renderPass writing?
    renderPassInfo.framebuffer = lveSwapChain.getFrameBuffer(i);

    // Shader loads and stores will take place in renderArea
    renderPassInfo.renderArea.offset = {0,0};
    renderPassInfo.renderArea.extent = lveSwapChain.getSwapChainExtent();

    // Clear values - what we want initial value of A FRAME BUFFER to be after it is cleared
    std::array<VkClearValue, 2> clearValues{};
    clearValues[0].color = {0.1f,0.1f,0.1f, 1.0f}; // first element
    clearValues[1].depthStencil = {1.0f, 0};       // 2nd element
    renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size()); 
    renderPassInfo.pClearValues = clearValues.data();

    // Inline here means that renderPass will be embedded in the commandBuffers
    vkCmdBeginRenderPass(commandBuffers[i], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
  }
}
void FirstApp::drawFrame() {}
}  // namespace lve