#pragma once

#include "lve_frame_info.hpp"
#include "lve_game_object.hpp"
#include "skinned_model_component.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

#include <vector>

namespace petscop {

// Somebody the haunting stands in a room, wearing the player's own model
//
// He is put somewhere with place() every frame he is meant to be seen, ticked
// once the events have all had their say, and drawn by the scene alongside the
// player. A frame that never places him draws nothing
class Figure {
 public:
  // The clips the player walks and stands on, out of statue.glb
  static constexpr int kWalkClip = 3;
  static constexpr int kIdleClip = 0;

  // The player's scale, and what the mesh measures standing at it
  static constexpr float kScale = 0.9f;
  static constexpr float kTall = 1.757f;

  // Where his eyes sit above his feet
  static constexpr float kEyeLift = 1.62f;

  // Grey, never the blue the player is walking around in
  glm::vec3 tint{0.42f, 0.40f, 0.46f};

  // Stands him somewhere for this frame, on his feet or mid stride
  void place(const glm::vec3& at, float yaw, bool walking) {
    wake();
    if (!skin || !skin->model) return;

    xform->translation = at;
    // Y up mesh in a Y down world, the same flip the player is stood with
    xform->rotation = glm::vec3(0.f, yaw, glm::pi<float>());
    skin->tint = tint;
    skin->setClipIndex(walking ? kWalkClip : kIdleClip);
    shown = true;
  }

  // Nothing of him is drawn until he is placed again
  void hide() { shown = false; }

  // Runs the clip on whoever was stood in the room this frame
  void tick(float dt) {
    if (shown) body.updateComponents(dt);
  }

  // Hands his draw to the scene, next to the player's
  void collect(std::vector<lve::SkinnedRenderItem>& items) {
    if (shown) collectSkinned(body, items);
  }

 private:
  // The mesh is loaded the first time somebody has to be stood in a room
  void wake() {
    if (skin) return;

    xform = body.addComponent<TransformComponent>();
    xform->scale = glm::vec3(kScale);

    skin = body.addComponent<SkinnedModelComponent>();
    skin->setModel(lve::LveSkinnedModel::createModelFromFile("models/statue.glb"));
  }

  GameObject body = GameObject::createGameObject();
  TransformComponent* xform = nullptr;
  SkinnedModelComponent* skin = nullptr;
  bool shown = false;
};

}  // namespace petscop
