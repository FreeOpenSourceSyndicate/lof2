// Native reader for the Legend of Faerghail dungeon / scenario maps
// (DNGS/DNGxx).  Re-implements the engine's own parser: it reads the game
// *data* files and never runs the original executables.
//
// Format (reverse-engineered from the game data; every shipped map decodes to
// the same fixed size):
//
//   byte 0      RLE escape byte (0xff in all shipped maps)
//   bytes 1..   RLE stream: a literal byte is copied as-is; the escape byte
//               introduces a (count, value) pair emitting `value` `count` times
//
// The decoded payload is a 4-byte header followed by three 36x36 byte layers
// (4 + 3*1296 = 3892 bytes for every shipped map):
//
//   header[0]  dungeon level / plane id
//   header[1]  style / tileset selector
//   header[2]  music / ambient id
//   header[3]  spawn / start cell index
//
//   layer 0    wall / feature plane (cell -> tile id; 0 = open floor)
//   layer 1    object plane (doors, items, stairs, decorations)
//   layer 2    entity / event plane
//
// Layer 0 is the floor plan consumed by the first-person renderer; the exact
// semantics of layers 1/2 are still being pinned down.
#ifndef LOF_PORT_DUNGEON_H
#define LOF_PORT_DUNGEON_H

#include <array>
#include <cstdint>
#include <string>

namespace lofport {

constexpr int kDungeonDim = 36;
constexpr size_t kDungeonLayer = (size_t)kDungeonDim * kDungeonDim;
constexpr size_t kDungeonDecoded = 4 + 3 * kDungeonLayer;

using DungeonLayer = std::array<std::array<uint8_t, kDungeonDim>, kDungeonDim>;

struct DungeonMap {
    bool ok = false;
    uint8_t header[4] = {0, 0, 0, 0};
    DungeonLayer walls{};     // layer 0
    DungeonLayer objects{};   // layer 1
    DungeonLayer entities{};  // layer 2

    uint8_t level() const { return header[0]; }
    uint8_t style() const { return header[1]; }
    uint8_t music() const { return header[2]; }
    uint8_t spawn() const { return header[3]; }

    // Wall / feature tile at (x,y); 0 = open floor.  Out-of-range reads as 0.
    uint8_t at(int x, int y) const {
        if (x < 0 || y < 0 || x >= kDungeonDim || y >= kDungeonDim) return 0;
        return walls[y][x];
    }
};

// Decode a DNGS/DNGxx file.  ok is false on a missing or malformed file (the
// decoded payload must be exactly kDungeonDecoded bytes).
DungeonMap load_dungeon(const std::string& path);

}  // namespace lofport

#endif  // LOF_PORT_DUNGEON_H
