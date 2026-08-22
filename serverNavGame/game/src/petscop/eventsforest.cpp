#include "petscop/events.hpp"

#include "lve_game_object.hpp"
#include "petscop/dialog_box.hpp"
#include "petscop/game_state.hpp"
#include "petscop/model_cache.hpp"

#include <lve_audio.hpp>

#include <glm/gtc/constants.hpp>

#include <cmath>

// The forest doing things the map file cannot say on its own
//
// Same two shapes as the house: a room may be changed before it is built, or an
// event sets an override that is worked out again from nothing every frame
//
// The forest runs in four phases, and each one is gated on something the save
// already counts -- visits, runs of the game, or a flag an earlier event set
namespace petscop {

namespace {

// How long the man takes to cross a room
const float manWalk = 3.2f;

// How long the tree that follows takes to close the gap, in units a second
const float followerSpeed = 0.55f;

// Seconds stood in the room before the tree across the path gives way
const float pathHold = 8.f;

// How long the face is held on the way back in after the game went out
const float stareFor = 1.8f;

// How close to a doorway counts as standing in it
const float doorReach = 1.1f;

// An item name the way a note about it would be printed
std::string shout(const std::string& name) {
  std::string out;
  for (char letter : name) {
    if (letter == '_') out += ' ';
    else if (letter >= 'a' && letter <= 'z') out += static_cast<char>(letter - 'a' + 'A');
    else out += letter;
  }
  return out;
}

// How far apart two things are stood, ignoring how high either one is
float apart(const glm::vec3& a, const glm::vec3& b) {
  return glm::length(glm::vec2(a.x - b.x, a.z - b.z));
}

}  // namespace

// Somebody stood in the room, wearing the player's model and walking his walk
void EventDirector::figure(const glm::vec3& at, float yaw, bool walking) {
  // One more body than the room has ever needed at once, made the first time
  if (peopleUsed >= people.size()) people.push_back(std::unique_ptr<Figure>(new Figure()));
  people[peopleUsed++]->place(at, yaw, walking);
}

// --- the room about to be built ---------------------------------------------

void EventDirector::dressForest(MapRoom& room) {
  if (room.name == "Camp_South") forestInvert(room);
  else if (room.name == "Bridge") forestBridge(room);
  else if (room.name == "Stump_End") forestSign(room);
  else if (room.name == "Shrine_Path") forestNote(room);
  else if (room.name == "Tall_Trees") forestWatched(room);

  // Any room at all may be the one his pockets were emptied into
  forestLost(room);
}

// F09: the room is stood on its head. Everything in it is mirrored across the
// room's middle and turned over, and the doorways are left where they were
void EventDirector::forestInvert(MapRoom& room) {
  if (visits(room.name) + 1 < 3) return;

  for (MapObject& object : room.objects) {
    if (object.solid) continue;  // the floor and the walls stay put

    object.translation.x = -object.translation.x;
    object.translation.y = -object.translation.y + 1.f;
    object.rotation.z += glm::pi<float>();
  }
}

// F18: the bridge is out until the game has been run three times, and then it
// has put itself back and nothing says so
void EventDirector::forestBridge(MapRoom& room) {
  if (!stage.state || stage.state->itemCount("@runs") < 3) return;

  addObject(room, glm::vec3(0.f, 0.430f, 0.f), glm::vec3(0.f), glm::vec3(3.4f, 0.06f, 1.1f),
            "deck", "THE PLANKS ARE NEW. NOBODY CARRIED THEM DOWN HERE.");
}

// F04: a sign at the end of the line west, counting off how long it waited
void EventDirector::forestSign(MapRoom& room) {
  if (!stage.state || stage.state->itemCount("@runs") < 2) return;

  // Seconds are counted a room at a time, which is close enough to a clock
  const int waited = stage.state->itemCount("@seconds");
  addObject(room, glm::vec3(-5.900f, -0.400f, -1.600f), glm::vec3(0.f, 0.35f, 0.f),
            glm::vec3(0.55f, 0.75f, 0.06f), "sign",
            "YOU LEFT ME HERE " + std::to_string(waited) + " SECONDS AGO.");
}

// F17: come back to the game and there is a note by the shrine that was not
// there, and it is addressed to you
void EventDirector::forestNote(MapRoom& room) {
  if (!stage.state || stage.state->itemCount("@runs") < 2) return;
  if (stage.state->hasFlag("read_sorry")) return;

  addObject(room, glm::vec3(0.900f, 0.460f, 3.200f), glm::vec3(0.f, 0.6f, 0.f),
            glm::vec3(0.26f, 0.02f, 0.34f), "sorry_note",
            "I AM SORRY FOR WHAT HAPPENED HERE.|"
            "I UNDERSTAND IF YOU DO NOT WANT TO SEE ME AGAIN.",
            false);
}

// F06: whatever left his pockets on the way out of this room is lying in it,
// and pressing E on it puts it back
void EventDirector::forestLost(MapRoom& room) {
  if (!stage.state) return;

  const std::string mark = "@left." + room.name + ".";
  float along = -1.f;

  for (const std::pair<const std::string, int>& held : stage.state->items) {
    if (held.second <= 0 || held.first.rfind(mark, 0) != 0) continue;

    const std::string item = held.first.substr(mark.size());
    addObject(room, glm::vec3(along, 0.440f, -1.400f), glm::vec3(0.f, 0.85f, 0.f),
              glm::vec3(0.180f, 0.060f, 0.110f), item,
              "YOUR " + shout(item) + ", LYING WHERE YOU DID NOT PUT IT.");
    along += 0.8f;

    // Back in his pocket, off the ground, and the room forgets it was ever here
    MapAction give;
    give.kind = ActionKind::Give;
    give.text = item;
    MapAction drop;
    drop.kind = ActionKind::Take;
    drop.text = held.first;
    MapAction gone;
    gone.kind = ActionKind::Hide;
    gone.target = item;

    room.objects.back().actions.push_back(give);
    room.objects.back().actions.push_back(drop);
    room.objects.back().actions.push_back(gone);
  }
}

// F12: the room is watched from further back than any other, with the two of
// them on the screen at once
void EventDirector::forestWatched(MapRoom& room) {
  if (!stage.state || stage.state->itemCount("@runs") < 2) return;

  room.cameraEye = room.cameraLook + (room.cameraEye - room.cameraLook) * 1.7f;
}

// --- the room is standing ---------------------------------------------------

void EventDirector::enterForest() {
  // The way back is open again, until this room shuts it
  wallUp = false;

  // F12: the mirror has not stood anywhere in this room yet
  mirrorSeen = false;

  // F07: both prompts are ready again every time he walks in
  // The one door that stays quiet is the one he stepped out of
  if (toldAfraid)
    stage.state->setFlag("forest_afraid", true);

  // F12: the run after the game went out opens on his face, and only that once
  if (justLaunched) {
    justLaunched = false;
    if (stage.state && stage.state->hasFlag("crash_seen")) {
      stareAt = 0.f;
      if (stage.player && built && roomName == "Foyer")
        stage.player->translation = glm::vec3(-built->size.x * 0.35f, 0.5f, 0.f);
    }
  }

  // F06: the forest goes through his pockets on the way in
  forestPockets();

  // F17: the game greets him on the way back in, once per run
  if (stage.state && stage.state->itemCount("@runs") >= 2 &&
      !stage.state->hasFlag("welcomed") && stage.dialog) {
    stage.state->setFlag("welcomed", true);
    stage.dialog->open("WELCOME BACK HOME.");
  }

  // F02: now and then somebody is already in the room, on his way out of it
  if (roomName == "Deep_Trees" && roomVisits >= 2 && canFire()) {
    std::uniform_int_distribution<int> odds(0, 2);
    if (odds(rng) == 0 && built && !built->doors.empty()) {
      int way = -1;
      for (std::size_t i = 0; i < built->doors.size(); i++) {
        if (static_cast<int>(i) != arrivedFrom) {
          way = static_cast<int>(i);
          break;
        }
      }
      if (way < 0) way = 0;

      manAt = 0.f;
      manTo = built->doors[way].translation;
      manTo.y = 0.5f;
      manFrom = glm::vec3(-manTo.x * 0.6f, 0.5f, -manTo.z * 0.6f);
      fired();
    }
  }

  // F05: the tree is stood in the middle of its own room, and is not in any other
  if (followerUp && roomName == followerRoom) {
    followerAt = glm::vec3(0.f, 0.5f, 0.f);
  }
}

// --- every frame ------------------------------------------------------------

void EventDirector::updateForest(float dt, bool playing, int startedProp) {
  // A rough clock, which is all F04's sign needs
  if (playing && stage.state) {
    forestTick += dt;
    while (forestTick >= 1.f) {
      forestTick -= 1.f;
      stage.state->addItem("@seconds", 1);
    }
  }

  forestMan(dt);
  forestDark(dt);
  forestFollower(dt);
  forestStare(dt);

  if (roomName == "False_Path") {
    forestAfraid();
    forestGrey();
  }
  if (roomName == "Path_West") forestBlocked();
  if (roomName == "Bank_West") forestDrift(dt);
  if (roomName == "Well_Path") forestDoll(dt, startedProp);
  if (roomName == "Trees_West") forestMannequin();
  if (roomName == "Camp_East") forestWall();
  if (roomName == "Tall_Trees") forestMirror();
  if (roomName == "Foyer") forestFoyer();
  if (roomName == "NOT_HERE_NOT_ANYWHERE") forestDoorway("out", "Start", "on");
}

// F06: something goes missing on the way into a room, and it is on the ground
// in the one he has just walked out of
void EventDirector::forestPockets() {
  if (!stage.state || lastRoom.empty() || lastRoom == roomName) return;
  if (stage.state->itemCount("@runs") < 2 || !canFire()) return;

  // Never into a room he has no way of walking back into
  if (lastRoom == "Foyer" || lastRoom == "NOT_HERE_NOT_ANYWHERE") return;

  std::uniform_int_distribution<int> odds(0, 2);
  if (odds(rng) != 0) return;

  for (const std::pair<const std::string, int>& held : stage.state->items) {
    if (held.second <= 0 || held.first.empty() || held.first[0] == '@') continue;

    // A stack that runs out is dropped, key and all
    const std::string item = held.first;
    stage.state->addItem(item, -1);
    stage.state->addItem("@left." + lastRoom + "." + item, 1);
    fired();
    return;
  }
}

// F02: a man stands across the room and walks out of it. The lights go down
// while he is crossing, so he is never more than a shape
void EventDirector::forestMan(float dt) {
  if (manAt < 0.f) return;

  manAt += dt;
  if (manAt > manWalk) {
    manAt = -1.f;
    if (stage.state) stage.state->setFlag("saw_man", true);
    return;
  }

  const float along = manAt / manWalk;
  const glm::vec3 at = manFrom + (manTo - manFrom) * along;

  // Nearly dark, and darker still the closer he gets to the way out
  gain = 0.22f * (1.f - along * 0.7f);

  // Nose pointed down the walk, at the door he is leaving by
  const glm::vec3 way = manTo - manFrom;
  figure(at, std::atan2(way.x, way.z), true);
}

// F03: once the man has been seen the hollow stops being lit by anything but
// him. The camera comes off its peg and there is nothing behind the room
void EventDirector::forestDark(float dt) {
  if (roomName != "Hollow" || roomVisits < 3) return;
  if (!stage.state || !stage.state->hasFlag("saw_man")) return;
  if (!stage.player || dressed.lights.empty()) return;

  noBackdrop = true;

  // One light, carried on him, and every other one out
  dressed.lights[0].position = stage.player->translation + glm::vec3(0.f, -1.1f, 0.f);
  dressed.lights[0].intensity = 9.f;
  for (std::size_t i = 1; i < dressed.lights.size(); i++) setLight(i, 0.f);

  // The camera walks after him rather than watching the room
  glm::vec3 want = stage.player->translation;
  const float chase = dt * 2.0f;
  follow += (want - follow) * (chase > 1.f ? 1.f : chase);

  hasCam = true;
  camLook = follow;
  camEye = follow + (dressed.cameraEye - dressed.cameraLook) * 0.7f;
}

// F05: a tree in the ring answers E and does nothing, and from then on it walks
// after him at a pace he can always outrun, room to room
void EventDirector::forestFollower(float dt) {
  if (!stage.state || !stage.player) return;

  if (!followerUp) {
    if (roomName != "Ring" || !stage.state->hasFlag("tree_walks")) return;
    followerUp = true;
    followerRoom = roomName;
    followerAt = stage.player->translation + glm::vec3(0.f, 0.f, 5.f);
  }

  // It is only ever in the room it started walking in
  if (roomName != followerRoom) return;

  // Always coming, never arriving
  const glm::vec3 gap = stage.player->translation - followerAt;
  const float away = glm::length(glm::vec2(gap.x, gap.z));
  if (away > 0.8f) followerAt += glm::normalize(glm::vec3(gap.x, 0.f, gap.z)) * followerSpeed * dt;

  conjure("tree", glm::vec3(followerAt.x, 0.5f, followerAt.z), glm::vec3(0.f), glm::vec3(0.30f));
}

// F07: one way out is shut because he does not want to take it, and the other
// one has an opinion about that
void EventDirector::forestAfraid() {
  if (!stage.player || !built || !stage.dialog) return;
  if (!stage.state || stage.state->itemCount("@runs") < 2) return;
  if (stage.state->hasFlag("forest_afraid")) return;

  const int shut = findDoor(room, "east");
  const int other = findDoor(room, "back");
  if (shut < 0 || other < 0) return;

  sealed = shut;

  const glm::vec3 here = stage.player->translation;
  const glm::vec3 shutAt = built->doors[shut].translation;
  const glm::vec3 otherAt = built->doors[other].translation;

  const float toShut = glm::length(glm::vec2(here.x - shutAt.x, here.z - shutAt.z));
  const float toOther = glm::length(glm::vec2(here.x - otherAt.x, here.z - otherAt.z));

  if (toShut < 2.2f) {
    if (!toldAfraid) {
      toldAfraid = true;
      stage.dialog->open("YOU ARE AFRAID TO GO THIS WAY.");
    }
  }

  if (toOther < 2.2f) {
    if (toldAfraid) {
      stage.dialog->open("YOU WOULD RATHER GO THE OTHER WAY.");
      stage.state->setFlag("forest_afraid", true);
    }
  }
}

// F11: a tree lies across the way west, and eight seconds stood still in the
// room is enough for it not to be there any more
void EventDirector::forestBlocked() {
  if (!stage.state || stage.state->hasFlag("path_opened")) return;

  if (sinceEntry >= pathHold) {
    if (!pathClear) {
      pathClear = true;
      stage.state->setFlag("path_opened", true);
      lve::LveAudio::instance().play("stone");
    }
    return;
  }

  const int west = findDoor(room, "west");
  if (west < 0 || !built) return;
  sealed = west;

  const glm::vec3 at = built->doors[west].translation;
  conjure("tree", glm::vec3(at.x + 1.2f, 0.5f, at.z), glm::vec3(0.f, 0.f, glm::half_pi<float>()),
          glm::vec3(0.28f));
}

// F14: the camera lets him walk away and stays looking at the door he came in
// by, and only catches up once he is nearly out of the room
void EventDirector::forestDrift(float dt) {
  if (!stage.player || !built || arrivedFrom < 0) return;
  if (!stage.state || !stage.state->hasFlag("saw_man")) return;
  if (arrivedFrom >= static_cast<int>(built->doors.size())) return;

  if (driftAt < 0.f) {
    driftAt = 0.f;
    driftTo = built->doors[arrivedFrom].translation;
  }

  driftAt += dt;
  const float hold = driftAt / 4.f;
  const float back = hold > 1.f ? 1.f : hold;

  hasCam = true;
  camLook = built->cameraLook + (driftTo - built->cameraLook) * back;
  camEye = camLook + (built->cameraEye - built->cameraLook);
}

// F15: a doll at the top of the room. Press E and it says nothing, and it has
// turned a little further every time he looks away
void EventDirector::forestDoll(float dt, int startedProp) {
  if (!built || !stage.state) return;
  if (stage.state->itemCount("@runs") < 2) return;

  // Gone the next time he walks in, whatever he did to it
  if (roomVisits % 2 == 0) return;

  const glm::vec3 at(0.f, 0.5f, built->size.z * 0.5f - 1.4f);

  if (startedProp >= 0 && !dollPressed && canFire()) {
    dollPressed = true;
    lve::LveAudio::instance().play("drone");
    fired();
  }

  // It only moves while he is walking away from it
  if (dollPressed && moved) dollTurn += dt * 0.7f;

  conjure("cube", glm::vec3(at.x, at.y - 0.24f, at.z), glm::vec3(0.f, dollTurn, 0.f),
          glm::vec3(0.13f, 0.24f, 0.11f));
  conjure("cube", glm::vec3(at.x, at.y - 0.60f, at.z), glm::vec3(0.f, dollTurn, 0.f),
          glm::vec3(0.10f, 0.11f, 0.09f));
}

// F16: somebody is standing at the north wall with their back to the room, and
// there is nothing to press E on
void EventDirector::forestMannequin() {
  if (!built || !stage.state) return;
  if (!stage.state->hasFlag("saw_man")) return;

  const float north = built->size.z * 0.5f - 0.6f;

  // Modelled with Y up and stood with Y down, the same flip the player gets
  // Nose first, and a yaw of nothing leaves him looking at the wall
  conjure(
      "mannequin",
      glm::vec3(-2.2f, 0.5f - 0.683f, north),
      glm::vec3(glm::pi<float>(), 0.f, glm::pi<float>()),
      glm::vec3(0.235f));
}

// F08: the doorway he came in by fills itself in once he is a few steps off it,
// and nothing in the room says which one it was
void EventDirector::forestWall() {
  if (!stage.player || !built || arrivedFrom < 0 || roomVisits < 2) return;
  if (arrivedFrom >= static_cast<int>(built->doors.size())) return;

  if (!wallUp && apart(stage.player->translation, built->doors[arrivedFrom].translation) > 3.f) {
    wallUp = true;
    lve::LveAudio::instance().play("stone");
  }
  if (wallUp) sealed = arrivedFrom;
}

// F08: the doorway west that goes nowhere takes him anyway. He comes out of it
// grey, walking through everything and held up by nothing
void EventDirector::forestGrey() {
  if (!stage.player || !built || !stage.state) return;
  if (stage.state->hasFlag("unbound") || stage.state->itemCount("@runs") < 2) return;

  const int way = findDoor(room, "path");
  if (way < 0 || way >= static_cast<int>(built->doors.size())) return;
  if (apart(stage.player->translation, built->doors[way].translation) > doorReach) return;

  stage.state->setFlag("unbound", true);
  lve::LveAudio::instance().play("drone");
}

// F08: the settings are the way back into his body, and he comes back into it
// in the ring of mushrooms
void EventDirector::onSettingsClosed() {
  if (!stage.state || !stage.state->hasFlag("unbound")) return;

  stage.state->setFlag("unbound", false);

  const int ring = findRoom("Ring");
  if (ring < 0) return;
  warpRoom = ring;
  warpDoor = findDoor(ring, "north");
}

// F12: somebody the other side of the room walks his walk back at him, step for
// step. Meeting him in the middle is where the run stops
void EventDirector::forestMirror() {
  if (!stage.player || !stage.state || stage.state->itemCount("@runs") < 2) return;

  // He meets him once a save, whatever the run after this one does
  if (stage.state->hasFlag("met_mirror")) return;

  const glm::vec3 here = stage.player->translation;
  const glm::vec3 mirror(-here.x, 0.5f, here.z);

  // His walk is the player's read backwards, so it is measured off his own feet
  const glm::vec3 step = mirror - mirrorLast;
  const float went = glm::length(glm::vec2(step.x, step.z));
  const bool walking = mirrorSeen && went > 0.002f;

  // Facing his own way when he moves, and back at the player when he stops
  const float yaw = walking ? std::atan2(step.x, step.z) : glm::pi<float>();
  mirrorLast = mirror;
  mirrorSeen = true;

  figure(mirror, yaw, walking);

  if (std::fabs(here.x) > 0.45f) return;

  stage.state->setFlag("met_mirror", true);
  stage.state->setFlag("crash_seen", true);
  crashing = true;
}

// F12: the run after the game went out opens on him up against the camera, with
// a bar across the eyes, and then he is not there
void EventDirector::forestStare(float dt) {
  if (stareAt < 0.f || !built) return;

  stareAt += dt;
  if (stareAt > stareFor) {
    stareAt = -1.f;
    if (stage.state) stage.state->setFlag("crash_seen", false);
    return;
  }

  frozen = true;

  const glm::vec3 at(0.f, 0.5f, 0.f);
  const glm::vec3 head(at.x, at.y - Figure::kEyeLift, at.z);

  hasCam = true;
  camLook = head;
  camEye = head + glm::vec3(0.f, -0.15f, -1.3f);

  // He is the only thing lit, and only just
  if (!dressed.lights.empty()) {
    dressed.lights[0].position = head + glm::vec3(0.f, -0.9f, -1.f);
    dressed.lights[0].intensity = 7.f;
  }
  for (std::size_t i = 1; i < dressed.lights.size(); i++) setLight(i, 0.f);

  figure(at, glm::pi<float>());
  conjure("cube", glm::vec3(head.x, head.y - 0.02f, head.z - 0.10f), glm::vec3(0.f),
          glm::vec3(0.13f, 0.030f, 0.020f));
}

// F01: the lever is the only thing in here that answers, and the way out stays
// shut until it has been pulled
void EventDirector::forestFoyer() {
  if (!stage.state) return;

  if (!stage.state->hasFlag("foyer_open")) {
    sealed = findDoor(room, "out");
    return;
  }
  forestDoorway("out", "Hollow", "west");
}

// A room nothing on the map leads into still needs a way out. Standing in the
// doorway hands him back to the forest at the door named here
void EventDirector::forestDoorway(const std::string& door, const std::string& backRoom,
                                 const std::string& backDoor) {
  if (!stage.player || !built) return;

  const int way = findDoor(room, door);
  if (way < 0 || way >= static_cast<int>(built->doors.size())) return;
  if (apart(stage.player->translation, built->doors[way].translation) > doorReach) return;

  const int back = findRoom(backRoom);
  if (back < 0) return;

  warpRoom = back;
  warpDoor = findDoor(back, backDoor);
}

// F01: the way east out of the trees does not come out where it should. The room
// the other side says Foyer, and it is not the one he remembers
bool EventDirector::forestReroute(int& toRoom, int& toDoor) {
  if (!stage.state || stage.state->hasFlag("saw_foyer")) return true;
  if (roomName != "Clearing_Trees" || toRoom != findRoom("Hollow")) return true;
  if (stage.state->itemCount("@runs") < 2 || !canFire()) return true;

  const int foyer = findRoom("Foyer");
  if (foyer < 0) return true;

  std::uniform_int_distribution<int> odds(0, 2);
  if (odds(rng) != 0) return true;

  stage.state->setFlag("saw_foyer", true);
  fired();
  toRoom = foyer;
  toDoor = findDoor(foyer, "out");
  return true;
}

// F10: some runs do not start where the last one stopped
int EventDirector::forestWake(int fallback) {
  const int nowhere = findRoom("NOT_HERE_NOT_ANYWHERE");
  if (nowhere < 0 || !stage.state) return fallback;

  // Quitting in there does not leave him in there
  if (fallback == nowhere) return stage.map->startRoom;
  if (stage.state->itemCount("@runs") < 3) return fallback;

  std::uniform_int_distribution<int> odds(0, 2);
  return odds(rng) == 0 ? nowhere : fallback;
}

// --- what the scene reads back ----------------------------------------------

bool EventDirector::untethered() const {
  return forest() && stage.state && stage.state->hasFlag("unbound");
}

bool EventDirector::invertsControls() const {
  return forest() && roomName == "NOT_HERE_NOT_ANYWHERE";
}

bool EventDirector::menuStripped() const {
  return forest() && stage.state && stage.state->itemCount("@runs") >= 3;
}

// F13: the name in the corner stops being a place
bool EventDirector::titled(std::string& name) const {
  if (!forest() || !stage.state || stage.state->itemCount("@runs") < 2) return false;
  if (roomName != "Car_South") return false;

  name = "YOU";
  return true;
}

}  // namespace petscop
