#include "petscop/events.hpp"

#include "lve_game_object.hpp"
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
  room = -1;
  roomName.clear();
  locked = false;
  frozen = false;
  black = 0.f;
  gain = 1.f;
  killedLight = -1;
  bgScale = 1.f;
  tinted = false;
  warpRoom = -1;
  warpDoor = -1;
  shutIn = -1.f;
  shapeAt = -1.f;
  waterAt = -1.f;
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

// --- the room about to be built ---------------------------------------------

// E11: the hall is half as long again from the fourth time you walk into it, and
// stays that way. Stretching the room data is the whole trick -- walls, doors,
// spawn points and lights all come out of it further apart
const MapRoom& EventDirector::dress(const MapRoom& source, int index) {
  if (source.name != "hall_main" || visits(source.name) + 1 < 4) return source;

  const float stretch = 0.5f;
  dressed = source;
  dressed.size.x *= stretch;

  for (MapObject& object : dressed.objects) {
    object.translation.x *= stretch;
    object.scale.x *= stretch;
  }
  for (MapDoor& door : dressed.doors) {
    door.translation.x *= stretch;
    door.scale.x *= stretch;
    door.spawn.x *= stretch;
  }
  for (MapLight& light : dressed.lights) light.position.x *= stretch;

  // Back the camera off by the same amount, or the ends fall off the screen
  dressed.cameraEye = dressed.cameraLook + (source.cameraEye - source.cameraLook) * stretch;
  return dressed;
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
  shutIn = -1.f;
  shapeAt = -1.f;
  waterAt = -1.f;
  onEdge = true;  // you have to step off a corner before leaning on it counts

  stage.state->addItem("@visits." + roomName, 1);
  roomVisits = visits(roomName);

  if (stage.player) lastPos = stage.player->translation;
  if (roomName == "yard") stage.state->setFlag("seen_yard", true);

  // E13: doors taken one after another until the screen stops coming back
  if (mash >= mashLimit) {
    black = 10.f;
    turnAround = true;
    mash = 0;
    lve::LveAudio::instance().play("footsteps");
  }

  if (roomName == "foyer") foyerTree();
  if (roomName == "yard") yardTuft();
  if (roomName == "shed") shedBoard();

  // E24: the piano is only there the first time
  if (roomName == "ballroom" && roomVisits >= 2) {
    if (Prop* piano = prop("piano")) {
      piano->disappeared = true;
      piano->collider.enabled = false;
    }
  }
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
  killedLight = -1;
  bgScale = 1.f;
  tinted = false;
  frozen = false;
  locked = false;

  // Standing still with nothing held down
  bool holding = false;
  GLFWwindow* window = lve::LveEngine::instance().getGLFWWindow();
  for (int key : watchedKeys) holding = holding || glfwGetKey(window, key) == GLFW_PRESS;

  if (stage.player) {
    if (glm::distance(stage.player->translation, lastPos) > 0.01f) holding = true;
    lastPos = stage.player->translation;
  }
  idle = (holding || !playing) ? 0.f : idle + dt;

  blackout(dt);

  if (roomName == "foyer") foyerLight();
  if (roomName == "closet") closetShutIn(dt, startedProp);
  if (roomName == "bathroom") bathroomWater(dt, playing);
  if (roomName == "billiard_room") billiardWord();
  if (roomName == "ballroom") ballroomPiano(dt, playing);
  if (roomName == "greenhouse") {
    bgScale = 0.f;  // E29: the only room the backdrop holds still in
    greenhouseShape(dt);
  }
  if (roomName == "field" || roomName == "field_red") fieldEdge(playing);
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
  killedLight = 0;
  stage.state->setFlag("saw_dark_foyer", true);
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

// E19: a tap you cannot find, in a room that has no sink
void EventDirector::bathroomWater(float dt, bool playing) {
  const float clip = 3.6f;

  if (!playing || idle < 10.f) {
    // Any input cuts it mid sample
    if (waterAt >= 0.f) {
      lve::LveAudio::instance().stopAll();
      waterAt = -1.f;
    }
    return;
  }

  if (waterAt < 0.f || waterAt >= clip) {
    lve::LveAudio::instance().play("water");
    waterAt = 0.f;
    return;
  }
  waterAt += dt;
}

// E21: the balls on the table spell one more letter of BYGONE every visit
void EventDirector::billiardWord() {
  const int shown = roomVisits < 6 ? roomVisits : 6;
  if (shown < 1) return;

  const float dot = 0.26f;
  const float pitch = 3.5f * dot;  // letter to letter
  const float startX = -9.75f * dot;

  const float tableZ = 1.0f;
  const float tableTop = -0.39f;

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

// E24: the piano is gone, and walking through where it stood plays it anyway
void EventDirector::ballroomPiano(float dt, bool playing) {
  if (!playing || pianoPlayed || roomVisits < 2 || !stage.player) return;

  const glm::vec3 here = stage.player->translation;
  if (glm::length(glm::vec2(here.x, here.z)) > 1.6f) return;

  lve::LveAudio::instance().play("piano");
  pianoPlayed = true;
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

  if (roomName == "field_red") {
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
    const int greenhouse = findRoom("greenhouse");
    if (greenhouse < 0) return;
    warpRoom = greenhouse;
    warpDoor = findDoor(greenhouse, "field");
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

  const int red = findRoom("field_red");
  if (red < 0) return;
  warpRoom = red;
  warpDoor = -1;
}

// --- what the scene reads back ----------------------------------------------

float EventDirector::lightGain(std::size_t index) const {
  if (killedLight >= 0 && static_cast<std::size_t>(killedLight) == index) return 0.f;
  return gain < 0.f ? 0.f : (gain > 1.f ? 1.f : gain);
}

bool EventDirector::takeWarp(int& toRoom, int& toDoor) {
  if (warpRoom < 0) return false;

  toRoom = warpRoom;
  toDoor = warpDoor;
  warpRoom = -1;
  warpDoor = -1;
  return true;
}

// E33: read the note, and the next time you come out of the shed you come out
// somewhere else. It only ever happens once
void EventDirector::reroute(int& toRoom, int& toDoor) {
  if (roomName != "shed" || !stage.state) return;
  if (!stage.state->hasFlag("read_note") || stage.state->hasFlag("shed_closet")) return;

  const int closet = findRoom("closet");
  if (closet < 0) return;

  const int out = findDoor(closet, "out");
  if (out < 0) return;

  stage.state->setFlag("shed_closet", true);
  toRoom = closet;
  toDoor = out;
}

}  // namespace petscop
