#include "lve_canvas.hpp"

#include <algorithm>

namespace lve {

namespace {

// Which palette slot a character asks for, or -1 for a hole
int slotOf(char digit) {
  if (digit >= '0' && digit <= '9') return digit - '0';
  if (digit >= 'a' && digit <= 'f') return 10 + (digit - 'a');
  if (digit >= 'A' && digit <= 'F') return 10 + (digit - 'A');
  return -1;
}

// Enough randomness for static, and the same seed gives the same static
unsigned int nextRandom(unsigned int& state) {
  state = state * 1664525u + 1013904223u;
  return state;
}

}  // namespace

LveCanvas::LveCanvas(int width, int height, glm::vec3 colour) { resize(width, height, colour); }

void LveCanvas::resize(int newWidth, int newHeight, glm::vec3 colour) {
  width = std::max(0, newWidth);
  height = std::max(0, newHeight);

  const std::size_t count = static_cast<std::size_t>(width) * height;
  pixels.assign(count, colour);
  shown.assign(count, 1);
}

void LveCanvas::fill(glm::vec3 colour) {
  std::fill(pixels.begin(), pixels.end(), colour);
  std::fill(shown.begin(), shown.end(), 1);
}

void LveCanvas::clear(int x, int y) {
  if (inside(x, y)) shown[index(x, y)] = 0;
}

bool LveCanvas::isClear(int x, int y) const {
  return !inside(x, y) || shown[index(x, y)] == 0;
}

void LveCanvas::set(int x, int y, glm::vec3 colour) {
  if (!inside(x, y)) return;
  pixels[index(x, y)] = colour;
  shown[index(x, y)] = 1;
}

glm::vec3 LveCanvas::at(int x, int y) const {
  return inside(x, y) ? pixels[index(x, y)] : glm::vec3(0.f);
}

void LveCanvas::hLine(int x0, int x1, int y, glm::vec3 colour) {
  if (x1 < x0) std::swap(x0, x1);
  for (int x = x0; x <= x1; x++) set(x, y, colour);
}

void LveCanvas::vLine(int x, int y0, int y1, glm::vec3 colour) {
  if (y1 < y0) std::swap(y0, y1);
  for (int y = y0; y <= y1; y++) set(x, y, colour);
}

void LveCanvas::box(int x, int y, int w, int h, glm::vec3 colour) {
  if (w <= 0 || h <= 0) return;
  hLine(x, x + w - 1, y, colour);
  hLine(x, x + w - 1, y + h - 1, colour);
  vLine(x, y, y + h - 1, colour);
  vLine(x + w - 1, y, y + h - 1, colour);
}

void LveCanvas::fillBox(int x, int y, int w, int h, glm::vec3 colour) {
  for (int row = y; row < y + h; row++)
    for (int column = x; column < x + w; column++) set(column, row, colour);
}

void LveCanvas::noise(unsigned int seed, glm::vec3 dark, glm::vec3 light) {
  unsigned int state = seed == 0 ? 1u : seed;
  for (std::size_t i = 0; i < pixels.size(); i++) {
    const float amount = static_cast<float>(nextRandom(state) >> 8 & 0xFFFF) / 65535.f;
    pixels[i] = dark + (light - dark) * amount;
    shown[i] = 1;
  }
}

LveCanvas LveCanvas::fromRows(const std::vector<glm::vec3>& palette,
                              const std::vector<std::string>& rows) {
  std::size_t widest = 0;
  for (const std::string& row : rows) widest = std::max(widest, row.size());

  LveCanvas canvas(static_cast<int>(widest), static_cast<int>(rows.size()));
  for (int y = 0; y < canvas.height; y++) {
    const std::string& row = rows[static_cast<std::size_t>(y)];
    for (int x = 0; x < canvas.width; x++) {
      const int slot = x < static_cast<int>(row.size()) ? slotOf(row[static_cast<std::size_t>(x)])
                                                        : -1;
      if (slot < 0 || slot >= static_cast<int>(palette.size())) {
        canvas.clear(x, y);
        continue;
      }
      canvas.set(x, y, palette[static_cast<std::size_t>(slot)]);
    }
  }
  return canvas;
}

void LveCanvas::emit(std::vector<UIRenderItem>& out, LveModel* quad, glm::vec2 topLeft,
                     glm::vec2 size, float alpha) const {
  if (quad == nullptr || pixels.empty() || alpha <= 0.f) return;

  for (int y = 0; y < height; y++) {
    // Both edges come off the box, so one pixel ends exactly where the next starts
    const float top = topLeft.y + size.y * static_cast<float>(y) / static_cast<float>(height);
    const float bottom =
        topLeft.y + size.y * static_cast<float>(y + 1) / static_cast<float>(height);

    for (int x = 0; x < width; x++) {
      if (shown[index(x, y)] == 0) continue;

      const float left = topLeft.x + size.x * static_cast<float>(x) / static_cast<float>(width);
      const float right =
          topLeft.x + size.x * static_cast<float>(x + 1) / static_cast<float>(width);

      UIRenderItem item{};
      item.transform = glm::mat2(right - left, 0.f, 0.f, bottom - top);
      item.offset = {left, top};
      item.color = pixels[index(x, y)];
      item.alpha = alpha;
      item.model = quad;
      out.push_back(item);
    }
  }
}

}  // namespace lve
