#pragma once

#include "lve_scene.hpp"
#include "lve_camera.hpp"
#include "lve_model.hpp"
#include "gamecharacter.hpp"
#include "keyboard_movement_controller.hpp"

#include <memory>
#include <vector>

// Minimal scene that shows a single skinned (skeletal) glTF mesh in isolation, so
// the new skinning path can be seen without the clutter of the other scenes

// Fly the camera with WASD / QE / arrow keys
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

  // The animated character - Owns its skinned model + procedural idle; driven via
  // setModel / animate / render below (see gamecharacter.hpp)
  GameCharacter man;

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
