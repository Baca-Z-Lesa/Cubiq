#pragma once
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <cstdint>
#include <vector>

namespace cubiq {

struct Vertex {
    glm::vec3 position;
    glm::vec2 uv;
    float shade;     // tieňovanie podľa strany kocky × ambient occlusion
    glm::vec3 tint;  // farba biómu pre tónované pixely (tráva, listy)
    glm::vec4 light; // jas: x = slnko, yzw = farebné svetlo zo zdrojov (R, G, B)
    float tile = 0;  // index dlaždice v atlase (uv je v blokoch a textúra sa opakuje)
};

// Vertexy + indexy na GPU (VAO/VBO/EBO).
// Indexy môžu byť rozdelené do skupín (napr. sekcia × smer) a kresliť sa len vybrané.
class Mesh {
public:
    Mesh(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices,
         const std::vector<uint32_t>& groupCounts = {});
    ~Mesh();

    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;

    void draw() const;
    // Nakreslí skupiny, pre ktoré enabled(index skupiny) vráti true – jedným volaním (glMultiDrawElements).
    template <typename F>
    int drawGroups(F&& enabled) const
    {
        m_counts.clear();
        m_offsets.clear();
        for (size_t g = 0; g < m_groupCounts.size(); ++g)
            if (m_groupCounts[g] && enabled((int)g)) {
                m_counts.push_back((int)m_groupCounts[g]);
                m_offsets.push_back((const void*)(uintptr_t)(m_groupStarts[g] * sizeof(uint32_t)));
            }
        drawMulti();
        int faces = 0;
        for (int c : m_counts) faces += c / 6;
        return faces;
    }
    int faceCount() const { return m_indexCount / 6; }

private:
    void drawMulti() const;

    unsigned int m_vao = 0, m_vbo = 0, m_ebo = 0;
    int m_indexCount = 0;
    std::vector<uint32_t> m_groupCounts, m_groupStarts;
    mutable std::vector<int> m_counts;
    mutable std::vector<const void*> m_offsets;
};

} // namespace cubiq
