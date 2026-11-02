#include "Game/Pathfinder.h"
#include "World/World.h"

#include <algorithm>
#include <cmath>
#include <queue>
#include <unordered_map>

namespace cubiq {

namespace {
    int64_t key(glm::ivec3 p)
    {
        return ((int64_t)(p.x & 0x1FFFFF) << 42) | ((int64_t)(p.z & 0x1FFFFF) << 21) | (int64_t)(p.y & 0x1FFFFF);
    }

    bool passable(const World& w, glm::ivec3 p) { return !isSolid(w.getBlock(p.x, p.y, p.z)); }

    // Octile vzdialenosť (8 smerov) + výška
    float heuristic(glm::ivec3 a, glm::ivec3 b)
    {
        int dx = std::abs(a.x - b.x), dz = std::abs(a.z - b.z);
        return (float)(std::max(dx, dz)) + 0.414f * (float)std::min(dx, dz) + 0.5f * std::abs(a.y - b.y);
    }
}

bool Pathfinder::standable(const World& world, glm::ivec3 p, const Options& o)
{
    if (p.y < 1 || p.y + o.height >= Chunk::SIZE_Y) return false;
    BlockId below = world.getBlock(p.x, p.y - 1, p.z);
    if (!isSolid(below)) return false;
    for (int h = 0; h < o.height; ++h) {
        BlockId b = world.getBlock(p.x, p.y + h, p.z);
        if (isSolid(b)) return false;
        if (o.avoidWater && isLiquid(b)) return false;
    }
    return true;
}

std::vector<glm::ivec3> Pathfinder::find(const World& world, glm::ivec3 start, glm::ivec3 goal, const Options& o)
{
    struct Node {
        glm::ivec3 pos;
        float g;
        int64_t parent;
        bool closed;
    };
    struct Open {
        float f;
        int64_t k;
        bool operator<(const Open& other) const { return f > other.f; } // min-heap
    };

    std::unordered_map<int64_t, Node> nodes;
    std::priority_queue<Open> open;
    nodes.reserve(o.maxNodes * 2);

    const int64_t startKey = key(start);
    nodes[startKey] = {start, 0.0f, startKey, false};
    open.push({heuristic(start, goal), startKey});

    int64_t best = startKey;
    float bestH = heuristic(start, goal);
    int expanded = 0;

    const int DIRS[8][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}, {1, 1}, {1, -1}, {-1, 1}, {-1, -1}};

    while (!open.empty() && expanded < o.maxNodes) {
        Open cur = open.top();
        open.pop();
        Node& n = nodes[cur.k];
        if (n.closed) continue;
        n.closed = true;
        ++expanded;

        float h = heuristic(n.pos, goal);
        if (h < bestH) { bestH = h; best = cur.k; }
        if (n.pos == goal) { best = cur.k; break; }

        const glm::ivec3 p = n.pos;
        const float g = n.g;
        for (const auto& d : DIRS) {
            bool diagonal = d[0] && d[1];
            // Diagonála len ak sú voľné obe priľahlé políčka (žiadne sekanie rohov).
            if (diagonal) {
                bool ok = true;
                for (int hh = 0; hh < o.height && ok; ++hh)
                    ok = passable(world, {p.x + d[0], p.y + hh, p.z}) && passable(world, {p.x, p.y + hh, p.z + d[1]});
                if (!ok) continue;
            }

            // Kam sa dá stúpiť: o 1 hore (ak je nad hlavou voľno), rovno, alebo zoskok.
            glm::ivec3 target;
            bool found = false;
            float extra = 0.0f;
            glm::ivec3 up(p.x + d[0], p.y + 1, p.z + d[1]);
            if (!diagonal && passable(world, {p.x, p.y + o.height, p.z}) && standable(world, up, o)) {
                target = up;
                found = true;
                extra = 0.6f;
            } else {
                for (int dy = 0; dy >= -o.maxDrop; --dy) {
                    glm::ivec3 c(p.x + d[0], p.y + dy, p.z + d[1]);
                    if (dy < 0 && isSolid(world.getBlock(c.x, c.y + o.height, c.z))) break; // stena v ceste
                    if (standable(world, c, o)) { target = c; found = true; extra = -dy * 0.3f; break; }
                }
            }
            if (!found) continue;

            float ng = g + (diagonal ? 1.414f : 1.0f) + extra;
            int64_t tk = key(target);
            auto it = nodes.find(tk);
            if (it == nodes.end()) {
                nodes[tk] = {target, ng, cur.k, false};
                open.push({ng + heuristic(target, goal), tk});
            } else if (!it->second.closed && ng < it->second.g) {
                it->second.g = ng;
                it->second.parent = cur.k;
                open.push({ng + heuristic(target, goal), tk});
            }
        }
    }

    std::vector<glm::ivec3> path;
    for (int64_t k = best;; k = nodes[k].parent) {
        path.push_back(nodes[k].pos);
        if (k == startKey) break;
    }
    std::reverse(path.begin(), path.end());
    if (path.size() < 2) path.clear();
    return path;
}

} // namespace cubiq
