#pragma once

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

 private:
  // The buttons as they stand, which the options page changes
  std::vector<std::string> entries() const;

  bool open = false;
  bool options = false;
  std::size_t cursor = 0;

  bool escDown = false;
  bool upDown = false;
  bool downDown = false;
  bool pickDown = false;
};

}  // namespace petscop
