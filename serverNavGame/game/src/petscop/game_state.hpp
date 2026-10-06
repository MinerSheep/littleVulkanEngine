#pragma once

#include "petscop/prop.hpp"

#include <glm/glm.hpp>

#include <map>
#include <set>
#include <string>
#include <vector>

namespace petscop {

// How a named prop was left the last time you walked out of its room
struct PropMemory {
  glm::vec3 translation{0.f};
  glm::vec3 rotation{0.f};
  glm::vec3 scale{1.f};
  bool hidden = false;
  std::vector<char> flipped;
};

// Somebody else who plays this save while the game is shut
//
// He gets an hour for every hour you are away. What he does is written into the
// same flags, items and prop memories everything else reads
struct Other {
  std::string room;                   // where he is standing now
  std::map<std::string, int> pocket;  // what he is carrying, which is not yours
  unsigned int seed = 0;              // the same gap always plays out the same way
  std::vector<std::string> trace;     // what he did last time, as the board prints it
};

// What the game carries between rooms and between runs
// Only named props are remembered, which is every prop a map can name
struct GameState {
  std::set<std::string> flags;
  std::map<std::string, int> items;
  std::map<std::string, PropMemory> memories;  // keyed "<room>.<prop>"
  std::string room;                            // the room you were in when it was written

  // When the save was last written, in seconds, and the man who reads it
  long long lastPlayed = 0;
  Other other;

  bool hasFlag(const std::string& name) const;
  void setFlag(const std::string& name, bool on);

  int itemCount(const std::string& name) const;
  bool hasItem(const std::string& name) const { return itemCount(name) > 0; }

  // Adds to a stack, or takes off it with a negative count
  // A stack that runs out is dropped rather than kept at zero
  void addItem(const std::string& name, int count);

  // Whether an action's 'when' lets it run
  bool allows(const MapAction& action) const;

  // Writes down how a room was left
  void rememberRoom(const std::string& roomName, const std::vector<Prop>& props);

  // Stands the props back where you left them, boxes and all
  void restoreRoom(const std::string& roomName, std::vector<Prop>& props) const;
};

}  // namespace petscop
