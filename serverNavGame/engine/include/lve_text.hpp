#pragma once

#include "lve_device.hpp"
#include "lve_model.hpp"
#include "lve_frame_info.hpp"  // UIRenderItem

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#include <memory>
#include <string>
#include <vector>

namespace lve {

// Uses existing UI drawing system, each letter is a quad with dots it can fill in using glyphs.
// Every lit cell in the glyph becomes one small UIRenderItem.
// emit() adds the text into the UIRenderItem list
class LveTextRenderer {
 public:
  explicit LveTextRenderer(LveDevice& device);

  // Appends one UIRenderItem per lit dot of `text` into `out`.
  //   originNdc : top-left corner of the text, in Vulkan NDC
  //               (x: -1 left .. +1 right,  y: -1 top .. +1 bottom).
  //   dotHeight : NDC height of a single glyph cell (a glyph is 7 cells tall). The
  //               cell width is aspect-corrected from the live window so glyphs stay
  //               square regardless of window shape.
  //   color/alpha : passed straight through to each dot's UIRenderItem.
  // '\n' starts a new line. Lowercase letters are drawn using the uppercase glyphs.
  // Characters with no glyph advance the cursor but draw nothing.
  void emit(std::vector<UIRenderItem>& out,
            const std::string& text,
            glm::vec2 originNdc,
            float dotHeight,
            glm::vec3 color,
            float alpha = 1.0f);

  // NDC width of the widest line of `text` at `dotHeight` (for centering / right-align).
  float measureWidth(const std::string& text, float dotHeight) const;

  // 1 + number of '\n' in `text`.
  static int lineCount(const std::string& text);

  // Unit quad ([0,1]x[0,1]) used for the dots; handy for drawing background panels.
  LveModel* quad() { return quadModel.get(); }

 private:
  std::unique_ptr<LveModel> quadModel;
};

}  // namespace lve
