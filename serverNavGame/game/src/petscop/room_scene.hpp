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

#include "petscop/dialog_box.hpp"
#include "petscop/events.hpp"
#include "petscop/game_state.hpp"
#include "petscop/prop.hpp"
#include "petscop/interactions.hpp"
#include "petscop/save_file.hpp"

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

// Walks a character through a map of rooms
//
// WASD walk, space jump
class RoomScene : public lve::LveScene {
 public:
  void loadModels() override;
  void update(float dt) override;
  void cleanup() override;

 private:
  // Built by tools/build_map.py, read from the repo root like the meshes are
  // Walking area two while it is being laid out -- area one is maps/petscop.map,
  // and the save on the line below has to be swapped back with it
  std::string mapPath = "maps/forest.map";

  petscop::GameMap map;
  petscop::ModelCache models;  // is saved between rooms
  int currentRoom = -1;

  // The room as it was actually built, which an event may have changed
  // Points either into map.rooms or at the director's own copy
  const petscop::MapRoom* liveRoom = nullptr;

  // Items, flags, and how each room was left. Delete the file to start over
  // One save per map, or the forest writes its rooms over the house's memories
  petscop::GameState state;
  std::string savePath = "saves/forest.save";

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

  // Prop and its Motion can be found in petscop/prop.hpp
  using Prop = petscop::Prop;
  std::vector<Prop> props;

  // A box with no collision sent to collision system
  struct Door {
    glm::vec3 translation{0.f};
    glm::vec3 rotation{0.f};
    glm::vec3 scale{1.f};
    ColliderComponent trigger{};

    // Fills the doorway while an event holds the doors shut
    // The wall has a real hole in it, so a trigger that does nothing is not enough
    ColliderComponent blocker{};

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

  // Walking into a door does a fade and swaps the room
  enum class Phase { Playing, FadingOut, FadingIn };
  Phase phase = Phase::Playing;
  float fade = 0.f;  // 0 clear, 1 black
  float fadeSeconds = 0.25f;
  int pendingRoom = -1;
  int pendingDoor = -1;

  // Dialog box, and the thing that reads E and runs a prop's list of actions
  // Script and Actions are in petscop/interactions.hpp
  petscop::DialogBox dialog;
  petscop::InteractionRunner interactions;
  float clock = 0.f;

  // Everything spooky the house does, in petscop/events.hpp
  petscop::EventDirector events;

  float hoverLift = 0.45f;
  float hoverMaxHeight = 2.0f;
  float hoverBob = 0.07f;
  float hoverBobSpeed = 2.6f;
  float hoverDotHeight = 0.018f;
  //

  // The backdrop the room floats over
  bool bgEnabled = true;
  int bgBars = 14;         // bars across the screen
  float bgBarFill = 0.45f; // how much of a bar's slot is inked
  float bgSpeed = 0.18f;   // screens travelled per second

  // How far the bars have marched, kept separately from the clock
  float bgPhase = 0.f;

  // How far the far edge of a bar leans across
  float bgSlant = 0.8f;  // bar angles
  glm::vec3 bgWash{0.02f, 0.02f, 0.05f};
  glm::vec3 bgBar{0.07f, 0.05f, 0.10f};

  // Throws out the old room and starts the character up in the new one
  // arriveDoor is which door of the new room he steps out of
  void enterRoom(int roomIndex, int arriveDoor = -1);

  // The room the save left him in, or the map's own start, or wherever an event
  // would rather he woke up
  int startingRoom();

  void emitHoverBox();

  // Fills backgroundItems with the moving backdrop behind the room
  void emitBackground();

  // Blocks walls that cover the camera
  void updateWallVisibility();

  // A wall is hidden when its outside is turned toward the camera
  // Zero would be the plain "more than 90 degrees away" test; going above it also
  // drops walls seen nearly edge on, which is what opens up a narrow room
  float hideThreshold = 0.25f;

  float groundY = 0.5f;  // the floor the map is built on

  // How much of a tree's width you can actually walk into, trunk against canopy
  float trunkFootprint = 0.32f;
};
