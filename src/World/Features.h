#pragma once
#include <cstdint>

namespace cubiq {

class Chunk;
class TerrainGenerator;

// Žily rúd v kameni (uhlie, železo, zlato, aetherit).
void placeOres(Chunk& chunk, uint32_t seed);

// Stromy a kaktusy. Strom môže presahovať do susedných chunkov, preto každý chunk
// prejde aj okolie a z každého stromu zapíše len bloky, ktoré ležia v ňom.
// Rozhodnutie "je tu strom?" závisí len od svetových súradníc, takže susedné
// chunky postavia ten istý strom a na hranici sa presne spoja.
void placeTrees(Chunk& chunk, const TerrainGenerator& generator);

} // namespace cubiq
