#include "petscop/interactions.hpp"

#include "collider_component.hpp"
#include "keyboard_movement_component.hpp"
#include "petscop/game_state.hpp"

#include <lve_audio.hpp>
#include <lve_engine.hpp>

namespace petscop {

void InteractionRunner::bind(std::vector<Prop>* propList, DialogBox* dialogBox,
                             GameState* gameState) {
  props = propList;
  dialog = dialogBox;
  state = gameState;
}

void InteractionRunner::reset() {
  script = Script{};
  nearest = -1;
  if (dialog) dialog->close();
}

bool InteractionRunner::applyAction(const MapAction& action, int owner, bool backwards) {
  // DIALOGUE
  if (action.kind == ActionKind::Say) {
    if (!dialog) return false;
    dialog->open(action.text);
    return dialog->isOpen();
  }

  // SFX
  if (action.kind == ActionKind::Sound) {
    lve::LveAudio::instance().play(action.text);
    return false;
  }

  // POCKET
  if (action.kind == ActionKind::Give || action.kind == ActionKind::Take) {
    const int move = action.kind == ActionKind::Give ? action.count : -action.count;
    if (state) state->addItem(action.text, move);
    return false;
  }

  // MEMORY
  if (action.kind == ActionKind::Flag || action.kind == ActionKind::Unflag) {
    if (state) state->setFlag(action.text, action.kind == ActionKind::Flag);
    return false;
  }

  // An empty target is the prop you pressed E on, anything else is by name
  const int index = action.target.empty() ? owner : findProp(*props, action.target);
  if (index < 0 || index >= static_cast<int>(props->size())) return false;

  Prop& prop = (*props)[index];

  // APPEAR
  if (action.kind == ActionKind::Show) {
    prop.disappeared = false;
    prop.collider.enabled = true;
    return false;
  }

  // DISAPPEAR
  if (action.kind == ActionKind::Hide) {
    prop.disappeared = true;
    prop.collider.enabled = false;
    return false;
  }

  // Reads where the prop is heading, not where it sits mid move
  glm::vec3 translation = prop.motion.active ? prop.motion.toTranslation : prop.translation;
  glm::vec3 rotation = prop.motion.active ? prop.motion.toRotation : prop.rotation;
  glm::vec3 scale = prop.motion.active ? prop.motion.toScale : prop.scale;

  // A toggle measures from where the map stood the prop
  if (action.toggle) {
    translation = prop.restTranslation;
    rotation = prop.restRotation;
    scale = prop.restScale;
  }

  if (!action.toggle || !backwards) {
    if (action.kind == ActionKind::Move) translation += action.amount;
    else if (action.kind == ActionKind::Rotate) rotation += action.amount;
    else scale *= action.amount;
  }

  prop.startMotion(translation, rotation, scale, action.seconds);
  return false;
}

// A prop whose lines are all gated off draws no floating E
bool InteractionRunner::hasSomethingToDo(const Prop& prop) const {
  if (!state) return prop.interactable();

  for (const MapAction& action : prop.actions) {
    if (state->allows(action)) return true;
  }
  return false;
}

void InteractionRunner::runScript() {
  while (script.running) {
    // check the prop index is within bounds
    if (script.prop < 0 || script.prop >= static_cast<int>(props->size())) break;

    Prop& owner = (*props)[script.prop];
    if (script.next >= owner.actions.size()) break;

    const std::size_t step = script.next++;
    const MapAction action = owner.actions[step];

    // A 'when' the save does not satisfy skips the line, toggle and all
    if (state && !state->allows(action)) continue;

    // BACKWARDS is a check for an object returning to its rest state
    bool backwards = false;
    if (action.toggle && step < owner.flipped.size()) {
      backwards = owner.flipped[step] != 0;
      owner.flipped[step] = backwards ? 0 : 1;
    }

    if (applyAction(action, script.prop, backwards)) return;
  }

  script = Script{};
}

void InteractionRunner::update(ColliderComponent* playerCollider,
                               KeyboardMovementComponent* playerMover) {
  if (!props || !dialog) return;

  GLFWwindow* window = lve::LveEngine::instance().getGLFWWindow();

  const bool actionDown = glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS;
  const bool actionPressed = actionDown && !actionPrevDown;
  actionPrevDown = actionDown;

  nearest = -1;

  // while a script is running, ALL interactions are blocked
  if (script.running) {
    if (dialog->isOpen()) {
      if (actionPressed) dialog->advance();

      if (dialog->isOpen()) {
        if (playerMover) playerMover->enabled = false;
        return;
      }
    }

    runScript();

    if (playerMover) playerMover->enabled = !dialog->isOpen();
    return;
  }

  if (playerCollider) {
    const ColliderComponent::Aabb& me = playerCollider->worldBox();

    float best = range;

    for (std::size_t i = 0; i < props->size(); i++) {
      const Prop& prop = (*props)[i];
      if (!prop.interactable()) continue;
      if (prop.disappeared) continue;
      if (!hasSomethingToDo(prop)) continue;

      const float gap = boxGap(me, prop.collider.worldBox());

      if (gap > best) continue;

      best = gap;
      nearest = static_cast<int>(i);
    }
  }

  if (actionPressed && nearest >= 0) {
    script.running = true;
    script.prop = nearest;
    script.next = 0;
    nearest = -1;
    runScript();
  }

  if (playerMover) playerMover->enabled = !dialog->isOpen();
}

}  // namespace petscop
