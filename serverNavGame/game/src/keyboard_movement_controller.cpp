#include "keyboard_movement_controller.hpp"

// std
#include <limits>

void KeyboardMovementController::moveInPlaneXZ(
    GLFWwindow* window, float dt, GameObject& gameObject) {
  glm::vec3 rotate{0};

  // During the last Window Event Buffer, lookRight key was in a pressed state
  if (glfwGetKey(window, keys.lookRight) == GLFW_PRESS) rotate.y += 1.f;
  if (glfwGetKey(window, keys.lookLeft) == GLFW_PRESS) rotate.y -= 1.f;
  if (!planarOnly && glfwGetKey(window, keys.lookUp) == GLFW_PRESS) rotate.x += 1.f;
  if (!planarOnly && glfwGetKey(window, keys.lookDown) == GLFW_PRESS) rotate.x -= 1.f;

  TransformComponent* transform = gameObject.getComponent<TransformComponent>();

  // Can't normalize a zero vector, this equation checks if rotate has non zero
  // Compare with epsilon to avoid floating point errors
  if (glm::dot(rotate, rotate) > std::numeric_limits<float>::epsilon()) {
    // uses frame rate scaled with lookSpeed to move
    transform->rotation += lookSpeed * dt * glm::normalize(rotate);
  }

  // limit pitch values between about +/- 85ish degrees
  transform->rotation.x = glm::clamp(transform->rotation.x, -1.5f, 1.5f);
  // mod 2pi prevents rotation overflow
  transform->rotation.y = glm::mod(transform->rotation.y, glm::two_pi<float>());

  float yaw = transform->rotation.y;
  const glm::vec3 forwardDir{sin(yaw), 0.f, cos(yaw)};
  // this is taking the normal vector on x z plane
  const glm::vec3 rightDir{forwardDir.z, 0.f, -forwardDir.x};
  const glm::vec3 upDir{0.f, -1.f, 0.f};

  glm::vec3 moveDir{0.f};
  if (glfwGetKey(window, keys.moveForward) == GLFW_PRESS) moveDir += forwardDir;
  if (glfwGetKey(window, keys.moveBackward) == GLFW_PRESS) moveDir -= forwardDir;
  if (glfwGetKey(window, keys.moveRight) == GLFW_PRESS) moveDir += rightDir;
  if (glfwGetKey(window, keys.moveLeft) == GLFW_PRESS) moveDir -= rightDir;
  if (!planarOnly && glfwGetKey(window, keys.moveUp) == GLFW_PRESS) moveDir += upDir;
  if (!planarOnly && glfwGetKey(window, keys.moveDown) == GLFW_PRESS) moveDir -= upDir;

  if (glm::dot(moveDir, moveDir) > std::numeric_limits<float>::epsilon()) {
    transform->translation += moveSpeed * dt * glm::normalize(moveDir);
  }
}