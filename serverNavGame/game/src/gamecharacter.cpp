
#include "gamecharacter.hpp"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <cmath>
#include <iostream>

void GameCharacter::setModel(std::unique_ptr<lve::LveSkinnedModel> newModel) {
  model = std::move(newModel);
  currentClip.clear();
  animTime = 0.f;
  if (!model) return;

  // Resolve the deform joints we wiggle (head carries the whole head weight)
  // returns -1 for names absent from this rig
  headNode = model->findNode("head.x");
  neckNode = model->findNode("neck.x");
  spineNode = model->findNode("spine_03.x");

  // Start on the file's first clip, if it ships any
  if (model->animationCount() > 0) currentClip = model->animationName(0);
}

void GameCharacter::setClip(const std::string& name) {
  if (name == currentClip) return;  // already playing it
  currentClip = name;
  animTime = 0.f;  // restart the new clip from its first frame
  std::cout << "[GameCharacter] playing clip '" << currentClip << "'\n";
}

void GameCharacter::setClipIndex(int i) {
  if (!model || i < 0 || i >= model->animationCount()) return;
  setClip(model->animationName(i));
}

void GameCharacter::headSpin()
{
  model->resetPose();
  
  // Look around (nod + turn) and a small breathing sway. Small amplitudes keep it
  // natural; rotateJoint is a safe no-op for any joint that wasn't found
  model->rotateJoint(headNode, {1.f, 0.f, 0.f}, 0.20f * std::sin(animTime * 1.3f));  // nod
  model->rotateJoint(headNode, {0.f, 1.f, 0.f}, 0.35f * std::sin(animTime * 0.7f));  // turn
  model->rotateJoint(neckNode, {1.f, 0.f, 0.f}, 0.08f * std::sin(animTime * 1.3f));
  model->rotateJoint(spineNode, {0.f, 0.f, 1.f}, 0.05f * std::sin(animTime * 0.6f));  // sway
  model->recomputePalette();
}

void GameCharacter::animate(float dt) {
  // animTime advances every frame so the motion is smooth and real-time
  // However repose is expensive and only runs ~kPoseHz times a second.
  // (uploadPose still copies the palette each frame; that's a cheap memcpy)
  animTime += dt;
  poseTimer += dt;
  if (!model || poseTimer < 1.f / kPoseHz) return;
  poseTimer = 0.f;  

  // Sample the active clip at the current clock (poseAnimation loops it and
  // rebuilds the palette). Falls back to the bind pose when no clip is set
  if (!currentClip.empty())
    model->poseAnimation(currentClip, animTime);
  else headSpin();
}

void GameCharacter::render(std::vector<lve::SkinnedRenderItem>& items) const {
  if (!model) return;

  // glTF is Y-up, this engine is Y-down, so flip the model 180 deg about Z
  glm::mat4 modelMatrix = glm::translate(glm::mat4(1.f), translation) *
                          glm::rotate(glm::mat4(1.f), glm::pi<float>(), glm::vec3(0.f, 0.f, 1.f)) *
                          glm::scale(glm::mat4(1.f), glm::vec3(scale));
  glm::mat4 normalMatrix = glm::mat4(glm::transpose(glm::inverse(glm::mat3(modelMatrix))));
  items.push_back({modelMatrix, normalMatrix, model.get()});
}
