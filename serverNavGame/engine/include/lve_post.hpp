#pragma once

#include "lve_descriptors.hpp"
#include "lve_device.hpp"
#include "lve_frame_info.hpp"  // PostEffect
#include "lve_pipeline.hpp"

#include <memory>

namespace lve {

// The room is drawn into a picture first, and the picture is what reaches the
// screen
//
// Everything the game draws goes into an image of its own, and one full-screen
// triangle puts that image on the screen through a shader. So the director can
// do things to the picture -- slide it, pull the colour apart, hold it still --
// without touching what is in the room
class LvePost {
 public:
  LvePost(LveDevice& device, LveDescriptorSetLayout& imageSetLayout, LveDescriptorPool& pool);
  ~LvePost();

  LvePost(const LvePost&) = delete;
  LvePost& operator=(const LvePost&) = delete;

  // Builds the picture at this size, and again whenever the window changes
  // screenPass is what the finished picture is drawn into, and it decides what
  // the full-screen pipeline has to be compatible with
  void rebuild(VkExtent2D extent, VkFormat colour, VkFormat depth, VkRenderPass screenPass);

  bool ready() const { return framebuffer != VK_NULL_HANDLE; }
  VkExtent2D size() const { return extent; }

  // The pass the room is drawn into, which is compatible with the screen's, so
  // every pipeline built against one works in the other
  VkRenderPass getRenderPass() const { return renderPass; }

  void beginPass(VkCommandBuffer commandBuffer);
  void endPass(VkCommandBuffer commandBuffer);

  // Puts the picture on the screen, with whatever is being done to it
  void drawTo(VkCommandBuffer commandBuffer, const PostEffect& effect, float time);

 private:
  void dropImages();
  void makeImages(VkFormat colour, VkFormat depth);
  void makeRenderPass(VkFormat colour, VkFormat depth);
  void makePipeline(VkRenderPass screenPass);

  LveDevice& lveDevice;
  LveDescriptorSetLayout& imageSetLayout;
  LveDescriptorPool& pool;

  VkExtent2D extent{0, 0};

  VkImage colourImage = VK_NULL_HANDLE;
  VkDeviceMemory colourMemory = VK_NULL_HANDLE;
  VkImageView colourView = VK_NULL_HANDLE;

  VkImage depthImage = VK_NULL_HANDLE;
  VkDeviceMemory depthMemory = VK_NULL_HANDLE;
  VkImageView depthView = VK_NULL_HANDLE;

  VkSampler sampler = VK_NULL_HANDLE;
  VkRenderPass renderPass = VK_NULL_HANDLE;
  VkFramebuffer framebuffer = VK_NULL_HANDLE;

  VkDescriptorSet imageSet = VK_NULL_HANDLE;
  VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
  std::unique_ptr<LvePipeline> pipeline;
};

}  // namespace lve
