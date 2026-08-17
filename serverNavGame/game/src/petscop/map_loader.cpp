#include "petscop/map_loader.hpp"

#include <fstream>
#include <sstream>

namespace petscop {

namespace {

bool readVec3(std::istringstream& ss, glm::vec3& out) {
  return static_cast<bool>(ss >> out.x >> out.y >> out.z);
}

std::string readRest(std::istringstream& ss) 
{
  std::string rest;
  std::getline(ss, rest);

  const std::size_t first = rest.find_first_not_of(" \t\r");
  if (first == std::string::npos) return std::string();

  const std::size_t last = rest.find_last_not_of(" \t\r");

  // get substring substr
  return rest.substr(first, last - first + 1);
}

bool readActionKind(const std::string& word, ActionKind& out) {
  if (word == "say") out = ActionKind::Say;
  else if (word == "move") out = ActionKind::Move;
  else if (word == "rotate") out = ActionKind::Rotate;
  else if (word == "scale") out = ActionKind::Scale;
  else if (word == "show") out = ActionKind::Show;
  else if (word == "hide") out = ActionKind::Hide;
  else if (word == "sound") out = ActionKind::Sound;
  else if (word == "give") out = ActionKind::Give;
  else if (word == "take") out = ActionKind::Take;
  else if (word == "flag") out = ActionKind::Flag;
  else if (word == "unflag") out = ActionKind::Unflag;
  else return false;
  return true;
}

bool readActionWhen(const std::string& word, ActionWhen& out) {
  if (word == "always") out = ActionWhen::Always;
  else if (word == "flag") out = ActionWhen::Flag;
  else if (word == "noflag") out = ActionWhen::NoFlag;
  else if (word == "item") out = ActionWhen::Item;
  else if (word == "noitem") out = ActionWhen::NoItem;
  else return false;
  return true;
}

bool movesSomething(ActionKind kind) {
  return kind == ActionKind::Move || kind == ActionKind::Rotate || kind == ActionKind::Scale;
}

// Touches the save instead of a prop, and names what it touches rather than a target
bool namesSomething(ActionKind kind) {
  return kind == ActionKind::Give || kind == ActionKind::Take || kind == ActionKind::Flag ||
         kind == ActionKind::Unflag;
}

bool carriesItems(ActionKind kind) {
  return kind == ActionKind::Give || kind == ActionKind::Take;
}

}  // namespace

bool loadMap(const std::string& path, GameMap& out, std::string& error) {
  std::ifstream in(path);
  if (!in) {
    error = "could not open " + path + " (run the game from the repo root, and build the map "
            "first with tools/build_map.py)";
    return false;
  }

  out = GameMap{};

  int lineNumber = 0;
  int room = -1;  // an index, not a pointer, so growing rooms can never strand it
  int lastObject = -1;
  std::string line;

  // A 'when' line gates every 'do' under it, up to the next 'when' or the next prop
  ActionWhen pendingWhen = ActionWhen::Always;
  std::string pendingWhenName;

  auto fail = [&](const std::string& why) {
    error = path + ":" + std::to_string(lineNumber) + ": " + why;
    return false;
  };

  while (std::getline(in, line)) {
    lineNumber++;

    const std::size_t comment = line.find('#');
    if (comment != std::string::npos) line.erase(comment);

    std::istringstream ss(line);
    std::string key;
    if (!(ss >> key)) continue;  // blank or all comment

    if (key == "map") {
      int version = 0;
      if (!(ss >> version)) return fail("map needs a version number");
      if (version != 1) return fail("map version " + std::to_string(version) + " is not supported");

    } else if (key == "name") {
      ss >> out.name;

    } else if (key == "start") {
      if (!(ss >> out.startRoom)) return fail("start needs a room number");

    } else if (key == "preset") {
      int index = 0;
      std::string name;
      if (!(ss >> index >> name)) return fail("preset reads: preset <index> <name>");
      if (index != static_cast<int>(out.presets.size()))
        return fail("preset " + std::to_string(index) + " is out of order");
      out.presets.push_back(name);

    } else if (key == "room") {
      int index = 0;
      std::string name;
      if (!(ss >> index >> name)) return fail("room reads: room <index> <name>");
      if (index != static_cast<int>(out.rooms.size()))
        return fail("room " + std::to_string(index) + " is out of order");
      out.rooms.push_back(MapRoom{});
      out.rooms.back().name = name;
      room = index;
      lastObject = -1;
      pendingWhen = ActionWhen::Always;
      pendingWhenName.clear();

    } else if (key == "endroom") {
      room = -1;
      lastObject = -1;
      pendingWhen = ActionWhen::Always;
      pendingWhenName.clear();

    } else if (room < 0) {
      return fail(key + " outside a room");

    } else if (key == "noname") {
      out.rooms[room].showName = false;

    } else if (key == "size") {
      if (!readVec3(ss, out.rooms[room].size)) return fail("size needs width, depth and height");

    } else if (key == "cam") {
      if (!readVec3(ss, out.rooms[room].cameraEye) || !readVec3(ss, out.rooms[room].cameraLook))
        return fail("cam needs an eye and a look point, six numbers");

    } else if (key == "light") {
      MapLight light;
      if (!readVec3(ss, light.position) || !readVec3(ss, light.color) || !(ss >> light.intensity))
        return fail("light needs a position, a colour and an intensity");
      out.rooms[room].lights.push_back(light);

    } else if (key == "obj" || key == "wall" || key == "interact") {
      // Same thing standing in the room either way, but a wall also says which
      // way it faces so it can be dropped when it is in front of the camera
      MapObject object;
      if (!(ss >> object.preset) || !readVec3(ss, object.translation) ||
          !readVec3(ss, object.rotation) || !readVec3(ss, object.scale))
        return fail(key + " reads: " + key + " <preset> tx ty tz  rx ry rz  sx sy sz" +
                    (key == "wall" ? "  face <nx ny nz>" : "") + "  [name <id>]" +
                    (key == "interact" ? "  [say <words>]" : ""));
      if (object.preset < 0 || object.preset >= static_cast<int>(out.presets.size()))
        return fail(key + " names preset " + std::to_string(object.preset) +
                    ", which is not declared");
      if (key == "wall") {
        std::string keyword;  // "face", there to keep the file readable
        if (!(ss >> keyword) || !readVec3(ss, object.face))
          return fail("wall is missing its 'face'");
      }

      std::string keyword;
      while (ss >> keyword) {
        if (keyword == "name") {
          if (!(ss >> object.name)) return fail("name needs a word after it");
        } else if (keyword == "pass") {
          object.solid = false;
        } else if (keyword == "say") {
          MapAction action;
          action.kind = ActionKind::Say;
          action.text = readRest(ss);
          if (action.text.empty()) return fail("say has nothing to say");
          object.actions.push_back(action);
          break;
        } else {
          return fail("unexpected '" + keyword + "' after " + key);
        }
      }

      out.rooms[room].objects.push_back(object);
      lastObject = static_cast<int>(out.rooms[room].objects.size()) - 1;
      pendingWhen = ActionWhen::Always;
      pendingWhenName.clear();

    } else if (key == "when") {
      std::string word;
      if (!(ss >> word)) return fail("when reads: when <always|flag|noflag|item|noitem> [name]");
      if (!readActionWhen(word, pendingWhen)) return fail("when does not know about '" + word + "'");

      pendingWhenName.clear();
      if (pendingWhen != ActionWhen::Always && !(ss >> pendingWhenName))
        return fail("when " + word + " needs a name after it");

    } else if (key == "do") {
      if (lastObject < 0) return fail("do comes after the thing it belongs to, and there is none yet");

      std::string word;
      if (!(ss >> word))
        return fail("do reads: do <say|move|rotate|scale|show|hide|sound|give|take|flag|unflag> ...");

      MapAction action;
      if (!readActionKind(word, action.kind)) return fail("do does not know how to '" + word + "'");
      action.when = pendingWhen;
      action.whenName = pendingWhenName;

      if (action.kind == ActionKind::Say) {
        action.text = readRest(ss);
        if (action.text.empty()) return fail("do say has nothing to say");

      } else if (action.kind == ActionKind::Sound) {
        if (!(ss >> action.text)) return fail("do sound needs a clip name");

      } else if (namesSomething(action.kind)) {
        if (!(ss >> action.text)) return fail("do " + word + " needs a name after it");
        if (carriesItems(action.kind) && !(ss >> action.count)) action.count = 1;
        if (action.count < 1) return fail("do " + word + " needs a count of one or more");

      } else {
        if (!(ss >> action.target)) return fail("do " + word + " needs something to act on");
        if (action.target == "self") action.target.clear();

        if (movesSomething(action.kind)) {
          std::string keyword;
          if (!readVec3(ss, action.amount)) return fail("do " + word + " needs three numbers");
          if (!(ss >> keyword) || keyword != "over" || !(ss >> action.seconds))
            return fail("do " + word + " is missing its 'over <seconds>'");
          if (action.seconds < 0.f) return fail("do " + word + " cannot take negative time");
          if (ss >> keyword) {
            if (keyword != "toggle") return fail("unexpected '" + keyword + "' after do " + word);
            action.toggle = true;
          }
        }
      }

      out.rooms[room].objects[lastObject].actions.push_back(action);

    } else if (key == "door") {
      MapDoor door;
      std::string keyword;
      if (!(ss >> door.name) || !readVec3(ss, door.translation) || !readVec3(ss, door.rotation) ||
          !readVec3(ss, door.scale))
        return fail("door reads: door <name> tx ty tz  rx ry rz  sx sy sz  to <room> <door>  "
                    "spawn <x y z> <yaw>");
      // "to" and "spawn" are there to keep the file readable, they carry nothing
      if (!(ss >> keyword >> door.toRoom >> door.toDoor)) return fail("door is missing its 'to'");
      if (!(ss >> keyword) || !readVec3(ss, door.spawn) || !(ss >> door.spawnYaw))
        return fail("door is missing its 'spawn'");
      out.rooms[room].doors.push_back(door);

    } else {
      return fail("unknown line '" + key + "'");
    }
  }

  // Everything is read, so the links can finally be checked against real rooms
  lineNumber = 0;
  if (out.rooms.empty()) {
    error = path + ": no rooms in this map";
    return false;
  }
  if (out.startRoom < 0 || out.startRoom >= static_cast<int>(out.rooms.size())) {
    error = path + ": start is room " + std::to_string(out.startRoom) + ", but the map has " +
            std::to_string(out.rooms.size()) + " room(s)";
    return false;
  }
  for (const MapRoom& current : out.rooms) {
    for (const MapDoor& door : current.doors) {
      if (door.toRoom < 0) continue;  // a door that leads nowhere is allowed
      if (door.toRoom >= static_cast<int>(out.rooms.size())) {
        error = path + ": door " + current.name + "." + door.name + " leads to room " +
                std::to_string(door.toRoom) + ", which does not exist";
        return false;
      }
      if (door.toDoor < 0 ||
          door.toDoor >= static_cast<int>(out.rooms[door.toRoom].doors.size())) {
        error = path + ": door " + current.name + "." + door.name + " arrives at door " +
                std::to_string(door.toDoor) + " of " + out.rooms[door.toRoom].name +
                ", which does not exist";
        return false;
      }
    }
  }

  for (const MapRoom& current : out.rooms) {
    for (const MapObject& object : current.objects) {
      for (const MapAction& action : object.actions) {
        if (action.target.empty()) continue;

        bool found = false;
        for (const MapObject& other : current.objects) {
          if (other.name == action.target) {
            found = true;
            break;
          }
        }
        if (!found) {
          error = path + ": in room " + current.name + ", an action points at '" + action.target +
                  "', and nothing in that room is called that";
          return false;
        }
      }
    }
  }

  return true;
}

}  // namespace petscop
