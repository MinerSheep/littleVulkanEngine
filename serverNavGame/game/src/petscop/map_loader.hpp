#pragma once

#include <glm/glm.hpp>

#include <string>
#include <vector>

// A map is a handful of rooms and the doors between them
//
// The file is written by **tools/build_map.py**, which has already worked out the
// walls, which door leads where, and where you stand when you come through one.
// Nothing here does any geometry, it only reads numbers off the page
namespace petscop {


// --------- ACTIONS ---------------------------
// Certain map objects are interactable, here's what they can do
enum class ActionKind {
  Say,
  Move,
  Rotate,
  Scale,
  Show,
  Hide,
  Sound,
  Give,    // puts an item in your pocket
  Take,    // takes one back out
  Flag,    // remembers that something happened
  Unflag,
};

// What has to be true of the save before an action runs
enum class ActionWhen {
  Always,
  Flag,
  NoFlag,
  Item,
  NoItem,
};

struct MapAction {
  ActionKind kind = ActionKind::Say;

  std::string target;    // gameobject target
  std::string text;      // - say -, and the item or flag a give or flag names
  glm::vec3 amount{0.f}; // magnitude
  float seconds = 0.f;   // duration
  bool toggle = false;

  int count = 1;  // how many of an item a give or take moves

  ActionWhen when = ActionWhen::Always;
  std::string whenName;  // the flag or item the 'when' asks about
};
// ----------------------------------------------



// One thing standing in a room
struct MapObject {
  int preset = 0;  // which mesh, an index into GameMap::presets
  glm::vec3 translation{0.f};
  glm::vec3 rotation{0.f};  // euler radians
  glm::vec3 scale{1.f};

  // Which way a wall faces out of the room, so the scene can drop a wall that
  // stands between the camera and the room
  // All zeroes for anything that is not a wall, and those are never hidden
  glm::vec3 face{0.f};

  // Scenery like grass has no box, you walk straight through it
  bool solid = true;

  std::string name;
  std::vector<MapAction> actions;
};

// A way out of a room
//
// The box you walk into is not stored here. The scene builds it from the placement
// below, the same way a prop builds its own, so the two can never drift apart
struct MapDoor {
  std::string name;
  glm::vec3 translation{0.f};
  glm::vec3 rotation{0.f};
  glm::vec3 scale{1.f};

  // Where it goes. toRoom is -1 for a door that leads nowhere yet, and walking
  // into one of those does nothing
  int toRoom = -1;
  int toDoor = -1;

  // Where you stand coming out the other side, and which way you face
  glm::vec3 spawn{0.f};
  float spawnYaw = 0.f;
};

struct MapLight {
  glm::vec3 position{0.f};
  glm::vec3 color{1.f};
  float intensity = 1.f;
};

// The camera holds still while you are in a room and cuts to a new spot when you
// walk into the next one
struct MapRoom {
  std::string name;

  // Whether the name goes in the corner of the screen
  // The forest keeps its rooms nameless, so you cannot tell one from another
  bool showName = true;

  glm::vec3 size{1.f};  // width, depth, height
  glm::vec3 cameraEye{0.f};
  glm::vec3 cameraLook{0.f};
  std::vector<MapObject> objects;
  std::vector<MapDoor> doors;
  std::vector<MapLight> lights;
};

struct GameMap {
  std::string name;
  int startRoom = 0;
  std::vector<std::string> presets;  // mesh names, "cube" means models/cube.obj
  std::vector<MapRoom> rooms;
};

// Reads a .map built by tools/build_map.py
// Returns false and fills in error when the file will not load, so the caller can
// say what was wrong with which line
bool loadMap(const std::string& path, GameMap& out, std::string& error);

}  // namespace petscop
