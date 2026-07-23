#include "keyboard_movement_component.hpp"

#include "lve_engine.hpp"

void KeyboardMovementComponent::update(float dt, GameObject& obj) {
  // The window lives on the engine singleton, the same source the scenes read
  GLFWwindow* window = lve::LveEngine::instance().getGLFWWindow();
  controller.moveInPlaneXZ(window, dt, obj);
}
