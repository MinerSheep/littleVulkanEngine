#include <lve_engine.hpp>
#include <scenes/skinneddemoscene.hpp>

#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>

// std
#include <fstream>
#include <iostream>
#include <sstream>

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

  // Press F to launch a sphere.obj "fireball" along the camera's look direction
  manAbility = man.addComponent<PlayerAbilityComponent>();
  manAbility->setModel(lve::LveModel::createModelFromFile("models/sphere.obj"));

  // Collision box, added LAST so it ticks after movement and ends the frame
  // in the correct position
  manCollider = man.addComponent<ColliderComponent>();
  manCollider->isStatic = false;  // he is the one that gets pushed
  if (manSkin->model) {
    manCollider->fitToModel(*manSkin->model);
    manCollider->localHalfExtent.x = glm::min(manCollider->localHalfExtent.x, manBodyRadius);
    manCollider->localHalfExtent.z = glm::min(manCollider->localHalfExtent.z, manBodyRadius);
  }

  // --- Static world models drawn via SimpleRenderSystem ---
  groundModel = lve::LveModel::createModelFromFile("models/quad.obj");          // flat XZ plane
  cubeModel = lve::LveModel::createModelFromFile("models/colored_cube.obj");    // vertex-colored
  blockModel = lve::LveModel::createModelFromFile("models/cube.obj");           // white

  // A few low-poly blocks resting on the ground around the man. A block scaled
  // by s (cube.obj half-extent 1) rests on the plane when its centre sits at
  // groundY - s (remember -Y is up, so "on top of" means offset toward -Y)
  props.clear();
  auto block = [&](lve::LveModel* m, float x, float z, float s, float yaw) {
    props.push_back({m, glm::vec3(x, groundY - s, z), glm::vec3(s), glm::vec3(0.f, yaw, 0.f)});
  };
  block(cubeModel.get(),  2.6f,  0.6f, 0.50f,  0.40f);
  block(cubeModel.get(), -2.4f, -1.2f, 0.35f, -0.20f);
  block(cubeModel.get(), -1.6f,  2.4f, 0.60f,  0.00f);
  block(blockModel.get(), 0.7f, -2.8f, 0.25f,  0.80f);
  block(blockModel.get(), 3.0f, -2.2f, 0.30f,  0.30f);

  // Reproduce objects authored in the layout editor. getExecutableDir() resolves
  // to the game/ dir, where the editor's Enter-save writes scene_layout.txt
  loadSceneLayout(getExecutableDir() + "/scene_layout.txt");

  // Every prop - whether hand-placed or authored - gets a box collider once its final
  // placement is known
  fitPropColliders();
  refreshPropColliders();
}

void SkinnedDemoScene::fitPropColliders() {
  for (auto& prop : props) {
    if (prop.model) prop.collider.fitToModel(*prop.model);
  }
}

glm::mat4 SkinnedDemoScene::propMatrix(const StaticProp& prop) {

  // Same TranslationRotScale order the editor uses (yaw, then pitch/roll, then scale)
  // A saved layout renders, collides, exactly how it was authored
  glm::mat4 mat = glm::translate(glm::mat4(1.f), prop.translation);
  mat = glm::rotate(mat, prop.rotation.y, glm::vec3(0.f, 1.f, 0.f));
  mat = glm::rotate(mat, prop.rotation.x, glm::vec3(1.f, 0.f, 0.f));
  mat = glm::rotate(mat, prop.rotation.z, glm::vec3(0.f, 0.f, 1.f));
  return glm::scale(mat, prop.scale);
}

void SkinnedDemoScene::refreshPropColliders() {
  for (auto& prop : props) 
    prop.collider.refresh(propMatrix(prop));
}


//  Temp function for resolving man's collider box
void SkinnedDemoScene::resolveManCollision() {
  TransformComponent* manXform = man.getComponent<TransformComponent>();
  if (!manCollider || !manCollider->enabled || !manXform) return;

  // Two passes: 
  // 1: First shove can slide him straight into a neighbouring prop
  // 2: Second handles that case and anything still overlapping
  // after that is "wedged", stopping beats jittering between two props
  for (int pass = 0; pass < 2; pass++) {

    // push check
    bool pushed = false;

    for (const auto& prop : props) {
      if (!prop.collider.enabled) continue;

      const glm::vec3 push = manCollider->worldBox().pushOutXZ(prop.collider.worldBox());
      if (push == glm::vec3(0.f)) continue;

      manXform->translation += push;
      manCollider->refresh(manXform->mat4());  // the next prop tests his new spot
      pushed = true;
    }

    // Exit early if no collision
    if (!pushed) break;
  }
}

lve::LveModel* SkinnedDemoScene::modelForPreset(const std::string& name) {
  auto it = layoutModels.find(name);
  if (it != layoutModels.end()) return it->second.get();  // cached (may be null on prior failure)

  // Preset names are the model's file basename (see the editor's presets)
  std::string path = "models/" + name + ".obj";
  try {
    std::unique_ptr<lve::LveModel> model = lve::LveModel::createModelFromFile(path);
    lve::LveModel* raw = model.get();
    layoutModels.emplace(name, std::move(model));
    return raw;
  } catch (const std::exception& e) {
    std::cerr << "[skinneddemo] could not load preset '" << name << "' from " << path << ": "
              << e.what() << '\n';
    layoutModels.emplace(name, nullptr);  // cache the failure so we don't retry every reference
    return nullptr;
  }
}

void SkinnedDemoScene::loadSceneLayout(const std::string& path) {
  std::ifstream in(path);
  if (!in) {
    std::cerr << "[skinneddemo] no scene layout at " << path << " (skipping)\n";
    return;
  }

  // One object per line: "<preset>  tx ty tz  rx ry rz  sx sy sz". Blank lines
  // and '#' comments are ignored, matching what the editor's save() writes
  std::string line;
  int loaded = 0;
  while (std::getline(in, line)) {
    if (line.empty() || line[0] == '#') continue;

    std::istringstream ss(line);
    std::string name;
    glm::vec3 t{0.f}, r{0.f}, s{1.f};
    if (!(ss >> name >> t.x >> t.y >> t.z >> r.x >> r.y >> r.z >> s.x >> s.y >> s.z)) {
      std::cerr << "[skinneddemo] skipping malformed layout line: " << line << '\n';
      continue;
    }

    lve::LveModel* model = modelForPreset(name);
    if (!model) continue;  // unknown/failed preset already logged

    props.push_back({model, t, s, r});  // StaticProp{model, translation, scale, rotation}
    loaded++;
  }
  std::cout << "[skinneddemo] loaded " << loaded << " object(s) from " << path << std::endl;
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
  // look direction (from the camera toward the man). The fireball aims the same way
  const float lookYaw = cameraYaw + glm::pi<float>();
  if (manMover) manMover->forwardYaw = lookYaw;
  if (manAbility) manAbility->aimYaw = lookYaw;

  // --- Character: tick its components (advances the clip AND walks the man) ---
  man.updateComponents(dt);

  // His components have moved him and his collider has followed; shove him back
  // out of anything he walked into before camera and draws read him
  resolveManCollision();

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

  // Props: hand-placed blocks + objects loaded from the editor's layout
  for (const auto& p : props) 
    pushItem(p.model, propMatrix(p));

  // Live fireballs: draw each as a small sphere at its current position
  if (manAbility && manAbility->model()) {
    for (const auto& shot : manAbility->activeProjectiles()) {
      glm::mat4 mat = glm::translate(glm::mat4(1.f), shot.position) *
                      glm::scale(glm::mat4(1.f), glm::vec3(manAbility->radius));
      pushItem(manAbility->model(), mat);
    }
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

  // A warm glow trailing each fireball so the grey sphere reads as fire
  // WARNING: The point light system asserts on more than MAX_LIGHTS, so stop once the budget is spent
  if (manAbility) {
    for (const auto& shot : manAbility->activeProjectiles()) {
      if (lightItems.size() >= MAX_LIGHTS) break;
      addLight(shot.position, {1.0f, 0.5f, 0.1f}, 4.f);
    }
  }

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
