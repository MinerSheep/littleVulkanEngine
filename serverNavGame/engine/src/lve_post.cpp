#include "lve_post.hpp"

#include <array>
#include <stdexcept>

namespace lve {

namespace {

// What the shader is handed about the state the picture is in
struct PostPush {
  float wobble;
  float separate;
  float invert;
  float vignette;
  float grain;
  float drain;
  float time;
};

}  // namespace

LvePost::LvePost(LveDevice& device, LveDescriptorSetLayout& setLayout, LveDescriptorPool& fromPool)
    : lveDevice{device}, imageSetLayout{setLayout}, pool{fromPool} {
  VkSamplerCreateInfo samplerInfo{};
  samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  samplerInfo.magFilter = VK_FILTER_LINEAR;
  samplerInfo.minFilter = VK_FILTER_LINEAR;
  samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
  samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

  if (vkCreateSampler(lveDevice.device(), &samplerInfo, nullptr, &sampler) != VK_SUCCESS)
    throw std::runtime_error("failed to create the post sampler");
}

LvePost::~LvePost() {
  dropImages();

  if (renderPass != VK_NULL_HANDLE) vkDestroyRenderPass(lveDevice.device(), renderPass, nullptr);
  if (pipelineLayout != VK_NULL_HANDLE)
    vkDestroyPipelineLayout(lveDevice.device(), pipelineLayout, nullptr);
  if (sampler != VK_NULL_HANDLE) vkDestroySampler(lveDevice.device(), sampler, nullptr);
}

void LvePost::dropImages() {
  VkDevice device = lveDevice.device();

  if (framebuffer != VK_NULL_HANDLE) vkDestroyFramebuffer(device, framebuffer, nullptr);
  if (colourView != VK_NULL_HANDLE) vkDestroyImageView(device, colourView, nullptr);
  if (colourImage != VK_NULL_HANDLE) vkDestroyImage(device, colourImage, nullptr);
  if (colourMemory != VK_NULL_HANDLE) vkFreeMemory(device, colourMemory, nullptr);
  if (depthView != VK_NULL_HANDLE) vkDestroyImageView(device, depthView, nullptr);
  if (depthImage != VK_NULL_HANDLE) vkDestroyImage(device, depthImage, nullptr);
  if (depthMemory != VK_NULL_HANDLE) vkFreeMemory(device, depthMemory, nullptr);

  framebuffer = VK_NULL_HANDLE;
  colourView = VK_NULL_HANDLE;
  colourImage = VK_NULL_HANDLE;
  colourMemory = VK_NULL_HANDLE;
  depthView = VK_NULL_HANDLE;
  depthImage = VK_NULL_HANDLE;
  depthMemory = VK_NULL_HANDLE;
}

void LvePost::makeImages(VkFormat colour, VkFormat depth) {
  VkImageCreateInfo imageInfo{};
  imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  imageInfo.imageType = VK_IMAGE_TYPE_2D;
  imageInfo.extent = {extent.width, extent.height, 1};
  imageInfo.mipLevels = 1;
  imageInfo.arrayLayers = 1;
  imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
  imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
  imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

  imageInfo.format = colour;
  imageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
  lveDevice.createImageWithInfo(imageInfo, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, colourImage,
                                colourMemory);

  imageInfo.format = depth;
  imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
  lveDevice.createImageWithInfo(imageInfo, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, depthImage,
                               depthMemory);

  VkImageViewCreateInfo viewInfo{};
  viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
  viewInfo.subresourceRange.levelCount = 1;
  viewInfo.subresourceRange.layerCount = 1;

  viewInfo.image = colourImage;
  viewInfo.format = colour;
  viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  if (vkCreateImageView(lveDevice.device(), &viewInfo, nullptr, &colourView) != VK_SUCCESS)
    throw std::runtime_error("failed to make the post colour view");

  viewInfo.image = depthImage;
  viewInfo.format = depth;
  viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
  if (vkCreateImageView(lveDevice.device(), &viewInfo, nullptr, &depthView) != VK_SUCCESS)
    throw std::runtime_error("failed to make the post depth view");
}

// The same shape as the screen's own pass -- same formats, same one subpass --
// so every pipeline built against the screen draws into this one unchanged
void LvePost::makeRenderPass(VkFormat colour, VkFormat depth) {
  VkAttachmentDescription colourAttachment{};
  colourAttachment.format = colour;
  colourAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
  colourAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  colourAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  colourAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  colourAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  colourAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

  // It is read by a shader next, not shown
  colourAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

  VkAttachmentDescription depthAttachment{};
  depthAttachment.format = depth;
  depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
  depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

  VkAttachmentReference colourRef{};
  colourRef.attachment = 0;
  colourRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  VkAttachmentReference depthRef{};
  depthRef.attachment = 1;
  depthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

  VkSubpassDescription subpass{};
  subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.colorAttachmentCount = 1;
  subpass.pColorAttachments = &colourRef;
  subpass.pDepthStencilAttachment = &depthRef;

  // The same one dependency the screen's pass has, because two passes only count
  // as compatible when their dependencies match as well as their attachments
  VkSubpassDependency dependency{};
  dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
  dependency.srcAccessMask = 0;
  dependency.srcStageMask =
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
  dependency.dstSubpass = 0;
  dependency.dstStageMask =
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
  dependency.dstAccessMask =
      VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

  std::array<VkAttachmentDescription, 2> attachments = {colourAttachment, depthAttachment};

  VkRenderPassCreateInfo passInfo{};
  passInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  passInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
  passInfo.pAttachments = attachments.data();
  passInfo.subpassCount = 1;
  passInfo.pSubpasses = &subpass;
  passInfo.dependencyCount = 1;
  passInfo.pDependencies = &dependency;

  if (vkCreateRenderPass(lveDevice.device(), &passInfo, nullptr, &renderPass) != VK_SUCCESS)
    throw std::runtime_error("failed to make the post render pass");
}

void LvePost::makePipeline(VkRenderPass screenPass) {
  if (pipelineLayout == VK_NULL_HANDLE) {
    VkPushConstantRange push{};
    push.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    push.offset = 0;
    push.size = sizeof(PostPush);

    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    VkDescriptorSetLayout setLayout = imageSetLayout.getDescriptorSetLayout();
    layoutInfo.setLayoutCount = 1;
    layoutInfo.pSetLayouts = &setLayout;
    layoutInfo.pushConstantRangeCount = 1;
    layoutInfo.pPushConstantRanges = &push;

    if (vkCreatePipelineLayout(lveDevice.device(), &layoutInfo, nullptr, &pipelineLayout) !=
        VK_SUCCESS)
      throw std::runtime_error("failed to make the post pipeline layout");
  }

  PipelineConfigInfo config{};
  LvePipeline::defaultPipelineConfigInfo(config);

  // One triangle worked out in the shader, covering everything, in front of nothing
  config.bindingDescriptions.clear();
  config.attributeDescriptions.clear();
  config.depthStencilInfo.depthTestEnable = VK_FALSE;
  config.depthStencilInfo.depthWriteEnable = VK_FALSE;
  config.rasterizationInfo.cullMode = VK_CULL_MODE_NONE;
  config.renderPass = screenPass;
  config.pipelineLayout = pipelineLayout;

  pipeline = std::make_unique<LvePipeline>(lveDevice, "shaders/post.vert.spv",
                                           "shaders/post.frag.spv", config);
}

void LvePost::rebuild(VkExtent2D newExtent, VkFormat colour, VkFormat depth,
                      VkRenderPass screenPass) {
  if (newExtent.width == 0 || newExtent.height == 0) return;

  dropImages();
  extent = newExtent;

  if (renderPass == VK_NULL_HANDLE) makeRenderPass(colour, depth);
  makeImages(colour, depth);

  std::array<VkImageView, 2> views = {colourView, depthView};

  VkFramebufferCreateInfo frameInfo{};
  frameInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
  frameInfo.renderPass = renderPass;
  frameInfo.attachmentCount = static_cast<uint32_t>(views.size());
  frameInfo.pAttachments = views.data();
  frameInfo.width = extent.width;
  frameInfo.height = extent.height;
  frameInfo.layers = 1;

  if (vkCreateFramebuffer(lveDevice.device(), &frameInfo, nullptr, &framebuffer) != VK_SUCCESS)
    throw std::runtime_error("failed to make the post framebuffer");

  if (!pipeline) makePipeline(screenPass);

  // The set points at the new image, so it is written again on every rebuild
  VkDescriptorImageInfo imageInfo{};
  imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  imageInfo.imageView = colourView;
  imageInfo.sampler = sampler;

  LveDescriptorWriter writer(imageSetLayout, pool);
  writer.writeImage(0, &imageInfo);

  if (imageSet == VK_NULL_HANDLE) writer.build(imageSet);
  else writer.overwrite(imageSet);
}

void LvePost::beginPass(VkCommandBuffer commandBuffer) {
  VkRenderPassBeginInfo passInfo{};
  passInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  passInfo.renderPass = renderPass;
  passInfo.framebuffer = framebuffer;
  passInfo.renderArea.offset = {0, 0};
  passInfo.renderArea.extent = extent;

  std::array<VkClearValue, 2> clears{};
  clears[0].color = {{0.01f, 0.01f, 0.01f, 1.0f}};
  clears[1].depthStencil = {1.0f, 0};
  passInfo.clearValueCount = static_cast<uint32_t>(clears.size());
  passInfo.pClearValues = clears.data();

  vkCmdBeginRenderPass(commandBuffer, &passInfo, VK_SUBPASS_CONTENTS_INLINE);

  VkViewport viewport{};
  viewport.width = static_cast<float>(extent.width);
  viewport.height = static_cast<float>(extent.height);
  viewport.maxDepth = 1.0f;
  VkRect2D scissor{{0, 0}, extent};
  vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
  vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
}

void LvePost::endPass(VkCommandBuffer commandBuffer) {
  vkCmdEndRenderPass(commandBuffer);

  // Written as an attachment, read next as a picture. The pass carries the same
  // one dependency the screen's does, so the wait for it is made here instead
  VkImageMemoryBarrier barrier{};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.image = colourImage;
  barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  barrier.subresourceRange.levelCount = 1;
  barrier.subresourceRange.layerCount = 1;
  barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
  barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

  vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                       VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1,
                       &barrier);
}

void LvePost::drawTo(VkCommandBuffer commandBuffer, const PostEffect& effect, float time) {
  if (!pipeline || imageSet == VK_NULL_HANDLE) return;

  pipeline->bind(commandBuffer);
  vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1,
                          &imageSet, 0, nullptr);

  PostPush push{};
  push.wobble = effect.wobble;
  push.separate = effect.separate;
  push.invert = effect.invert;
  push.vignette = effect.vignette;
  push.grain = effect.grain;
  push.drain = effect.drain;
  push.time = time;

  vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                     sizeof(PostPush), &push);

  vkCmdDraw(commandBuffer, 3, 1, 0, 0);
}

}  // namespace lve
