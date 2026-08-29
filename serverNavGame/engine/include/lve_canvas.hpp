#pragma once

#include "lve_frame_info.hpp"  // UIRenderItem
#include "lve_model.hpp"

#include <glm/glm.hpp>

#include <string>
#include <vector>

namespace lve {

// A small picture, drawn as one UI quad per pixel
//
// LveTextRenderer already draws a glyph as a quad per lit dot. This is the same
// trick with a colour on every dot, which is how the engine puts a picture on the
// screen without a texture behind it
//
// Keep one small. A 64 x 48 picture is 3072 quads in the frame's UI list
class LveCanvas {
 public:
  LveCanvas() = default;
  LveCanvas(int width, int height, glm::vec3 colour = glm::vec3(0.f));

  int getWidth() const { return width; }
  int getHeight() const { return height; }
  bool empty() const { return pixels.empty(); }

  // How much wider the picture is than it is tall
  float aspect() const {
    return height == 0 ? 1.f : static_cast<float>(width) / static_cast<float>(height);
  }

  void resize(int width, int height, glm::vec3 colour = glm::vec3(0.f));

  // Paints every pixel, holes and all
  void fill(glm::vec3 colour);

  // A cleared pixel is not drawn, so a picture can have holes in it
  void clear(int x, int y);
  bool isClear(int x, int y) const;

  void set(int x, int y, glm::vec3 colour);
  glm::vec3 at(int x, int y) const;

  // Straight edges, which is all a floor plan ever needs
  void hLine(int x0, int x1, int y, glm::vec3 colour);
  void vLine(int x, int y0, int y1, glm::vec3 colour);
  void box(int x, int y, int w, int h, glm::vec3 colour);       // the outline
  void fillBox(int x, int y, int w, int h, glm::vec3 colour);   // and the whole thing

  // Television, for when there is nothing to show
  void noise(unsigned int seed, glm::vec3 dark, glm::vec3 light);

  // A picture written out as rows of palette digits, 0-9 and a-f, where '.' is a
  // hole. Rows shorter than the widest are padded with holes
  static LveCanvas fromRows(const std::vector<glm::vec3>& palette,
                            const std::vector<std::string>& rows);

  // Fills the box from topLeft to topLeft + size, one item per pixel
  // Pixel edges are worked out from the box, so no seams open up between them
  void emit(std::vector<UIRenderItem>& out, LveModel* quad, glm::vec2 topLeft, glm::vec2 size,
            float alpha = 1.f) const;

 private:
  int index(int x, int y) const { return y * width + x; }
  bool inside(int x, int y) const { return x >= 0 && y >= 0 && x < width && y < height; }

  int width = 0;
  int height = 0;
  std::vector<glm::vec3> pixels;

  // One per pixel, saying whether it is drawn at all
  std::vector<char> shown;
};

}  // namespace lve
