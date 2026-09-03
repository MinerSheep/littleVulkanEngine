#include "lve_screenshot.hpp"

#include <algorithm>
#include <cstring>

namespace lve {

namespace {

// Moves an image from one layout to another, which is all the barriers here do
void moveTo(VkCommandBuffer commandBuffer, VkImage image, VkImageLayout from, VkImageLayout to,
            VkAccessFlags was, VkAccessFlags will,
            VkPipelineStageFlags after = VK_PIPELINE_STAGE_TRANSFER_BIT) {
  VkImageMemoryBarrier barrier{};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.oldLayout = from;
  barrier.newLayout = to;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.image = image;
  barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  barrier.subresourceRange.levelCount = 1;
  barrier.subresourceRange.layerCount = 1;
  barrier.srcAccessMask = was;
  barrier.dstAccessMask = will;

  vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, after, 0, 0, nullptr, 0,
                       nullptr, 1, &barrier);
}

// Whether the frame comes off the swap chain with blue and red the other way round
bool swapsRedAndBlue(VkFormat format) {
  return format == VK_FORMAT_B8G8R8A8_UNORM || format == VK_FORMAT_B8G8R8A8_SRGB;
}

}  // namespace

LveScreenshot::~LveScreenshot() { dropCopyImage(); }

void LveScreenshot::want(int width, int height) {
  if (width <= 0 || height <= 0) return;
  wantWide = width;
  wantTall = height;
  held = false;
}

void LveScreenshot::dropCopyImage() {
  if (copy != VK_NULL_HANDLE) vkDestroyImage(lveDevice.device(), copy, nullptr);
  if (copyMemory != VK_NULL_HANDLE) vkFreeMemory(lveDevice.device(), copyMemory, nullptr);
  copy = VK_NULL_HANDLE;
  copyMemory = VK_NULL_HANDLE;
  copyExtent = {0, 0};
}

// The frame is copied at the size it was drawn and shrunk afterwards, so nothing
// here depends on the driver being willing to scale while it copies
void LveScreenshot::makeCopyImage(VkFormat format, VkExtent2D extent) {
  if (copy != VK_NULL_HANDLE && copyFormat == format && copyExtent.width == extent.width &&
      copyExtent.height == extent.height)
    return;

  dropCopyImage();

  VkImageCreateInfo imageInfo{};
  imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  imageInfo.imageType = VK_IMAGE_TYPE_2D;
  imageInfo.format = format;
  imageInfo.extent = {extent.width, extent.height, 1};
  imageInfo.mipLevels = 1;
  imageInfo.arrayLayers = 1;
  imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
  imageInfo.tiling = VK_IMAGE_TILING_LINEAR;
  imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

  lveDevice.createImageWithInfo(
      imageInfo, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, copy,
      copyMemory);

  copyFormat = format;
  copyExtent = extent;
}

void LveScreenshot::record(VkCommandBuffer commandBuffer, VkImage frame, VkFormat format,
                           VkExtent2D extent) {
  if (!waiting() || frame == VK_NULL_HANDLE) return;

  makeCopyImage(format, extent);
  if (copy == VK_NULL_HANDLE) return;

  // The frame is finished with and on its way to the screen, so it is borrowed
  // and handed straight back
  moveTo(commandBuffer, frame, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
         VK_ACCESS_MEMORY_READ_BIT, VK_ACCESS_TRANSFER_READ_BIT);
  moveTo(commandBuffer, copy, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0,
         VK_ACCESS_TRANSFER_WRITE_BIT);

  VkImageCopy region{};
  region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  region.srcSubresource.layerCount = 1;
  region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  region.dstSubresource.layerCount = 1;
  region.extent = {extent.width, extent.height, 1};

  vkCmdCopyImage(commandBuffer, frame, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, copy,
                 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

  // The CPU is what reads it next, so the wait has to name the host
  moveTo(commandBuffer, copy, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
         VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT, VK_PIPELINE_STAGE_HOST_BIT);
  moveTo(commandBuffer, frame, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
         VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_MEMORY_READ_BIT);
}

bool LveScreenshot::take(LveCanvas& out) {
  if (!held || copy == VK_NULL_HANDLE) return false;

  // The size that was asked for, before the ask is cleared
  const int wide = std::max(1, wantWide);
  const int tall = std::max(1, wantTall);
  held = false;
  wantWide = 0;

  VkImageSubresource where{};
  where.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  VkSubresourceLayout laidOut{};
  vkGetImageSubresourceLayout(lveDevice.device(), copy, &where, &laidOut);

  void* mapped = nullptr;
  if (vkMapMemory(lveDevice.device(), copyMemory, 0, VK_WHOLE_SIZE, 0, &mapped) != VK_SUCCESS)
    return false;

  const unsigned char* bytes = static_cast<const unsigned char*>(mapped) + laidOut.offset;
  const bool flipped = swapsRedAndBlue(copyFormat);

  out.resize(wide, tall);

  // Every pixel of the picture is the average of the block of screen behind it
  for (int y = 0; y < tall; y++) {
    const int fromY = static_cast<int>(static_cast<long>(y) * copyExtent.height / tall);
    const int toY = std::max(fromY + 1,
                             static_cast<int>(static_cast<long>(y + 1) * copyExtent.height / tall));

    for (int x = 0; x < wide; x++) {
      const int fromX = static_cast<int>(static_cast<long>(x) * copyExtent.width / wide);
      const int toX = std::max(fromX + 1,
                               static_cast<int>(static_cast<long>(x + 1) * copyExtent.width / wide));

      glm::vec3 total(0.f);
      int counted = 0;
      for (int row = fromY; row < toY; row++) {
        const unsigned char* line = bytes + row * laidOut.rowPitch;
        for (int column = fromX; column < toX; column++) {
          const unsigned char* dot = line + column * 4;
          const float r = dot[flipped ? 2 : 0] / 255.f;
          const float g = dot[1] / 255.f;
          const float b = dot[flipped ? 0 : 2] / 255.f;
          total += glm::vec3(r, g, b);
          counted++;
        }
      }
      if (counted > 0) out.set(x, y, total / static_cast<float>(counted));
    }
  }

  vkUnmapMemory(lveDevice.device(), copyMemory);
  return true;
}

}  // namespace lve
