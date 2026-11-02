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
class Mesh {
public:
    Mesh(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices);
    ~Mesh();

    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;

    void draw() const;
    int faceCount() const { return m_indexCount / 6; }

private:
    unsigned int m_vao = 0, m_vbo = 0, m_ebo = 0;
    int m_indexCount = 0;
};

} // namespace cubiq
