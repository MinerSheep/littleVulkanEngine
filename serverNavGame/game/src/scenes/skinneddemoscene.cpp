#include <lve_engine.hpp>
#include <scenes/skinneddemoscene.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <cmath>

void SkinnedDemoScene::loadModels() {
  static GameObject cameraObject = GameObject::createGameObject();
  viewerObject = &cameraObject;
  viewerObject->addComponent<TransformComponent>()->translation.z = -2.5f;

  man = lve::LveSkinnedModel::createModelFromFile("models/man.glb");

  // Resolve the deform joints we wiggle (head carries the whole head weight)
  headNode = man->findNode("head.x");
  neckNode = man->findNode("neck.x");
  spineNode = man->findNode("spine_03.x");
}

void SkinnedDemoScene::setupLights() {
}

void SkinnedDemoScene::update(float dt) {
  GLFWwindow* window = lve::LveEngine::instance().getGLFWWindow();

  cameraController.moveInPlaneXZ(window, dt, *viewerObject);
  TransformComponent* view = viewerObject->getComponent<TransformComponent>();
  camera.setViewYXZ(view->translation, view->rotation);

  float aspect = lve::LveEngine::instance().getAspectRatio();
  camera.setPerspectiveProjection(glm::radians(50.f), aspect, 0.1f, 100.f);

  // --- Procedurally animate the rig
  animTime += dt;
  if (man) {
    man->resetPose();
    // Look around (nod + turn) and a small breathing sway. Small amplitudes keep
    // it natural; rotateJoint is a safe no-op for any joint that wasn't found
    man->rotateJoint(headNode, {1.f, 0.f, 0.f}, 0.20f * std::sin(animTime * 1.3f));  // nod
    man->rotateJoint(headNode, {0.f, 1.f, 0.f}, 0.35f * std::sin(animTime * 0.7f));  // turn
    man->rotateJoint(neckNode, {1.f, 0.f, 0.f}, 0.08f * std::sin(animTime * 1.3f));
    man->rotateJoint(spineNode, {0.f, 0.f, 1.f}, 0.05f * std::sin(animTime * 0.6f));  // sway
    man->recomputePalette();
  }

  // --- Place the skinned man -------------------------------------------------
  skinnedRenderItems.clear();
  if (man) {
    glm::mat4 model = glm::translate(glm::mat4(1.f), manTranslation) *
                      glm::rotate(glm::mat4(1.f), glm::pi<float>(), glm::vec3(0.f, 0.f, 1.f)) *
                      glm::scale(glm::mat4(1.f), glm::vec3(manScale));
    glm::mat4 normalMatrix = glm::mat4(glm::transpose(glm::inverse(glm::mat3(model))));
    skinnedRenderItems.push_back({model, normalMatrix, man.get()});
  }

  // --- A couple of point lights so the grey mesh is clearly shaded -----------
  lightItems.clear();
  auto addLight = [&](glm::vec3 pos, glm::vec3 color, float intensity) {
    lve::LightRenderItem item;
    item.position = pos;
    item.color = color;
    item.intensity = intensity;
    item.radius = 0.1f;
    glm::vec3 offset = camera.getPosition() - pos;
    item.distanceToCamera = glm::dot(offset, offset);
    lightItems.push_back(item);
  };
  // Remember: -Y is "up" in this engine.
  addLight({1.5f, -1.5f, -1.8f}, {1.0f, 1.0f, 1.0f}, 6.f);
  addLight({-2.0f, -0.5f, -1.0f}, {0.6f, 0.7f, 1.0f}, 5.f);

  ubo.ambientLightColor = {1.f, 1.f, 1.f, 0.15f};
  ubo.projection = camera.getProjection();
  ubo.view = camera.getView();
  ubo.inverseView = camera.getInverseView();
}

void SkinnedDemoScene::cleanup() {}
