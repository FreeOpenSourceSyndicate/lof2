// Dungeon map decoder for Legend of Faerghail (DNGS/DNGnn).
//
// Native re-implementation: reads the game's *data* files only, never the
// original executables.  See tools/dngs.py for the offline explorer and
// docs/notes-a.md for the derivation of the format.
#ifndef LOF_PORT_DUNGEON_H
#define LOF_PORT_DUNGEON_H

#include "gfx.h"

#include <cstdint>
#include <string>
#include <vector>

namespace lofport {

// Fixed geometry of a dungeon level.  Every DNGS file decodes to exactly
// 4 header bytes + 1296 tile bytes + 2592 object bytes = 3892 bytes.
constexpr int kDungeonW = 36;
constexpr int kDungeonH = 36;
constexpr int kDungeonHeader = 4;
constexpr int kDungeonTiles = kDungeonW * kDungeonH;          // 1296
constexpr int kDungeonObjects = kDungeonW * kDungeonH * 2;    // 2592
constexpr int kDungeonDecoded = kDungeonHeader + kDungeonTiles + kDungeonObjects;

// One dungeon level.
struct Dungeon {
    // Header.  The meaning of h0/h1 is not yet pinned down (h0 is 0 for all but
    // DNG13; h1 varies).  `field` is the little-endian h2|h3 word; it is not a
    // decompressed size (that is always 3892) and is kept verbatim for now.
    uint8_t h0 = 0, h1 = 0;
    uint16_t field = 0;

    // 36x36 tile codes, row-major.  Values seen: 0,3,6,7,8,12; 6 dominates and
    // is the "empty/floor" tile for most levels.
    std::vector<uint8_t> tiles;

    // 36x36 per-cell object records, two bytes each, in the same row-major
    // order as `tiles`.  Across all 29 files the first byte is 0 for 99.2% of
    // cells (and otherwise 1..4) while the second byte is 0 for 91% of cells
    // (and otherwise 1..~20).  So the two bytes read as two sparse per-cell
    // codes: a low "sub-type/index" and a high "feature/kind".  Stored as a
    // little-endian word; use `obj_low`/`obj_code` for the two parts.
    std::vector<uint16_t> objects;

    inline uint8_t obj_low(int x, int y) const {
        return (uint8_t)(object(x, y) & 0xFF);
    }
    inline uint8_t obj_code(int x, int y) const {
        return (uint8_t)(object(x, y) >> 8);
    }

    inline uint8_t tile(int x, int y) const {
        return (x >= 0 && y >= 0 && x < kDungeonW && y < kDungeonH)
                   ? tiles[(size_t)y * kDungeonW + x] : 0;
    }
    inline uint16_t object(int x, int y) const {
        return (x >= 0 && y >= 0 && x < kDungeonW && y < kDungeonH)
                   ? objects[(size_t)y * kDungeonW + x] : 0;
    }
};

// Run-length decode a DNGS file's bytes into a Dungeon.  `raw` is the whole
// file contents.  Returns false (with a message in *err) on a malformed file.
bool decode_dungeon(const std::vector<uint8_t>& raw, Dungeon& out,
                    std::string* err = nullptr);

// Load and decode a DNGS/DNGnn file from disk.
bool load_dungeon(const std::string& path, Dungeon& out,
                  std::string* err = nullptr);

// The raw RLE decoder shared with the EGA image loader's scheme: the first
// byte is the escape marker, then literal bytes copy through and the escape
// byte introduces a (count, value) pair emitting `value` `count` times.
std::vector<uint8_t> dngs_rle_decode(const std::vector<uint8_t>& raw);

// One character per tile, for debugging / headless inspection.  `d` tiles are
// mapped to a printable symbol (see tools/dngs.py for the same table); the
// object code, when non-zero, is shown instead.  Returns kDungeonH lines.
std::vector<std::string> dungeon_ascii(const Dungeon& d, bool show_objects = false);

// --- Walkable map ---------------------------------------------------------

// The full tile-code set seen across the 29 files is
//   0,1,2,3,4,5,6,7,8,9,10,11,12,13,20,21,36,37,52,53,68,69,84,85,101,244
// with 0/6 dominating (0 = "nothing"/void, 6 = floor) and 3/12 the structural
// walls.  The four stride-16 groups (20/21, 36/37, 52/53, 68/69, 84/85) are
// almost certainly wall/orientation variants, but their solid/open status is
// PROVISIONAL until a real dungeon frame is captured from the oracle.
//
// A cell is treated as solid iff its tile code is in kSolidTiles.  Everything
// else (including the void code 0) is walkable; out-of-range cells are solid.
bool dungeon_is_solid_tile(uint8_t code);
bool dungeon_passable(const Dungeon& d, int x, int y);

// Flood-fill reachability on the walkable graph (4-neighbour).  Returns true
// if (x1,y1) can be reached from (x0,y0) without stepping on a solid cell.
// Both endpoints must themselves be walkable.  Used by the play loop and to
// sanity-check that a level is connected.
bool dungeon_reachable(const Dungeon& d, int x0, int y0, int x1, int y1);

// Draw a top-down map of the level onto `scr`, `cell` pixels per tile,
// centred, with the player drawn at (px,py).  Tile codes are colour-coded and
// a cell with an object code is marked.  Intended for headless verification
// (--shot) and as the basis for the in-game map/automap.
void render_dungeon_map(Screen& scr, const Dungeon& d, int px, int py,
                        int cell = 4);

}  // namespace lofport

#endif
