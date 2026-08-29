#include "petscop/house_map.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace petscop {

namespace {

// The plan's colours, dim enough to read as a drawing rather than a screen
const glm::vec3 kWall{0.42f, 0.46f, 0.40f};
const glm::vec3 kDoor{0.70f, 0.74f, 0.58f};
const glm::vec3 kHere{0.90f, 0.86f, 0.55f};

// How deep the room that is not there is drawn
const float kPhantomDepth = 6.f;

// Air left between two rooms, so the plan reads as rooms and not as one shape
const float kGap = 2.f;

// The house does not lay out flat -- two rooms reached different ways round can
// want the same corner. They are shoved apart until none of them share ground
void spreadApart(std::vector<RoomPlace>& places) {
  for (int pass = 0; pass < 32; pass++) {
    bool shoved = false;

    for (std::size_t a = 0; a < places.size(); a++) {
      if (!places[a].placed) continue;

      for (std::size_t b = a + 1; b < places.size(); b++) {
        if (!places[b].placed) continue;

        const glm::vec2 apart = places[b].centre - places[a].centre;
        const glm::vec2 room = (places[a].size + places[b].size) * 0.5f + kGap;
        const glm::vec2 over = room - glm::abs(apart);
        if (over.x <= 0.f || over.y <= 0.f) continue;

        // Out the way that needs the least moving, half of it each
        glm::vec2 push(0.f);
        if (over.x < over.y)
          push.x = (apart.x < 0.f ? -1.f : 1.f) * over.x * 0.5f;
        else
          push.y = (apart.y < 0.f ? -1.f : 1.f) * over.y * 0.5f;

        places[a].centre -= push;
        places[b].centre += push;
        shoved = true;
      }
    }

    if (!shoved) return;
  }
}

// Which wall a door stands in: 0 north, 1 south, 2 east, 3 west
// The map does not say, but a door sits on the edge it opens through
int wallOf(const MapRoom& room, const MapDoor& door) {
  const float halfWide = room.size.x * 0.5f;
  const float halfDeep = room.size.y * 0.5f;

  const float gaps[4] = {
      std::fabs(door.translation.z - halfDeep),
      std::fabs(door.translation.z + halfDeep),
      std::fabs(door.translation.x - halfWide),
      std::fabs(door.translation.x + halfWide),
  };

  int wall = 0;
  for (int i = 1; i < 4; i++)
    if (gaps[i] < gaps[wall]) wall = i;
  return wall;
}

}  // namespace

std::vector<RoomPlace> layoutRooms(const GameMap& map) {
  std::vector<RoomPlace> places(map.rooms.size());
  if (map.rooms.empty()) return places;

  const int rooms = static_cast<int>(map.rooms.size());
  const int start = (map.startRoom >= 0 && map.startRoom < rooms) ? map.startRoom : 0;

  places[start].placed = true;
  places[start].centre = glm::vec2(0.f);
  places[start].size = glm::vec2(map.rooms[start].size.x, map.rooms[start].size.y);

  // Breadth first, so a room is placed by the shortest way round to it
  std::vector<int> queue{start};
  for (std::size_t at = 0; at < queue.size(); at++) {
    const int here = queue[at];
    const MapRoom& room = map.rooms[here];

    for (const MapDoor& door : room.doors) {
      if (door.toRoom < 0 || door.toRoom >= rooms) continue;
      if (places[door.toRoom].placed) continue;

      const MapRoom& next = map.rooms[door.toRoom];

      // Where the same doorway comes out on the far side, so the two line up
      glm::vec3 back(0.f);
      if (door.toDoor >= 0 && door.toDoor < static_cast<int>(next.doors.size()))
        back = next.doors[static_cast<std::size_t>(door.toDoor)].translation;

      glm::vec2 centre = places[here].centre;
      const int wall = wallOf(room, door);
      if (wall == 0 || wall == 1) {
        centre.y += (wall == 0 ? 1.f : -1.f) * ((room.size.y + next.size.y) * 0.5f + kGap);
        centre.x += door.translation.x - back.x;
      } else {
        centre.x += (wall == 2 ? 1.f : -1.f) * ((room.size.x + next.size.x) * 0.5f + kGap);
        centre.y += door.translation.z - back.z;
      }

      places[door.toRoom].placed = true;
      places[door.toRoom].centre = centre;
      places[door.toRoom].size = glm::vec2(next.size.x, next.size.y);
      queue.push_back(door.toRoom);
    }
  }

  spreadApart(places);
  return places;
}

lve::LveCanvas drawHouseMap(const GameMap& map, int currentRoom, int width, int height) {
  lve::LveCanvas canvas(width, height);

  // Only the lines are drawn, the rest of the picture is a hole
  for (int y = 0; y < height; y++)
    for (int x = 0; x < width; x++) canvas.clear(x, y);

  std::vector<RoomPlace> places = layoutRooms(map);
  if (places.empty()) return canvas;

  // The room that is not in the map, stood against the back of the start room
  // It is drawn exactly like the others, which is the whole point of it
  const int start = (map.startRoom >= 0 && map.startRoom < static_cast<int>(map.rooms.size()))
                        ? map.startRoom
                        : 0;
  const int phantom = static_cast<int>(places.size());
  if (places[start].placed) {
    RoomPlace extra;
    extra.placed = true;
    extra.size = glm::vec2(places[start].size.x * 0.7f, kPhantomDepth);
    extra.centre = places[start].centre;
    extra.centre.y -= (places[start].size.y + kPhantomDepth) * 0.5f + kGap;
    places.push_back(extra);
  }

  glm::vec2 low(1e9f);
  glm::vec2 high(-1e9f);
  for (const RoomPlace& place : places) {
    if (!place.placed) continue;
    low = glm::min(low, place.centre - place.size * 0.5f);
    high = glm::max(high, place.centre + place.size * 0.5f);
  }
  if (low.x > high.x) return canvas;

  // One margin pixel all the way round, and square pixels either way
  const glm::vec2 span = glm::max(high - low, glm::vec2(1.f));
  const float scale = std::min((width - 3) / span.x, (height - 3) / span.y);

  // North is up, so a room's z grows toward the top of the picture
  auto toPixel = [&](glm::vec2 at) {
    const int x = 1 + static_cast<int>((at.x - low.x) * scale + 0.5f);
    const int y = height - 2 - static_cast<int>((at.y - low.y) * scale + 0.5f);
    return glm::ivec2(x, y);
  };

  for (std::size_t i = 0; i < places.size(); i++) {
    const RoomPlace& place = places[i];
    if (!place.placed) continue;

    const glm::ivec2 corner = toPixel(place.centre - place.size * 0.5f);
    const glm::ivec2 far = toPixel(place.centre + place.size * 0.5f);
    const int x = std::min(corner.x, far.x);
    const int y = std::min(corner.y, far.y);
    const int w = std::max(2, std::abs(far.x - corner.x) + 1);
    const int h = std::max(2, std::abs(far.y - corner.y) + 1);

    // The room he is standing in is the only one filled in
    if (static_cast<int>(i) == currentRoom && i != static_cast<std::size_t>(phantom))
      canvas.fillBox(x + 1, y + 1, w - 2, h - 2, kHere);

    canvas.box(x, y, w, h, kWall);
  }

  // A lit dot wherever a room has a way out, drawn over the walls
  for (std::size_t i = 0; i < map.rooms.size(); i++) {
    if (!places[i].placed) continue;
    for (const MapDoor& door : map.rooms[i].doors) {
      const glm::vec2 at = places[i].centre + glm::vec2(door.translation.x, door.translation.z);
      const glm::ivec2 dot = toPixel(at);
      canvas.set(dot.x, dot.y, kDoor);
    }
  }

  return canvas;
}

}  // namespace petscop
