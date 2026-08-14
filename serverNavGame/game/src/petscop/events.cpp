#include "petscop/events.hpp"

#include "lve_game_object.hpp"
#include "petscop/dialog_box.hpp"
#include "petscop/game_state.hpp"
#include "petscop/model_cache.hpp"

#include <lve_audio.hpp>
#include <lve_engine.hpp>
#include <lve_window.hpp>

#include <glm/gtc/constants.hpp>

#include <cmath>

// The house doing things the map file cannot say on its own
//
// Two kinds of event live here. One kind puts props right the moment a room
// stands up, and runs from onEnterRoom. The other kind only sets an override --
// a light gain, a tint, a lock -- and is worked out again from scratch every
// frame, so nothing has to be undone when it stops
namespace petscop {

namespace {

// How long a room has to be left in for the next door not to count as mashing
const float mashWindow = 1.2f;
const int mashLimit = 6;

// Keys that mean you are still playing
const int watchedKeys[] = {GLFW_KEY_W, GLFW_KEY_A, GLFW_KEY_S, GLFW_KEY_D,
                           GLFW_KEY_SPACE, GLFW_KEY_E};

// BYGONE, three dots across and five down, top row first
const char* word[6][5] = {
    {"110", "101", "110", "101", "110"},  // B
    {"101", "101", "010", "010", "010"},  // Y
    {"011", "100", "101", "101", "011"},  // G
    {"010", "101", "101", "101", "010"},  // O
    {"101", "111", "111", "101", "101"},  // N
    {"111", "100", "110", "100", "111"},  // E
};

// The four things building two waits on
const char* quests[] = {"quest_stone", "quest_mirror", "quest_tiles", "quest_dig"};
const int questCount = 4;

// The pale tiles across the ballroom floor, one row per 2 units of depth and one
// letter per 2 across. They run from the west doorway to the piano, and the only
// way through is the way they go
const char* tiles[6] = {
    ".......",
    ".WWWWW.",
    "WW.W...",
    "W..WW..",
    "W......",
    ".......",
};

// Where somebody else has been digging in the yard, in the order they turn up
const float digSpots[4][2] = {{-9.5f, 4.2f}, {2.5f, -2.0f}, {8.5f, 3.4f}, {-2.5f, -1.6f}};

// A game of billiards going on without you, six frames of it, on the table. Each
// one is a shot on from the one before, and the first ball is the cue
const float frames[6][6][2] = {
    {{-2.1f, 1.00f}, {1.6f, 0.75f}, {1.9f, 1.00f}, {1.9f, 0.50f}, {2.2f, 1.25f}, {2.2f, 0.75f}},
    {{0.4f, 0.95f}, {1.2f, 0.55f}, {1.8f, 1.35f}, {2.3f, 0.60f}, {0.9f, 1.60f}, {2.4f, 1.10f}},
    {{-0.8f, 1.40f}, {1.2f, 0.55f}, {1.5f, 1.70f}, {2.3f, 0.60f}, {0.1f, 1.90f}, {2.4f, 1.10f}},
    {{1.1f, 0.30f}, {0.6f, 0.60f}, {1.5f, 1.70f}, {2.5f, 0.35f}, {0.1f, 1.90f}, {2.4f, 1.10f}},
    {{-1.6f, 0.60f}, {0.6f, 0.60f}, {-0.4f, 1.80f}, {2.5f, 0.35f}, {0.1f, 1.90f}, {1.7f, 1.50f}},
    {{-2.3f, 1.60f}, {-1.0f, 0.90f}, {-0.4f, 1.80f}, {2.5f, 0.35f}, {-1.8f, 0.40f}, {1.7f, 1.50f}},
};

// A flag as the board would print it
std::string shout(const std::string& flag) {
  std::string out;
  out.reserve(flag.size());
  for (char letter : flag) {
    if (letter == '_') out += ' ';
    else if (letter >= 'a' && letter <= 'z') out += static_cast<char>(letter - 'a' + 'A');
    else out += letter;
  }
  return out;
}

}  // namespace

void EventDirector::bind(const Stage& newStage) {
  stage = newStage;

  // Meshes only an event ever stands up, so none of them loads mid frame
  if (stage.models) {
    stage.models->get("cube");
    stage.models->get("sphere");
  }
}

void EventDirector::reset() {
  spawned.clear();
  stops.clear();
  room = -1;
  roomName.clear();
  built = nullptr;
  locked = false;
  frozen = false;
  black = 0.f;
  gain = 1.f;
  gains.clear();
  bgScale = 1.f;
  tinted = false;
  hasCam = false;
  warpRoom = -1;
  warpDoor = -1;
  shutIn = -1.f;
  shapeAt = -1.f;
  waterAt = -1.f;
  echoAt = -1.f;
  waking = false;
}

// --- odds and ends ----------------------------------------------------------

int EventDirector::visits(const std::string& name) const {
  return stage.state ? stage.state->itemCount("@visits." + name) : 0;
}

int EventDirector::findRoom(const std::string& name) const {
  if (!stage.map) return -1;
  for (std::size_t i = 0; i < stage.map->rooms.size(); i++) {
    if (stage.map->rooms[i].name == name) return static_cast<int>(i);
  }
  return -1;
}

int EventDirector::findDoor(int index, const std::string& name) const {
  if (!stage.map || index < 0 || index >= static_cast<int>(stage.map->rooms.size())) return -1;
  const std::vector<MapDoor>& list = stage.map->rooms[index].doors;
  for (std::size_t i = 0; i < list.size(); i++) {
    if (list[i].name == name) return static_cast<int>(i);
  }
  return -1;
}

const MapObject* EventDirector::mapObject(const std::string& roomName,
                                          const std::string& name) const {
  const int index = findRoom(roomName);
  if (index < 0) return nullptr;

  for (const MapObject& object : stage.map->rooms[index].objects) {
    if (object.name == name) return &object;
  }
  return nullptr;
}

// The live prop if you are standing in its room, the save's memory of it if you
// are not, and where the map stood it if nothing has ever touched it
bool EventDirector::placeOf(const std::string& where, const std::string& name,
                            glm::vec3& translation, glm::vec3& rotation) const {
  if (where == roomName && stage.props) {
    const int index = findProp(*stage.props, name);
    if (index >= 0) {
      const Prop& live = (*stage.props)[index];
      translation = live.motion.active ? live.motion.toTranslation : live.translation;
      rotation = live.motion.active ? live.motion.toRotation : live.rotation;
      return true;
    }
  }

  if (stage.state) {
    const std::map<std::string, PropMemory>::const_iterator found =
        stage.state->memories.find(where + "." + name);
    if (found != stage.state->memories.end()) {
      translation = found->second.translation;
      rotation = found->second.rotation;
      return true;
    }
  }

  const MapObject* rest = mapObject(where, name);
  if (!rest) return false;
  translation = rest->translation;
  rotation = rest->rotation;
  return true;
}

Prop* EventDirector::prop(const std::string& name) {
  if (!stage.props) return nullptr;
  const int index = findProp(*stage.props, name);
  if (index < 0) return nullptr;
  return &(*stage.props)[index];
}

void EventDirector::conjure(const std::string& mesh, const glm::vec3& t, const glm::vec3& r,
                            const glm::vec3& s) {
  if (!stage.models) return;

  Prop extra;
  extra.model = stage.models->get(mesh);
  if (!extra.model) return;

  extra.translation = t;
  extra.rotation = r;
  extra.scale = s;
  spawned.push_back(extra);
}

void EventDirector::rewrite(const std::string& name, const std::string& words) {
  Prop* target = prop(name);
  if (!target) return;

  MapAction say;
  say.kind = ActionKind::Say;
  say.text = words;
  target->actions.assign(1, say);
  target->flipped.assign(1, 0);
}

int EventDirector::preset(const std::string& mesh) const {
  if (!stage.map) return -1;
  for (std::size_t i = 0; i < stage.map->presets.size(); i++) {
    if (stage.map->presets[i] == mesh) return static_cast<int>(i);
  }
  return -1;
}

// Everything an event adds to a room is a cube, which is every shape the house
// puts up on its own -- a slab of earth, a pane of glass, a seam in the floor
void EventDirector::addObject(MapRoom& room, const glm::vec3& t, const glm::vec3& r,
                              const glm::vec3& s, const std::string& name,
                              const std::string& words, bool solid) {
  const int cube = preset("cube");
  if (cube < 0) return;

  MapObject extra;
  extra.preset = cube;
  extra.translation = t;
  extra.rotation = r;
  extra.scale = s;
  extra.solid = solid;
  extra.name = name;

  if (!words.empty()) {
    MapAction say;
    say.kind = ActionKind::Say;
    say.text = words;
    extra.actions.push_back(say);
  }
  room.objects.push_back(extra);
}

void EventDirector::setLight(std::size_t index, float value) {
  if (gains.size() <= index) gains.resize(index + 1, 1.f);
  gains[index] = value;
}

// --- the room about to be built ---------------------------------------------

// Every room comes through as a copy, and a helper may change anything on it --
// its size, its camera, the things standing in it. The map never knows
const MapRoom& EventDirector::dress(const MapRoom& source, int index) {
  dressed = source;

  if (source.name == "Foyer") foyerPull(dressed);
  else if (source.name == "Hall_Main") hallStretch(dressed);
  else if (source.name == "Yard") yardEarth(dressed);
  else if (source.name == "Bathroom") bathroomSink(dressed);
  else if (source.name == "Greenhouse") greenhousePanes(dressed);
  else if (source.name == "Shed") shedSeam(dressed);

  built = &dressed;
  return dressed;
}

// E03: every fourth visit the foyer camera comes in a step closer, and it never
// goes back out. foyerCamera is the half that walks after him
void EventDirector::foyerPull(MapRoom& room) {
  const int step = (visits(room.name) + 1) / 4;
  if (step < 1) return;

  const float close = 1.f - 0.16f * static_cast<float>(step);
  const float pull = close < 0.34f ? 0.34f : close;
  room.cameraEye = room.cameraLook + (room.cameraEye - room.cameraLook) * pull;
}

// E11: the hall is half as long again from the fourth time you walk into it, and
// stays that way. Stretching the room data is the whole trick -- walls, doors,
// spawn points and lights all come out of it further apart
void EventDirector::hallStretch(MapRoom& room) {
  if (visits(room.name) + 1 < 4) return;

  const float stretch = 0.5f;
  const glm::vec3 back = room.cameraEye - room.cameraLook;
  room.size.x *= stretch;

  for (MapObject& object : room.objects) {
    object.translation.x *= stretch;
    object.scale.x *= stretch;
  }
  for (MapDoor& door : room.doors) {
    door.translation.x *= stretch;
    door.scale.x *= stretch;
    door.spawn.x *= stretch;
  }
  for (MapLight& light : room.lights) light.position.x *= stretch;

  // Back the camera off by the same amount, or the ends fall off the screen
  room.cameraEye = room.cameraLook + back * stretch;
}

// E14 gives every tuft a name, which is what gets it remembered
// E17 stands one more empty patch of turned earth in the yard every visit
void EventDirector::yardEarth(MapRoom& room) {
  int tuft = 0;
  for (MapObject& object : room.objects) {
    if (object.solid || !object.name.empty()) continue;
    object.name = "grass_" + std::to_string(tuft++);
  }

  if (!stage.state || !stage.state->hasFlag("quest_dig")) return;

  stage.state->addItem("@patches.yard", 1);
  int digs = stage.state->itemCount("@patches.yard");
  if (digs > 4) digs = 4;

  for (int i = 0; i < digs; i++) {
    addObject(room, glm::vec3(digSpots[i][0], 0.470f, digSpots[i][1]), glm::vec3(0.f),
              glm::vec3(0.6f, 0.03f, 0.6f), "dig_old_" + std::to_string(i),
              "THIS ONE IS ALREADY EMPTY.");
  }
}

// E18: heard the water three times and there is a sink on the wall the bathroom
// has never had. Finding it stops the water for good
void EventDirector::bathroomSink(MapRoom& room) {
  if (!stage.state || stage.state->itemCount("@water.heard") < 3) return;

  addObject(room, glm::vec3(3.600f, -0.350f, 0.500f), glm::vec3(0.f),
            glm::vec3(0.30f, 0.30f, 0.55f), "sink", "IT IS DRY.");
}

// E28: after the shape went over, a pane of glass is leaning on the north wall,
// and one more of them every visit until you are walking around the stack
void EventDirector::greenhousePanes(MapRoom& room) {
  if (!stage.state || !stage.state->hasFlag("saw_shape")) return;

  stage.state->addItem("@panes.greenhouse", 1);
  int panes = stage.state->itemCount("@panes.greenhouse");
  if (panes > 6) panes = 6;

  // Each one is propped further out than the last and lies over that bit more
  const float tall = 1.15f;
  for (int i = 0; i < panes; i++) {
    const float out = static_cast<float>(i);
    const float lean = 0.200f + 0.040f * out;
    addObject(room, glm::vec3(-3.400f, 0.500f - tall * std::cos(lean), 2.500f - 0.300f * out),
              glm::vec3(-lean, 0.f, 0.f), glm::vec3(1.35f, tall, 0.05f),
              i == 0 ? "panes" : "", i == 0 ? "SOMETHING HAS TO BE REPLACED." : "");
  }
}

// E35: a seam in the shed floor that does nothing at all, until the shed has put
// you out into the closet. Then it tells you what is under it
void EventDirector::shedSeam(MapRoom& room) {
  const bool under = stage.state && stage.state->hasFlag("shed_closet");

  addObject(room, glm::vec3(0.900f, 0.470f, 0.800f), glm::vec3(0.f, 0.350f, 0.f),
            glm::vec3(1.2f, 0.02f, 0.22f), "seam",
            under ? "THE CLOSET IS UNDER HERE." : "", false);
}

// --- the room is standing ---------------------------------------------------

void EventDirector::onEnterRoom(int index) {
  if (!stage.map || !stage.state) return;
  if (index < 0 || index >= static_cast<int>(stage.map->rooms.size())) return;

  // How long the room you just left held you, before it is thrown away
  if (room >= 0 && sinceEntry < mashWindow) mash++;
  else mash = 0;

  room = index;
  roomName = stage.map->rooms[index].name;
  sinceEntry = 0.f;
  idle = 0.f;
  locked = false;
  pianoPlayed = false;
  pianoBack = false;
  shutIn = -1.f;
  shapeAt = -1.f;
  waterAt = -1.f;
  echoAt = -1.f;
  stepAt = 0.f;
  onEdge = true;  // you have to step off a corner before leaning on it counts

  // Nothing about the last room's walking carries into this one
  stops.clear();
  sawWest = false;
  sawEast = false;

  stage.state->addItem("@visits." + roomName, 1);
  roomVisits = visits(roomName);

  // E39: he is not always started up in the room he quit in
  if (waking && roomName == "Terrace") standAtDoors();
  waking = false;

  if (stage.player) {
    lastPos = stage.player->translation;
    entryX = stage.player->translation.x;
  }
  if (built) follow = built->cameraLook;
  if (roomName == "Yard") stage.state->setFlag("seen_yard", true);

  // E13: doors taken one after another until the screen stops coming back
  if (mash >= mashLimit) {
    black = 10.f;
    turnAround = true;
    mash = 0;
    lve::LveAudio::instance().play("footsteps");
  }

  if (roomName == "Foyer") foyerTree();
  if (roomName == "Yard") yardTuft();
  if (roomName == "Shed") shedBoard();
  if (roomName == "Ballroom") ballroomStage();
}

// E39: he wakes up on the terrace whatever room he left off in, stood in front
// of the doors and looking straight at them
void EventDirector::standAtDoors() {
  if (!stage.player) return;

  const int door = findDoor(room, "doors");
  if (door < 0) return;

  const MapDoor& doors = stage.map->rooms[room].doors[door];
  stage.player->translation = doors.spawn;
  stage.player->rotation.y =
      std::atan2(doors.translation.x - doors.spawn.x, doors.translation.z - doors.spawn.z);
}

// E24: the piano is only in the middle of the floor the first time. E25: three
// things done and it is standing there again, moved, with the room lit one side
void EventDirector::ballroomStage() {
  Prop* piano = prop("piano");
  if (!piano) return;

  pianoBack = questsDone() >= 3;
  if (pianoBack) {
    piano->disappeared = false;
    piano->collider.enabled = true;
    rewrite("piano", "IT HAS BEEN MOVED.");
    return;
  }

  if (roomVisits < 2) return;
  piano->disappeared = true;
  piano->collider.enabled = false;
}

// E01: the tree goes on the third time you walk in, and the bare floor it stood
// on is what is left to press E on
void EventDirector::foyerTree() {
  Prop* tree = prop("tree");
  Prop* hole = prop("tree_hole");
  if (!tree || !hole) return;

  const bool taken = roomVisits >= 3 || stage.state->hasFlag("tree_taken");
  if (taken) stage.state->setFlag("tree_taken", true);

  tree->disappeared = taken;
  tree->collider.enabled = !taken;
  hole->disappeared = !taken;
  hole->collider.enabled = taken;
}

// E15: once the foyer has gone dark, one tuft of grass in the yard answers E
void EventDirector::yardTuft() {
  if (!stage.state->hasFlag("saw_dark_foyer")) return;
  if (stage.state->hasFlag("tuft_named")) return;
  if (!stage.models || !stage.props) return;

  lve::LveModel* grass = stage.models->get("grass");
  if (!grass) return;

  // Whichever tuft is nearest this spot, so it is the same one every run
  const glm::vec3 spot{-2.f, 0.5f, -3.f};
  int best = -1;
  float nearest = 0.f;
  for (std::size_t i = 0; i < stage.props->size(); i++) {
    const Prop& tuft = (*stage.props)[i];
    if (tuft.model != grass) continue;

    const float away = glm::distance(tuft.translation, spot);
    if (best >= 0 && away >= nearest) continue;
    best = static_cast<int>(i);
    nearest = away;
  }
  if (best < 0) return;

  MapAction say;
  say.kind = ActionKind::Say;
  say.text = "ELEANOR.";

  MapAction remember;
  remember.kind = ActionKind::Flag;
  remember.text = "tuft_named";

  Prop& tuft = (*stage.props)[best];
  tuft.actions.assign(1, say);
  tuft.actions.push_back(remember);
  tuft.flipped.assign(2, 0);
}

// E36: the board in the shed reads your own save back, and is one line ahead
void EventDirector::shedBoard() {
  std::string words = "THE BOARD IS A LIST.";

  int onPage = 0;
  for (const std::string& flag : stage.state->flags) {
    words += onPage == 0 ? "|" : " ";
    words += shout(flag);
    if (++onPage == 3) onPage = 0;
  }

  words += "|SAW YOU READING THIS";
  rewrite("board", words);
}

// --- every frame ------------------------------------------------------------

void EventDirector::update(float dt, bool playing, int startedProp) {
  sinceEntry += dt;

  // Everything an event overrides is worked out again from nothing each frame,
  // so an event that stops running leaves nothing behind to undo
  spawned.clear();
  gain = 1.f;
  gains.clear();
  bgScale = 1.f;
  tinted = false;
  frozen = false;
  locked = false;
  sealed = -1;
  hasCam = false;

  // Standing still with nothing held down
  bool holding = false;
  GLFWwindow* window = lve::LveEngine::instance().getGLFWWindow();
  for (int key : watchedKeys) holding = holding || glfwGetKey(window, key) == GLFW_PRESS;

  wasMoving = moved;
  moved = false;
  if (stage.player) {
    moved = playing && glm::distance(stage.player->translation, lastPos) > 0.01f;
    lastPos = stage.player->translation;
  }
  idle = (holding || moved || !playing) ? 0.f : idle + dt;

  // Where the room stands its camera, before an event walks off with it
  if (built) {
    camEye = built->cameraEye;
    camLook = built->cameraLook;
  }

  blackout(dt);

  // Reads the rock and the gate out of the save, so it finishes from any room
  stoneAndGate();

  if (roomName == "Ballroom") ballroomTiles(playing);
  if (roomName == "Terrace") terraceDoor();

  if (roomName == "Foyer") {
    foyerLight();
    foyerCamera(dt);
  }
  if (roomName == "Closet") closetShutIn(dt, startedProp);
  if (roomName == "Hall_Main") {
    hallLightBehind();
    hallFootprints();
  }
  if (roomName == "Yard") yardPath();
  if (roomName == "Bathroom") bathroomWater(dt, playing);
  if (roomName == "Billiard_Room") billiardWord();
  if (roomName == "Ballroom") {
    ballroomPiano(dt, playing);
    ballroomWalker(dt, playing);
    if (pianoBack) setLight(1, 0.f);  // E25: lit from one side, with it back
  }
  if (roomName == "Greenhouse") {
    bgScale = 0.f;  // E29: the only room the backdrop holds still in
    greenhouseShape(dt);
  }
  if (roomName == "Field" || roomName == "Field_Red") fieldEdge(playing);

  // The two that are the same in every room
  turnToCamera(dt);
  lateLights();
}

// E13: the screen is held shut, and he is facing the other way when it opens
void EventDirector::blackout(float dt) {
  if (black <= 0.f) return;

  frozen = true;
  black -= dt;
  if (black > 0.f) return;

  black = 0.f;
  if (turnAround && stage.player) stage.player->rotation.y += glm::pi<float>();
  turnAround = false;
}

// E05: the warm light over the foyer does not come back on after the yard
void EventDirector::foyerLight() {
  if (!stage.state->hasFlag("seen_yard")) return;
  setLight(0, 0.f);
  stage.state->setFlag("saw_dark_foyer", true);
}

// E03: once the foyer camera has started coming in it stops holding still, and
// drifts after him wherever he goes. It is always a little behind him
void EventDirector::foyerCamera(float dt) {
  if (!built || !stage.player || roomVisits / 4 < 1) return;

  glm::vec3 want = built->cameraLook;
  want.x = stage.player->translation.x;
  want.z = stage.player->translation.z;

  const float chase = dt * 1.6f;
  follow += (want - follow) * (chase > 1.f ? 1.f : chase);

  hasCam = true;
  camLook = follow;
  camEye = follow + (built->cameraEye - built->cameraLook);
}

// E07: now and then, turning the rock shuts you in for twenty seconds
void EventDirector::closetShutIn(float dt, int startedProp) {
  if (shutIn < 0.f) {
    if (startedProp < 0 || !stage.props) return;
    if (startedProp >= static_cast<int>(stage.props->size())) return;
    if ((*stage.props)[startedProp].name != "rock") return;

    std::uniform_int_distribution<int> odds(0, 2);
    if (odds(rng) != 0) return;
    shutIn = 0.f;
    return;
  }

  const float dark = 8.f;
  const float hold = 20.f;

  shutIn += dt;
  locked = true;

  if (shutIn < dark) gain = 1.f - shutIn / dark;
  else if (shutIn < hold) gain = 0.f;
  else gain = shutIn - hold;

  if (shutIn >= hold + 1.f) {
    shutIn = -1.f;
    locked = false;
    gain = 1.f;
  }
}

// E09: the light nearest the door you came in by goes out behind you, and lifts
// again as you walk back toward it
void EventDirector::hallLightBehind() {
  if (questsDone() < 1 || !built || !stage.player || built->lights.empty()) return;

  std::size_t behind = 0;
  for (std::size_t i = 1; i < built->lights.size(); i++) {
    if (std::fabs(built->lights[i].position.x - entryX) <
        std::fabs(built->lights[behind].position.x - entryX))
      behind = i;
  }

  // Steep enough to be wrong, smooth enough to pass for the light falling off
  const float near = built->size.x * 0.18f;
  const float far = built->size.x * 0.45f;
  const float away = std::fabs(stage.player->translation.x - built->lights[behind].position.x);
  const float lift = (far - away) / (far - near);
  setLight(behind, lift < 0.f ? 0.f : (lift > 1.f ? 1.f : lift));
}

// E10: walk the whole length of the hall and every place you stopped on the way
// stands up behind you at once, as a flat dark slab
void EventDirector::hallFootprints() {
  if (!built || !stage.player) return;

  const glm::vec3 here = stage.player->translation;
  const float end = built->size.x * 0.5f - 2.f;
  if (here.x < -end) sawWest = true;
  if (here.x > end) sawEast = true;

  // Where he came to a stop, never two of them on top of each other
  if (wasMoving && !moved && (stops.empty() || glm::distance(stops.back(), here) > 1.5f)) {
    if (stops.size() >= 14) stops.erase(stops.begin());
    stops.push_back(here);
  }

  if (questsDone() < 3 || !sawWest || !sawEast) return;
  for (const glm::vec3& stop : stops)
    conjure("cube", glm::vec3(stop.x, 0.470f, stop.z), glm::vec3(0.f),
            glm::vec3(0.30f, 0.02f, 0.46f));
}

// E14: every tuft he walks over is flattened out of sight and stays that way
// The yard wears a path shaped like the way he always crosses it
void EventDirector::yardPath() {
  if (questsDone() < 2 || !stage.player || !stage.props) return;

  const glm::vec3 here = stage.player->translation;
  for (Prop& tuft : *stage.props) {
    if (tuft.disappeared || tuft.name.rfind("grass_", 0) != 0) continue;

    // The one tuft with something to say is not walked flat
    if (!tuft.actions.empty()) continue;

    if (glm::distance(glm::vec2(tuft.translation.x, tuft.translation.z),
                      glm::vec2(here.x, here.z)) > 0.55f)
      continue;
    tuft.disappeared = true;
  }
}

// E19: a tap you cannot find, in a room that has no sink
void EventDirector::bathroomWater(float dt, bool playing) {
  const float clip = 3.6f;

  // Once the sink is on the wall the water has nothing left to be
  if (stage.state->itemCount("@water.heard") >= 3) return;

  if (!playing || idle < 10.f) {
    // Any input cuts it mid sample
    if (waterAt >= 0.f) {
      lve::LveAudio::instance().stopAll();
      waterAt = -1.f;
    }
    return;
  }

  if (waterAt < 0.f || waterAt >= clip) {
    // Only the first one of a visit counts, which is what E18 is waiting on
    if (waterAt < 0.f) stage.state->addItem("@water.heard", 1);
    lve::LveAudio::instance().play("water");
    waterAt = 0.f;
    return;
  }
  waterAt += dt;
}

// E21: the balls on the table spell one more letter of BYGONE every visit.
// E22: once it is spelt they stop, and play a game instead
void EventDirector::billiardWord() {
  const int shown = roomVisits < 6 ? roomVisits : 6;
  if (shown < 1) return;

  const float dot = 0.26f;
  const float pitch = 3.5f * dot;  // letter to letter
  const float startX = -9.75f * dot;

  const float tableZ = 1.0f;
  const float tableTop = -0.39f;

  // The table has moved on again while you were out of the room
  if (roomVisits > 6) {
    const int frame = (roomVisits - 7) % 6;
    for (int ball = 0; ball < 6; ball++) {
      conjure("sphere", glm::vec3(frames[frame][ball][0], tableTop, frames[frame][ball][1]),
              glm::vec3(0.f), glm::vec3(0.09f));
    }
    return;
  }

  for (int letter = 0; letter < shown; letter++) {
    for (int row = 0; row < 5; row++) {
      for (int column = 0; column < 3; column++) {
        if (word[letter][row][column] != '1') continue;

        const float x = startX + static_cast<float>(letter) * pitch +
                        static_cast<float>(column) * dot;
        const float z = tableZ + (2.f - static_cast<float>(row)) * dot;
        conjure("sphere", glm::vec3(x, tableTop, z), glm::vec3(0.f), glm::vec3(0.09f));
      }
    }
  }
}

// E24: the piano is gone, and walking through where it stood plays it anyway.
// It goes quiet again once E25 has stood the thing back up
void EventDirector::ballroomPiano(float dt, bool playing) {
  if (!playing || pianoPlayed || pianoBack || roomVisits < 2 || !stage.player) return;

  const glm::vec3 here = stage.player->translation;
  if (glm::length(glm::vec2(here.x, here.z)) > 1.6f) return;

  lve::LveAudio::instance().play("piano");
  pianoPlayed = true;
}

// E27: somebody walks the ballroom a beat behind him. The late set takes one
// more step after he has already stopped
void EventDirector::ballroomWalker(float dt, bool playing) {
  const float stride = 0.44f;
  const float behind = 0.36f;

  if (!playing) {
    echoAt = -1.f;
    return;
  }

  if (moved) {
    stepAt -= dt;
    if (stepAt <= 0.f) {
      stepAt = stride;
      echoAt = behind;
    }
  }

  if (echoAt < 0.f) return;
  echoAt -= dt;
  if (echoAt > 0.f) return;

  echoAt = -1.f;
  lve::LveAudio::instance().play("step");
}

// E30: the greenhouse lights go out, and something far too big passes over
void EventDirector::greenhouseShape(float dt) {
  const float seconds = 2.f;

  if (shapeAt < 0.f) {
    if (stage.state->hasFlag("saw_shape") || roomVisits < 3 || sinceEntry < 3.f) return;
    shapeAt = 0.f;
    lve::LveAudio::instance().play("drone");
    return;
  }

  shapeAt += dt;
  if (shapeAt > seconds) {
    stage.state->setFlag("saw_shape", true);
    shapeAt = -1.f;
    return;
  }

  // Nothing lit, and a flat wash so the shape reads as a silhouette
  gain = 0.f;
  tinted = true;
  tint = glm::vec4(0.55f, 0.62f, 0.78f, 0.45f);

  const float drift = (shapeAt / seconds - 0.5f) * 3.f;
  conjure("cube", glm::vec3(drift, -6.f, 0.f), glm::vec3(0.f, 0.35f, 0.f),
          glm::vec3(7.f, 0.4f, 3.5f));
}

// E32: lean on the north west corner of the field enough times and it gives way
void EventDirector::fieldEdge(bool playing) {
  if (!playing || !stage.player) return;
  const glm::vec3 here = stage.player->translation;

  if (roomName == "Field_Red") {
    // Red on everything, and slabs laid out where the grass was
    tinted = true;
    tint = glm::vec4(1.f, 0.10f, 0.09f, 0.65f);

    for (int i = 0; i < 26; i++) {
      const float turn = static_cast<float>(i) * 2.399963f;  // golden angle
      const float out = 1.2f + 0.42f * static_cast<float>(i);
      conjure("cube", glm::vec3(std::cos(turn) * out * 0.9f, 0.47f, std::sin(turn) * out * 0.42f),
              glm::vec3(0.f, turn, 0.f), glm::vec3(0.8f, 0.02f, 0.55f));
    }

    // Walk far enough into it and you are back in the greenhouse
    if (glm::length(glm::vec2(here.x, here.z)) < 9.f) return;
    const int greenhouse = findRoom("Greenhouse");
    if (greenhouse < 0) return;
    warpRoom = greenhouse;
    warpDoor = findDoor(greenhouse, "Field");
    return;
  }

  const bool corner = here.x < -10.5f && here.z > 3.5f;
  if (!corner) {
    onEdge = false;
    return;
  }
  if (onEdge) return;
  onEdge = true;

  stage.state->addItem("@edge.field", 1);
  if (stage.state->itemCount("@edge.field") < 5) return;

  const int red = findRoom("Field_Red");
  if (red < 0) return;
  warpRoom = red;
  warpDoor = -1;
}

// E40: forty seconds stood still and he stops facing the way he was walking and
// turns to look at the camera. Any input and he snaps back mid turn
void EventDirector::turnToCamera(float dt) {
  if (idle < 40.f || !stage.player) return;

  const glm::vec3 away = camEye - stage.player->translation;
  const float want = std::atan2(away.x, away.z);

  // The short way round, never the long way about
  const float circle = glm::two_pi<float>();
  const float turn =
      std::fmod(want - stage.player->rotation.y + glm::pi<float>() + circle, circle) -
      glm::pi<float>();

  const float step = 1.3f * dt;
  if (std::fabs(turn) <= step) {
    stage.player->rotation.y = want;
    return;
  }
  stage.player->rotation.y += turn < 0.f ? -step : step;
}

// E41: from the third thing on, every room in the house comes up dark for a
// quarter of a second after the fade has already finished
void EventDirector::lateLights() {
  if (questsDone() < 3 || sinceEntry >= 0.5f) return;
  gain = 0.f;
}

// --- progression ------------------------------------------------------------

int EventDirector::questsLeft() const {
  if (!stage.state) return questCount;

  int left = 0;
  for (int i = 0; i < questCount; i++) {
    if (!stage.state->hasFlag(quests[i])) left++;
  }
  return left;
}

int EventDirector::questsDone() const { return questCount - questsLeft(); }

// Quest 1: the rock stood three quarter turns off where it started, and the slab
// back down across the closet doorway. Opening the gate is the only way in to the
// rock, so putting it back is the half people forget
void EventDirector::stoneAndGate() {
  if (!stage.state || stage.state->hasFlag("quest_stone")) return;

  glm::vec3 rockAt, rockTurn, gateAt, gateTurn;
  if (!placeOf("Closet", "rock", rockAt, rockTurn)) return;
  if (!placeOf("Foyer", "gate", gateAt, gateTurn)) return;

  const MapObject* rockRest = mapObject("Closet", "rock");
  const MapObject* gateRest = mapObject("Foyer", "gate");
  if (!rockRest || !gateRest) return;

  // Three turns, seven turns and eleven all leave it facing the same way, so an
  // overshoot is something you can walk off rather than a run you have to restart
  const float circle = glm::two_pi<float>();
  const float turned = std::fmod(rockTurn.y - rockRest->rotation.y + circle * 8.f, circle);
  if (std::fabs(turned - glm::pi<float>() * 1.5f) > 0.25f) return;

  if (std::fabs(gateAt.y - gateRest->translation.y) > 0.1f) return;

  stage.state->setFlag("quest_stone", true);
  if (stage.dialog) stage.dialog->open("SOMETHING GIVES, TWO ROOMS AWAY.");
}

// Quest 3: the pale tiles are the only floor that counts. Step off them and the
// crossing is over, and the only place it starts again is the doorway
void EventDirector::ballroomTiles(bool playing) {
  for (int row = 0; row < 6; row++) {
    for (int column = 0; column < 7; column++) {
      if (tiles[row][column] != 'W') continue;
      conjure("cube", glm::vec3(-6.f + 2.f * column, 0.47f, -5.f + 2.f * row), glm::vec3(0.f),
              glm::vec3(0.92f, 0.02f, 0.92f));
    }
  }

  if (!playing || !stage.player || !stage.state) return;
  if (stage.state->hasFlag("quest_tiles")) return;

  const glm::vec3 here = stage.player->translation;
  const int column = static_cast<int>(std::floor((here.x + 7.f) * 0.5f));
  const int row = static_cast<int>(std::floor((here.z + 6.f) * 0.5f));
  const bool onFloor = column >= 0 && column < 7 && row >= 0 && row < 6;
  const bool pale = onFloor && tiles[row][column] == 'W';

  if (column == 0 && row == 3) {
    tileRun = true;  // back at the doorway, which is where a crossing starts
  } else if (!pale && tileRun) {
    tileRun = false;
    lve::LveAudio::instance().play("stone");
  }

  if (!tileRun || column != 3 || row != 3) return;

  stage.state->setFlag("quest_tiles", true);
  if (stage.dialog) stage.dialog->open("YOU REACH THE MIDDLE WITHOUT TOUCHING THE FLOOR.");
}

// The way into building two, plugged until all four are done
void EventDirector::terraceDoor() {
  const int left = questsLeft();
  if (left == 0) return;

  const int door = findDoor(room, "doors");
  if (door < 0) return;
  sealed = door;

  if (!stage.player) return;
  const glm::vec3 at = stage.map->rooms[room].doors[door].translation;
  const glm::vec3 here = stage.player->translation;

  // Says its piece once, and not again until you have stepped away from it
  if (glm::length(glm::vec2(here.x - at.x, here.z - at.z)) > 2.2f) {
    toldDoor = false;
    return;
  }
  if (toldDoor || !stage.dialog) return;
  toldDoor = true;

  const char* counted[] = {"", "ONE THING IS", "TWO THINGS ARE", "THREE THINGS ARE",
                           "FOUR THINGS ARE"};
  stage.dialog->open("THE DOOR DOES NOT OPEN.|" + std::string(counted[left]) + " UNFINISHED.");
}

// --- what the scene reads back ----------------------------------------------

float EventDirector::lightGain(std::size_t index) const {
  const float own = index < gains.size() ? gains[index] : 1.f;
  const float lit = gain * own;
  return lit < 0.f ? 0.f : (lit > 1.f ? 1.f : lit);
}

bool EventDirector::cameraOverride(glm::vec3& eye, glm::vec3& look) const {
  if (!hasCam) return false;
  eye = camEye;
  look = camLook;
  return true;
}

// E39: late on he does not start up in the room the save left him in. It is
// always the terrace, which is always further along than where he stopped
int EventDirector::wakeRoom(int fallback) {
  if (!stage.state || questsDone() < 3) return fallback;

  const int terrace = findRoom("Terrace");
  if (terrace < 0) return fallback;

  waking = true;
  return terrace;
}

bool EventDirector::takeWarp(int& toRoom, int& toDoor) {
  if (warpRoom < 0) return false;

  toRoom = warpRoom;
  toDoor = warpDoor;
  warpRoom = -1;
  warpDoor = -1;
  return true;
}

// E12: one time only, a north door out of the hall does not lead out of the
// hall. You come back in at the far end of it, facing the way you were going
bool EventDirector::hallGivesBack(int& toRoom, int& toDoor) {
  if (questsDone() < 2 || stage.state->hasFlag("hall_gave_back")) return false;
  if (toRoom < 0 || toRoom >= static_cast<int>(stage.map->rooms.size())) return false;

  const std::string& ahead = stage.map->rooms[toRoom].name;
  if (ahead != "Greenhouse" && ahead != "Bathroom" && ahead != "Billiard_Room") return false;

  const int hall = findRoom("Hall_Main");
  const int far = findDoor(hall, "Ballroom");
  if (hall < 0 || far < 0) return false;

  stage.state->setFlag("hall_gave_back", true);
  toRoom = hall;
  toDoor = far;
  return true;
}

// E33: read the note, and the next time you come out of the shed you come out
// somewhere else. It only ever happens once
//
// Also the last word on the way into building two, in case a slow frame carries
// him through the plug in the doorway
bool EventDirector::reroute(int& toRoom, int& toDoor) {
  if (!stage.state) return true;

  if (roomName == "Terrace" && toRoom == findRoom("Building_Two") && questsLeft() > 0)
    return false;

  if (roomName == "Hall_Main" && hallGivesBack(toRoom, toDoor)) return true;

  if (roomName != "Shed") return true;
  if (!stage.state->hasFlag("read_note") || stage.state->hasFlag("shed_closet")) return true;

  const int closet = findRoom("Closet");
  if (closet < 0) return true;

  const int out = findDoor(closet, "out");
  if (out < 0) return true;

  stage.state->setFlag("shed_closet", true);
  toRoom = closet;
  toDoor = out;
  return true;
}

}  // namespace petscop
