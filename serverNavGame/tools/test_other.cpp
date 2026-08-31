// Hammers X07 against a map and checks he never takes the game away from you
//
// He plays your save while the game is shut. The thing that would ruin a run is
// him finishing, hiding or pocketing one of the four steps of the poem, so this
// runs a few hundred imaginary players who each leave the game alone for a while
// and asserts that everything the four steps need is still there afterwards
//
// Build and run from the repo root:
//   g++ -std=c++17 -O2 -Iengine/include -Iengine/libs -Igame/src \
//       tools/test_other.cpp game/src/petscop/other.cpp \
//       game/src/petscop/outside.cpp game/src/petscop/game_state.cpp \
//       game/src/petscop/map_loader.cpp game/src/petscop/save_file.cpp \
//       game/src/petscop/prop.cpp game/src/collider_component.cpp \
//       game/src/lve_game_object.cpp -o /tmp/test_other
//
//   /tmp/test_other maps/petscop.map 300
//   /tmp/test_other maps/forest.map 300
//
// It says "all good" and exits 0, or prints what went wrong and exits 1

#include "petscop/other.hpp"
#include "petscop/outside.hpp"
#include "petscop/save_file.hpp"

#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <random>
#include <string>
#include <vector>

namespace {

int failures = 0;

void complain(const std::string& what) {
  std::cout << "  FAIL " << what << "\n";
  failures++;
}

// Whether the thing can still be got hold of, one way or another
//
// He is allowed to walk off with a step of the poem and he is allowed to lift one
// out of your pocket. What he is not allowed to do is leave it nowhere
bool obtainable(const petscop::GameMap& map, const petscop::GameState& state,
                const std::string& item) {
  if (state.itemCount(item) > 0) return true;

  // Lying on the floor of some room, where lostThings will stand it back up
  const std::string tail = "." + item;
  for (const std::pair<const std::string, int>& held : state.items) {
    if (held.second <= 0 || held.first.rfind("@left.", 0) != 0) continue;
    if (held.first.size() > tail.size() &&
        held.first.compare(held.first.size() - tail.size(), tail.size(), tail) == 0)
      return true;
  }

  // Or still standing wherever the map first put it
  for (const petscop::MapRoom& room : map.rooms) {
    for (const petscop::MapObject& object : room.objects) {
      bool gives = false;
      for (const petscop::MapAction& action : object.actions)
        gives = gives || (action.kind == petscop::ActionKind::Give && action.text == item);
      if (!gives) continue;

      const std::map<std::string, petscop::PropMemory>::const_iterator found =
          state.memories.find(room.name + "." + object.name);
      if (found == state.memories.end() || !found->second.hidden) return true;
    }
  }
  return false;
}

// Everything the four steps run through is still there to be walked up to
void checkQuestsReachable(const petscop::GameMap& map, const petscop::GameState& state,
                          const std::string& trial) {
  for (const petscop::MapRoom& room : map.rooms) {
    for (const petscop::MapObject& object : room.objects) {
      if (object.name.empty() || !petscop::isQuestProp(object.name)) continue;

      const std::string key = room.name + "." + object.name;
      const std::map<std::string, petscop::PropMemory>::const_iterator found =
          state.memories.find(key);
      if (found == state.memories.end()) continue;

      if (found->second.hidden)
        complain(trial + ": " + key + " has been hidden");
    }
  }

  // Every step of the poem the map hands out is somewhere you can still get it
  for (const petscop::MapRoom& room : map.rooms) {
    for (const petscop::MapObject& object : room.objects) {
      for (const petscop::MapAction& action : object.actions) {
        if (action.kind != petscop::ActionKind::Give) continue;
        if (!petscop::isQuestItem(action.text)) continue;

        if (!obtainable(map, state, action.text))
          complain(trial + ": " + action.text + " cannot be got hold of any more");
      }
    }
  }

  // He never holds anything once you are back at the controls
  for (const std::pair<const std::string, int>& held : state.other.pocket) {
    if (held.second > 0) complain(trial + ": he is still holding " + held.first);
  }

  // The four steps are yours to finish
  const char* quests[] = {"quest_stone", "quest_mirror", "quest_tiles", "quest_dig"};
  for (const char* quest : quests) {
    if (state.hasFlag(quest)) complain(trial + ": " + quest + " was set for you");
  }
}

}  // namespace

int main(int argc, char** argv) {
  const std::string path = argc > 1 ? argv[1] : "maps/petscop.map";
  const int trials = argc > 2 ? std::atoi(argv[2]) : 200;

  petscop::GameMap map;
  std::string error;
  if (!petscop::loadMap(path, map, error)) {
    std::cerr << "test_other: " << error << "\n";
    return 2;
  }

  std::mt19937 rng(20260829u);
  std::uniform_int_distribution<int> gapHours(1, 72);
  std::uniform_int_distribution<int> sessions(1, 8);
  std::uniform_int_distribution<std::size_t> anyRoom(0, map.rooms.size() - 1);

  int turnsTaken = 0;
  int roomsMoved = 0;

  for (int trial = 0; trial < trials; trial++) {
    petscop::GameState state;
    state.lastPlayed = 1700000000;
    state.room = map.rooms[anyRoom(rng)].name;

    // Something in your pockets for him to go through, the four steps included
    state.addItem("cue", 1);
    state.addItem("spade", 1);
    state.addItem("map", 1);
    state.addItem("lantern", 1);

    const int runs = sessions(rng);
    for (int run = 0; run < runs; run++) {
      const long long then = state.lastPlayed;
      const long long now = then + static_cast<long long>(gapHours(rng)) * 3600;

      const int hours = petscop::hoursBetween(then, now);
      const std::string was = state.other.room;
      turnsTaken += petscop::runOffline(map, state, hours, state.room);
      if (!was.empty() && was != state.other.room) roomsMoved++;

      // Whatever he did, the room you are standing in is not his to change
      for (const std::pair<const std::string, petscop::PropMemory>& entry : state.memories) {
        if (entry.first.rfind(state.room + ".", 0) == 0)
          complain("trial " + std::to_string(trial) + ": he wrote into your room " + entry.first);
      }

      checkQuestsReachable(map, state, "trial " + std::to_string(trial));

      // He always ends up somewhere the map actually has
      bool real = state.other.room.empty();
      for (const petscop::MapRoom& room : map.rooms) real = real || room.name == state.other.room;
      if (!real) complain("trial " + std::to_string(trial) + ": he is in '" + state.other.room + "'");

      state.lastPlayed = now;
    }
  }

  // The same save and the same gap play out the same way, twice running
  {
    petscop::GameState a;
    a.lastPlayed = 1700000000;
    a.room = "Foyer";
    petscop::GameState b = a;

    petscop::runOffline(map, a, 9, a.room);
    petscop::runOffline(map, b, 9, b.room);
    if (a.other.trace != b.other.trace) complain("the same gap played out two different ways");
    if (a.other.room != b.other.room) complain("the same gap left him in two places");
  }

  // A gap under an hour, and a clock that has gone backwards, are worth nothing
  {
    petscop::GameState state;
    state.lastPlayed = 1700000000;
    if (petscop::hoursBetween(state.lastPlayed, state.lastPlayed + 3599) != 0)
      complain("a gap under an hour counted");
    if (petscop::hoursBetween(state.lastPlayed, state.lastPlayed - 40000) != 0)
      complain("a clock going backwards counted");
    if (petscop::hoursBetween(state.lastPlayed, state.lastPlayed + 336LL * 3600) != 12)
      complain("a fortnight away was not capped");
  }

  printf("%d trial(s), %d turn(s) taken, he changed room %d time(s)\n", trials, turnsTaken,
         roomsMoved);
  printf("%s\n", failures == 0 ? "all good" : "SOMETHING IS WRONG");
  return failures == 0 ? 0 : 1;
}
