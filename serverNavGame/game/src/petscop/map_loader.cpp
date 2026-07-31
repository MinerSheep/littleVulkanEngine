#include "petscop/map_loader.hpp"

#include <fstream>
#include <sstream>

namespace petscop {

namespace {

bool readVec3(std::istringstream& ss, glm::vec3& out) {
  return static_cast<bool>(ss >> out.x >> out.y >> out.z);
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
  std::string line;

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

    } else if (key == "endroom") {
      room = -1;

    } else if (room < 0) {
      return fail(key + " outside a room");

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

    } else if (key == "obj") {
      MapObject object;
      if (!(ss >> object.preset) || !readVec3(ss, object.translation) ||
          !readVec3(ss, object.rotation) || !readVec3(ss, object.scale))
        return fail("obj reads: obj <preset> tx ty tz  rx ry rz  sx sy sz");
      if (object.preset < 0 || object.preset >= static_cast<int>(out.presets.size()))
        return fail("obj names preset " + std::to_string(object.preset) + ", which is not declared");
      out.rooms[room].objects.push_back(object);

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

  return true;
}

}  // namespace petscop
