#pragma once

#include "lve_window.hpp"  // GLFW key codes + GLFWwindow

#include <glm/glm.hpp>

namespace lve {

// Classic fly-camera controls: WASD moves in the XZ plane relative to the current
// yaw, Q/E move down/up, and the arrow keys look around
//
// Operates directly on a translation + rotation (euler, radians) pair so it can drive a transform
class KeyboardMovementController {
 public:
  struct KeyMappings {
    int moveLeft = GLFW_KEY_A;
    int moveRight = GLFW_KEY_D;
    int moveForward = GLFW_KEY_W;
    int moveBackward = GLFW_KEY_S;
    int moveUp = GLFW_KEY_E;
    int moveDown = GLFW_KEY_Q;
    int lookLeft = GLFW_KEY_LEFT;
    int lookRight = GLFW_KEY_RIGHT;
    int lookUp = GLFW_KEY_UP;
    int lookDown = GLFW_KEY_DOWN;
  };

  void moveInPlaneXZ(GLFWwindow* window, float dt, glm::vec3& translation, glm::vec3& rotation);

  KeyMappings keys{};
  float moveSpeed{3.f};
  float lookSpeed{1.5f};
};

}  // namespace lve
