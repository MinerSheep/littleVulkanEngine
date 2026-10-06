#pragma once

#include "lve_frame_info.hpp"
#include "lve_text.hpp"

#include <glm/glm.hpp>

#include <cstddef>
#include <string>
#include <vector>

namespace petscop {

// splits pages based on '|'
std::vector<std::string> splitPages(const std::string& text);

std::string wrapText(const std::string& text, int columns);

// Draw the panel
void emitPanel(std::vector<lve::UIRenderItem>& out,
               lve::LveTextRenderer& text,
               glm::vec2 topLeft,
               glm::vec2 size,
               glm::vec3 fill,
               glm::vec3 border,
               float borderThickness,
               float alpha);

// Hovers over the panel
void emitHoverPrompt(std::vector<lve::UIRenderItem>& out,
                     lve::LveTextRenderer& text,
                     glm::vec2 footNdc,
                     const std::string& key,
                     float dotHeight,
                     float alpha);

class DialogBox {
 public:
  void open(const std::string& source);
  void close();
  void advance();

  bool isOpen() const { return showing; }
  bool onLastPage() const { return page + 1 >= pages.size(); }

  void emit(std::vector<lve::UIRenderItem>& out, lve::LveTextRenderer& text) const;

 private:
  bool showing = false;
  std::size_t page = 0;
  std::vector<std::string> pages;

  float dotHeight = 0.014f;
  float left = -0.78f;
  float right = 0.78f;
  float bottom = 0.88f;
  float padding = 0.03f;
};

}  // namespace petscop
