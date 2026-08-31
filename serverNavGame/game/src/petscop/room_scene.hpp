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
#include "petscop/texture_cache.hpp"

#include "petscop/dialog_box.hpp"
#include "petscop/events.hpp"
#include "petscop/house_map.hpp"
#include "petscop/game_state.hpp"
#include "petscop/pause_menu.hpp"
#include "petscop/player_watch.hpp"
#include "petscop/prop.hpp"
#include "petscop/interactions.hpp"
#include "petscop/other.hpp"
#include "petscop/outside.hpp"
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
  // Both of these are set from the progress marker at startup, and swapped again
  // when a door carries him on to the next area
  std::string mapPath = "maps/petscop.map";

  petscop::GameMap map;
  petscop::ModelCache models;      // is saved between rooms
  petscop::TextureCache textures;  // and so are the pictures
  int currentRoom = -1;

  // How far one width of a picture reaches, across a floor and up a wall
  // Nothing wears a wall picture today, walltex in the mapsrc is what turns one on
  float floorTileMetres = 3.f;
  float wallTileMetres = 1.8f;

  // The room as it was actually built, which an event may have changed
  // Points either into map.rooms or at the director's own copy
  const petscop::MapRoom* liveRoom = nullptr;

  // Items, flags, and how each room was left. Delete the file to start over
  // One save per map, or the forest writes its rooms over the house's memories
  petscop::GameState state;
  std::string savePath = "saves/petscop.save";

  // Which area the last run reached, kept beside the saves rather than inside one
  // Delete saves/ and the game starts over in area one
  std::string progressPath = "saves/progress.save";
  int areaIndex = 0;

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

  // Where he came into this room, kept so a fall can put him back on the doorstep
  glm::vec3 arriveSpawn{0.f};
  float arriveYaw = 0.f;
  int arrivedDoor = -1;

  // How far under the floor counts as off the map, -Y is up so a fall grows y
  float fallLimit = 8.f;

  // Prop and its Motion can be found in petscop/prop.hpp
  using Prop = petscop::Prop;
  std::vector<Prop> props;

  // A box with no collision sent to collision system
  struct Door {
    // What the map calls it, matched against the area's way on
    std::string name;

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

  // The house as the pause menu draws it, redrawn whenever the room changes
  lve::LveCanvas houseMap;

  // Walking into a door does a fade and swaps the room
  enum class Phase { Playing, FadingOut, FadingIn };
  Phase phase = Phase::Playing;
  float fade = 0.f;  // 0 clear, 1 black
  float fadeSeconds = 0.25f;
  int pendingRoom = -1;
  int pendingDoor = -1;

  // Set instead of pendingRoom when the fade is carrying him to the next area
  int pendingArea = -1;

  // Dialog box, and the thing that reads E and runs a prop's list of actions
  // Script and Actions are in petscop/interactions.hpp
  petscop::DialogBox dialog;
  petscop::InteractionRunner interactions;
  float clock = 0.f;

  // ESC stops the walk, in petscop/pause_menu.hpp
  petscop::PauseMenu menu;

  // Shouts on the console when he moves in a way walking cannot explain
  // In petscop/player_watch.hpp, and only there to chase the jolt in the forest
  petscop::PlayerWatch watch;

  // What shoved him hardest last settle, looked up off collisions.lastPusher
  std::string pusherName() const;

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

  // Throws out the whole area and stands the next one up, map and save and all
  void enterArea(int area);

  // The bed running under the room he is in, or empty where a room keeps quiet
  std::string roomBed;

  // Starts the room's bed and stops the last one. Called every frame, so a bed
  // something else silenced comes back on its own
  void keepRoomBed();

  // Writes the save down, stamped with the time it was written
  void save();

  // Hands the save to somebody else for however long the game was shut
  void letHimPlay();

  // The room the save left him in, or the map's own start, or wherever an event
  // would rather he woke up
  int startingRoom();

  // Puts him back where he came in after he drops through the floor
  void recoverFromFall();

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

  // Which way forward points in this room, before an event turns it round
  float roomYaw = 0.f;

  // How much of a tree's width you can actually walk into, trunk against canopy
  float trunkFootprint = 0.32f;
};
