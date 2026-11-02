#pragma once
#include <glm/glm.hpp>
#include <cstdint>
#include <vector>

namespace cubiq {

class World;
class Frustum;

// Cave culling (ako v Minecrafte): od sekcie s kamerou sa prehľadávajú susedné sekcie 16×16×16,
// ale len cez strany, ktoré sú vo vnútri sekcie prepojené vzduchom, a nikdy späť ku kamere.
// Sekcie, ku ktorým sa takto nedá dostať (zakryté jaskyne, podzemie), sa nekreslia.
class VisibleSections {
public:
    void compute(const World& world, const glm::vec3& camera, int radius, const Frustum& frustum);

    // Bitová maska viditeľných sekcií chunku (bit = index sekcie).
    uint16_t mask(glm::ivec2 chunk) const;
    int visibleCount() const { return m_count; }

private:
    glm::ivec2 m_center{0};
    int m_radius = 0;
    int m_count = 0;
    std::vector<uint16_t> m_masks;
    std::vector<uint8_t> m_visited;

    struct Node {
        int x, y, z;     // relatívne ku kamere: x, z v chunkoch, y = sekcia
        int8_t entered;  // strana, ktorou sme vošli (-1 = štart)
        uint8_t dirs;    // smery, ktorými sme už išli (späť sa nechodí)
    };
    std::vector<Node> m_queue;
};

} // namespace cubiq
