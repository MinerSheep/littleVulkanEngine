#pragma once

#include "lve_canvas.hpp"
#include "petscop/map_loader.hpp"

#include <glm/glm.hpp>

#include <vector>

namespace petscop {

// Where a room sits on the floor plan, in map units
// centre.y is the room's z, since a plan has no height in it
struct RoomPlace {
  bool placed = false;
  glm::vec2 centre{0.f};
  glm::vec2 size{0.f};
};

// Walks the doors out from the start room and works out where each one would sit
// if the house joined up, leaving air between them
//
// Two rooms reached different ways round can land on the same ground, and those
// are shoved apart afterwards. Rooms nothing links to are left unplaced
std::vector<RoomPlace> layoutRooms(const GameMap& map);

// The house as a picture, with the room you are in picked out
//
// Every room is on it whether or not you have been in one, and one room on it is
// not in the map at all
lve::LveCanvas drawHouseMap(const GameMap& map, int currentRoom, int width = 80, int height = 60);

}  // namespace petscop
