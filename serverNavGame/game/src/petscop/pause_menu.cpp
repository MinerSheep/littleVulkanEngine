#include "petscop/pause_menu.hpp"

#include "petscop/dialog_box.hpp"  // emitPanel, wrapText
#include "petscop/game_state.hpp"

#include <lve_engine.hpp>  // the window's shape
#include <lve_window.hpp>  // GLFW keys

#include <algorithm>

namespace petscop {

namespace {

// The two panels, and the room a line of text has inside one
const glm::vec2 panelSize{0.64f, 0.80f};
const glm::vec2 leftPanel{-0.70f, -0.40f};
const glm::vec2 rightPanel{0.06f, -0.40f};
const float padX = 0.06f;
const float padY = 0.10f;
const float textRoom = panelSize.x - 2.f * padX;
const float pocketTop = 0.28f;
const float footPad = 0.04f;

// The text standing in the panels
const float entryDot = 0.018f;
const float itemDot = 0.014f;
const float rowStep = 0.16f;
const glm::vec3 lit{0.95f, 0.92f, 0.70f};
const glm::vec3 dim{0.58f, 0.56f, 0.46f};

// A letter keeps its height and widens as the window narrows
float screenAspect() {
  const float aspect = lve::LveEngine::instance().getAspectRatio();
  return aspect > 0.f ? aspect : 1.f;
}

// How many letters fit across a panel at this size
int columnsFor(float dot) {
  const float dotW = dot / screenAspect();
  return std::max(4, static_cast<int>((textRoom + dotW) / (6.f * dotW)));
}

// How far a line has to shrink to stay inside the panel
float fitScale(lve::LveTextRenderer& text, const std::string& line, float dot) {
  const float width = text.measureWidth(line, dot);
  return width > textRoom ? textRoom / width : 1.f;
}

// The same for a block of lines, all of them shrinking to the longest
float fitScale(lve::LveTextRenderer& text, const std::vector<std::string>& lines, float dot) {
  float scale = 1.f;
  for (const std::string& line : lines) scale = std::min(scale, fitScale(text, line, dot));
  return scale;
}

// Draws one line, shrunk if it would run past the panel edge
void emitFitted(std::vector<lve::UIRenderItem>& out, lve::LveTextRenderer& text,
                const std::string& line, glm::vec2 at, float dot, glm::vec3 color) {
  text.emit(out, line, at, dot * fitScale(text, line, dot), color);
}

// An item name the way the menu prints it
std::string shout(const std::string& name) {
  std::string out;
  for (char letter : name) {
    if (letter == '_') out += ' ';
    else if (letter >= 'a' && letter <= 'z') out += static_cast<char>(letter - 'a' + 'A');
    else out += letter;
  }
  return out;
}

// One line for each thing he is carrying
std::vector<std::string> pocketLines(const GameState& state) {
  std::vector<std::string> lines;
  for (const std::pair<const std::string, int>& held : state.items) {
    // The counters the save keeps in his pockets are not his to look at
    if (held.second <= 0 || held.first.empty() || held.first[0] == '@') continue;

    lines.push_back(shout(held.first) + " X" + std::to_string(held.second));
  }
  return lines;
}

}  // namespace

std::vector<std::string> PauseMenu::entries() const {
  if (options || showingMap) return {"BACK"};
  if (stripped) return {"LEAVE"};
  if (hasMap) return {"RESUME", "MAP", "OPTIONS", "EXIT"};
  return {"RESUME", "OPTIONS", "EXIT"};
}

void PauseMenu::close() {
  open = false;
  options = false;
  showingMap = false;
  cursor = 0;
}

PauseMenu::Choice PauseMenu::update(GLFWwindow* window) {
  const bool esc = glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS;
  const bool pressedEsc = esc && !escDown;
  escDown = esc;

  if (pressedEsc) {
    if (!open) {
      open = true;
      options = false;
      cursor = 0;
      return Choice::None;
    }
    // Backing out of a page is not the same as putting the menu away
    if (showingMap) {
      showingMap = false;
      cursor = 0;
      return Choice::None;
    }
    if (options) {
      options = false;
      cursor = 0;
      return Choice::Back;
    }
    close();
    return Choice::Resume;
  }
  if (!open) return Choice::None;

  const std::vector<std::string> list = entries();

  const bool up = glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS ||
                  glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS;
  const bool down = glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS ||
                    glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS;
  if (up && !upDown && cursor > 0) cursor--;
  if (down && !downDown && cursor + 1 < list.size()) cursor++;
  upDown = up;
  downDown = down;

  const bool pick = glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS ||
                    glfwGetKey(window, GLFW_KEY_ENTER) == GLFW_PRESS;
  const bool pressed = pick && !pickDown;
  pickDown = pick;
  if (!pressed || cursor >= list.size()) return Choice::None;

  const std::string& chosen = list[cursor];
  if (chosen == "RESUME") {
    close();
    return Choice::Resume;
  }
  if (chosen == "OPTIONS") {
    options = true;
    cursor = 0;
    return Choice::Options;
  }
  if (chosen == "MAP") {
    showingMap = true;
    cursor = 0;
    return Choice::None;
  }
  if (chosen == "BACK") {
    // Only the settings count as having been opened and shut again
    const bool wasOptions = options;
    options = false;
    showingMap = false;
    cursor = 0;
    return wasOptions ? Choice::Back : Choice::None;
  }
  return Choice::Leave;
}

// The plan of the house, as big as it will go inside the right panel
void PauseMenu::emitMap(std::vector<lve::UIRenderItem>& out, lve::LveTextRenderer& text) const {
  const float shelf = rightPanel.x + padX;
  emitFitted(out, text, "THE HOUSE", {shelf, rightPanel.y + padY}, entryDot, lit);

  if (mapPicture == nullptr || mapPicture->empty()) {
    emitFitted(out, text, "IT IS BLANK", {shelf, rightPanel.y + pocketTop}, itemDot, dim);
    return;
  }

  const glm::vec2 room{panelSize.x - 2.f * padX, panelSize.y - pocketTop - footPad};

  // A pixel only comes out square if the box leans the way the window does
  const float wanted = mapPicture->aspect() / screenAspect();
  glm::vec2 size{room.y * wanted, room.y};
  if (size.x > room.x) size = {room.x, room.x / wanted};

  const glm::vec2 at{shelf + (room.x - size.x) * 0.5f,
                     rightPanel.y + pocketTop + (room.y - size.y) * 0.5f};
  mapPicture->emit(out, text.quad(), at, size, 1.f);
}

void PauseMenu::emit(std::vector<lve::UIRenderItem>& out, lve::LveTextRenderer& text,
                     const GameState& state) const {
  if (!open) return;

  // The room is still there behind it, most of the way down
  out.push_back(
      {glm::mat2(2.f, 0.f, 0.f, 2.f), glm::vec2(-1.f), glm::vec3(0.f), 0.72f, text.quad()});

  const glm::vec3 fill{0.03f, 0.03f, 0.06f};
  const glm::vec3 border{0.75f, 0.72f, 0.60f};
  emitPanel(out, text, leftPanel, panelSize, fill, border, 0.008f, 1.f);
  emitPanel(out, text, rightPanel, panelSize, fill, border, 0.008f, 1.f);

  const float left = leftPanel.x + padX;
  float y = leftPanel.y + padY;

  if (options) {
    const std::string note = wrapText("NOTHING HERE CAN BE CHANGED", columnsFor(itemDot));
    emitFitted(out, text, note, {left, y}, itemDot, dim);
    // The buttons pick up a blank line under the note
    y += (lve::LveTextRenderer::lineCount(note) + 1) * 8.f * itemDot;
  }

  // The buttons shrink together, keeping the longest one inside the panel
  const std::vector<std::string> list = entries();
  std::vector<std::string> buttons;
  for (std::size_t i = 0; i < list.size(); i++)
    buttons.push_back((i == cursor ? "> " : "  ") + list[i]);

  const float buttonDot = entryDot * fitScale(text, buttons, entryDot);
  for (std::size_t i = 0; i < buttons.size(); i++) {
    text.emit(out, buttons[i], {left, y}, buttonDot, i == cursor ? lit : dim);
    y += rowStep * (buttonDot / entryDot);
  }

  // The map takes the right panel over while he is reading it
  if (showingMap) {
    emitMap(out, text);
    return;
  }

  const float shelf = rightPanel.x + padX;
  emitFitted(out, text, "POCKETS", {shelf, rightPanel.y + padY}, entryDot, lit);

  const std::vector<std::string> pockets = pocketLines(state);
  const float pocketDot = itemDot * fitScale(text, pockets, itemDot);
  const float step = 8.f * pocketDot;
  float row = rightPanel.y + pocketTop;

  if (pockets.empty()) {
    emitFitted(out, text, "NOTHING", {shelf, row}, pocketDot, dim);
    return;
  }

  // The last row that still stands on the panel, the rest counted off the end
  const float lastRow = rightPanel.y + panelSize.y - footPad - 7.f * pocketDot;
  const std::size_t fits = static_cast<std::size_t>(std::max(1.f, (lastRow - row) / step + 1.f));
  const bool spills = pockets.size() > fits;
  const std::size_t shown = spills ? fits - 1 : pockets.size();

  for (std::size_t i = 0; i < shown; i++) {
    text.emit(out, pockets[i], {shelf, row}, pocketDot, dim);
    row += step;
  }
  if (spills)
    emitFitted(out, text, "+" + std::to_string(pockets.size() - shown) + " MORE", {shelf, row},
               pocketDot, dim);
}

}  // namespace petscop
