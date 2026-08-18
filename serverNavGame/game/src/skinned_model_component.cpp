#include "skinned_model_component.hpp"

#include <cmath>
#include <iostream>

void SkinnedModelComponent::setModel(std::unique_ptr<lve::LveSkinnedModel> newModel) {
  model = std::move(newModel);
  currentClip.clear();
  animTime = 0.f;
  if (!model) return;

  // Resolve the deform joints the headSpin fallback wiggles (head carries the whole
  // head weight); returns -1 for names absent from this rig
  headNode = model->findNode("head.x");
  neckNode = model->findNode("neck.x");
  spineNode = model->findNode("spine_03.x");

  // Start on the file's first clip, if it ships any
  if (model->animationCount() > 0) currentClip = model->animationName(0);
}

void SkinnedModelComponent::setClip(const std::string& name) {
  if (name == currentClip) return;  // already playing it
  currentClip = name;
  animTime = 0.f;  // restart the new clip from its first frame
  std::cout << "[SkinnedModelComponent] playing clip '" << currentClip << "'\n";
}

void SkinnedModelComponent::setClipIndex(int i) {
  if (!model || i < 0 || i >= model->animationCount()) return;
  setClip(model->animationName(i));
}

void SkinnedModelComponent::headSpin() {
  model->resetPose();

  // Look around (nod + turn) and a small breathing sway. Small amplitudes keep it
  // natural; rotateJoint is a safe no-op for any joint that wasn't found
  model->rotateJoint(headNode, {1.f, 0.f, 0.f}, 0.20f * std::sin(animTime * 1.3f));  // nod
  model->rotateJoint(headNode, {0.f, 1.f, 0.f}, 0.35f * std::sin(animTime * 0.7f));  // turn
  model->rotateJoint(neckNode, {1.f, 0.f, 0.f}, 0.08f * std::sin(animTime * 1.3f));
  model->rotateJoint(spineNode, {0.f, 0.f, 1.f}, 0.05f * std::sin(animTime * 0.6f));  // sway
  model->recomputePalette();
}

void SkinnedModelComponent::update(float dt, GameObject&) {
  // animTime advances every frame so the motion is smooth and real-time
  // However the re-pose is expensive and only runs ~kPoseHz times a second
  // (uploadPose still copies the palette each frame; that's a cheap memcpy)
  animTime += dt;
  poseTimer += dt;
  if (!model || poseTimer < 1.f / kPoseHz) return;
  poseTimer = 0.f;

  // Sample the active clip at the current clock (poseAnimation loops it and rebuilds
  // the palette). Falls back to the bind-pose wiggle when no clip is set
  if (!currentClip.empty())
    model->poseAnimation(currentClip, animTime);
  else
    headSpin();
}

void collectSkinned(GameObject& obj, std::vector<lve::SkinnedRenderItem>& items) {
  SkinnedModelComponent* skin = obj.getComponent<SkinnedModelComponent>();
  if (!skin || !skin->model) return;
  TransformComponent* t = obj.getComponent<TransformComponent>();
  if (!t) return;

  // Placement (with the Y-up -> Y-down flip baked into rotation.z) comes straight
  // from the TransformComponent, exactly like static models in levelscene
  items.push_back({t->mat4(), glm::mat4(t->normalMatrix()), skin->model.get(), skin->tint});
}
