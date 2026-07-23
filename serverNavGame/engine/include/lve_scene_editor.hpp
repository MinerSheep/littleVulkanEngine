#pragma once

#include "lve_scene.hpp"
#include "lve_camera.hpp"
#include "lve_model.hpp"
#include "lve_text.hpp"

// std
#include <memory>
#include <string>
#include <vector>

namespace lve {

  // An in-engine scene for authoring object layouts
  // Spawn preset meshes with space, select one with the arrow keys, transform it with
  // WASDQE (+ Shift to yaw, [ ] to scale), and press Enter to dump every
  // object's full transform to a text file that a loader can replay later.
  //
  // Self-contained: it reads the keyboard directly via LveEngine's GLFW window
  class LveSceneEditor : public LveScene {
   public:
    LveSceneEditor() = default;

    void loadModels() override;
    void update(float dt) override;
    void cleanup() override {}

   private:
    // Spawnable meshes have a display/save name plus the model it draws
    // Add more by pushing to presets in loadModels
    struct Preset {
      std::string name;                 // shown on screen and written to the save file
      std::unique_ptr<LveModel> model;  // the mesh drawn for this preset
    };

    struct EditorObject {
      int       preset = 0;
      glm::vec3 translation{0.f};
      glm::vec3 rotation{0.f};
      glm::vec3 scale{1.f};
    };

    void spawn(int preset);                          // add at origin, select it
    void save() const;                               // write scene_layout.txt
    glm::mat4 matrixOf(const EditorObject& o) const; // translate * R * scale

    std::vector<EditorObject> objects;
    int   selected = 0;
    float elapsed  = 0.f;  // drives the selection marker's bob

    // The spawnable presets, and which one the next Space will spawn
    std::vector<Preset> presets;
    int spawnPreset = 0;

    // Instances reference a preset's model via RenderItem's non-owning pointer.
    // Ground is a scaled-out quad; marker flags the selected object
    std::unique_ptr<LveModel> groundModel;
    std::unique_ptr<LveModel> markerModel;

    // Draws the on-screen HUD (current spawn preset, counts) into UIrenderItems
    std::unique_ptr<LveTextRenderer> textRenderer;

    LveCamera camera{};

    // Keys fire once per press, not per frame held
    bool prevLeft = false, prevRight = false, prevSpace = false, prevEnter = false;
    bool prevUp = false, prevDown = false;  // cycle the spawn preset

    // Tuning (units per second, scaled by dt)
    static constexpr float moveSpeed  = 3.0f;
    static constexpr float rotSpeed   = 1.5f;
    static constexpr float scaleSpeed = 1.5f;
    static constexpr float minScale   = 0.05f;

    // -Y is "up" in this engine, so the floor is placed below the objects
    static constexpr float groundY = 0.5f;
    static constexpr float groundHalfExtent = 8.f;
  };

}  // namespace lve
