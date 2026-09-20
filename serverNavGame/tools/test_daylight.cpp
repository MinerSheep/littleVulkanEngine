// Checks X11: what the sun does to the house, and what it takes out of a room
//
// The band maths is a pure function and is printed as a table. The hiding is not
// -- it runs inside EventDirector::dress -- so this links the game's own objects
// and dresses real rooms at a forced hour
//
// Build and run from the repo root:
//   g++ -std=c++17 -g3 -O0 -fsanitize=address -Iengine/include -Iengine/libs -Igame/src \
//       tools/test_daylight.cpp $(find game/obj -name '*.o' ! -name 'main.o') \
//       engine/libvulkan_engine.a $(pkg-config --libs glfw3) -lvulkan -o /tmp/test_daylight
//
//   /tmp/test_daylight maps/petscop.map
//
// It says "all good" and exits 0, or prints what went wrong and exits 1

#include "petscop/events.hpp"
#include "petscop/game_state.hpp"
#include "petscop/outside.hpp"

#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>

namespace {

int failures = 0;

void complain(const std::string& what) {
  std::cout << "  FAIL " << what << "\n";
  failures++;
}

// Stands the game at an hour of the day, whatever the machine's clock says
// The director walks between hours rather than jumping, so it is reset as well
void standAt(float hour, petscop::EventDirector* events = nullptr) {
  char said[32];
  std::snprintf(said, sizeof(said), "%.2f", hour);
  setenv("PETSCOP_HOUR", said, 1);
  if (events) events->reset();
}

const petscop::MapRoom* roomNamed(const petscop::GameMap& map, const std::string& name) {
  for (const petscop::MapRoom& room : map.rooms)
    if (room.name == name) return &room;
  return nullptr;
}

const petscop::MapObject* objectNamed(const petscop::MapRoom& room, const std::string& name) {
  for (const petscop::MapObject& object : room.objects)
    if (object.name == name) return &object;
  return nullptr;
}

// Whether the thing is standing in the room once the house has dressed it
bool standing(petscop::EventDirector& events, const petscop::GameMap& map,
              const std::string& where, const std::string& what) {
  const petscop::MapRoom* room = roomNamed(map, where);
  if (!room) return false;
  return objectNamed(events.dress(*room, 0), what) != nullptr;
}

// Whether pressing E on it does anything at all
bool answers(petscop::EventDirector& events, const petscop::GameMap& map,
             const std::string& where, const std::string& what) {
  const petscop::MapRoom* room = roomNamed(map, where);
  if (!room) return false;
  const petscop::MapObject* object = objectNamed(events.dress(*room, 0), what);
  return object && !object->actions.empty();
}

}  // namespace

int main(int argc, char** argv) {
  const std::string path = argc > 1 ? argv[1] : "maps/petscop.map";

  petscop::GameMap map;
  std::string error;
  if (!petscop::loadMap(path, map, error)) {
    std::cerr << "test_daylight: " << error << "\n";
    return 2;
  }

  printf("  hour   sun   gain  ambient  backdrop  hold  quests\n");
  for (float hour = 0.f; hour < 24.f; hour += 1.f) {
    const petscop::Daylight sky = petscop::daylightAt(hour);
    printf("  %4.1f  %5.2f  %5.2f  %7.2f  %8.2f  %4.0fs  %s\n", hour, sky.sun, sky.gain,
           sky.ambient, sky.backdrop, sky.holdOff, sky.hidesQuests ? "gone" : "there");
  }

  // The ramps run the right way and nobody sees a step in them
  if (petscop::daylightAt(3.f).sun != 0.f) complain("three in the morning is not dark");
  if (petscop::daylightAt(13.f).sun != 1.f) complain("one in the afternoon is not full daylight");
  if (petscop::daylightAt(23.f).sun != 0.f) complain("eleven at night is not dark");
  if (petscop::daylightAt(6.f).sun <= 0.f || petscop::daylightAt(6.f).sun >= 1.f)
    complain("dawn is a switch rather than a ramp");
  if (petscop::daylightAt(19.f).sun <= 0.f || petscop::daylightAt(19.f).sun >= 1.f)
    complain("dusk is a switch rather than a ramp");

  float last = petscop::daylightAt(0.f).sun;
  for (float hour = 0.f; hour < 24.f; hour += 0.02f) {
    const float now = petscop::daylightAt(hour).sun;
    if (std::abs(now - last) > 0.02f) complain("the sun jumps at " + std::to_string(hour));
    last = now;
  }

  if (petscop::daylightAt(13.f).gain <= petscop::daylightAt(2.f).gain)
    complain("the house is no brighter at midday than at two in the morning");
  if (petscop::daylightAt(13.f).holdOff <= petscop::daylightAt(2.f).holdOff)
    complain("the house is as busy in the afternoon as it is at night");

  // --- what the hour takes out of a room ------------------------------------
  petscop::GameState state;
  petscop::Stage stage;
  stage.map = &map;
  stage.state = &state;

  petscop::EventDirector events;
  events.bind(stage);

  standAt(2.f, &events);
  if (!standing(events, map, "Billiard_Room", "cue")) complain("the cue is gone at night");
  if (!standing(events, map, "Shed", "spade")) complain("the spade is gone at night");
  if (!answers(events, map, "Closet", "rock")) complain("the rock will not answer at night");

  standAt(13.f, &events);
  if (standing(events, map, "Billiard_Room", "cue")) complain("the cue is out in the afternoon");
  if (standing(events, map, "Shed", "spade")) complain("the spade is out in the afternoon");
  if (!answers(events, map, "Closet", "rock")) complain("the rock will not answer by day");

  standAt(19.f, &events);
  if (!standing(events, map, "Billiard_Room", "cue")) complain("the cue has not come back by dusk");

  // Three afternoons finishing nothing and the house gives the cue back
  standAt(13.f, &events);
  for (int run = 0; run < 3; run++) events.newRun();
  if (!standing(events, map, "Billiard_Room", "cue"))
    complain("the house never relents on a player who only plays in daylight");
  if (standing(events, map, "Shed", "spade")) complain("the house relented with both, not one");

  // --- an event that only happens by day, and one that only happens at night --
  //
  // The hall is stretched from the fourth visit, and only in daylight
  state.addItem("@visits.Hall_Main", 4);

  const petscop::MapRoom* hall = roomNamed(map, "Hall_Main");
  if (!hall) complain("there is no Hall_Main to stretch");

  if (hall) {
    standAt(13.f, &events);
    const float lit = events.dress(*hall, 0).size.x;

    standAt(2.f, &events);
    const float dark = events.dress(*hall, 0).size.x;

    if (lit >= dark) complain("the hall is not stretched by day");
    if (std::fabs(dark - hall->size.x) > 0.01f) complain("the hall is stretched at night");
  }

  // --- the settings pick the hour, whatever the machine says -----------------
  //
  // 1 is day and 2 is night, and the machine's clock is left saying the opposite
  // The spade, because the cue has already been given back by the relent above
  standAt(2.f, &events);
  state.addItem("@timeofday", 1);
  if (standing(events, map, "Shed", "spade"))
    complain("the settings could not make it day");

  standAt(13.f, &events);
  state.addItem("@timeofday", 1);  // now 2, which is night
  if (!standing(events, map, "Shed", "spade"))
    complain("the settings could not make it night");

  state.addItem("@timeofday", -2);

  // --- nothing is ever standing there dead -------------------------------------
  //
  // A prop the map gave something to say must still have something to say at
  // every hour. If daylight takes it away, it takes the whole prop away
  state.addItem("@timeofday", -state.itemCount("@timeofday"));

  for (int hour = 0; hour < 24; hour += 1) {
    standAt(static_cast<float>(hour), &events);

    for (const petscop::MapRoom& each : map.rooms) {
      const petscop::MapRoom& dressed = events.dress(each, 0);

      for (const petscop::MapObject& stood : dressed.objects) {
        if (stood.name.empty()) continue;

        const petscop::MapObject* asBuilt = objectNamed(each, stood.name);
        if (!asBuilt || asBuilt->actions.empty()) continue;

        if (stood.actions.empty())
          complain(stood.name + " is standing in " + each.name + " with nothing to say at " +
                   std::to_string(hour) + ":00");
      }
    }
  }

  // The rock answers at both ends of the day, and differently
  //
  // dress() hands back the director's one room copy, so each hour has to be read
  // out before the next one is dressed
  std::string dayWords;
  std::string nightWords;
  bool spinsByDay = false;
  bool spinsByNight = false;

  for (int pass = 0; pass < 2; pass++) {
    const bool lit = pass == 0;
    standAt(lit ? 13.f : 2.f, &events);

    const petscop::MapRoom* closet = roomNamed(map, "Closet");
    if (!closet) break;

    const petscop::MapObject* rock = objectNamed(events.dress(*closet, 0), "rock");
    if (!rock) {
      complain("the rock is not in the closet at all");
      break;
    }

    std::string& words = lit ? dayWords : nightWords;
    bool& spins = lit ? spinsByDay : spinsByNight;

    if (!rock->actions.empty()) words = rock->actions[0].text;
    for (const petscop::MapAction& act : rock->actions)
      spins = spins || act.kind == petscop::ActionKind::Rotate;
  }

  if (dayWords.find("stuck in place") == std::string::npos)
    complain("the rock does not say it is stuck by day");
  if (nightWords.find("little spin") == std::string::npos)
    complain("the rock does not offer a spin at night");

  if (spinsByDay) complain("the rock turns in daylight");
  if (!spinsByNight) complain("the rock will not turn at night");

  printf("%s\n", failures == 0 ? "all good" : "SOMETHING IS WRONG");
  return failures == 0 ? 0 : 1;
}
