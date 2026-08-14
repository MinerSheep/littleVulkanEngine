#include "lve_text.hpp"

#include "lve_engine.hpp"    // aspect ratio for square glyphs
#include "lve_font5x7.hpp"   // the glyph bitmaps and their cell geometry

#include <algorithm>
#include <cctype>
#include <cstdint>

namespace lve {

namespace {

// The glyph a character draws with, falling back on its capital when the font
// has no lowercase for it
const GlyphRows* glyphFor(char c) {
  const auto& table = fontTable();

  auto it = table.find(c);
  if (it != table.end()) return &it->second;

  const char upper = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
  it = table.find(upper);
  return it != table.end() ? &it->second : nullptr;
}

}  // namespace

LveTextRenderer::LveTextRenderer(LveDevice& device) {
  // Unit quad spanning [0,1] x [0,1]. A dot at cell (col,row) is this quad scaled by
  // (dotW, dotH) via the UIRenderItem's mat2 transform and shifted by its offset.
  LveModel::Builder builder{};
  builder.vertices = {
      {{0.f, 0.f, 0.f}, {1.f, 1.f, 1.f}},
      {{1.f, 0.f, 0.f}, {1.f, 1.f, 1.f}},
      {{1.f, 1.f, 0.f}, {1.f, 1.f, 1.f}},
      {{0.f, 1.f, 0.f}, {1.f, 1.f, 1.f}},
  };
  builder.indices = {0, 1, 2, 2, 3, 0};  // cull mode is NONE, so winding is irrelevant
  quadModel = std::make_unique<LveModel>(device, builder);
}

void LveTextRenderer::emit(std::vector<UIRenderItem>& out,
                           const std::string& text,
                           glm::vec2 originNdc,
                           float dotHeight,
                           glm::vec3 color,
                           float alpha) {

  // Get the window aspect ratio so dots stay square
  float aspect = LveEngine::instance().getAspectRatio();
  if (aspect <= 0.f) aspect = 1.f;

  const float dotW = dotHeight / aspect;  // aspect-correct so cells are square
  const float dotH = dotHeight;

  // A 2×2 matrix that scales the quad model into a dot-sized square.
  const glm::mat2 cellTransform{dotW, 0.f, 0.f, dotH};

  // Current position for drawing the glyph
  float penX = originNdc.x;
  float penY = originNdc.y;

  // Loop through each character
  for (char rawc : text) {

    // NEWLINE HANDLE
    if (rawc == '\n') {
      penX = originNdc.x;
      penY += kLineStep * dotH;
      continue;
    }

    // Lookup glyph
    const GlyphRows* glyph = glyphFor(rawc);
    if (glyph) {

      const GlyphRows& rows = *glyph;

      // Loop through row
      for (int row = 0; row < kGlyphRows; ++row) {
        uint8_t bits = rows[row];

        // Loop through col
        for (int col = 0; col < kGlyphCols; ++col) {

          // Check if bit is enabled
          if (bits & (1 << (kGlyphCols - 1 - col))) {
            UIRenderItem dot{};
            dot.transform = cellTransform;
            dot.offset = glm::vec2(penX + col * dotW, penY + row * dotH);
            dot.color = color;
            dot.alpha = alpha;
            dot.model = quadModel.get();

            // ADD to render list
            out.push_back(dot);
          }
        }
      }
    }

    // Move pen to the right for the next character.
    penX += kAdvance * dotW;
  }
}

float LveTextRenderer::measureWidth(const std::string& text, float dotHeight) const {
  float aspect = LveEngine::instance().getAspectRatio();
  if (aspect <= 0.f) aspect = 1.f;
  const float dotW = dotHeight / aspect;

  int maxCells = 0;
  int lineLen = 0;
  auto flush = [&]() {
    // inked width of a line = len glyphs of 5 cells + (len-1) spacing cells
    int cells = lineLen > 0 ? lineLen * kAdvance - 1 : 0;
    maxCells = std::max(maxCells, cells);
  };
  for (char c : text) {
    if (c == '\n') {
      flush();
      lineLen = 0;
    } else {
      ++lineLen;
    }
  }
  flush();
  return maxCells * dotW;
}

int LveTextRenderer::lineCount(const std::string& text) {
  int lines = 1;
  for (char c : text) {
    if (c == '\n') ++lines;
  }
  return lines;
}

}  // namespace lve
