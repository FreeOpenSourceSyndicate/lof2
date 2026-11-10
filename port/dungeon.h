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
    // order as `tiles`.  Read as 1296 words, both bytes are meaningful *per
    // cell*: the low byte is 0 for 99.2% of cells (otherwise 1..4) while the
    // high byte is 0 for 91% of cells (otherwise 1..245, 206 distinct values).
    // Both byte layers are spatially coherent on their own (adj/nz 0.69 / 0.58),
    // unlike the non-interleaved half-planes (0.04 / 0.03), which is why the
    // records are read as words.  Stored little-endian; use `obj_low`/`obj_code`
    // for the two bytes.
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

// The 29 levels are named DNGS/DNG<dungeon><level>: the first digit is the
// dungeon number (0..6), the second the level within it (0..7), matching the
// game's own format string `DNGS\DNG%01ld%01ld` (LOF.EXE offset 0x4bb2c).
// E.g. "DNG17" is dungeon 1, level 7.  `dungeon` is a single digit, so
// dungeon * 10 + level recovers the file's two-digit code.
std::string dungeon_file_name(int dungeon, int level);

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

// --- Walk model -----------------------------------------------------------

// Grid position + facing for the party.  Facing: 0=N (-y), 1=E (+x),
// 2=S (+y), 3=W (-x).
struct DungeonPos {
    int x = 0, y = 0, facing = 0;
};

int dungeon_facing_dx(int facing);   // 0,1,0,-1 for N,E,S,W
int dungeon_facing_dy(int facing);   // -1,0,1,0 for N,E,S,W

// Turn the facing left / right (no collision).  Wraps 0..3.
void dungeon_turn(DungeonPos& p, bool right);

// True if the cell directly ahead of `p` can be entered.
bool dungeon_can_step(const Dungeon& d, const DungeonPos& p);

// Step forward if the cell ahead is passable.  Returns true on a move.  The
// cell *behind* is not checked: a move always leaves the current cell.
bool dungeon_step(const Dungeon& d, DungeonPos& p);

// Place `p` at the nearest walkable cell to (x,y) (spiral search), or leave it
// unchanged if the whole level is solid.  Returns true if a spot was found.
bool dungeon_place(const Dungeon& d, DungeonPos& p, int x, int y);

// Draw a top-down map of the level onto `scr`, `cell` pixels per tile,
// centred, with the player drawn at (px,py).  Tile codes are colour-coded and
// a cell with an object code is marked.  Intended for headless verification
// (--shot) and as the basis for the in-game map/automap.
void render_dungeon_map(Screen& scr, const Dungeon& d, int px, int py,
                        int cell = 4);

// Same, but draws the party with a short arrow showing its facing.
void render_dungeon_map(Screen& scr, const Dungeon& d, const DungeonPos& p,
                        int cell = 4);

// --- First-person viewport ------------------------------------------------

// The engine draws the 3D window from **pre-rendered sprites**
// `EGA/D<dungeon>/DPnnn`, blitted at (10,11) over the `EGA/UP/MSCR` frame — it
// does not ray-cast.  Verified against the DOSBox oracle: `EGA/D5/DP001`
// reproduces the slot-1 Temple spawn window at 95.2% (the residue is a thin
// horizon band the engine overlays from map data).  The full 120x119 views are
// DP001/DP014/DP036/DP058/DP059/DP060/DP067; the smaller `DPnnn` (64x59, 32x59,
// 16x29, ...) are the side-wall pieces composited near map edges.  The index the
// engine selects is not yet decoded (see docs/notes-a.md), so the caller names
// the sprite.
constexpr int kViewportX = 10;
constexpr int kViewportY = 11;
constexpr int kViewportW = 120;
constexpr int kViewportH = 119;

// Blit the dungeon view sprite at `sprite_path` into `scr` at (vx,vy).  Returns
// false (leaving `scr` untouched) if the image cannot be loaded.  This is the
// engine's real model: a full-colour sprite blit, not a projection.
bool render_dungeon_view(Screen& scr, const std::string& sprite_path,
                         int vx = kViewportX, int vy = kViewportY);

// Path of a view sprite inside the game directory: `<dir>/EGA/D<d>/DP%03d`.
// `dungeon` is the map id (the same two digits as `DNGS/DNG<dd>`).  The view
// directory is the map id's *ones digit*: the engine composes the map name as
// `DNGS\DNG%01ld%01ld` and reads the tiles from `EGA\D<ones-digit>`, so dungeon
// 05 (the Temple) -> `EGA/D5` and dungeon 10 (the wilderness) -> `EGA/D0`.
// Verified by the maintainer by patching the save's location bytes and watching
// the emulator's open trace (GAME2 0x9a2 0->1 opens `DNGS\DNG11` + `ega\d1`).
// `index` is the 0-based entry into the engine's sprite table (LOF.EXE ships a
// contiguous `ega\d0\dp000`..`ega\d0\dp069` list; the ones digit is substituted
// at run time).  Index 1 (`DP001`) is the canonical entry view, pixel-verified
// against the Temple spawn oracle.
std::string dungeon_view_path(const std::string& dir, int dungeon, int index = 1);

// The view directory name for a map id: `D<ones-digit>` (see dungeon_view_path).
std::string dungeon_view_dir(int dungeon);

// Convenience: blit the dungeon's canonical view sprite (DP001) at (10,11).
// This is the base first-person view the engine shows on entry; it is verified
// for dungeon 5 (the slot-1 Temple oracle) and structurally valid for all
// levels.  The engine then overlays map tiles over this base (see notes-a.md).
bool render_dungeon_default_view(Screen& scr, const std::string& dir,
                                 int dungeon);

// Synthetic *fallback* for a level with no pre-rendered view sprite (and useful
// as an automap-style aid): ray-cast the 36x36 tile grid along the party facing
// with a 60-degree FOV and draw a grey (7) wall band whose height is the
// projected wall height over a black (0) ceiling/floor.  A solid wall one cell
// ahead clamps to the full window.  This approximates a first-person view but
// is NOT what the engine does (the engine blits DPnnn sprites).
void render_dungeon_viewport(Screen& scr, const Dungeon& d, const DungeonPos& p,
                             int vx = kViewportX, int vy = kViewportY,
                             int vw = kViewportW, int vh = kViewportH);

}  // namespace lofport

#endif
