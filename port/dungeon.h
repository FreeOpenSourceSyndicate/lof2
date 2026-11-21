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
    // Header bytes: `[h0, h1, h2, h3]` = `[flag, entry_facing, entry_pos_hi,
    // entry_pos_lo]`.  `h0` (flag) is 0 for every level but DNG13 (4).  `h1` is
    // the party's **entry facing**.  `field` (h2|h3, BIG-endian) is the party's
    // **entry position**: a row-major cell index y*36+x into `tiles` (so
    // `entry_x()`/`entry_y()` decode it).  Verified against the saves: DNG05
    // entry=125/facing=2 equals GAME1 (0x9a0=125, 0x9a6=2); DNG10 entry=745/
    // facing=0 equals GAME2 (0x9a0=745, 0x9a6=0).  See docs/notes-a.md Round 22.
    uint8_t h0 = 0;       // flag
    uint8_t h1 = 0;       // entry facing
    uint16_t field = 0;   // entry position, big-endian h2|h3

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

    // Destination cell of a stair / portal on this cell, or -1 if the cell has
    // none.  The two object bytes are a BIG-endian row-major cell index (the
    // first byte is always 0..4 because the index is < 1296, the second 0..245),
    // which is exactly how LOF.EXE stores it.  See dungeon_stair_target().
    inline int dest_cell(int x, int y) const {
        uint16_t v = object(x, y);
        int c = (int)((v & 0xFF) << 8) | (int)(v >> 8);
        return c == 0 ? -1 : c;
    }

    // Party entry cell from the header (see above).
    inline int entry_x() const { return (int)(field % kDungeonW); }
    inline int entry_y() const { return (int)(field / kDungeonW); }
    inline int entry_facing() const { return h1; }

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
// with 0 (void/open) and 6 (wall) dominating.  The structural walls are tiles
// 6 and 13; every other code is walkable.  Ground truth: the emulator opacity
// sweep (Round 27, docs/notes-a.md) — patching a tile X directly ahead while
// varying the cell behind it only leaves the view unchanged for X in {6,13};
// the rendered sprite for those is the full-screen wall DP036.  The earlier
// {3,12} guess is disproved (DNG06 spawns on tile 3).
//
// A cell is treated as solid iff its tile code is a wall (6 or 13).  Everything
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

// Place the party at the level's entry cell and facing, taken from the DNG
// header (see Dungeon).  This is where the game starts the party: for DNG05 the
// header gives (17,3) facing 2, which is exactly GAME1's saved position/facing.
bool dungeon_place_entry(const Dungeon& d, DungeonPos& p);

// Place the party at a row-major cell index (`y*36 + x`) and facing, which is
// how the save stores the party position (`kSavePartyPos` / `kSaveFacing`).
// Falls back to the nearest passable cell exactly like `dungeon_place()`, so a
// save written on a cell that has since become solid still loads.  Returns
// false only if no passable cell exists at all.
bool dungeon_place_cell(const Dungeon& d, DungeonPos& p, int cell, int facing);

// --- Stairs, holes and portals -------------------------------------------
//
// A stair is a *tile code*, not an object: the low nibble is the direction
// (4 = up, 5 = down) and the high nibble is the **dungeon number to travel to**,
// so 0x15 is "down to dungeon 1".  The destination *cell* is the cell's object
// record, a big-endian row-major index (see Dungeon::dest_cell).  A third
// variant sets the high nibble to 0xF (0xf4 seen in DNG03/DNG43): a portal whose
// target comes from the table at LOF.EXE ds:0xa546, indexed by the current
// dungeon number, rather than from the object record.
//
// Ground truth, from the handler at LOF.EXE image 0x2e7fa..0x2e9c6 which reads
// the tile under the party (`[bx+0x46d8]`), switches on the low nibble, and for
// the 0xF case does `si = [0x37fe] << 2` then loads ds:[si-0x5aba] (dungeon)
// and ds:[si-0x5ab8] (cell):
//
//  * Every one of the 57 stair cells across the 29 shipped levels has a
//    non-zero destination record (57/57) -- no other cell kind does reliably.
//  * 55/55 of those destinations land in an existing level, and every one lands
//    on a cell *adjacent* to the matching opposite stair (an up-stair arrives
//    next to a down-stair and vice versa) -- the reciprocity a hand-authored
//    staircase has to satisfy.
//  * Live check: injecting tile 0x15 + record 644 into DNG05 in front of the
//    party made the original open DNGS/DNG15 immediately on stepping forward.
constexpr uint8_t kTileStairMask = 0x0F;
constexpr uint8_t kTileStairUp = 0x04;
constexpr uint8_t kTileStairDown = 0x05;
constexpr uint8_t kTilePortal = 0x0F;

// True if `code` is an up stair (low nibble 4) or a down stair (low nibble 5).
// The high nibble says where it goes: the destination dungeon number, or 0xF
// for a portal (whose target comes from the ds:0xa546 table instead).
bool dungeon_is_stair(uint8_t code);
// True if `code` is specifically an up (4) or a down (5) stair.
bool dungeon_is_up_stair(uint8_t code);
bool dungeon_is_down_stair(uint8_t code);
// True for a stair whose high nibble is 0xF (a portal).
bool dungeon_is_portal(uint8_t code);

// Dungeon number a stair travels to: the tile's high nibble for 4/5, or the
// portal table entry for 0xF.  `dungeon` is the current dungeon number (0..6),
// needed only for the portal case.
int dungeon_stair_dungeon(uint8_t code, int dungeon);

// Where a stair at (x,y) leads.  Returns false when the cell is not a stair.
// `dungeon` is the current dungeon number (see above).
struct DungeonLink {
    int dungeon = 0;   // destination dungeon number (0..6)
    int cell = -1;     // destination row-major cell index
};
bool dungeon_stair_target(const Dungeon& d, int x, int y, int dungeon,
                          DungeonLink* out);

// The message the engine prints when a stair is used (LOF.EXE ds:0xa40c..0xa473).
// The stairs/hole choice is made at run time from game state that is not yet
// mapped, so the caller supplies `hole`; `portal` reports the "You enter " form
// used for the 0xF tiles.
const char* dungeon_stair_message(uint8_t code, bool hole, bool* portal = nullptr);

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

// The engine draws the 3D window from **pre-rendered sprites**, blitted over the
// `EGA/UP/MSCR` frame — it does not ray-cast.  Two banks are used:
//   * `EGA/D<dungeon>/DPnnn` — the door / wall art.  Verified against the DOSBox
//     oracle: `EGA/D5/DP001` reproduces the slot-1 Temple spawn window at 95.2%.
//     The full 120x119 views are DP001/DP014/DP036/DP058/DP059/DP060/DP067; the
//     smaller `DPnnn` (64x59, 32x59, 16x29, ...) are side-wall pieces.
//   * `EGA/CHAR<B>/CHnn` — the **position-selected encounter/area sprite**
//     (Rounds 38-45).  The engine picks an `SYST/NSC<dd>` record from the
//     party's map cell and level, stores that record's id in `es:[0x259]`, and
//     the room routine blits `NSC<dd>[id].+0x48` -> `ega\char<B>\ch<NN>`.  So
//     the window shows the NPC/area sprite for that spot, NOT a procedural
//     composite: zeroing `CHAR2/CH25` turns the window black (ink 4941 -> 0).
//     The earlier "the engine ray-composes the corridor" reading was wrong — it
//     only ever searched the `DPnnn`/`ANIM` banks, not `CHAR`.
// The room routine at image 0x431E (`ip 0x2A4E`) takes a CH number, formats
// `ega\char<B>\chNN` into its buffer and calls the sprite loader; the CH number
// is `NSC<dd>[id].+0x48` for the id chosen from the party's position (see
// `dungeon_room_art_path` and docs/notes-a.md Rounds 38-39).
constexpr int kViewportX = 10;
constexpr int kViewportY = 11;
constexpr int kViewportW = 120;
constexpr int kViewportH = 119;

// Scenario NPC/monster table (`SYST/NSC<dd>`) record geometry.  On disk each
// record is `kRoomArtDiskStride` bytes after a one-byte record count; the engine
// expands them to `kRoomArtRecordStride` bytes in memory, and the room-art
// number is the byte at `kRoomArtField` (0x48) of the selected record.
constexpr int kRoomArtDiskStride = 78;
constexpr int kRoomArtRecordStride = 0x11A;
constexpr int kRoomArtField = 0x48;

// The room routine branches on the room-art number: a number below this selects
// the CHAR3 bank, otherwise the bank comes from the runtime table (CHAR2 for the
// DNG05 spawn).  The threshold is the routine's `cmp ax, 0x16` literal.
constexpr int kRoomArtSmallThreshold = 0x16;

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

// Path of a `CHAR<B>/CHnn` sprite.  This is the *position-selected encounter/
// area sprite* the engine blits into the first-person window: the routine at
// image 0x27780 picks an `SYST/NSC<dd>` record from the party's map cell and
// level, stores that record's id in `es:[0x259]`, and the room routine (image
// 0x431E, `ip 0x2A4E`) resolves `NSC<dd>[id].+0x48` -> `ega\char<B>\ch<NN>`.
// The numbers are per-record creature sprites (NSC05: Lizard warrior -> 25,
// Lizard priest -> 26, Priest -> 5), so the Temple spawn's view is CH25.  The
// function name keeps `room_art` for history; see docs/notes-a.md Rounds 38-45.
std::string dungeon_room_art_path(const std::string& dir, int ch);

// Read the encounter/area sprite number from the scenario's NPC/monster table
// `SYST/NSC<dd>` (Round 41, Task 2b).  The engine's view code reads a byte at
// offset +0x48 of the record selected by the party's current location id, and
// that byte is the `NN` the room routine formats into `ega\char<B>\ch<NN>`.
// Records are `kRoomArtRecordStride` (0x11A = 282) bytes in memory; on disk the
// same records are stored packed at `kRoomArtDiskStride` (78) bytes after a
// one-byte count, so this helper reads the disk layout directly.
//
// `scenario` is the level number (the same two digits as `DNGS/DNG<dd>` and
// `SYST/NSC<dd>`; NSC05 pairs with DNG05).  `location` is the party's location
// id (`es:[0x259]` in the engine; for the Temple spawn this is 16).  Returns the
// room-art number (0..255), or -1 if the file or record is unavailable.
int dungeon_room_art_number(const std::string& dir, int scenario, int location);

// The first-person sprite for the party's current position on a scenario.
// The engine resolves this as `NSC<dd>[location].+0x48` -> `ega\char<B>\ch<NN>`
// (Rounds 38-45): `scenario` is the map id digits (`NSC05` pairs with `DNG05`)
// and `location` is the party's encounter/location id (`es:[0x259]` in the
// engine; 16 at the Temple spawn).  Returns the sprite path, or an empty string
// if the scenario file or record is unavailable.  Deriving `location` from a
// save without the emulator is the open lane-A question (docs/notes-a.md
// Round 45); callers that already have the id use this directly.
std::string dungeon_encounter_sprite(const std::string& dir, int scenario,
                                     int location);

// The view directory name for a map id: `D<ones-digit>` (see dungeon_view_path).
std::string dungeon_view_dir(int dungeon);

// The cell directly ahead of the party (one step in the facing direction).
// Returns false (leaving *ax,*ay untouched) if that cell is off-map.
bool dungeon_ahead_cell(const Dungeon& d, const DungeonPos& p, int& ax, int& ay);

// The DPnnn index of the first-person *base* view sprite the engine blits for
// the party at `p`.  DECODED for the D5 (Temple) tileset — see notes-a.md
// Rounds 26 and 28.
//
// Ground truth: patch DNG05's RLE tile layer (`tools/dngs.py patch`), run the
// analysis emulator and dump the EGA planes (`LOF_EDUMP=`), then compare the
// 120x119 view window to every `EGA/D5/DP*` sprite by pixel IoU.  DNG05's party
// spawns at (17,3) facing 2 (S), so the cell directly ahead is (17,4).
//
// Sweeping the ahead cell (the cell behind it held at 0) gives:
//
//   ahead code | view                                   | IoU
//   -----------+----------------------------------------+------
//   0x4        | DP058  side passage                    | 1.000
//   0x5        | DP059  side passage variant            | 1.000
//   0x6, 0xd   | DP060  (= DP036, the wall face)        | 1.000
//   0xa        | DP014  doorway                         | 0.997
//   0x1,0x3,   | DP001  textured wall/room              | 0.91
//   0xb,0xc    |                                        |
//   0x0,0x2,   | **no single sprite** (a "corridor" view | 0.35
//   0x7,0x8    |  the engine ray-composes)              |
//   0x9        | **no single sprite** (corridor variant) | 0.32
//
// Codes 0x6/0xd are the walls (see dungeon_is_solid_tile): looking *at* a wall
// shows the flat wall face.  Codes 0x0/0x2/0x7/0x8 are open cells; looking down
// an open corridor shows a `EGA/CHAR2/CHnn` room sprite (Round 38), not a
// composed view.  The drop-in still falls back to DP001 for those codes because
// the room-index -> CHnn table is not yet decoded, but that is a *data* gap, not
// a missing algorithm (BLK-A003 / G-004).
//
// DEPTH MODEL (Round 29, corrected Round 38).  The view is a function of the
// distance to the first cell the *view* treats as opaque, and that set is NOT
// the movement wall set: tile 0x3 is passable for movement but renders opaque in
// the view.  With the party at DNG05 (17,3) facing S and an open cell at (17,4),
// patching the wall distance gives:
//
//   first opaque cell at depth 1  ->  EGA/D5/DP001   (full-screen wall texture)
//   first opaque cell at depth 2  ->  EGA/CHAR2/CH40 (zeroing it blacks the
//                                      depth-2 window, 5111 -> 0)
//   first opaque cell at depth >=3 ->  EGA/CHAR2/CH25 (zeroing it blacks the
//                                      depth-3 window, 4941 -> 0)
//
// The depth-2 and depth->=3 images are `CHAR2/CHnn` blits (not engine-drawn as
// previously thought) and saturate from depth 3 on.  So `dungeon_base_view_index`
// is exact only when the ahead cell is itself the first opaque cell (the common
// case: walls, doors, side passages) and an approximation otherwise.
//
// NOTE (Round 28): the view is *not* a pure function of the ahead cell alone.
// With the ahead cell fixed at 0, changing the depth-2 cell changes the window
// (ink 4941 -> 5111).  `dungeon_base_view_index` therefore only approximates the
// engine, which marches the whole column.  The feature/object byte of a cell has
// no effect on the view (Round 29).
//
// `index` 0 is never returned: DP000 does not exist on disk.
int dungeon_base_view_index(const Dungeon& d, const DungeonPos& p);

// A tile is *view-opaque* when the first-person renderer stops marching on it.
// This is NOT the movement wall set (`dungeon_is_solid_tile`): code 3 is
// walkable but renders opaque, so the view column stops on it.  Ground truth:
// the emulator depth sweep (notes-a.md Round 47).
bool dungeon_view_opaque(uint8_t code);

// The first-person view sprite for the party's position, as a path (relative to
// `dir`) plus the *screen* position to blit it at (clipped to the viewport
// window).  The engine marches the column of cells ahead of the party to the
// first view-opaque cell and blits a pre-rendered sprite chosen by that depth:
//
//   depth 1  -> EGA/D<d>/DP<index>  at (10,11)  (tileset sprite, nibble-mapped
//                                                by dungeon_base_view_index)
//   depth 2  -> EGA/CHAR2/CH40      at (10,11)
//   depth >=3-> EGA/CHAR2/CH25      at (10,11)
//
// Every view cell is a full 120x119 image (the viewport size), so all depths
// blit plainly at the viewport origin.  Verified pixel-exact against the DOSBox
// oracle (ref/oracle/dungeon_slot1.png spawn = DP001;
// ref/oracle/dungeon_slot1_moved.png = CH25, 1.0000) for the D5/Temple scenario.
// The depth-2/>=3 sprites are the scenario's encounter-sprite bank
// (`NSC<dd>[id].+0x48`, BLK-A005) and are pinned for D5 only; other scenarios
// still resolve their depth-1 tileset sprite correctly.
struct DungeonView {
    std::string path;   // e.g. "<dir>/EGA/D5/DP001"
    int x = kViewportX; // screen blit position
    int y = kViewportY;
    // Horizon-band side pieces blitted over the base, in order.  The outdoor
    // view is a composite, not a single sprite: see notes-a.md Round 24.
    struct Overlay {
        std::string path;
        int x;
        int y;
    };
    std::vector<Overlay> overlays;
};
DungeonView dungeon_view_for(const std::string& dir, int dungeon,
                             const Dungeon& d, const DungeonPos& p);

// The horizon-band side pieces that overlay a *named* view sprite, in blit
// order.  Empty for every view except the outdoor (D0) base view, which is a
// composite (notes-a.md Rounds 24-25).  Path-based so the `--play` / `--screen`
// renderers draw exactly what the play loop draws.
std::vector<DungeonView::Overlay> dungeon_view_overlays(const std::string& dir,
                                                        int dungeon, int index);

// Convenience: blit the dungeon's canonical view sprite (DP001) at (10,11).
// This is the base first-person view the engine shows on entry; it is verified
// for dungeon 5 (the slot-1 Temple oracle) and structurally valid for all
// levels.  The engine then overlays map tiles over this base (see notes-a.md).
bool render_dungeon_default_view(Screen& scr, const std::string& dir,
                                 int dungeon);

// Compose the whole dungeon play screen: the static frame `EGA/UP/MSCR` plus
// the view sprite `EGA/D<d>/DP<index>` blitted at (kViewportX,kViewportY).
// Verified against the DOSBox oracle (ref/oracle/dungeon_slot1.png): the frame
// region matches at 92.1% and the composite at 92.8% (the residual is a
// procedurally drawn overlay, see docs/notes-a.md).  Returns false if the frame
// or sprite cannot be loaded.
bool render_dungeon_screen(Screen& scr, const std::string& dir, int dungeon,
                           int index = 1);

// Compose the play screen and blit the base view selected for the party's
// current position (dungeon_base_view_index).  This is the drop-in's walkable
// first-person view: as the party moves/turns the view changes with the tile
// code of the cell ahead.  Returns false if the frame or sprite is missing.
bool render_dungeon_play(Screen& scr, const std::string& dir, int dungeon,
                         const Dungeon& d, const DungeonPos& p);

// Blit only the first-person view sprite for the party's position (see
// dungeon_view_for) into an already-composed screen, clipped to the viewport
// window.  Returns false if the sprite cannot be loaded.
bool render_dungeon_play_view(Screen& scr, const std::string& dir, int dungeon,
                              const Dungeon& d, const DungeonPos& p);

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
