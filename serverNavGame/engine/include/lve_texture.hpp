#pragma once

#include "lve_device.hpp"

#include <memory>
#include <string>

namespace lve {

// A picture something in the world is painted with
//
// The engine has no image loader. The file is the flat RGBA that
// tools/png2tex.py writes, and one texture owns its image, its sampler and the
// descriptor set (set = 1) the floor pipeline binds
class LveTexture {
 public:
  LveTexture(LveDevice& device, const std::string& path);
  ~LveTexture();

  LveTexture(const LveTexture&) = delete;
  LveTexture& operator=(const LveTexture&) = delete;

  // Reads a .tex off the disk, throwing when it will not load
  static std::unique_ptr<LveTexture> createFromFile(const std::string& path);

  VkDescriptorSet getDescriptorSet() const { return descriptorSet; }

  uint32_t getWidth() const { return texWidth; }
  uint32_t getHeight() const { return texHeight; }

  // How much wider the picture is than it is tall, which keeps a tiled floor
  // from stretching its grain
  float aspect() const {
    return texHeight == 0 ? 1.f : static_cast<float>(texWidth) / static_cast<float>(texHeight);
  }

 private:
  void createImage();
  void upload(const void* pixels, VkDeviceSize bytes);

  // Halves the picture down the mip chain, one blit a level
  void buildMips();

  void createView();
  void createSampler();
  void claimDescriptorSet();

  LveDevice& lveDevice;

  VkImage image = VK_NULL_HANDLE;
  VkDeviceMemory memory = VK_NULL_HANDLE;
  VkImageView view = VK_NULL_HANDLE;
  VkSampler sampler = VK_NULL_HANDLE;
  VkDescriptorSet descriptorSet = VK_NULL_HANDLE;

  uint32_t texWidth = 0;
  uint32_t texHeight = 0;
  uint32_t mipLevels = 1;
};

}  // namespace lve
