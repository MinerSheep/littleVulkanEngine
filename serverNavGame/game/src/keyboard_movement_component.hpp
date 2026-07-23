#pragma once

#include "lve_game_object.hpp"  // Component, GameObject, TransformComponent
#include "lve_window.hpp"       // GLFW key codes + GLFWwindow

// Walks its owning GameObject around the XZ plane with WASD, relative to a forward
// direction the owner supplies each frame (forwardYaw from Camera). 

// Placement lives on the sibling TransformComponent, which
// this mutates (its Y-up -> Y-down flip on rotation.z is left untouched)
struct KeyboardMovementComponent : public Component {
  struct KeyMappings {
    int moveLeft = GLFW_KEY_A;
    int moveRight = GLFW_KEY_D;
    int moveForward = GLFW_KEY_W;
    int moveBackward = GLFW_KEY_S;
  };

  KeyMappings keys{};
  float moveSpeed{3.f};

  // Yaw (radians about the vertical axis) that "forward" points along. Set this
  // each frame before updateComponents; the scene feeds it the camera's yaw so
  // movement follows where the camera looks
  float forwardYaw = 0.f;

  // Turn the body to face the movement direction while walking
  bool faceMoveDir = true;

  void update(float dt, GameObject& obj) override;
};
