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
class DialogBox;
class ModelCache;

// What the haunting is allowed to reach into
struct Stage {
  GameMap* map = nullptr;
  GameState* state = nullptr;
  std::vector<Prop>* props = nullptr;
  ModelCache* models = nullptr;
  TransformComponent* player = nullptr;
  DialogBox* dialog = nullptr;
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
  // Returns false when the door is not to be taken at all
  bool reroute(int& toRoom, int& toDoor);

  // The room to start up in, which is not always the one the save left him in
  int wakeRoom(int fallback);

  // --- what the scene reads back each frame ---
  bool locksDoors() const { return locked; }
  bool holdsBlack() const { return black > 0.f; }
  bool takesControl() const { return frozen; }

  // Writes over the room's fixed camera while an event has hold of it
  bool cameraOverride(glm::vec3& eye, glm::vec3& look) const;

  // A multiplier on one of the room's own lights
  float lightGain(std::size_t index) const;

  float backgroundSpeed(float normal) const { return normal * bgScale; }
  glm::vec4 ambient(const glm::vec4& normal) const { return tinted ? tint : normal; }

  // Props an event conjured, drawn by the scene and nothing else
  const std::vector<Prop>& extras() const { return spawned; }

  // A room an event wants you moved to, handed over once
  bool takeWarp(int& room, int& door);

  // Which door of this room is plugged shut, or -1 for none
  int sealedDoor() const { return sealed; }

 private:
  // --- progression: four things to finish before building two opens ---
  int questsLeft() const;
  int questsDone() const;
  void stoneAndGate();          // turn the rock three times, put the gate back down
  void ballroomTiles(bool playing);  // cross to the piano on the pale tiles only
  void terraceDoor();           // the door that will not open yet

  // Where a named prop stands now, whether or not you are in its room
  bool placeOf(const std::string& room, const std::string& name, glm::vec3& translation,
               glm::vec3& rotation) const;

  // What the map file stood a prop at, before anything moved it
  const MapObject* mapObject(const std::string& room, const std::string& name) const;

  // How many times you have walked into a room, kept in the save as an item
  int visits(const std::string& room) const;
  int findRoom(const std::string& name) const;
  int findDoor(int room, const std::string& name) const;

  // The live prop by that name, or null
  Prop* prop(const std::string& name);

  // Stands a prop that only exists to be looked at
  void conjure(const std::string& mesh, const glm::vec3& t, const glm::vec3& r,
               const glm::vec3& s);

  // Which mesh a room's objects mean by that name, or -1 when the map has none
  int preset(const std::string& mesh) const;

  // Stands one more thing in the room before it is built, box, name and all
  // Unlike conjure this one is real -- you walk into it and can press E on it
  void addObject(MapRoom& room, const glm::vec3& t, const glm::vec3& r, const glm::vec3& s,
                 const std::string& name, const std::string& words, bool solid = true);

  // Replaces what pressing E on a prop does, flips and all
  void rewrite(const std::string& name, const std::string& words);

  // One light pulled down on its own, on top of whatever the room is doing
  void setLight(std::size_t index, float value);

  // Rooms an event changes before they are built
  void foyerPull(MapRoom& room);        // E03
  void hallStretch(MapRoom& room);      // E11
  void yardEarth(MapRoom& room);        // E14 names the grass, E17 digs the patches
  void bathroomSink(MapRoom& room);     // E18
  void greenhousePanes(MapRoom& room);  // E28
  void shedSeam(MapRoom& room);         // E35

  // E12: sends a door out of the hall back into the hall, once
  bool hallGivesBack(int& toRoom, int& toDoor);

  // Events that put props right as the room stands up
  void foyerTree();      // E01
  void yardTuft();       // E15
  void shedBoard();      // E36
  void ballroomStage();  // E24 takes the piano away, E25 brings it back
  void standAtDoors();   // E39

  // Events that only set an override, worked out again every frame
  void blackout(float dt);                       // E13
  void foyerLight();                             // E05
  void foyerCamera(float dt);                    // E03
  void closetShutIn(float dt, int startedProp);  // E07
  void hallLightBehind();                        // E09
  void hallFootprints();                         // E10
  void yardPath();                               // E14
  void bathroomWater(float dt, bool playing);    // E19
  void billiardWord();                           // E21, and E22 once it is spelt
  void ballroomPiano(float dt, bool playing);    // E24
  void ballroomWalker(float dt, bool playing);   // E27
  void greenhouseShape(float dt);                // E30
  void fieldEdge(bool playing);                  // E32
  void turnToCamera(float dt);                   // E40
  void lateLights();                             // E41

  Stage stage;

  int room = -1;
  std::string roomName;
  int roomVisits = 0;
  float sinceEntry = 0.f;

  // The room as it was actually built, which dress() may have changed
  const MapRoom* built = nullptr;

  // Standing still with nothing held down
  float idle = 0.f;
  glm::vec3 lastPos{0.f};

  // Whether he covered ground this frame, and whether he did last frame
  bool moved = false;
  bool wasMoving = false;

  bool locked = false;
  bool frozen = false;
  float black = 0.f;  // seconds the screen is held shut

  // Every light in the room is scaled by this, and any one of them on its own
  float gain = 1.f;
  std::vector<float> gains;

  float bgScale = 1.f;
  bool tinted = false;
  glm::vec4 tint{1.f};

  std::vector<Prop> spawned;

  int warpRoom = -1;
  int warpDoor = -1;

  // The room copy dress() hands back in place of the real one
  MapRoom dressed;

  // Where the camera is this frame, and whether an event put it there
  bool hasCam = false;
  glm::vec3 camEye{0.f};
  glm::vec3 camLook{0.f};

  // The spot the foyer camera is drifting after
  glm::vec3 follow{0.f};

  // How long the closet has had you, and whether it is going to
  float shutIn = -1.f;

  // Doors taken in quick succession, and how long since the last one
  int mash = 0;
  bool turnAround = false;

  // Where he came into the hall, and the two ends of it he has reached
  float entryX = 0.f;
  bool sawWest = false;
  bool sawEast = false;

  // Everywhere he came to a stop in this room
  std::vector<glm::vec3> stops;

  // The piano only plays itself once per visit, and later stands there again
  bool pianoPlayed = false;
  bool pianoBack = false;

  // The walker a beat behind him: time to his next step, and to its late one
  float stepAt = 0.f;
  float echoAt = -1.f;

  // He is being started up somewhere he did not leave off
  bool waking = false;

  // The shape over the greenhouse, and the tap you cannot find
  float shapeAt = -1.f;
  float waterAt = -1.f;

  // You have to step off the corner before leaning on it counts again
  bool onEdge = false;

  // The tile crossing is unbroken, and the terrace door has already spoken
  bool tileRun = false;
  bool toldDoor = false;
  int sealed = -1;

  std::mt19937 rng{std::random_device{}()};
};

}  // namespace petscop
