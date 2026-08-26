#include "lve_scene_editor.hpp"
#include "lve_engine.hpp"       // instance(): GLFW window, aspect ratio, GLFW_KEY_*

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>

// std
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace lve {

namespace {

// Where typed letters land while the load line is open, and null while it is shut
std::string* typingTarget = nullptr;

void charTyped(GLFWwindow*, unsigned int codepoint) {
  if (typingTarget && codepoint >= 32 && codepoint < 127) {
    typingTarget->push_back(static_cast<char>(codepoint));
  }
}

}  // namespace

void LveSceneEditor::loadModels() {
  // Spawnable presets list
  // Order here is the Up/Down cycle order and the `preset`
  // index stored per object / written to the save file
  presets.clear();
  // Blockout primitives - all unit-sized (fit a 2x2x2 box) so they scale predictably
  presets.push_back({"plane", LveModel::createModelFromFile("models/plane.obj")});
  presets.push_back({"cube", LveModel::createModelFromFile("models/cube.obj")});
  presets.push_back({"sphere", LveModel::createModelFromFile("models/sphere.obj")});
  presets.push_back({"cylinder", LveModel::createModelFromFile("models/cylinder.obj")});
  
  presets.push_back({"flat_vase", LveModel::createModelFromFile("models/flat_vase.obj")});
  presets.push_back({"grass", LveModel::createModelFromFile("models/grass.obj")});
  presets.push_back({"tree", LveModel::createModelFromFile("models/tree.obj")});
  presets.push_back({"rock", LveModel::createModelFromFile("models/rock.obj")});

  presets.push_back({"piano", LveModel::createModelFromFile("models/piano.obj")});
  presets.push_back({"fence", LveModel::createModelFromFile("models/fence.obj")});

  groundModel = LveModel::createModelFromFile("models/quad.obj");  // scaled-out floor
  markerModel = LveModel::createModelFromFile("models/cube.obj");  // selection marker

  textRenderer = std::make_unique<LveTextRenderer>(LveEngine::instance().getDevice());

  // Letters typed into the load line arrive here rather than through glfwGetKey
  glfwSetCharCallback(LveEngine::instance().getGLFWWindow(), charTyped);

  objects.clear();
  spawnPreset = 0;
  spawn(spawnPreset);  // start with a single object, selected
}

void LveSceneEditor::spawn(int preset) {
  EditorObject o;
  o.preset = preset;
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
    const std::string& name =
        (o.preset >= 0 && o.preset < static_cast<int>(presets.size())) ? presets[o.preset].name
                                                                       : std::string("unknown");
    out << name << "  " << o.translation.x << ' ' << o.translation.y << ' ' << o.translation.z
        << "  " << o.rotation.x << ' ' << o.rotation.y << ' ' << o.rotation.z
        << "  " << o.scale.x << ' ' << o.scale.y << ' ' << o.scale.z << '\n';
  }
  std::cout << "[editor] saved " << objects.size() << " object(s) to " << path << std::endl;
}

int LveSceneEditor::presetIndex(const std::string& name) {
  for (std::size_t i = 0; i < presets.size(); ++i) {
    if (presets[i].name == name) return static_cast<int>(i);
  }

  // The map named a mesh the editor was not carrying, so fetch it where build_map.py keeps it
  try {
    presets.push_back({name, LveModel::createModelFromFile("models/" + name + ".obj")});
  } catch (const std::exception& e) {
    std::cerr << "[editor] no mesh for preset " << name << ": " << e.what() << '\n';
    return -1;
  }
  return static_cast<int>(presets.size()) - 1;
}

void LveSceneEditor::loadRoom(const std::string& path, const std::string& room) {
  MapFileRoom parsed;
  std::string error;
  if (!readMapRoom(path, room, parsed, error)) {
    status = "LOAD FAILED";
    std::cerr << "[editor] " << error << '\n';
    return;
  }

  objects.clear();
  locked.clear();

  for (const MapFileObject& source : parsed.objects) {
    EditorObject o;
    o.preset = presetIndex(source.preset);
    o.translation = source.translation;
    o.rotation = source.rotation;
    o.scale = source.scale;
    o.interact = source.interact;
    o.solid = source.solid;
    o.name = source.name;
    o.script = source.script;
    (source.locked ? locked : objects).push_back(o);
  }

  mapPath = path;
  roomName = parsed.ident;
  selected = 0;

  // Start off where the room's own camera stands, looking where it looks
  const glm::vec3 heading = parsed.cameraLook - parsed.cameraEye;
  if (glm::dot(heading, heading) > 0.f) {
    const glm::vec3 d = glm::normalize(heading);
    camTranslation = parsed.cameraEye;
    camRotation = {-std::asin(d.y), std::atan2(d.x, d.z), 0.f};
  }

  status = "LOADED " + roomName;
  std::cout << "[editor] loaded " << objects.size() << " prop(s) and " << locked.size()
            << " fixed piece(s) from " << path << " room " << roomName << std::endl;
}

void LveSceneEditor::saveRoom() {
  MapFileRoom out;
  out.ident = roomName;

  for (const EditorObject& o : objects) {
    MapFileObject piece;
    piece.preset = (o.preset >= 0 && o.preset < static_cast<int>(presets.size()))
                       ? presets[o.preset].name
                       : std::string("cube");
    piece.translation = o.translation;
    piece.rotation = o.rotation;
    piece.scale = o.scale;
    piece.interact = o.interact;
    piece.solid = o.solid;
    piece.name = o.name;
    piece.script = o.script;
    out.objects.push_back(piece);
  }

  const std::string path = "maps/" + roomName + ".props.mapsrc";
  std::string error;
  if (!writeMapsrcProps(path, out, error)) {
    status = "SAVE FAILED";
    std::cerr << "[editor] " << error << '\n';
    return;
  }

  status = "SAVED TO " + path;
  std::cout << "[editor] wrote " << objects.size() << " prop line(s) to " << path << std::endl;
}

void LveSceneEditor::update(float dt) {
  // elapsed is a variable that makes the marker float up & down
  elapsed += dt;

  GLFWwindow* window = LveEngine::instance().getGLFWWindow();

  // lambda function for quickly checking if a key is pressed
  auto down = [&](int key) { return glfwGetKey(window, key) == GLFW_PRESS; };

  // L opens the load line, Enter takes what is typed, Escape drops it
  const bool lNow = down(GLFW_KEY_L);
  const bool enterNow = down(GLFW_KEY_ENTER);
  const bool backNow = down(GLFW_KEY_BACKSPACE);
  const bool escNow = down(GLFW_KEY_ESCAPE);

  if (typing) {
    if (backNow && !prevBack && !entry.empty()) entry.pop_back();

    if (escNow && !prevEsc) {
      typing = false;
      typingTarget = nullptr;
      status = "LOAD CANCELLED";
    } else if (enterNow && !prevEnter) {
      typing = false;
      typingTarget = nullptr;

      // "<file> <room>", or just the room once a file has been named once
      std::istringstream words(entry);
      std::string first, second;
      words >> first >> second;
      if (first.empty()) {
        status = "NOTHING TYPED";
      } else if (second.empty()) {
        loadRoom(mapPath.empty() ? std::string("maps/forest.map") : mapPath, first);
      } else {
        loadRoom(first, second);
      }
    }
  } else {
    if (lNow && !prevL) {
      typing = true;
      entry = mapPath.empty() ? std::string("maps/forest.map ") : mapPath + " ";
      typingTarget = &entry;
    }

    // T flips between editing objects and flying the camera
    const bool tNow = down(GLFW_KEY_T);
    if (tNow && !prevT) cameraMode = !cameraMode;
    prevT = tNow;

    // select the placed object to edit (left/right)
    const bool leftNow = down(GLFW_KEY_LEFT);
    const bool rightNow = down(GLFW_KEY_RIGHT);

    // choose which preset the next Space spawns (up/down)
    const bool upNow = down(GLFW_KEY_UP);
    const bool downNow = down(GLFW_KEY_DOWN);

    // spawn
    const bool spaceNow = down(GLFW_KEY_SPACE);

    // Object-mode edits are suppressed while flying so the shared WASDQE/arrow keys
    // drive the camera instead of the selected object
    if (!cameraMode)
    {
      if (!objects.empty()) {
        int count = static_cast<int>(objects.size());

        // change selected
        if (leftNow && !prevLeft) selected = (selected + count - 1) % count;
        if (rightNow && !prevRight) selected = (selected + 1) % count;
      }
      if (!presets.empty()) {
        int pc = static_cast<int>(presets.size());
        if (upNow && !prevUp) spawnPreset = (spawnPreset + pc - 1) % pc;
        if (downNow && !prevDown) spawnPreset = (spawnPreset + 1) % pc;
      }
      if (spaceNow && !prevSpace) spawn(spawnPreset);
    }

    // Enter writes the room's props back out, or the plain layout when no room is up
    if (enterNow && !prevEnter) {
      if (roomName.empty()) save();
      else saveRoom();
    }

    prevLeft = leftNow;
    prevRight = rightNow;
    prevUp = upNow;
    prevDown = downNow;
    prevSpace = spaceNow;
  }

  prevL = lNow;
  prevEnter = enterNow;
  prevBack = backNow;
  prevEsc = escNow;

  // Loading a smaller room can leave the old selection past the end
  if (selected >= static_cast<int>(objects.size())) selected = 0;

  // --- Continuous transform of the selected object ---------------------------
  if (!typing && !cameraMode && !objects.empty()) {
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

  // Camera - fly in camera mode, otherwise stays
  if (cameraMode && !typing)
    cameraController.moveInPlaneXZ(window, dt, camTranslation, camRotation);
    
  float aspect = LveEngine::instance().getAspectRatio();
  camera.setViewYXZ(camTranslation, camRotation);
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

  // The room's floor, walls and doorways, drawn only so props can be lined up on them
  for (const auto& o : locked) {
    LveModel* mdl = (o.preset >= 0 && o.preset < static_cast<int>(presets.size()))
                        ? presets[o.preset].model.get()
                        : nullptr;
    pushItem(mdl, matrixOf(o));
  }

  // Placed objects: each draws its preset's mesh
  for (const auto& o : objects) {
    LveModel* mdl = (o.preset >= 0 && o.preset < static_cast<int>(presets.size()))
                        ? presets[o.preset].model.get()
                        : nullptr;
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

  // --- HUD: current spawn preset + counts, drawn as on-screen text -----------
  UIrenderItems.clear();
  if (textRenderer) {
    const std::string spawnName = presets.empty() ? "none" : presets[spawnPreset].name;
    const std::string room = roomName.empty() ? std::string("none") : mapPath + " " + roomName;

    std::string hud = "EDITOR\n";
    if (typing) {
      hud += "LOAD: " + entry + "_\n"
             "TYPE <FILE> <ROOM>\n"
             "ENTER LOADS  ESC CANCELS";
    } else {
      hud += "ROOM: " + room + "\n" +
             "MODE: " + std::string(cameraMode ? "CAMERA" : "OBJECT") + " (T)\n" +
             "SPAWN: " + spawnName + " (UP/DOWN)\n" +
             "PROPS: " + std::to_string(objects.size()) +
             "  FIXED: " + std::to_string(locked.size()) +
             "  SEL: " + std::to_string(objects.empty() ? 0 : selected + 1) + "\n" +
             "SPACE ADD  L LOAD  ENTER SAVE\n" +
             status;
    }

    const glm::vec2 origin{-0.97f, -0.95f};  // top-left in NDC (y is down)
    const float dotHeight = 0.008f;

    float aspect = LveEngine::instance().getAspectRatio();
    if (aspect <= 0.f) aspect = 1.f;
    const float dotW = dotHeight / aspect;

    // Dark panel behind the text so it stays readable over the scene
    UIRenderItem panel{};
    panel.transform = glm::mat2(
        textRenderer->measureWidth(hud, dotHeight) + 2.f * dotW, 0.f,
        0.f, (LveTextRenderer::lineCount(hud) * 8 - 1) * dotHeight + 2.f * dotHeight);
    panel.offset = {origin.x - dotW, origin.y - dotHeight};
    panel.color = {0.03f, 0.03f, 0.06f};
    panel.alpha = 1.f;
    panel.model = textRenderer->quad();
    UIrenderItems.push_back(panel);

    // Emitted after the panel so the text glyphs will paint on top of it
    textRenderer->emit(UIrenderItems, hud, origin, dotHeight, {1.f, 1.f, 1.f});
  }

  // last rendering steps
  ubo.ambientLightColor = {1.f, 1.f, 1.f, 0.20f};
  ubo.projection = camera.getProjection();
  ubo.view = camera.getView();
  ubo.inverseView = camera.getInverseView();
}

}  // namespace lve
