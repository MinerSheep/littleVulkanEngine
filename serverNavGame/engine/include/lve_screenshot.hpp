#pragma once

#include "lve_canvas.hpp"
#include "lve_device.hpp"

#include <vulkan/vulkan.h>

#include <vector>

namespace lve {

// Keeps a copy of a frame the moment it has finished being drawn
//
// The frame is copied aside into an image the CPU can read, before it goes to
// the screen, and shrunk down to whatever size the picture wants
class LveScreenshot {
 public:
  LveScreenshot(LveDevice& device) : lveDevice{device} {}
  ~LveScreenshot();

  LveScreenshot(const LveScreenshot&) = delete;
  LveScreenshot& operator=(const LveScreenshot&) = delete;

  // The next frame drawn is kept, shrunk to this many pixels across and down
  void want(int width, int height);

  // A frame has been asked for and none has been copied yet
  bool waiting() const { return wantWide > 0 && !held; }

  // Copies the finished frame aside, before it is handed over to the screen
  // Recorded into the frame's own command buffer, after the render pass ends
  void record(VkCommandBuffer commandBuffer, VkImage frame, VkFormat format, VkExtent2D extent);

  // The copy has gone through and the pixels can be read
  void arrived() { held = true; }

  // What was kept, shrunk to the size that was asked for
  bool take(LveCanvas& out);

 private:
  void makeCopyImage(VkFormat format, VkExtent2D extent);
  void dropCopyImage();

  LveDevice& lveDevice;

  // The frame as it was drawn, in an image laid out the way the CPU reads
  VkImage copy = VK_NULL_HANDLE;
  VkDeviceMemory copyMemory = VK_NULL_HANDLE;
  VkFormat copyFormat = VK_FORMAT_UNDEFINED;
  VkExtent2D copyExtent{0, 0};

  int wantWide = 0;
  int wantTall = 0;
  bool held = false;
};

}  // namespace lve
