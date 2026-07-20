#include <lve_engine.hpp>
#include <scenes/skinneddemoscene.hpp>

#include <glm/gtc/matrix_transform.hpp>

void SkinnedDemoScene::loadModels() {
  static GameObject cameraObject = GameObject::createGameObject();
  viewerObject = &cameraObject;
  viewerObject->addComponent<TransformComponent>()->translation.z = -2.5f;

  // statue.glb ships 6 baked animation clips (Idle01/02, Walk01/02, Run, MutanWalk)
  // GameCharacter starts on the first and keys 4..9 switch between them
  man.setModel(lve::LveSkinnedModel::createModelFromFile("models/statue.glb"));
  man.scale = 0.9f;
  man.translation = {0.f, groundY, 0.f};

  // --- Static world models drawn via SimpleRenderSystem ---
  groundModel = lve::LveModel::createModelFromFile("models/quad.obj");          // flat XZ plane
  cubeModel = lve::LveModel::createModelFromFile("models/colored_cube.obj");    // vertex-colored
  blockModel = lve::LveModel::createModelFromFile("models/cube.obj");           // white

  // A few low-poly blocks resting on the ground around the man. A block scaled
  // by s (cube.obj half-extent 1) rests on the plane when its centre sits at
  // groundY - s (remember -Y is up, so "on top of" means offset toward -Y)
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

  // Number keys 4..9 pick an animation clip
  for (int i = 0; i < man.clipCount() && i < 6; i++)
    if (glfwGetKey(window, GLFW_KEY_4 + i) == GLFW_PRESS)
      man.setClipIndex(i);

  // --- Character: advance its animation clip, then emit its skinned draw -----
  man.animate(dt);

  skinnedRenderItems.clear();
  man.render(skinnedRenderItems);

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
