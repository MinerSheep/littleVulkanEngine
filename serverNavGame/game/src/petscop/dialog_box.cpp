#include "petscop/dialog_box.hpp"

#include <lve_engine.hpp>

#include <algorithm>
#include <sstream>

namespace petscop {

namespace {

float screenAspect() {
  float aspect = lve::LveEngine::instance().getAspectRatio();
  if (aspect <= 0.f) aspect = 1.f;
  return aspect;
}

}  // namespace

std::vector<std::string> splitPages(const std::string& text) {
  std::vector<std::string> pages;
  std::string current;
  for (char c : text) {
    // if we encounter | then we split the page
    if (c == '|') {
      pages.push_back(current);
      current.clear();
    } else {
      // build the string
      current.push_back(c);
    }
  }
  pages.push_back(current);

  std::vector<std::string> kept;
  for (std::string& page : pages) {
    const std::size_t first = page.find_first_not_of(" \t");

    if (first == std::string::npos) continue;

    const std::size_t last = page.find_last_not_of(" \t");
    kept.push_back(page.substr(first, last - first + 1));
  }
  return kept;
}

std::string wrapText(const std::string& text, int columns) {
  if (columns < 1) columns = 1;

  std::istringstream words(text);
  std::string word;
  std::string out;
  int used = 0;

  while (words >> word) {
    if (used == 0) {
      out += word;
      used = static_cast<int>(word.size());
      continue;
    }
    if (used + 1 + static_cast<int>(word.size()) <= columns) {
      out += ' ';
      out += word;
      used += 1 + static_cast<int>(word.size());
    } else {
      out += '\n';
      out += word;
      used = static_cast<int>(word.size());
    }
  }
  return out;
}

void emitPanel(std::vector<lve::UIRenderItem>& out,
               lve::LveTextRenderer& text,
               glm::vec2 topLeft,
               glm::vec2 size,
               glm::vec3 fill,
               glm::vec3 border,
               float borderThickness,
               float alpha) {
  const float edgeY = borderThickness;
  const float edgeX = borderThickness / screenAspect();

  lve::UIRenderItem frame{};
  frame.transform = glm::mat2(size.x + 2.f * edgeX, 0.f, 0.f, size.y + 2.f * edgeY);
  frame.offset = {topLeft.x - edgeX, topLeft.y - edgeY};
  frame.color = border;
  frame.alpha = alpha;
  frame.model = text.quad();
  out.push_back(frame);

  lve::UIRenderItem body{};
  body.transform = glm::mat2(size.x, 0.f, 0.f, size.y);
  body.offset = topLeft;
  body.color = fill;
  body.alpha = alpha;
  body.model = text.quad();
  out.push_back(body);
}

// This draws on top of the panel, near the bottom center
void emitHoverPrompt(std::vector<lve::UIRenderItem>& out,
                     lve::LveTextRenderer& text,
                     glm::vec2 footNdc,
                     const std::string& key,
                     float dotHeight,
                     float alpha) {
  const float dotW = dotHeight / screenAspect();

  const float padX = 2.f * dotW;
  const float padY = 2.f * dotHeight;
  const float boxW = text.measureWidth(key, dotHeight) + 2.f * padX;
  const float boxH = 7.f * dotHeight + 2.f * padY;

  const glm::vec2 topLeft{footNdc.x - 0.5f * boxW, footNdc.y - boxH};

  emitPanel(out, text, topLeft, {boxW, boxH}, {0.04f, 0.04f, 0.07f}, {0.95f, 0.92f, 0.70f},
            0.006f, alpha);

  text.emit(out, key, {topLeft.x + padX, topLeft.y + padY}, dotHeight, {0.95f, 0.92f, 0.70f},
            alpha);
}

void DialogBox::open(const std::string& source) 
{
  pages = splitPages(source);
  page = 0;
  showing = !pages.empty();
}

void DialogBox::close() 
{
  showing = false;
  page = 0;
  pages.clear();
}

void DialogBox::advance() 
{
  if (!showing) return;
  if (onLastPage()) {
    close();
    return;
  }
  page++;
}

void DialogBox::emit(std::vector<lve::UIRenderItem>& out, lve::LveTextRenderer& text) const 
{
  if (!showing || page >= pages.size()) return;

  const float aspect = screenAspect();
  const float dotW = dotHeight / aspect;
  const float padX = padding / aspect;
  const float padY = padding;

  const float width = right - left;
  const float innerWidth = width - 2.f * padX;
  const int columns = std::max(8, static_cast<int>((innerWidth + dotW) / (6.f * dotW)));

  const std::string body = wrapText(pages[page], columns);
  const std::string hint = onLastPage() ? "(E) CLOSE" : "(E) MORE";

  const int lines = lve::LveTextRenderer::lineCount(body);
  const float blockHeight = ((lines + 1) * 8 - 1) * dotHeight;
  const float height = blockHeight + 2.f * padY;
  const float top = bottom - height;

  emitPanel(out, text, {left, top}, {width, height}, {0.03f, 0.03f, 0.06f},
            {0.75f, 0.72f, 0.60f}, 0.008f, 1.f);

  text.emit(out, body, {left + padX, top + padY}, dotHeight, {0.92f, 0.92f, 0.88f});

  const float hintX = right - padX - text.measureWidth(hint, dotHeight);
  const float hintY = top + padY + lines * 8 * dotHeight;
  text.emit(out, hint, {hintX, hintY}, dotHeight, {0.60f, 0.58f, 0.45f});
}

}  // namespace petscop
