#pragma once

#include "petscop/prop.hpp"

#include <glm/glm.hpp>

#include <cstddef>
#include <random>
#include <string>
#include <vector>

struct TransformComponent;

namespace petscop {

struct GameState;
class ModelCache;

// What the haunting is allowed to reach into
struct Stage {
  GameMap* map = nullptr;
  GameState* state = nullptr;
  std::vector<Prop>* props = nullptr;
  ModelCache* models = nullptr;
  TransformComponent* player = nullptr;
};

// Everything the house does that the map file cannot say on its own
//
// The scene calls the hooks and then reads the overrides back out. Nothing here
// ever pushes onto the room's own prop list -- the collision system holds raw
// pointers into it, so anything an event conjures goes in extras() instead
class EventDirector {
 public:
  void bind(const Stage& stage);
  void reset();

  // The room the scene is about to build, with whatever an event changes about it
  const MapRoom& dress(const MapRoom& room, int index);

  // The room is standing and its props are back the way they were left
  void onEnterRoom(int index);

  // startedProp is the prop a press just set going, or -1 for none
  void update(float dt, bool playing, int startedProp);

  // Where a door leads, which an event may point somewhere else
  void reroute(int& toRoom, int& toDoor);

  // --- what the scene reads back each frame ---
  bool locksDoors() const { return locked; }
  bool holdsBlack() const { return black > 0.f; }
  bool takesControl() const { return frozen; }

  // A multiplier on one of the room's own lights
  float lightGain(std::size_t index) const;

  float backgroundSpeed(float normal) const { return normal * bgScale; }
  glm::vec4 ambient(const glm::vec4& normal) const { return tinted ? tint : normal; }

  // Props an event conjured, drawn by the scene and nothing else
  const std::vector<Prop>& extras() const { return spawned; }

  // A room an event wants you moved to, handed over once
  bool takeWarp(int& room, int& door);

 private:
  // How many times you have walked into a room, kept in the save as an item
  int visits(const std::string& room) const;
  int findRoom(const std::string& name) const;
  int findDoor(int room, const std::string& name) const;

  // The live prop by that name, or null
  Prop* prop(const std::string& name);

  // Stands a prop that only exists to be looked at
  void conjure(const std::string& mesh, const glm::vec3& t, const glm::vec3& r,
               const glm::vec3& s);

  // Replaces what pressing E on a prop does, flips and all
  void rewrite(const std::string& name, const std::string& words);

  // Events that put props right as the room stands up
  void foyerTree();   // E01
  void yardTuft();    // E15
  void shedBoard();   // E36

  // Events that only set an override, worked out again every frame
  void blackout(float dt);                       // E13
  void foyerLight();                             // E05
  void closetShutIn(float dt, int startedProp);  // E07
  void bathroomWater(float dt, bool playing);    // E19
  void billiardWord();                           // E21
  void ballroomPiano(float dt, bool playing);    // E24
  void greenhouseShape(float dt);                // E30
  void fieldEdge(bool playing);                  // E32

  Stage stage;

  int room = -1;
  std::string roomName;
  int roomVisits = 0;
  float sinceEntry = 0.f;

  // Standing still with nothing held down
  float idle = 0.f;
  glm::vec3 lastPos{0.f};

  bool locked = false;
  bool frozen = false;
  float black = 0.f;  // seconds the screen is held shut

  // Every light in the room is scaled by this, and one may be killed outright
  float gain = 1.f;
  int killedLight = -1;

  float bgScale = 1.f;
  bool tinted = false;
  glm::vec4 tint{1.f};

  std::vector<Prop> spawned;

  int warpRoom = -1;
  int warpDoor = -1;

  // The room copy E11 hands back in place of the real one
  MapRoom dressed;

  // How long the closet has had you, and whether it is going to
  float shutIn = -1.f;

  // Doors taken in quick succession, and how long since the last one
  int mash = 0;
  bool turnAround = false;

  // The piano only plays itself once per visit
  bool pianoPlayed = false;

  // The shape over the greenhouse, and the tap you cannot find
  float shapeAt = -1.f;
  float waterAt = -1.f;

  // You have to step off the corner before leaning on it counts again
  bool onEdge = false;

  std::mt19937 rng{std::random_device{}()};
};

}  // namespace petscop
