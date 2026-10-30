#pragma once
#include <glm/vec2.hpp>
#include <vector>

namespace cubiq {

class Chunk;
class TerrainGenerator;

// Polomer oblasti dediny (v nej nerastú stromy).
constexpr int VILLAGE_RADIUS = 46;

// Dediny, ruiny a strážne veže.
//
// Svet je rozdelený na veľké regióny; v každom je stavba s istou pravdepodobnosťou.
// Jej poloha aj plán závisia len od seedu a súradníc regiónu, takže každý chunk
// si plán vypočíta sám a zapíše z neho len bloky, ktoré ležia v ňom.
void placeStructures(Chunk& chunk, const TerrainGenerator& generator);

// Stredy dedín, ktorých oblasť zasahuje do obdĺžnika (min..max vo svetových súradniciach).
std::vector<glm::ivec2> villageCentersNear(int minX, int minZ, int maxX, int maxZ, const TerrainGenerator& generator);

} // namespace cubiq
