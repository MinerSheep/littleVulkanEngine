#pragma once

#include <glm/glm.hpp> // glm::ivec2

#include <functional>
#include <vector>

// Minimal grid pathfinding for the ServerNav sim.
//
// This is deliberately the simplest thing that works: a breadth-first search
// over the square cell grid that treats "blocked" (land) cells as impassable
// and returns a route of cells around them. It knows nothing about ServerNav,
// weather, or vessels -- the caller supplies the grid size and a blocked()
// predicate -- so it can be reused and unit-tested on its own.
namespace pathfinding
{

// An ordered list of grid cells to pass through. It starts with the first cell
// AFTER `start` and ends at `goal` (the start cell is implicit and omitted).
using Path = std::vector<glm::ivec2>;

// Breadth-first search on a `gridSize` x `gridSize` grid from `start` to `goal`.
// `blocked(x, y)` returns true for impassable (land) cells.
//
// Movement is 8-connected, but a diagonal step is only taken when both of the
// orthogonal cells it slips between are open, so a route can never squeeze
// through the corner gap between two land cells that merely touch at a corner.
//
// Returns an empty Path when: start == goal, either endpoint is out of range or
// itself blocked, or goal is unreachable (walled off by blocked cells). An
// empty result therefore means "no detour needed / possible" -- the caller
// should fall back to steering straight at the destination.
Path findPath(int gridSize, glm::ivec2 start, glm::ivec2 goal,
              const std::function<bool(int x, int y)>& blocked);

} // namespace pathfinding
