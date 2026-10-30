#pragma once
#include <glm/glm.hpp>

namespace cubiq {

// Zorný ihlan kamery (6 rovín). Slúži na preskočenie chunkov, ktoré nie sú vidno.
class Frustum {
public:
    explicit Frustum(const glm::mat4& viewProjection)
    {
        // Gribb & Hartmann: roviny priamo z riadkov matice view × projection.
        const glm::mat4& m = viewProjection;
        for (int i = 0; i < 3; ++i) {
            glm::vec4 rowW(m[0][3], m[1][3], m[2][3], m[3][3]);
            glm::vec4 row(m[0][i], m[1][i], m[2][i], m[3][i]);
            m_planes[i * 2 + 0] = rowW + row;
            m_planes[i * 2 + 1] = rowW - row;
        }
        for (glm::vec4& p : m_planes) p /= glm::length(glm::vec3(p));
    }

    // Je aspoň časť kvádra vo vnútri?
    bool intersects(const glm::vec3& min, const glm::vec3& max) const
    {
        for (const glm::vec4& p : m_planes) {
            // Roh kvádra, ktorý je najďalej v smere normály roviny.
            glm::vec3 v(p.x > 0 ? max.x : min.x, p.y > 0 ? max.y : min.y, p.z > 0 ? max.z : min.z);
            if (glm::dot(glm::vec3(p), v) + p.w < 0.0f) return false;
        }
        return true;
    }

private:
    glm::vec4 m_planes[6];
};

} // namespace cubiq
