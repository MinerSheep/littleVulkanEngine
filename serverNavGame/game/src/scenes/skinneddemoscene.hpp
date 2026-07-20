#pragma once

#include "lve_scene.hpp"
#include "lve_camera.hpp"
#include "lve_model.hpp"
#include "lve_skinned_model.hpp"
#include "keyboard_movement_controller.hpp"

#include <memory>
#include <vector>

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

  // Gently wiggles a few deform joints so the skinning is visible.
  // Node indices are resolved once in loadModels; -1 means "not found" and animates nothing.
  float animTime = 0.f;
  int headNode = -1;
  int neckNode = -1;
  int spineNode = -1;

  // --- Re-pose throttle (A3) ------------------------------------------------
  // Re-posing the 344-joint skeleton (resetPose + rotateJoint + recomputePalette)
  // is CPU work; there is no need to redo it at full frame rate. We accumulate
  // real time and only re-pose at kPoseHz, decoupling skeleton cost from FPS.
  // animTime still advances every frame so the motion stays smooth/real-time.
  float poseTimer = 0.f;
  static constexpr float kPoseHz = 45.f;

  // --- Static world (Phase B) ----------------------------------------------
  // The man stands on a ground plane surrounded by a few low-poly blocks, drawn
  // through the ordinary LveModel + renderItems + SimpleRenderSystem path (no
  // skinning). Models are owned here; transforms are rebuilt into renderItems
  // each frame in update().
  std::unique_ptr<lve::LveModel> groundModel;  // models/quad.obj (flat XZ plane)
  std::unique_ptr<lve::LveModel> cubeModel;    // models/colored_cube.obj
  std::unique_ptr<lve::LveModel> blockModel;   // models/cube.obj (white)

  struct StaticProp {
    lve::LveModel* model = nullptr;
    glm::vec3 translation{0.f};
    glm::vec3 scale{1.f};
    float yaw = 0.f;  // rotation about the vertical (Y) axis, radians
  };
  std::vector<StaticProp> props;

  // World Y of the ground plane. Derived from the man's transform so his feet
  // rest on it: feet sit at world Y ~= manTranslation.y - manScale * yMin_model,
  // with yMin_model ~= -0.43 for man.glb. Tune if you swap the mesh.
  float groundY = 1.10f;
  float groundHalfExtent = 8.f;  // quad spans [-1,1]; this scales it to +/-8
};
