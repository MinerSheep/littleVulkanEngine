#pragma once

#include "lve_game_object.hpp"  // Component, GameObject, TransformComponent
#include "lve_window.hpp"       // GLFW key codes + GLFWwindow

// Only a pointer is kept, so the skinned model header stays in the .cpp
struct SkinnedModelComponent;

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

  bool enabled = true;

  // Yaw (radians about the vertical axis) that "forward" points along. Set this
  // each frame before updateComponents; the scene feeds it the camera's yaw so
  // movement follows where the camera looks
  float forwardYaw = 0.f;

  // Turn the body to face the movement direction while walking
  bool faceMoveDir = true;

  // Optional walk/idle swap
  // Hand it the character's skinned model plus which clip is the walk and which is
  // the stand-still, and it plays the right one as the keys go down and come up
  // Left unset, this component only moves the body and never touches the animation
  SkinnedModelComponent* animator = nullptr;
  int moveClipIndex = -1;
  int idleClipIndex = -1;

  // Turn the walk/idle swap on
  // The two numbers are positions in the model's own clip list, the same order
  // SkinnedModelComponent::setClipIndex takes
  void setAnimations(SkinnedModelComponent* skin, int moveClip, int idleClip) {
    animator = skin;
    moveClipIndex = moveClip;
    idleClipIndex = idleClip;
  }

  void update(float dt, GameObject& obj) override;
};
