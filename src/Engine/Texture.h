#pragma once
#include <cstdint>
#include <vector>

namespace cubiq {

// 2D textúra z RGBA pixelov. Pixel-art filtrovanie (GL_NEAREST) ako v Minecrafte.
class Texture {
public:
    Texture(int width, int height, const std::vector<uint8_t>& rgba);
    ~Texture();

    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;

    void bind(int slot = 0) const;

private:
    unsigned int m_id = 0;
};

} // namespace cubiq
