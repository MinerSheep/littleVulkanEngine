#pragma once

// Reads one room out of a compiled .map so the editor can move its props about,
// and writes those props back as the .mapsrc lines that would rebuild them
//
// Only props come back out. Floors, walls, doorways and lights are the map
// generator's work, so they are read for reference and never written

#include <glm/glm.hpp>

#include <string>
#include <vector>

namespace lve {

// Something standing in a room, as the .map spells it
struct MapFileObject {
  std::string preset;  // mesh name, already looked up in the map's preset table
  glm::vec3 translation{0.f};
  glm::vec3 rotation{0.f};
  glm::vec3 scale{1.f};

  bool interact = false;  // answers E, and keeps its when/do lines
  bool solid = true;      // grass and the like, walked straight through
  std::string name;

  // The when/do lines under an interact, kept word for word
  std::vector<std::string> script;

  // Floors, walls and doorways are drawn so props can be lined up against them,
  // and the editor refuses to move them
  bool locked = false;
};

// One room lifted out of a .map
struct MapFileRoom {
  std::string ident;
  glm::vec3 size{0.f};  // width, depth, height
  glm::vec3 cameraEye{0.f};
  glm::vec3 cameraLook{0.f};
  std::vector<MapFileObject> objects;
};

// Pulls one room out of a compiled .map, matching the name either way round on case
// Returns false and fills error when the file or the room is not there
bool readMapRoom(const std::string& path, const std::string& roomName, MapFileRoom& out,
                 std::string& error);

// Writes the room's props as prop/interact lines to paste into the .mapsrc
// Skips everything locked, because build_map.py lays those out itself
bool writeMapsrcProps(const std::string& path, const MapFileRoom& room, std::string& error);

}  // namespace lve
