#include "lve_scene_editor.hpp"
#include "lve_engine.hpp"       // instance(): GLFW window, aspect ratio, GLFW_KEY_*

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>

// std
#include <fstream>
#include <iomanip>
#include <iostream>

namespace lve {

void LveSceneEditor::loadModels() {
  // cube.obj marks the current selection
  quadModel = LveModel::createModelFromFile("models/quad.obj");
  groundModel = LveModel::createModelFromFile("models/quad.obj");
  markerModel = LveModel::createModelFromFile("models/cube.obj");

  objects.clear();
  spawn(0);  // start with a single quad, selected
}

void LveSceneEditor::spawn(int preset) {
  EditorObject o;
  o.preset = preset;  // 0 = quad (only preset for now)
  objects.push_back(o);
  selected = static_cast<int>(objects.size()) - 1;
}

glm::mat4 LveSceneEditor::matrixOf(const EditorObject& o) const {
  glm::mat4 m = glm::translate(glm::mat4(1.f), o.translation);

  // Yaw first (the axis WASDQE editing cares about), then pitch/roll for
  // completeness; only yaw is edited today but the save file carries all three
  m = glm::rotate(m, o.rotation.y, glm::vec3(0.f, 1.f, 0.f));
  m = glm::rotate(m, o.rotation.x, glm::vec3(1.f, 0.f, 0.f));
  m = glm::rotate(m, o.rotation.z, glm::vec3(0.f, 0.f, 1.f));
  m = glm::scale(m, o.scale);
  return m;
}

void LveSceneEditor::save() const {
  // Next to the binary so the path is stable regardless of working dir
  std::string path = getExecutableDir() + "/scene_layout.txt";
  std::ofstream out(path);

  if (!out) {
    std::cerr << "[editor] could not open " << path << " for writing\n";
    return;
  }

  out << "# serverNavGame scene layout v1\n";
  out << "# preset  tx ty tz  rx ry rz  sx sy sz\n";
  out << std::fixed << std::setprecision(3);
  for (const auto& o : objects) {
    const char* name = (o.preset == 0) ? "quad" : "unknown";
    out << name << "  " << o.translation.x << ' ' << o.translation.y << ' ' << o.translation.z
        << "  " << o.rotation.x << ' ' << o.rotation.y << ' ' << o.rotation.z
        << "  " << o.scale.x << ' ' << o.scale.y << ' ' << o.scale.z << '\n';
  }
  std::cout << "[editor] saved " << objects.size() << " object(s) to " << path << std::endl;
}

void LveSceneEditor::update(float dt) {
  // elapsed is a variable that makes the marker float up & down
  elapsed += dt;

  GLFWwindow* window = LveEngine::instance().getGLFWWindow();

  // lambda function for quickly checking if a key is pressed
  auto down = [&](int key) { return glfwGetKey(window, key) == GLFW_PRESS; };

  // select
  bool leftNow = down(GLFW_KEY_LEFT);
  bool rightNow = down(GLFW_KEY_RIGHT);

  // spawn
  bool spaceNow = down(GLFW_KEY_SPACE);

  // save
  bool enterNow = down(GLFW_KEY_ENTER);

  if (!objects.empty()) {
    int count = static_cast<int>(objects.size());

    // change selected
    if (leftNow && !prevLeft) selected = (selected + count - 1) % count;
    if (rightNow && !prevRight) selected = (selected + 1) % count;
  }
  if (spaceNow && !prevSpace) spawn(0);
  if (enterNow && !prevEnter) save();

  prevLeft = leftNow;
  prevRight = rightNow;
  prevSpace = spaceNow;
  prevEnter = enterNow;

  // --- Continuous transform of the selected object ---------------------------
  if (!objects.empty()) {
    EditorObject& o = objects[selected];

    const float m = moveSpeed * dt;
    const bool shift = down(GLFW_KEY_LEFT_SHIFT) || down(GLFW_KEY_RIGHT_SHIFT);

    // o is our object, we use WASDQE to change translation
    if (down(GLFW_KEY_W)) o.translation.z += m;
    if (down(GLFW_KEY_S)) o.translation.z -= m;
    if (down(GLFW_KEY_Q)) o.translation.y -= m;  // -Y is up
    if (down(GLFW_KEY_E)) o.translation.y += m;

    if (shift) {
      // Shift makes A/D yaw instead of translate
      if (down(GLFW_KEY_A)) o.rotation.y -= rotSpeed * dt;
      if (down(GLFW_KEY_D)) o.rotation.y += rotSpeed * dt;
    } else {
      if (down(GLFW_KEY_A)) o.translation.x -= m;
      if (down(GLFW_KEY_D)) o.translation.x += m;
    }

    // change the scale
    if (down(GLFW_KEY_LEFT_BRACKET)) o.scale -= glm::vec3(scaleSpeed * dt);
    if (down(GLFW_KEY_RIGHT_BRACKET)) o.scale += glm::vec3(scaleSpeed * dt);
    o.scale = glm::max(o.scale, glm::vec3(minScale));
  }

  // Camera is a fixed angled view pointing at origin
  float aspect = LveEngine::instance().getAspectRatio();
  camera.setViewTarget({0.f, -7.f, -7.f}, {0.f, 0.f, 0.f});
  camera.setPerspectiveProjection(glm::radians(50.f), aspect, 0.1f, 100.f);

  // Rebuild the draw list each frame into renderItems
  renderItems.clear();

  // lambda for putting models (mdl) with mat into renderItems
  auto pushItem = [&](LveModel* mdl, const glm::mat4& mat) {
    if (!mdl) return;
    glm::mat4 nm = glm::mat4(glm::transpose(glm::inverse(glm::mat3(mat))));
    renderItems.push_back({mat, nm, mdl});
  };

  // Ground plane
  pushItem(groundModel.get(),
           glm::translate(glm::mat4(1.f), glm::vec3(0.f, groundY, 0.f)) *
           glm::scale(glm::mat4(1.f), glm::vec3(groundHalfExtent, 1.f, groundHalfExtent)));

  // Placed objects
  for (const auto& o : objects) {
    LveModel* mdl = (o.preset == 0) ? quadModel.get() : nullptr;
    pushItem(mdl, matrixOf(o));
  }

  // Selection marker is a small cube bobbing above the selected object
  if (!objects.empty()) {
    glm::vec3 p = objects[selected].translation;
    float bob = 0.1f * glm::sin(elapsed * 4.f);
    glm::mat4 mm = glm::translate(glm::mat4(1.f), p + glm::vec3(0.f, -1.2f + bob, 0.f)) *
                   glm::scale(glm::mat4(1.f), glm::vec3(0.15f));
    pushItem(markerModel.get(), mm);
  }

  // Lights
  lightItems.clear();

  // lambda to render a light
  auto addLight = [&](glm::vec3 pos, glm::vec3 color, float intensity) {
    LightRenderItem item;
    item.position = pos;
    item.color = color;
    item.intensity = intensity;
    item.radius = 0.1f;
    glm::vec3 offset = camera.getPosition() - pos;
    item.distanceToCamera = glm::dot(offset, offset);
    lightItems.push_back(item);
  };

  addLight({2.0f, -3.0f, -2.0f}, {1.0f, 1.0f, 1.0f}, 8.f);
  addLight({-3.0f, -2.0f, 1.0f}, {0.7f, 0.8f, 1.0f}, 6.f);

  // last rendering steps
  ubo.ambientLightColor = {1.f, 1.f, 1.f, 0.20f};
  ubo.projection = camera.getProjection();
  ubo.view = camera.getView();
  ubo.inverseView = camera.getInverseView();
}

}  // namespace lve
