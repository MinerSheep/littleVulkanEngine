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

  // --- Static world models (drawn via the ordinary SimpleRenderSystem path) ---
  groundModel = lve::LveModel::createModelFromFile("models/quad.obj");          // flat XZ plane
  cubeModel = lve::LveModel::createModelFromFile("models/colored_cube.obj");    // vertex-colored
  blockModel = lve::LveModel::createModelFromFile("models/cube.obj");           // white

  // A few low-poly blocks resting on the ground around the man. A block scaled
  // by s (cube.obj half-extent 1) rests on the plane when its centre sits at
  // groundY - s (remember -Y is up, so "on top of" means offset toward -Y).
  props.clear();
  auto block = [&](lve::LveModel* m, float x, float z, float s, float yaw) {
    props.push_back({m, glm::vec3(x, groundY - s, z), glm::vec3(s), yaw});
  };
  block(cubeModel.get(),  2.6f,  0.6f, 0.50f,  0.40f);
  block(cubeModel.get(), -2.4f, -1.2f, 0.35f, -0.20f);
  block(cubeModel.get(), -1.6f,  2.4f, 0.60f,  0.00f);
  block(blockModel.get(), 0.7f, -2.8f, 0.25f,  0.80f);
  block(blockModel.get(), 3.0f, -2.2f, 0.30f,  0.30f);
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

  // --- Procedurally animate the rig (throttled to kPoseHz; see header A3) ------
  // animTime advances every frame so the motion is smooth and real-time, but the
  // expensive skeleton re-pose only runs ~kPoseHz times a second. (uploadPose in
  // the render system still copies the palette each frame; that's a cheap memcpy.)
  animTime += dt;
  poseTimer += dt;
  if (man && poseTimer >= 1.f / kPoseHz) {
    poseTimer = 0.f;
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

  // --- Static world: ground plane + low-poly blocks --------------------------
  // Rebuilt each frame into the base-class renderItems, which the engine packs
  // into FrameInfo and draws with SimpleRenderSystem
  renderItems.clear();
  auto pushItem = [&](lve::LveModel* m, const glm::mat4& mat) {
    if (!m) return;
    glm::mat4 nm = glm::mat4(glm::transpose(glm::inverse(glm::mat3(mat))));
    renderItems.push_back({mat, nm, m});
  };

  // Ground: quad.obj is a unit XZ plane at y=0; scale it out and drop it to the
  // man's feet (groundY)
  pushItem(groundModel.get(),
           glm::translate(glm::mat4(1.f), glm::vec3(0.f, groundY, 0.f)) *
               glm::scale(glm::mat4(1.f), glm::vec3(groundHalfExtent, 1.f, groundHalfExtent)));

  // Blocks
  for (const auto& p : props) {
    glm::mat4 mat = glm::translate(glm::mat4(1.f), p.translation) *
                    glm::rotate(glm::mat4(1.f), p.yaw, glm::vec3(0.f, 1.f, 0.f)) *
                    glm::scale(glm::mat4(1.f), p.scale);
    pushItem(p.model, mat);
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
