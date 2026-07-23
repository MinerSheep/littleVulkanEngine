#pragma once

#include "lve_scene.hpp"
#include "lve_camera.hpp"
#include "lve_model.hpp"
#include "skinned_model_component.hpp"
#include "keyboard_movement_component.hpp"

#include <memory>
#include <vector>

// Minimal scene that shows a single skinned (skeletal) glTF mesh in isolation, so
// the new skinning path can be seen without the clutter of the other scenes

// WASD walks the man around the XZ plane, left/right arrows turn him; the camera
// follows him third-person
class SkinnedDemoScene : public lve::LveScene {
 public:
  SkinnedDemoScene() {}
  void update(float dt) override;
  void onEvent(const lve::Event& event) override;
  void cleanup() override;

  void loadModels() override;
  void setupLights() override;

 private:
  lve::LveCamera camera{};

  // Third-person follow: camera sits at man.translation + cameraOffset and looks
  // at the man each frame (remember -Y is up, so a -Y offset lifts the camera)
  glm::vec3 cameraOffset{0.f, -2.f, -4.f};

  // The character is a GameObject carrying a TransformComponent (placement) + a
  // SkinnedModelComponent (owns the skinned model + clip playback). manSkin is
  // cached from addComponent so update() can switch clips without a lookup
  GameObject man = GameObject::createGameObject();
  SkinnedModelComponent* manSkin = nullptr;

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

  // Man's feet sit at world Y ~= manTranslation.y - manScale * yMin_model,
  // with yMin_model ~= -0.43 for man.glb. Tune if you swap the mesh
  float groundY = 1.10f;
  float groundHalfExtent = 8.f;  // quad spans [-1,1]; this scales it to +/-8
};
