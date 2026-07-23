#include <lve_engine.hpp>
#include <scenes/skinneddemoscene.hpp>

#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>

void SkinnedDemoScene::loadModels() {
  // statue.glb ships 6 baked animation clips (Idle01/02, Walk01/02, Run, MutanWalk)
  // The character starts on the first; keys 4..9 switch between them (see update)
  TransformComponent* manXform = man.addComponent<TransformComponent>();
  manXform->translation = {0.f, groundY, 0.f};
  manXform->scale = glm::vec3(0.9f);
  manXform->rotation = {0.f, 0.f, glm::pi<float>()};  // glTF Y-up -> engine Y-down
  manSkin = man.addComponent<SkinnedModelComponent>();
  manSkin->setModel(lve::LveSkinnedModel::createModelFromFile("models/statue.glb"));

  // WASD walks the man across the XZ plane, relative to the camera. update() sets
  // the component's forwardYaw to the camera's look direction each frame
  manMover = man.addComponent<KeyboardMovementComponent>();

  // --- Static world models drawn via SimpleRenderSystem ---
  groundModel = lve::LveModel::createModelFromFile("models/grass.obj");          // flat XZ plane
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

  // --- Drag-to-orbit: hold the left mouse button and move to spin the camera --
  bool dragBtn = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;

  double cx, cy;
  glfwGetCursorPos(window, &cx, &cy);
  glm::vec2 cur{static_cast<float>(cx), static_cast<float>(cy)};

  if (dragBtn && !dragging) lastCursor = cur;             // drag just began: zero the first delta
  if (dragBtn) {
    glm::vec2 delta = cur - lastCursor;
    lastCursor = cur;
    cameraYaw -= delta.x * mouseSensitivity;              // drag right swings the camera round
    cameraPitch -= delta.y * mouseSensitivity;            // drag up lifts it
    cameraPitch = glm::clamp(cameraPitch, 0.05f, 1.45f);  // stay above ground, below top
  }
  dragging = dragBtn;

  // WASD is camera-relative: point the mover's forward down the camera's ground
  // look direction (from the camera toward the man)
  if (manMover) manMover->forwardYaw = cameraYaw + glm::pi<float>();

  // --- Character: tick its components (advances the clip AND walks the man) ---
  man.updateComponents(dt);

  // --- Orbit-follow camera: spherical offset around the man, looking at him ---
  TransformComponent* manXform = man.getComponent<TransformComponent>();
  const float cp = glm::cos(cameraPitch);
  const float sp = glm::sin(cameraPitch);
  glm::vec3 offset{cameraDistance * cp * glm::sin(cameraYaw),
                   -cameraDistance * sp,  // -Y is up, so subtract to lift the camera
                   cameraDistance * cp * glm::cos(cameraYaw)};
  camera.setViewTarget(manXform->translation + offset, manXform->translation);

  float aspect = lve::LveEngine::instance().getAspectRatio();
  camera.setPerspectiveProjection(glm::radians(50.f), aspect, 0.1f, 100.f);

  skinnedRenderItems.clear();
  collectSkinned(man, skinnedRenderItems);

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

void SkinnedDemoScene::onEvent(const lve::Event& event) {
  if (event.type != lve::EventType::KeyPressed) return;

  // Number keys 4..9 pick an animation clip (statue.glb ships 6). main posts a
  // KeyPressed on each key-down edge, and the dispatcher hands it here
  int clip = event.i - GLFW_KEY_4;
  if (manSkin && clip >= 0 && clip < manSkin->clipCount() && clip < 6)
    manSkin->setClipIndex(clip);
}

void SkinnedDemoScene::cleanup() {}
