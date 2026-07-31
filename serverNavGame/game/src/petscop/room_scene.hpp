#pragma once

#include "lve_scene.hpp"
#include "lve_camera.hpp"
#include "lve_model.hpp"
#include "lve_text.hpp"

#include "collider_component.hpp"
#include "collision_system.hpp"
#include "keyboard_movement_component.hpp"
#include "lve_game_object.hpp"
#include "rigidbody_component.hpp"
#include "skinned_model_component.hpp"

#include "petscop/map_loader.hpp"
#include "petscop/model_cache.hpp"

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

// Walks a character through a map of rooms
//
// One room is standing at a time. Walk into a doorway and the screen fades out,
// the room is swapped for whatever that door leads to, and it fades back in with
// the character just inside the door on the far side
//
// WASD walks, space jumps. The camera holds still for the whole room and cuts to
// a new spot on the way in, so W always walks away from the camera
class RoomScene : public lve::LveScene {
 public:
  void loadModels() override;
  void update(float dt) override;
  void cleanup() override;

 private:
  // Built by tools/build_map.py, read from the repo root like the meshes are
  std::string mapPath = "maps/petscop.map";

  petscop::GameMap map;
  petscop::ModelCache models;  // outlives every room, so a door loads nothing
  int currentRoom = -1;

  // The character, put together the same way the skinned demo puts its man together
  // The component pointers are kept so update does not have to look them up
  GameObject player = GameObject::createGameObject();
  SkinnedModelComponent* playerSkin = nullptr;
  KeyboardMovementComponent* playerMover = nullptr;
  RigidbodyComponent* playerBody = nullptr;
  ColliderComponent* playerCollider = nullptr;

  // Half width of his box, so outstretched arms do not widen his footprint
  float playerRadius = 0.35f;

  float jumpSpeed = 4.5f;  // upward, so it goes in as -Y
  bool jumpPrevDown = false;

  // Something solid standing in the room
  //
  // The box is held here by value, which is why the collision system must not be
  // handed a pointer to it until props has stopped growing
  struct Prop {
    lve::LveModel* model = nullptr;
    glm::vec3 translation{0.f};
    glm::vec3 rotation{0.f};
    glm::vec3 scale{1.f};
    ColliderComponent collider{};
  };
  std::vector<Prop> props;

  // A doorway
  //
  // Its box is never given to the collision system, you are meant to walk through it
  struct Door {
    glm::vec3 translation{0.f};
    glm::vec3 rotation{0.f};
    glm::vec3 scale{1.f};
    ColliderComponent trigger{};

    int toRoom = -1;  // -1 leads nowhere, walking in does nothing
    int toDoor = -1;
    glm::vec3 spawn{0.f};
    float spawnYaw = 0.f;

    // The door you just came out of does not send you back until you step off it
    bool armed = true;
  };
  std::vector<Door> doors;

  // Declared after props and doors, so it lets go of their boxes before they go
  CollisionSystem collisions;

  lve::LveCamera camera{};
  glm::vec3 cameraEye{0.f};
  glm::vec3 cameraLook{0.f};

  std::unique_ptr<lve::LveTextRenderer> textRenderer;  // room name, and the fade quad

  // Walking into a door does not swap the room there and then
  // It starts a fade, and the swap happens at the bottom of it, when nothing else
  // is reading the world
  enum class Phase { Playing, FadingOut, FadingIn };
  Phase phase = Phase::Playing;
  float fade = 0.f;  // 0 clear, 1 black
  float fadeSeconds = 0.25f;
  int pendingRoom = -1;
  int pendingDoor = -1;

  // Throws out the old room and stands the character up in the new one
  // arriveDoor is which door of the new room he steps out of, or -1 to start in
  // the middle of it
  void enterRoom(int roomIndex, int arriveDoor);

  // Same translate, rotate, scale order the editor saves in, so a box lands
  // exactly where its mesh is drawn
  static glm::mat4 placement(const glm::vec3& t, const glm::vec3& r, const glm::vec3& s);

  float groundY = 0.5f;  // the floor the map is built on
};
