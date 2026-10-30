#pragma once
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <cstdint>
#include <vector>

namespace cubiq {

struct Vertex {
    glm::vec3 position;
    glm::vec2 uv;
    float shade; // jednoduché tieňovanie podľa strany kocky
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
