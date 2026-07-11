#include "lve_text.hpp"

#include "lve_engine.hpp"  // aspect ratio for square glyphs

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <unordered_map>

namespace lve {

namespace {

// Glyph geometry, in dot-matrix cells.
constexpr int kGlyphCols = 5;   // inked columns per glyph
constexpr int kGlyphRows = 7;   // inked rows per glyph
constexpr int kAdvance = 6;     // glyph width + 1 column of spacing
constexpr int kLineStep = 8;    // glyph height + 1 row of spacing

using GlyphRows = std::array<uint8_t, kGlyphRows>;

// 5x7 bitmap font. Each 0b01110 is a row that uses 5 bits; read from L - R
// Read from L - R, top - bottom
const std::unordered_map<char, GlyphRows>& fontTable() {
  static const std::unordered_map<char, GlyphRows> table = {
      {' ', {{0, 0, 0, 0, 0, 0, 0}}},

      {'0', {{0b01110, 0b10001, 0b10011, 0b10101, 0b11001, 0b10001, 0b01110}}},
      {'1', {{0b00100, 0b01100, 0b00100, 0b00100, 0b00100, 0b00100, 0b01110}}},
      {'2', {{0b01110, 0b10001, 0b00001, 0b00010, 0b00100, 0b01000, 0b11111}}},
      {'3', {{0b11111, 0b00010, 0b00100, 0b00010, 0b00001, 0b10001, 0b01110}}},
      {'4', {{0b00010, 0b00110, 0b01010, 0b10010, 0b11111, 0b00010, 0b00010}}},
      {'5', {{0b11111, 0b10000, 0b11110, 0b00001, 0b00001, 0b10001, 0b01110}}},
      {'6', {{0b00110, 0b01000, 0b10000, 0b11110, 0b10001, 0b10001, 0b01110}}},
      {'7', {{0b11111, 0b00001, 0b00010, 0b00100, 0b01000, 0b01000, 0b01000}}},
      {'8', {{0b01110, 0b10001, 0b10001, 0b01110, 0b10001, 0b10001, 0b01110}}},
      {'9', {{0b01110, 0b10001, 0b10001, 0b01111, 0b00001, 0b00010, 0b01100}}},

      {'A', {{0b01110, 0b10001, 0b10001, 0b11111, 0b10001, 0b10001, 0b10001}}},
      {'B', {{0b11110, 0b10001, 0b10001, 0b11110, 0b10001, 0b10001, 0b11110}}},
      {'C', {{0b01110, 0b10001, 0b10000, 0b10000, 0b10000, 0b10001, 0b01110}}},
      {'D', {{0b11100, 0b10010, 0b10001, 0b10001, 0b10001, 0b10010, 0b11100}}},
      {'E', {{0b11111, 0b10000, 0b10000, 0b11110, 0b10000, 0b10000, 0b11111}}},
      {'F', {{0b11111, 0b10000, 0b10000, 0b11110, 0b10000, 0b10000, 0b10000}}},
      {'G', {{0b01110, 0b10001, 0b10000, 0b10111, 0b10001, 0b10001, 0b01111}}},
      {'H', {{0b10001, 0b10001, 0b10001, 0b11111, 0b10001, 0b10001, 0b10001}}},
      {'I', {{0b01110, 0b00100, 0b00100, 0b00100, 0b00100, 0b00100, 0b01110}}},
      {'J', {{0b00111, 0b00010, 0b00010, 0b00010, 0b00010, 0b10010, 0b01100}}},
      {'K', {{0b10001, 0b10010, 0b10100, 0b11000, 0b10100, 0b10010, 0b10001}}},
      {'L', {{0b10000, 0b10000, 0b10000, 0b10000, 0b10000, 0b10000, 0b11111}}},
      {'M', {{0b10001, 0b11011, 0b10101, 0b10101, 0b10001, 0b10001, 0b10001}}},
      {'N', {{0b10001, 0b11001, 0b10101, 0b10011, 0b10001, 0b10001, 0b10001}}},
      {'O', {{0b01110, 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b01110}}},
      {'P', {{0b11110, 0b10001, 0b10001, 0b11110, 0b10000, 0b10000, 0b10000}}},
      {'Q', {{0b01110, 0b10001, 0b10001, 0b10001, 0b10101, 0b10010, 0b01101}}},
      {'R', {{0b11110, 0b10001, 0b10001, 0b11110, 0b10100, 0b10010, 0b10001}}},
      {'S', {{0b01111, 0b10000, 0b10000, 0b01110, 0b00001, 0b00001, 0b11110}}},
      {'T', {{0b11111, 0b00100, 0b00100, 0b00100, 0b00100, 0b00100, 0b00100}}},
      {'U', {{0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b01110}}},
      {'V', {{0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b01010, 0b00100}}},
      {'W', {{0b10001, 0b10001, 0b10001, 0b10101, 0b10101, 0b10101, 0b01010}}},
      {'X', {{0b10001, 0b10001, 0b01010, 0b00100, 0b01010, 0b10001, 0b10001}}},
      {'Y', {{0b10001, 0b10001, 0b01010, 0b00100, 0b00100, 0b00100, 0b00100}}},
      {'Z', {{0b11111, 0b00001, 0b00010, 0b00100, 0b01000, 0b10000, 0b11111}}},

      {':', {{0b00000, 0b00100, 0b00100, 0b00000, 0b00100, 0b00100, 0b00000}}},
      {'.', {{0b00000, 0b00000, 0b00000, 0b00000, 0b00000, 0b00110, 0b00110}}},
      {',', {{0b00000, 0b00000, 0b00000, 0b00000, 0b00110, 0b00110, 0b01000}}},
      {'-', {{0b00000, 0b00000, 0b00000, 0b11111, 0b00000, 0b00000, 0b00000}}},
      {'+', {{0b00000, 0b00100, 0b00100, 0b11111, 0b00100, 0b00100, 0b00000}}},
      {'=', {{0b00000, 0b00000, 0b11111, 0b00000, 0b11111, 0b00000, 0b00000}}},
      {'_', {{0b00000, 0b00000, 0b00000, 0b00000, 0b00000, 0b00000, 0b11111}}},
      {'/', {{0b00001, 0b00001, 0b00010, 0b00100, 0b01000, 0b10000, 0b10000}}},
      {'%', {{0b11001, 0b11010, 0b00010, 0b00100, 0b01000, 0b01011, 0b10011}}},
      {'*', {{0b00000, 0b10101, 0b01110, 0b11111, 0b01110, 0b10101, 0b00000}}},
      {'#', {{0b01010, 0b01010, 0b11111, 0b01010, 0b11111, 0b01010, 0b01010}}},
      {'!', {{0b00100, 0b00100, 0b00100, 0b00100, 0b00100, 0b00000, 0b00100}}},
      {'?', {{0b01110, 0b10001, 0b00001, 0b00010, 0b00100, 0b00000, 0b00100}}},
      {'(', {{0b00010, 0b00100, 0b01000, 0b01000, 0b01000, 0b00100, 0b00010}}},
      {')', {{0b01000, 0b00100, 0b00010, 0b00010, 0b00010, 0b00100, 0b01000}}},
      {'<', {{0b00010, 0b00100, 0b01000, 0b10000, 0b01000, 0b00100, 0b00010}}},
      {'>', {{0b01000, 0b00100, 0b00010, 0b00001, 0b00010, 0b00100, 0b01000}}},
  };
  return table;
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

  // Summon lookup table
  const auto& table = fontTable();

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

    // Convert to uppercase
    char c = static_cast<char>(std::toupper(static_cast<unsigned char>(rawc)));

    // Lookup glyph
    auto it = table.find(c);
    if (it != table.end()) {

      const GlyphRows& rows = it->second;

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
