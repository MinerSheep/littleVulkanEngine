#pragma once

#include "keyboard_movement_controller.hpp"  // Component, GameObject, controller
#include "lve_game_object.hpp"

// Walks its owning GameObject around the XZ plane from the keyboard. This is a thin
// bridge onto KeyboardMovementController::moveInPlaneXZ (the same code the fly-camera
// uses), run in planar mode: left/right arrows turn (yaw), WASD move relative to
// facing, and pitch / vertical are disabled so the character stays on the ground.
// Placement lives on the sibling TransformComponent, which moveInPlaneXZ mutates
struct KeyboardMovementComponent : public Component {
  KeyboardMovementController controller{};

  KeyboardMovementComponent() { controller.planarOnly = true; }

  void update(float dt, GameObject& obj) override;
};
