#pragma once

#include "lve_canvas.hpp"
#include "lve_frame_info.hpp"
#include "lve_text.hpp"

#include <cstddef>
#include <string>
#include <vector>

struct GLFWwindow;

namespace petscop {

struct GameState;

// ESC stops the walk and puts the buttons on the left, his pockets on the right
class PauseMenu {
 public:
  // What he did with it this frame, which the scene reads once
  enum class Choice { None, Resume, Options, Back, Leave };

  bool isOpen() const { return open; }
  void close();

  // ESC opens and shuts it, W and S move, E or enter chooses
  Choice update(GLFWwindow* window);

  void emit(std::vector<lve::UIRenderItem>& out, lve::LveTextRenderer& text,
            const GameState& state) const;

  // F19: one button left late on, and it does not say exit
  bool stripped = false;

  // He is carrying the map, which puts a button in the list
  bool hasMap = false;

  // The house as the map draws it, which the scene hands over and still owns
  const lve::LveCanvas* mapPicture = nullptr;

  // X12: there are pictures filed, which puts another button in the list
  bool hasPhotos = false;

  // Which one the page is showing, and how many there are to step through
  std::size_t photoIndex = 0;
  std::size_t photoCount = 0;

  // The picture and the line under it, both drawn and owned by the scene
  const lve::LveCanvas* photoPicture = nullptr;
  std::string photoCaption;

 private:
  // The buttons as they stand, which the pages change
  std::vector<std::string> entries() const;

  // The map, fitted into a panel with its pixels still square
  void emitMap(std::vector<lve::UIRenderItem>& out, lve::LveTextRenderer& text) const;

  // One photograph, in the same panel, with what it is underneath it
  void emitPhotos(std::vector<lve::UIRenderItem>& out, lve::LveTextRenderer& text) const;

  // A picture fitted into the right panel, however tall it is, keeping inset
  // clear down each side. Hands back where it landed, as x, y, width, height
  glm::vec4 emitPicture(std::vector<lve::UIRenderItem>& out, lve::LveTextRenderer& text,
                        const lve::LveCanvas& picture, float top, float inset) const;

  bool open = false;
  bool options = false;
  bool showingMap = false;
  bool showingPhotos = false;
  std::size_t cursor = 0;

  bool escDown = false;
  bool upDown = false;
  bool downDown = false;
  bool pickDown = false;
  bool leftDown = false;
  bool rightDown = false;
};

}  // namespace petscop
