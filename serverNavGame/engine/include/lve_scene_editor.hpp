#pragma once

#include "lve_scene.hpp"
#include "lve_camera.hpp"
#include "lve_model.hpp"

// std
#include <memory>
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
    // One placed instance. `preset` selects which mesh (0 = quad today); the
    // rest is a plain TRS transform. rotation is euler radians
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

    // All quad instances share one model via RenderItem's non-owning pointer
    // ground is the same mesh scaled out; marker flags the selection
    std::unique_ptr<LveModel> quadModel;
    std::unique_ptr<LveModel> groundModel;
    std::unique_ptr<LveModel> markerModel;

    LveCamera camera{};

    // Keys fire once per press, not per frame held
    bool prevLeft = false, prevRight = false, prevSpace = false, prevEnter = false;

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
