#pragma once

#include "lve_game_object.hpp"    // Component, GameObject, TransformComponent
#include "lve_frame_info.hpp"     // lve::SkinnedRenderItem
#include "lve_skinned_model.hpp"  // lve::LveSkinnedModel

#include <memory>
#include <string>
#include <vector>

// A skinned, animated mesh attached to a GameObject. Owns its LveSkinnedModel and
// plays one of the model's baked glTF clips (or a procedural head-wiggle fallback
// for clip-less rigs). Placement lives on the sibling TransformComponent, so this
// component only animates the skeleton; the scene emits the draw via collectSkinned
struct SkinnedModelComponent : public Component {
  std::unique_ptr<lve::LveSkinnedModel> model;

  // Which clip is playing, and its playback clock. animTime feeds
  // LveSkinnedModel::poseAnimation, which loops it over the clip length
  std::string currentClip;
  float animTime = 0.f;
  float poseTimer = 0.f;
  static constexpr float kPoseHz = 45.f;

  // Deform joints for the clip-less headSpin fallback; -1 means "not present in this
  // rig", which rotateJoint treats as a safe no-op
  int headNode = -1;
  int neckNode = -1;
  int spineNode = -1;

  // Take ownership of a skinned model and default to its first clip, if any
  // Safe to call with nullptr (leaves the component empty)
  void setModel(std::unique_ptr<lve::LveSkinnedModel> model);

  // Switch which clip is playing. Restarts the clock so the new clip begins at its
  // first frame; a no-op if the same clip is already active. setClipIndex selects by
  // the model's clip order (see LveSkinnedModel::animationName)
  void setClip(const std::string& name);
  void setClipIndex(int i);
  int clipCount() const { return model ? model->animationCount() : 0; }
  const std::string& clipName() const { return currentClip; }

  // Advance the active clip's clock and re-pose the skeleton. Throttled to kPoseHz so
  // the re-pose doesn't run at full frame rate; animTime still advances every frame so
  // the motion stays smooth. No-op until a model is set
  void update(float dt, GameObject& obj) override;

  // Clip-less fallback: a small procedural nod/turn/sway idle
  void headSpin();
};

// Append the skinned draw for a GameObject that carries a SkinnedModelComponent + a
// TransformComponent. Placement (including the Y-up -> Y-down flip on rotation.z)
// comes from the transform, mirroring how static models are gathered in levelscene
void collectSkinned(GameObject& obj, std::vector<lve::SkinnedRenderItem>& items);
