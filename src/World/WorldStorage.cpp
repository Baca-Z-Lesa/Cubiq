#include "World/WorldStorage.h"
#include "World/Chunk.h"

#include <cstring>
#include <fstream>

namespace cubiq {

namespace {
    constexpr char MAGIC[4] = {'C', 'B', 'Q', 'R'};
    constexpr uint32_t VERSION = 1;

    int floorDiv(int a, int b) { return (a >= 0) ? a / b : -((-a + b - 1) / b); }
    int64_t key(glm::ivec2 c) { return ((int64_t)c.x << 32) | (uint32_t)c.y; }

    void putU32(std::vector<uint8_t>& out, uint32_t v)
    {
        for (int i = 0; i < 4; ++i) out.push_back((uint8_t)(v >> (i * 8)));
    }

    // RLE: dvojice (blok, dĺžka úseku 1–65535) cez všetky bloky chunku v poradí Y, Z, X.
    std::vector<uint8_t> encode(const Chunk& chunk)
    {
        std::vector<uint8_t> out;
        BlockId runId = BlockId::Air;
        uint32_t runLen = 0;
        auto flushRun = [&] {
            out.push_back((uint8_t)runId);
            out.push_back((uint8_t)(runLen & 0xFF));
            out.push_back((uint8_t)(runLen >> 8));
        };
        for (int y = 0; y < Chunk::SIZE_Y; ++y)
            for (int z = 0; z < Chunk::SIZE_Z; ++z) {
                const BlockId* row = chunk.row(y, z);
                for (int x = 0; x < Chunk::SIZE_X; ++x) {
                    if (runLen > 0 && (row[x] != runId || runLen == 0xFFFF)) {
                        flushRun();
                        runLen = 0;
                    }
                    runId = row[x];
                    ++runLen;
                }
            }
        if (runLen) flushRun();
        return out;
    }

    bool decode(const std::vector<uint8_t>& data, Chunk& chunk)
    {
        const int total = Chunk::SIZE_X * Chunk::SIZE_Y * Chunk::SIZE_Z;
        int i = 0;
        for (size_t p = 0; p + 3 <= data.size(); p += 3) {
            BlockId id = (BlockId)data[p];
            int len = data[p + 1] | (data[p + 2] << 8);
            if ((int)id >= (int)BlockId::Count || i + len > total) return false;
            if (id != BlockId::Air) {
                for (int k = 0; k < len; ++k) {
                    int idx = i + k;
                    int x = idx % Chunk::SIZE_X, z = (idx / Chunk::SIZE_X) % Chunk::SIZE_Z, y = idx / (Chunk::SIZE_X * Chunk::SIZE_Z);
                    chunk.set(x, y, z, id);
                }
            }
            i += len;
        }
        return i == total;
    }
}

WorldStorage::WorldStorage(std::filesystem::path directory) : m_dir(std::move(directory))
{
    std::filesystem::create_directories(m_dir);
}

std::filesystem::path WorldStorage::regionPath(glm::ivec2 r) const
{
    return m_dir / ("r." + std::to_string(r.x) + "." + std::to_string(r.y) + ".cbq");
}

WorldStorage::Region& WorldStorage::region(glm::ivec2 rc)
{
    auto it = m_regions.find(key(rc));
    if (it != m_regions.end()) return it->second;

    Region& reg = m_regions[key(rc)];
    std::ifstream in(regionPath(rc), std::ios::binary);
    if (!in) return reg;

    // Hlavička: "CBQR", verzia, počet chunkov; potom (index, veľkosť, dáta) pre každý chunk.
    char magic[4];
    uint32_t version = 0, count = 0;
    in.read(magic, 4);
    in.read((char*)&version, 4);
    in.read((char*)&count, 4);
    if (!in || std::memcmp(magic, MAGIC, 4) != 0 || version != VERSION) return reg;
    for (uint32_t c = 0; c < count; ++c) {
        uint32_t index = 0, size = 0;
        in.read((char*)&index, 4);
        in.read((char*)&size, 4);
        if (!in || size > 4 * 1024 * 1024) break;
        std::vector<uint8_t> data(size);
        in.read((char*)data.data(), size);
        if (!in) break;
        reg.chunks[(int)index] = std::move(data);
    }
    return reg;
}

bool WorldStorage::load(glm::ivec2 coord, Chunk& chunk)
{
    std::lock_guard lock(m_mutex);
    glm::ivec2 rc(floorDiv(coord.x, REGION), floorDiv(coord.y, REGION));
    Region& reg = region(rc);
    int index = (coord.y - rc.y * REGION) * REGION + (coord.x - rc.x * REGION);
    auto it = reg.chunks.find(index);
    if (it == reg.chunks.end()) return false;
    return decode(it->second, chunk);
}

void WorldStorage::store(const Chunk& chunk)
{
    std::vector<uint8_t> data = encode(chunk);
    std::lock_guard lock(m_mutex);
    glm::ivec2 c = chunk.coord();
    glm::ivec2 rc(floorDiv(c.x, REGION), floorDiv(c.y, REGION));
    Region& reg = region(rc);
    reg.chunks[(c.y - rc.y * REGION) * REGION + (c.x - rc.x * REGION)] = std::move(data);
    reg.dirty = true;
}

void WorldStorage::flush()
{
    std::lock_guard lock(m_mutex);
    for (auto& [k, reg] : m_regions) {
        if (!reg.dirty) continue;
        glm::ivec2 rc((int)(k >> 32), (int)(int32_t)(k & 0xFFFFFFFF));

        std::vector<uint8_t> out(MAGIC, MAGIC + 4);
        putU32(out, VERSION);
        putU32(out, (uint32_t)reg.chunks.size());
        for (auto& [index, data] : reg.chunks) {
            putU32(out, (uint32_t)index);
            putU32(out, (uint32_t)data.size());
            out.insert(out.end(), data.begin(), data.end());
        }

        // Najprv do dočasného súboru, potom premenovať – pád uprostred zápisu nepoškodí starý región.
        std::filesystem::path path = regionPath(rc), tmp = path;
        tmp += ".tmp";
        {
            std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
            f.write((const char*)out.data(), (std::streamsize)out.size());
        }
        std::error_code ec;
        std::filesystem::rename(tmp, path, ec);
        reg.dirty = false;
    }
}

} // namespace cubiq
