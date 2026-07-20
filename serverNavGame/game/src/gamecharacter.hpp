#pragma once

#include "lve_frame_info.hpp"     // lve::SkinnedRenderItem
#include "lve_skinned_model.hpp"  // lve::LveSkinnedModel

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#include <memory>
#include <vector>

// A skinned, animated character with a LveSkinnedModel, drives 
// a small procedural idle animation, and emits one SkinnedRenderItem per frame 

// A scene just holds GameCharacters and calls
// setModel / animate / render on each.
class GameCharacter {
 public:
  // Take ownership of a skinned model and resolve the deform joints we animate
  // Safe to call with nullptr (leaves the character empty)
  void setModel(std::unique_ptr<lve::LveSkinnedModel> model);

  // Advance and apply the procedural pose. Throttled to kPoseHz so the (344-joint)
  // re-pose doesn't run at full frame rate; animTime still advances every frame so
  // the motion stays smooth. No-op until a model is set
  void animate(float dt);

  // Append this character's skinned draw (with the Y-up -> Y-down flip) to the
  // scene's render list. No-op until a model is set
  void render(std::vector<lve::SkinnedRenderItem>& items) const;

  bool hasModel() const { return model != nullptr; }

  // Placement. glTF is Y-up but this engine is Y-down, so render() flips the model
  // 180 deg about Z; tune these to move/scale the character
  glm::vec3 translation{0.f, 0.71f, 0.f};
  float scale = 0.9f;

 private:
  std::unique_ptr<lve::LveSkinnedModel> model;

  // Deform joints wiggled by animate(); -1 means "not present in this rig", which
  // rotateJoint treats as a safe no-op
  int headNode = -1;
  int neckNode = -1;
  int spineNode = -1;

  // Procedural-idle clock + re-pose throttle (see animate())
  float animTime = 0.f;
  float poseTimer = 0.f;
  static constexpr float kPoseHz = 45.f;
};
