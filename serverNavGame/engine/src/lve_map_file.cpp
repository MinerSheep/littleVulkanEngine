#include "lve_map_file.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace lve {

namespace {

std::vector<std::string> split(const std::string& line) {
  std::vector<std::string> out;
  std::istringstream in(line);
  std::string word;
  while (in >> word) out.push_back(word);
  return out;
}

bool sameName(const std::string& a, const std::string& b) {
  if (a.size() != b.size()) return false;
  for (std::size_t i = 0; i < a.size(); ++i) {
    if (std::tolower(a[i]) != std::tolower(b[i])) return false;
  }
  return true;
}

// A word that is not a number leaves the value alone rather than throwing
bool toFloat(const std::string& word, float& out) {
  std::istringstream in(word);
  return static_cast<bool>(in >> out);
}

bool toInt(const std::string& word, int& out) {
  std::istringstream in(word);
  return static_cast<bool>(in >> out);
}

// Reads three numbers starting at word i, or leaves the vector alone when the line is short
bool readVec(const std::vector<std::string>& words, std::size_t i, glm::vec3& out) {
  if (i + 2 >= words.size()) return false;
  return toFloat(words[i], out.x) && toFloat(words[i + 1], out.y) && toFloat(words[i + 2], out.z);
}

// Pulls preset, placement and the trailing pass/name words off an obj, wall or interact
bool readPlacement(const std::vector<std::string>& words, const std::vector<std::string>& presets,
                   MapFileObject& out) {
  if (words.size() < 11) return false;

  int index = -1;
  if (!toInt(words[1], index)) return false;
  if (index < 0 || index >= static_cast<int>(presets.size())) return false;
  out.preset = presets[index];

  if (!readVec(words, 2, out.translation)) return false;
  if (!readVec(words, 5, out.rotation)) return false;
  if (!readVec(words, 8, out.scale)) return false;

  for (std::size_t i = 11; i < words.size();) {
    if (words[i] == "pass") {
      out.solid = false;
      ++i;
    } else if (words[i] == "name" && i + 1 < words.size()) {
      out.name = words[i + 1];
      i += 2;
    } else if (words[i] == "face") {
      i += 4;  // a wall's outward normal, which the editor does not touch
    } else {
      ++i;
    }
  }
  return true;
}

std::string f3(float value) {
  std::ostringstream out;
  out << std::fixed << std::setprecision(3) << value;
  return out.str();
}

std::string vec3(const glm::vec3& v) { return f3(v.x) + " " + f3(v.y) + " " + f3(v.z); }

}  // namespace

bool readMapRoom(const std::string& path, const std::string& roomName, MapFileRoom& out,
                 std::string& error) {
  std::ifstream in(path);
  if (!in) {
    error = "could not open " + path;
    return false;
  }

  std::vector<std::string> presets;
  bool inRoom = false;
  bool floorSeen = false;
  out = MapFileRoom{};

  std::string line;
  while (std::getline(in, line)) {
    const std::size_t hash = line.find('#');
    if (hash != std::string::npos) line = line.substr(0, hash);

    const std::vector<std::string> words = split(line);
    if (words.empty()) continue;

    if (!inRoom) {
      // The preset table is above every room and names the meshes by index
      if (words[0] == "preset" && words.size() >= 3) {
        int index = -1;
        if (!toInt(words[1], index) || index < 0) continue;
        if (index >= static_cast<int>(presets.size())) presets.resize(index + 1);
        presets[index] = words[2];
      } else if (words[0] == "room" && words.size() >= 3 && sameName(words[2], roomName)) {
        inRoom = true;
        out.ident = words[2];
      }
      continue;
    }

    if (words[0] == "endroom") break;

    if (words[0] == "size") {
      readVec(words, 1, out.size);
    } else if (words[0] == "cam") {
      readVec(words, 1, out.cameraEye);
      readVec(words, 4, out.cameraLook);
    } else if (words[0] == "obj" || words[0] == "wall" || words[0] == "interact") {
      MapFileObject object;
      object.interact = words[0] == "interact";

      // build_map.py writes the floor as the room's first obj, then the walls
      object.locked = words[0] == "wall" || (words[0] == "obj" && !floorSeen);
      if (words[0] == "obj" && !floorSeen) floorSeen = true;

      if (readPlacement(words, presets, object)) out.objects.push_back(object);
    } else if ((words[0] == "when" || words[0] == "do") && !out.objects.empty() &&
               out.objects.back().interact) {
      // Everything under an interact belongs to it until the next placement
      out.objects.back().script.push_back(line);
    } else if (words[0] == "door" && words.size() >= 11) {
      // Drawn as a plain box so props can be kept clear of the doorway
      MapFileObject door;
      door.preset = "cube";
      door.locked = true;
      door.name = words[1];
      readVec(words, 2, door.translation);
      readVec(words, 5, door.rotation);
      readVec(words, 8, door.scale);
      out.objects.push_back(door);
    }
  }

  if (!inRoom) {
    error = "no room called " + roomName + " in " + path;
    return false;
  }
  return true;
}

bool writeMapsrcProps(const std::string& path, const MapFileRoom& room, std::string& error) {
  std::ofstream out(path);
  if (!out) {
    error = "could not open " + path + " for writing";
    return false;
  }

  out << "# props for room " << room.ident << ", laid out in the editor\n";
  out << "# paste these into the room's block in the .mapsrc, replacing its old prop lines\n";
  out << "# floors, walls, doorways and lights are left out -- build_map.py makes those\n";

  int written = 0;
  for (const MapFileObject& object : room.objects) {
    if (object.locked) continue;

    // A single prop line has no way to say 'walk through me' -- only a props file does
    if (!object.solid) out << "  # the line below was walk-through, put it in a props file\n";

    out << (object.interact ? "  interact " : "  prop ") << object.preset << "  "
        << vec3(object.translation) << "   " << vec3(object.rotation) << "   "
        << vec3(object.scale);
    if (!object.name.empty()) out << "  name " << object.name;
    out << '\n';

    for (const std::string& scriptLine : object.script) {
      std::istringstream trim(scriptLine);
      std::string rest;
      std::getline(trim >> std::ws, rest);
      out << "  " << rest << '\n';
    }
    ++written;
  }

  if (written == 0) out << "# (the room has no props)\n";
  return true;
}

}  // namespace lve
