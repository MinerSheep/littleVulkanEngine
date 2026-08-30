#include "petscop/other.hpp"

#include <algorithm>
#include <cstddef>
#include <deque>
#include <vector>

namespace petscop {

namespace {

// Everything the four quests run through. He walks past all of it
const char* kQuestProps[] = {"rock", "gate",      "lever", "mirror",
                             "cue",  "dig_patch", "spade", "piano"};
const char* kQuestItems[] = {"cue", "spade", "piece", "map"};

// One in this many turns he does nothing at all, so the trace is not a machine
const unsigned int kIdleInEvery = 4;

// And one in this many he spends going through your pockets instead
const unsigned int kRifleInEvery = 6;

bool listed(const char* const* names, std::size_t count, const std::string& name) {
  for (std::size_t i = 0; i < count; i++)
    if (name == names[i]) return true;
  return false;
}

// Enough randomness to pick a door, and the same seed always picks the same one
unsigned int roll(unsigned int& seed) {
  seed = seed * 1664525u + 1013904223u;
  return seed >> 8;
}

// A name the way the board prints it
std::string shout(const std::string& name) {
  std::string out;
  for (char letter : name) {
    if (letter == '_') out += ' ';
    else if (letter >= 'a' && letter <= 'z') out += static_cast<char>(letter - 'a' + 'A');
    else out += letter;
  }
  return out;
}

int roomIndexOf(const GameMap& map, const std::string& name) {
  for (std::size_t i = 0; i < map.rooms.size(); i++)
    if (map.rooms[i].name == name) return static_cast<int>(i);
  return -1;
}

const MapObject* findObject(const MapRoom& room, const std::string& name) {
  for (const MapObject& object : room.objects)
    if (object.name == name) return &object;
  return nullptr;
}

// Where every door in the map comes out
std::vector<std::vector<int>> doorGraph(const GameMap& map) {
  std::vector<std::vector<int>> graph(map.rooms.size());
  for (std::size_t i = 0; i < map.rooms.size(); i++) {
    for (const MapDoor& door : map.rooms[i].doors) {
      if (door.toRoom < 0 || door.toRoom >= static_cast<int>(map.rooms.size())) continue;
      graph[i].push_back(door.toRoom);
    }
  }
  return graph;
}

// The room to step into first on the way to another, or -1 when there is no way
// through. Doors that do not come back the way they went are why this is a walk
// and not a subtraction
int firstHop(const std::vector<std::vector<int>>& graph, int from, int to) {
  if (from == to) return from;

  std::vector<int> cameFrom(graph.size(), -1);
  std::deque<int> queue;
  cameFrom[static_cast<std::size_t>(from)] = from;
  queue.push_back(from);

  while (!queue.empty()) {
    const int here = queue.front();
    queue.pop_front();

    for (int next : graph[static_cast<std::size_t>(here)]) {
      if (cameFrom[static_cast<std::size_t>(next)] >= 0) continue;
      cameFrom[static_cast<std::size_t>(next)] = here;

      if (next == to) {
        // Back down the trail until the step out of the room he is stood in
        int step = next;
        while (cameFrom[static_cast<std::size_t>(step)] != from) {
          step = cameFrom[static_cast<std::size_t>(step)];
        }
        return step;
      }
      queue.push_back(next);
    }
  }
  return -1;
}

// What his pockets let him do, which is not what yours do
// Flags are the world's memory and he reads the same ones you do
bool allowsFor(const MapAction& action, const GameState& state, const Other& him) {
  switch (action.when) {
    case ActionWhen::Flag: return state.hasFlag(action.whenName);
    case ActionWhen::NoFlag: return !state.hasFlag(action.whenName);
    case ActionWhen::Item: return him.pocket.count(action.whenName) != 0;
    case ActionWhen::NoItem: return him.pocket.count(action.whenName) == 0;
    case ActionWhen::Always: break;
  }
  return true;
}

// Puts a prop somewhere else and writes it down, the way walking out of a room
// writes down everything standing in it
void shiftProp(GameState& state, const MapRoom& room, const std::string& target,
               const MapAction& action) {
  const MapObject* object = findObject(room, target);
  if (!object) return;

  const std::string key = room.name + "." + target;
  const std::map<std::string, PropMemory>::const_iterator found = state.memories.find(key);

  PropMemory memory;
  if (found != state.memories.end()) {
    memory = found->second;
  } else {
    memory.translation = object->translation;
    memory.rotation = object->rotation;
    memory.scale = object->scale;
  }

  // A toggle measures from where the map stood it, the same as it does for you
  if (action.toggle) {
    memory.translation = object->translation;
    memory.rotation = object->rotation;
    memory.scale = object->scale;
  }

  switch (action.kind) {
    case ActionKind::Move: memory.translation += action.amount; break;
    case ActionKind::Rotate: memory.rotation += action.amount; break;
    case ActionKind::Scale: memory.scale *= action.amount; break;
    case ActionKind::Show: memory.hidden = false; break;
    case ActionKind::Hide: memory.hidden = true; break;
    default: return;
  }

  state.memories[key] = memory;
}

// Runs a prop's own list against his pockets. He is not there to read a line or
// hear a sound, so those two do nothing
//
// He is welcome to anything, a step of the poem included. He does not keep it --
// everything in his hands goes on the floor the moment you come back
// Returns what he came away with, for the trace, or empty when nothing happened
std::string press(GameState& state, const MapRoom& room, const std::string& propName) {
  const MapObject* object = findObject(room, propName);
  if (!object) return std::string();

  std::string took;
  bool did = false;

  for (const MapAction& action : object->actions) {
    if (!allowsFor(action, state, state.other)) continue;

    if (action.kind == ActionKind::Say || action.kind == ActionKind::Sound) continue;

    if (action.kind == ActionKind::Give) {
      state.other.pocket[action.text] += action.count;
      state.addItem("@handled." + action.text, action.count);
      took = action.text;
      did = true;
      continue;
    }

    if (action.kind == ActionKind::Take) {
      // The same lock it is for you: nothing to hand over and the list stops
      const std::map<std::string, int>::iterator held = state.other.pocket.find(action.text);
      if (held == state.other.pocket.end() || held->second < action.count) break;

      held->second -= action.count;
      if (held->second <= 0) state.other.pocket.erase(held);
      did = true;
      continue;
    }

    if (action.kind == ActionKind::Flag || action.kind == ActionKind::Unflag) {
      // The four steps are yours to finish, so their flags are not his to set
      if (action.text.rfind("quest_", 0) == 0) continue;
      state.setFlag(action.text, action.kind == ActionKind::Flag);
      did = true;
      continue;
    }

    shiftProp(state, room, action.target.empty() ? propName : action.target, action);
    did = true;
  }

  if (!did) return std::string();
  return took.empty() ? "TOUCHED THE " + shout(propName) : "TOOK THE " + shout(took);
}

// Now and then he goes through your pockets. Nothing is safe from this, the four
// steps included -- he puts it all down again the moment you come back
bool steal(GameState& state, unsigned int& seed, std::string& taken) {
  std::vector<std::string> yours;
  for (const std::pair<const std::string, int>& held : state.items) {
    // The counters the save keeps in your pockets are not things
    if (held.second <= 0 || held.first.empty() || held.first[0] == '@') continue;
    yours.push_back(held.first);
  }
  if (yours.empty()) return false;

  taken = yours[roll(seed) % yours.size()];
  state.addItem(taken, -1);
  state.other.pocket[taken] += 1;
  state.addItem("@handled." + taken, 1);
  return true;
}

// Everything in his hands goes on the floor where he stands, the moment you are
// back at the controls
//
// This is the whole of his honesty. He can walk off with a step of the poem, and
// he can lift one out of your own pocket, but he cannot keep either -- it is left
// lying in whatever room he happened to stop in, and lostThings stands it up
void dropPocket(GameState& state) {
  if (state.other.room.empty() || state.other.pocket.empty()) return;

  for (const std::pair<const std::string, int>& held : state.other.pocket) {
    if (held.second <= 0) continue;
    state.addItem("@left." + state.other.room + "." + held.first, held.second);
  }
  state.other.pocket.clear();
}

// Something worth walking to: a prop he may press, in a room that is not yours
// Never the thing he has just had his hands on, or he stands there rattling it
bool pickGoal(const GameMap& map, const std::string& playerRoom, const std::string& lastPressed,
              unsigned int& seed, int& goalRoom, std::string& goalProp) {
  std::vector<std::pair<int, std::string>> open;

  for (std::size_t i = 0; i < map.rooms.size(); i++) {
    const MapRoom& room = map.rooms[i];
    if (room.name == playerRoom) continue;

    for (const MapObject& object : room.objects) {
      if (object.name.empty() || object.actions.empty()) continue;
      if (isQuestProp(object.name) || object.name == lastPressed) continue;
      open.push_back(std::make_pair(static_cast<int>(i), object.name));
    }
  }

  if (open.empty()) return false;

  const std::size_t pick = roll(seed) % open.size();
  goalRoom = open[pick].first;
  goalProp = open[pick].second;
  return true;
}

}  // namespace

bool isQuestProp(const std::string& name) {
  return listed(kQuestProps, sizeof(kQuestProps) / sizeof(kQuestProps[0]), name);
}

bool isQuestItem(const std::string& name) {
  return listed(kQuestItems, sizeof(kQuestItems) / sizeof(kQuestItems[0]), name);
}

int runOffline(const GameMap& map, GameState& state, int hours, const std::string& playerRoom) {
  if (hours <= 0 || map.rooms.empty()) return 0;

  // The trace is what he did in this gap and nothing before it
  state.other.trace.clear();

  if (state.other.seed == 0)
    state.other.seed = static_cast<unsigned int>(state.lastPlayed) | 1u;
  unsigned int seed = state.other.seed;

  int here = roomIndexOf(map, state.other.room);
  if (here < 0) here = (map.startRoom >= 0 && map.startRoom < static_cast<int>(map.rooms.size()))
                           ? map.startRoom
                           : 0;

  const std::vector<std::vector<int>> graph = doorGraph(map);

  int goalRoom = -1;
  std::string goalProp;
  std::string lastPressed;
  int turns = 0;

  for (int turn = 0; turn < hours; turn++) {
    turns++;

    if (roll(seed) % kIdleInEvery == 0) {
      state.other.trace.push_back("STOOD STILL");
      continue;
    }

    // One turn in six he is in your pockets rather than anywhere in particular
    if (roll(seed) % kRifleInEvery == 0) {
      std::string taken;
      if (steal(state, seed, taken)) {
        state.other.trace.push_back("TOOK YOUR " + shout(taken));
        continue;
      }
    }

    if (goalRoom < 0 && !pickGoal(map, playerRoom, lastPressed, seed, goalRoom, goalProp)) {
      state.other.trace.push_back("STOOD STILL");
      continue;
    }

    if (goalRoom != here) {
      const int step = firstHop(graph, here, goalRoom);

      // A clearing with no way on to where he was going, so he wants somewhere else
      if (step < 0 || step == here) {
        goalRoom = -1;
        state.other.trace.push_back("STOOD STILL");
        continue;
      }

      here = step;
      state.other.trace.push_back("WENT INTO " + shout(map.rooms[static_cast<std::size_t>(here)].name));
      continue;
    }

    // He is standing at it, and the room he is in is never yours
    const std::string what = press(state, map.rooms[static_cast<std::size_t>(here)], goalProp);
    lastPressed = goalProp;
    goalRoom = -1;
    state.other.trace.push_back(what.empty() ? "STOOD STILL" : what);
  }

  state.other.room = map.rooms[static_cast<std::size_t>(here)].name;
  state.other.seed = seed;

  // You are back, so he puts down everything he was carrying, right where he is
  dropPocket(state);
  return turns;
}

}  // namespace petscop
