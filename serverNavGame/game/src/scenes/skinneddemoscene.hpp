#pragma once

#include "lve_scene.hpp"
#include "lve_camera.hpp"
#include "lve_model.hpp"
#include "skinned_model_component.hpp"
#include "keyboard_movement_component.hpp"

#include <glm/gtc/constants.hpp>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

// Minimal scene that shows a single skinned (skeletal) glTF mesh in isolation, so
// the new skinning path can be seen without the clutter of the other scenes

// WASD walks the man around the XZ plane (relative to the camera); hold the left
// mouse button and drag to orbit the camera around him
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

  // Orbit-follow camera: the mouse spins it around the man. Camera position is the
  // man's location plus a spherical offset (yaw around, pitch up, cameraDistance
  // out); it always looks at the man (remember -Y is up)
  float cameraYaw = glm::pi<float>();  // start behind the man (his +Z is forward)
  float cameraPitch = 0.45f;           // elevation above the horizon, radians
  float cameraDistance = 4.5f;
  float mouseSensitivity = 0.005f;

  // Drag-to-orbit bookkeeping: cursor pos at the previous frame (for deltas) and
  // whether the left mouse button was held last frame
  glm::vec2 lastCursor{0.f};
  bool dragging = false;

  // The character is a GameObject carrying a TransformComponent (placement), a
  // SkinnedModelComponent (skinned model + clip playback) and a
  // KeyboardMovementComponent (WASD walk). The component pointers are cached from
  // addComponent so update() can drive them without a lookup
  GameObject man = GameObject::createGameObject();
  SkinnedModelComponent* manSkin = nullptr;
  KeyboardMovementComponent* manMover = nullptr;

  std::unique_ptr<lve::LveModel> groundModel;  // models/quad.obj (flat XZ plane)
  std::unique_ptr<lve::LveModel> cubeModel;    // models/colored_cube.obj
  std::unique_ptr<lve::LveModel> blockModel;   // models/cube.obj (white)

  struct StaticProp {
    lve::LveModel* model = nullptr;
    glm::vec3 translation{0.f};
    glm::vec3 scale{1.f};
    glm::vec3 rotation{0.f};  // euler radians (matches the editor's TRS order)
  };
  std::vector<StaticProp> props;

  // Reads scene_layout.txt and reproduces here, resolving each line's preset name
  // to a model (the name is the model's file basename, e.g. "grass" -> models/grass.obj) 
  // and appending a StaticProp to it
  // Models are owned by layoutModels, keyed by preset name so a mesh referenced 
  // by several objects is loaded only once
  void loadSceneLayout(const std::string& path);
  lve::LveModel* modelForPreset(const std::string& name);
  std::unordered_map<std::string, std::unique_ptr<lve::LveModel>> layoutModels;

  // Man's feet sit at world Y ~= manTranslation.y - manScale * yMin_model,
  // with yMin_model ~= -0.43 for man.glb. Tune if you swap the mesh
  float groundY = 0.5f;
  float groundHalfExtent = 8.f;  // quad spans [-1,1]; this scales it to +/-8
};
