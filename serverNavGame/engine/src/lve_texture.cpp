#include "lve_texture.hpp"

#include "lve_descriptors.hpp"
#include "lve_engine.hpp"

// std
#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <vector>

namespace lve {

namespace {

const char kMagic[8] = {'L', 'V', 'E', 'T', 'E', 'X', '0', '1'};

// The format every .tex is uploaded as, colour picked out of an sRGB file
const VkFormat kFormat = VK_FORMAT_R8G8B8A8_SRGB;

// One step of the trip from a fresh image to something a shader can read
void moveLayout(VkCommandBuffer commandBuffer, VkImage image, uint32_t baseMip, uint32_t levels,
                VkImageLayout from, VkImageLayout to) {
  VkImageMemoryBarrier barrier{};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.oldLayout = from;
  barrier.newLayout = to;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.image = image;
  barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  barrier.subresourceRange.baseMipLevel = baseMip;
  barrier.subresourceRange.levelCount = levels;
  barrier.subresourceRange.baseArrayLayer = 0;
  barrier.subresourceRange.layerCount = 1;

  VkPipelineStageFlags before = VK_PIPELINE_STAGE_TRANSFER_BIT;
  VkPipelineStageFlags after = VK_PIPELINE_STAGE_TRANSFER_BIT;

  if (from == VK_IMAGE_LAYOUT_UNDEFINED) {
    barrier.srcAccessMask = 0;
    before = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
  } else if (from == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  } else {
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
  }

  if (to == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    after = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
  } else if (to == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  } else {
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
  }

  vkCmdPipelineBarrier(commandBuffer, before, after, 0, 0, nullptr, 0, nullptr, 1, &barrier);
}

}  // namespace

LveTexture::LveTexture(LveDevice& device, const std::string& path) : lveDevice{device} {
  std::ifstream file(path, std::ios::binary);
  if (!file.is_open()) throw std::runtime_error("could not open texture [" + path + "]");

  char magic[8] = {};
  uint32_t header[2] = {0, 0};
  file.read(magic, sizeof(magic));
  file.read(reinterpret_cast<char*>(header), sizeof(header));
  if (!file || std::memcmp(magic, kMagic, sizeof(magic)) != 0)
    throw std::runtime_error("[" + path + "] is not a .tex, run tools/png2tex.py on the picture");

  texWidth = header[0];
  texHeight = header[1];
  if (texWidth == 0 || texHeight == 0)
    throw std::runtime_error("[" + path + "] says it is " + std::to_string(texWidth) + " x " +
                             std::to_string(texHeight));

  const VkDeviceSize bytes = static_cast<VkDeviceSize>(texWidth) * texHeight * 4;
  std::vector<unsigned char> pixels(bytes);
  file.read(reinterpret_cast<char*>(pixels.data()), static_cast<std::streamsize>(bytes));
  if (static_cast<VkDeviceSize>(file.gcount()) != bytes)
    throw std::runtime_error("[" + path + "] stops before its last row");

  // Mips need the driver to blit, and a driver that will not just gets one level
  mipLevels = 1;
  try {
    lveDevice.findSupportedFormat(
        {kFormat}, VK_IMAGE_TILING_OPTIMAL,
        VK_FORMAT_FEATURE_BLIT_SRC_BIT | VK_FORMAT_FEATURE_BLIT_DST_BIT |
            VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT);
    const uint32_t longest = std::max(texWidth, texHeight);
    mipLevels = 1 + static_cast<uint32_t>(std::floor(std::log2(static_cast<float>(longest))));
  } catch (const std::exception&) {
    mipLevels = 1;
  }

  createImage();
  upload(pixels.data(), bytes);
  if (mipLevels > 1) buildMips();
  createView();
  createSampler();
  claimDescriptorSet();
}

LveTexture::~LveTexture() {
  VkDevice device = lveDevice.device();
  if (sampler != VK_NULL_HANDLE) vkDestroySampler(device, sampler, nullptr);
  if (view != VK_NULL_HANDLE) vkDestroyImageView(device, view, nullptr);
  if (image != VK_NULL_HANDLE) vkDestroyImage(device, image, nullptr);
  if (memory != VK_NULL_HANDLE) vkFreeMemory(device, memory, nullptr);
}

std::unique_ptr<LveTexture> LveTexture::createFromFile(const std::string& path) {
  return std::make_unique<LveTexture>(LveEngine::instance().getDevice(), path);
}

void LveTexture::createImage() {
  VkImageCreateInfo info{};
  info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  info.imageType = VK_IMAGE_TYPE_2D;
  info.extent = {texWidth, texHeight, 1};
  info.mipLevels = mipLevels;
  info.arrayLayers = 1;
  info.format = kFormat;
  info.tiling = VK_IMAGE_TILING_OPTIMAL;
  info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

  // Read from as well as written to, since each mip is blitted off the one above
  info.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
               VK_IMAGE_USAGE_SAMPLED_BIT;
  info.samples = VK_SAMPLE_COUNT_1_BIT;
  info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  lveDevice.createImageWithInfo(info, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, image, memory);
}

void LveTexture::upload(const void* pixels, VkDeviceSize bytes) {
  VkBuffer staging = VK_NULL_HANDLE;
  VkDeviceMemory stagingMemory = VK_NULL_HANDLE;
  lveDevice.createBuffer(bytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                         staging, stagingMemory);

  void* mapped = nullptr;
  vkMapMemory(lveDevice.device(), stagingMemory, 0, bytes, 0, &mapped);
  std::memcpy(mapped, pixels, static_cast<std::size_t>(bytes));
  vkUnmapMemory(lveDevice.device(), stagingMemory);

  VkCommandBuffer commandBuffer = lveDevice.beginSingleTimeCommands();
  moveLayout(commandBuffer, image, 0, mipLevels, VK_IMAGE_LAYOUT_UNDEFINED,
             VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
  lveDevice.endSingleTimeCommands(commandBuffer);

  lveDevice.copyBufferToImage(staging, image, texWidth, texHeight, 1);

  // With no mip chain the one level goes straight to being readable
  if (mipLevels == 1) {
    commandBuffer = lveDevice.beginSingleTimeCommands();
    moveLayout(commandBuffer, image, 0, 1, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
               VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    lveDevice.endSingleTimeCommands(commandBuffer);
  }

  vkDestroyBuffer(lveDevice.device(), staging, nullptr);
  vkFreeMemory(lveDevice.device(), stagingMemory, nullptr);
}

void LveTexture::buildMips() {
  VkCommandBuffer commandBuffer = lveDevice.beginSingleTimeCommands();

  int32_t wide = static_cast<int32_t>(texWidth);
  int32_t tall = static_cast<int32_t>(texHeight);

  for (uint32_t level = 1; level < mipLevels; level++) {
    moveLayout(commandBuffer, image, level - 1, 1, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
               VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

    const int32_t nextWide = wide > 1 ? wide / 2 : 1;
    const int32_t nextTall = tall > 1 ? tall / 2 : 1;

    VkImageBlit blit{};
    blit.srcOffsets[0] = {0, 0, 0};
    blit.srcOffsets[1] = {wide, tall, 1};
    blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    blit.srcSubresource.mipLevel = level - 1;
    blit.srcSubresource.baseArrayLayer = 0;
    blit.srcSubresource.layerCount = 1;
    blit.dstOffsets[0] = {0, 0, 0};
    blit.dstOffsets[1] = {nextWide, nextTall, 1};
    blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    blit.dstSubresource.mipLevel = level;
    blit.dstSubresource.baseArrayLayer = 0;
    blit.dstSubresource.layerCount = 1;

    vkCmdBlitImage(commandBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, image,
                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR);

    moveLayout(commandBuffer, image, level - 1, 1, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
               VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    wide = nextWide;
    tall = nextTall;
  }

  // The smallest level was only ever written to
  moveLayout(commandBuffer, image, mipLevels - 1, 1, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
             VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

  lveDevice.endSingleTimeCommands(commandBuffer);
}

void LveTexture::createView() {
  VkImageViewCreateInfo info{};
  info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  info.image = image;
  info.viewType = VK_IMAGE_VIEW_TYPE_2D;
  info.format = kFormat;
  info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  info.subresourceRange.baseMipLevel = 0;
  info.subresourceRange.levelCount = mipLevels;
  info.subresourceRange.baseArrayLayer = 0;
  info.subresourceRange.layerCount = 1;

  if (vkCreateImageView(lveDevice.device(), &info, nullptr, &view) != VK_SUCCESS)
    throw std::runtime_error("failed to create a texture view");
}

void LveTexture::createSampler() {
  VkSamplerCreateInfo info{};
  info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  info.magFilter = VK_FILTER_LINEAR;
  info.minFilter = VK_FILTER_LINEAR;

  // A photograph of floorboards does not line up with itself, and mirroring the
  // repeat hides the seam a plain wrap would leave across the room
  info.addressModeU = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
  info.addressModeV = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
  info.addressModeW = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;

  // samplerAnisotropy is not among the features the device is created with
  info.anisotropyEnable = VK_FALSE;
  info.maxAnisotropy = 1.f;

  info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
  info.unnormalizedCoordinates = VK_FALSE;
  info.compareEnable = VK_FALSE;
  info.compareOp = VK_COMPARE_OP_ALWAYS;
  info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
  info.mipLodBias = 0.f;
  info.minLod = 0.f;
  info.maxLod = static_cast<float>(mipLevels);

  if (vkCreateSampler(lveDevice.device(), &info, nullptr, &sampler) != VK_SUCCESS)
    throw std::runtime_error("failed to create a texture sampler");
}

void LveTexture::claimDescriptorSet() {
  VkDescriptorImageInfo info{};
  info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  info.imageView = view;
  info.sampler = sampler;

  LveEngine& engine = LveEngine::instance();
  if (!LveDescriptorWriter(engine.getTextureSetLayout(), engine.getTexturePool())
           .writeImage(0, &info)
           .build(descriptorSet))
    throw std::runtime_error("ran out of texture descriptor sets");
}

}  // namespace lve
