#include "keyboard_movement_component.hpp"

#include "lve_engine.hpp"
#include "skinned_model_component.hpp"

#include <cmath>
#include <limits>

namespace {
// Put the character on one of its clips
// Quiet no-op with no skinned model, or when that clip was never hooked up
// setClipIndex ignores a clip that is already playing, so this is safe every frame
void playClip(SkinnedModelComponent* skin, int clipIndex) {
  if (!skin || clipIndex < 0) return;
  skin->setClipIndex(clipIndex);
}
}  // namespace

void KeyboardMovementComponent::update(float dt, GameObject& obj) {
  // The window lives on the engine singleton, the same source the scenes read
  GLFWwindow* window = lve::LveEngine::instance().getGLFWWindow();
  TransformComponent* t = obj.getComponent<TransformComponent>();
  if (!t) return;

  if (!enabled) {
    playClip(animator, idleClipIndex);
    return;
  }

  // Move basis: forward is forwardYaw about the vertical axis (the scene aims it
  // down the camera), right is its perpendicular on the XZ plane
  const glm::vec3 forwardDir{std::sin(forwardYaw), 0.f, std::cos(forwardYaw)};  // forward ^
  const glm::vec3 rightDir{forwardDir.z, 0.f, -forwardDir.x};                   // right ->

  glm::vec3 moveDir{0.f};
  if (glfwGetKey(window, keys.moveForward) == GLFW_PRESS) moveDir += forwardDir;
  if (glfwGetKey(window, keys.moveBackward) == GLFW_PRESS) moveDir -= forwardDir;
  if (glfwGetKey(window, keys.moveRight) == GLFW_PRESS) moveDir += rightDir;
  if (glfwGetKey(window, keys.moveLeft) == GLFW_PRESS) moveDir -= rightDir;

  // Can't normalize a zero vector: bail if no move key is held
  // Standing still, so drop back to the idle clip
  if (glm::dot(moveDir, moveDir) < std::numeric_limits<float>::epsilon()) {
    playClip(animator, idleClipIndex);
    return;
  }

  // Walking, so play the walk clip
  playClip(animator, moveClipIndex);

  moveDir = glm::normalize(moveDir);
  t->translation += moveSpeed * dt * moveDir;

  // Face where we're walking
  // rotation.y is the yaw whose forward is {sin,0,cos}
  // atan2(x,z) points the body straight down moveDir
  if (faceMoveDir) t->rotation.y = std::atan2(moveDir.x, moveDir.z);
}
