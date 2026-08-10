#include "petscop/game_state.hpp"

#include <cstddef>

namespace petscop {

namespace {

std::string keyFor(const std::string& roomName, const std::string& propName) {
  return roomName + "." + propName;
}

}  // namespace

bool GameState::hasFlag(const std::string& name) const { return flags.count(name) != 0; }

void GameState::setFlag(const std::string& name, bool on) {
  if (on) flags.insert(name);
  else flags.erase(name);
}

int GameState::itemCount(const std::string& name) const {
  const std::map<std::string, int>::const_iterator found = items.find(name);
  return found == items.end() ? 0 : found->second;
}

void GameState::addItem(const std::string& name, int count) {
  if (name.empty()) return;

  int& held = items[name];
  held += count;
  if (held <= 0) items.erase(name);
}

bool GameState::allows(const MapAction& action) const {
  switch (action.when) {
    case ActionWhen::Flag: return hasFlag(action.whenName);
    case ActionWhen::NoFlag: return !hasFlag(action.whenName);
    case ActionWhen::Item: return hasItem(action.whenName);
    case ActionWhen::NoItem: return !hasItem(action.whenName);
    case ActionWhen::Always: break;
  }
  return true;
}

void GameState::rememberRoom(const std::string& roomName, const std::vector<Prop>& props) {
  for (const Prop& prop : props) {
    if (prop.name.empty()) continue;

    // A prop caught mid move is written down where it was heading
    PropMemory memory;
    memory.translation = prop.motion.active ? prop.motion.toTranslation : prop.translation;
    memory.rotation = prop.motion.active ? prop.motion.toRotation : prop.rotation;
    memory.scale = prop.motion.active ? prop.motion.toScale : prop.scale;
    memory.hidden = prop.disappeared;
    memory.flipped = prop.flipped;

    memories[keyFor(roomName, prop.name)] = memory;
  }
}

void GameState::restoreRoom(const std::string& roomName, std::vector<Prop>& props) const {
  for (Prop& prop : props) {
    if (prop.name.empty()) continue;

    const std::map<std::string, PropMemory>::const_iterator found =
        memories.find(keyFor(roomName, prop.name));
    if (found == memories.end()) continue;

    const PropMemory& memory = found->second;
    prop.translation = memory.translation;
    prop.rotation = memory.rotation;
    prop.scale = memory.scale;
    prop.disappeared = memory.hidden;
    prop.collider.enabled = !memory.hidden && prop.solid;
    prop.motion.active = false;

    // An old save may carry fewer flips than the prop now has actions
    for (std::size_t i = 0; i < prop.flipped.size() && i < memory.flipped.size(); i++)
      prop.flipped[i] = memory.flipped[i];

    prop.refreshBox();
  }
}

}  // namespace petscop
