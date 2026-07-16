#pragma once

#include "lve_scene.hpp"
#include "lve_camera.hpp"
#include "lve_skinned_model.hpp"
#include "keyboard_movement_controller.hpp"

#include <memory>

// Minimal scene that shows a single skinned (skeletal) glTF mesh in isolation, so
// the new skinning path can be seen without the clutter of the other scenes.
// Fly the camera with WASD / QE / arrow keys.
class SkinnedDemoScene : public lve::LveScene {
 public:
  SkinnedDemoScene() {}
  void update(float dt) override;
  void cleanup() override;

  void loadModels() override;
  void setupLights() override;

 private:
  lve::LveCamera camera{};
  GameObject* viewerObject = nullptr;
  KeyboardMovementController cameraController{};

  std::unique_ptr<lve::LveSkinnedModel> man;

  // glTF is Y-up; this engine is Y-down, so the model is flipped 180 deg about Z.
  // Tune these to reframe the character (AABB is printed at load time).
  glm::vec3 manTranslation{0.f, 0.71f, 0.f};
  float manScale = 0.9f;
};
