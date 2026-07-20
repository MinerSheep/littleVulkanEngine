#include "pathfinding.hpp"

#include <algorithm> // std::reverse
#include <cstddef>   // std::size_t
#include <queue>

namespace pathfinding
{
namespace
{
// Flatten a cell to a single index so the visited/parent bookkeeping is a plain
// contiguous array (cache-friendly, O(1) lookup) instead of a hashed map.
inline int flatIndex(int x, int y, int n) { return y * n + x; }
} // namespace

Path findPath(int gridSize, glm::ivec2 start, glm::ivec2 goal,
              const std::function<bool(int, int)>& blocked)
{
    Path path;
    if (gridSize <= 0)
        return path;

    auto inBounds = [&](int x, int y) {
        return x >= 0 && y >= 0 && x < gridSize && y < gridSize;
    };

    // Nothing to do (or nothing possible) for degenerate / blocked endpoints.
    if (!inBounds(start.x, start.y) || !inBounds(goal.x, goal.y))
        return path;
    if (blocked(start.x, start.y) || blocked(goal.x, goal.y))
        return path;
    if (start == goal)
        return path;

    // parent[cell] holds the flat index of the cell we first reached it from;
    // -1 means unvisited. The start cell is its own parent, which both marks it
    // visited and terminates the walk-back below.
    const std::size_t cellCount = static_cast<std::size_t>(gridSize) * gridSize;
    std::vector<int>  parent(cellCount, -1);
    const int         startFlat = flatIndex(start.x, start.y, gridSize);
    parent[startFlat] = startFlat;

    std::queue<glm::ivec2> frontier;
    frontier.push(start);

    // Neighbour offsets: the four orthogonal steps first, then the four
    // diagonals.
    static const int dx[8] = {1, -1, 0, 0, 1, 1, -1, -1};
    static const int dy[8] = {0, 0, 1, -1, 1, -1, 1, -1};

    bool found = false;
    while (!frontier.empty() && !found)
    {
        const glm::ivec2 c = frontier.front();
        frontier.pop();

        for (int k = 0; k < 8; ++k)
        {
            const int nx = c.x + dx[k];
            const int ny = c.y + dy[k];
            if (!inBounds(nx, ny) || blocked(nx, ny))
                continue;
            if (parent[flatIndex(nx, ny, gridSize)] != -1)
                continue; // already reached by a shorter/earlier path

            // Corner-cutting guard: a diagonal step is only legal if both of the
            // orthogonal cells bracketing it are open, so the route never clips
            // the corner where two land cells touch.
            if (dx[k] != 0 && dy[k] != 0 &&
                (blocked(c.x + dx[k], c.y) || blocked(c.x, c.y + dy[k])))
                continue;

            parent[flatIndex(nx, ny, gridSize)] = flatIndex(c.x, c.y, gridSize);
            if (nx == goal.x && ny == goal.y)
            {
                found = true;
                break;
            }
            frontier.push(glm::ivec2(nx, ny));
        }
    }

    if (!found)
        return path;

    // Reconstruct by following parents from goal back to start, then reverse so
    // the route reads start -> goal. The start cell itself is left off.
    for (glm::ivec2 c = goal;;)
    {
        const int i = flatIndex(c.x, c.y, gridSize);
        if (i == startFlat)
            break;
        path.push_back(c);
        const int p = parent[i];
        c = glm::ivec2(p % gridSize, p / gridSize);
    }
    std::reverse(path.begin(), path.end());
    return path;
}

} // namespace pathfinding
