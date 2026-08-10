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

// What the game carries between rooms and between runs
// Only named props are remembered, which is every prop a map can name
struct GameState {
  std::set<std::string> flags;
  std::map<std::string, int> items;
  std::map<std::string, PropMemory> memories;  // keyed "<room>.<prop>"
  std::string room;                            // the room you were in when it was written

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
