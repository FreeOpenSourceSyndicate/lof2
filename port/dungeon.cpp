// Dungeon map decoder for Legend of Faerghail (DNGS/DNGnn).
// See port/dungeon.h and docs/notes-a.md.
#include "dungeon.h"
#include "char.h"     // dungeon_map_name() — the DNGS file-name encoding

#include <cstdio>
#include <cstdlib>

namespace lofport {

std::vector<uint8_t> dngs_rle_decode(const std::vector<uint8_t>& raw) {
    std::vector<uint8_t> out;
    if (raw.empty()) return out;
    const uint8_t esc = raw[0];
    size_t i = 1;
    const size_t n = raw.size();
    while (i < n) {
        uint8_t b = raw[i++];
        if (b == esc) {
            if (i + 1 >= n) break;
            uint8_t cnt = raw[i];
            uint8_t val = raw[i + 1];
            i += 2;
            out.insert(out.end(), cnt, val);
        } else {
            out.push_back(b);
        }
    }
    return out;
}

bool decode_dungeon(const std::vector<uint8_t>& raw, Dungeon& out,
                    std::string* err) {
    auto fail = [&](const char* m) {
        if (err) *err = m;
        return false;
    };
    if (raw.size() < 2) return fail("dungeon file too small");

    std::vector<uint8_t> d = dngs_rle_decode(raw);
    if (d.size() != (size_t)kDungeonDecoded) {
        if (err) {
            char buf[96];
            std::snprintf(buf, sizeof buf,
                          "dungeon decoded to %zu bytes, expected %d",
                          d.size(), kDungeonDecoded);
            *err = buf;
        }
        return false;
    }

    out.h0 = d[0];                                        // flag
    out.h1 = d[1];                                        // entry facing
    out.field = (uint16_t)((d[2] << 8) | d[3]);           // entry pos, big-endian

    out.tiles.assign(d.begin() + kDungeonHeader,
                     d.begin() + kDungeonHeader + kDungeonTiles);
    const uint8_t* obj = d.data() + kDungeonHeader + kDungeonTiles;
    out.objects.resize(kDungeonTiles);
    for (int i = 0; i < kDungeonTiles; i++)
        out.objects[i] = (uint16_t)(obj[2 * i] | (obj[2 * i + 1] << 8));
    return true;
}

static char tile_symbol(uint8_t v) {
    switch (v) {
        case 0:  return '#';
        case 6:  return '%';
        case 13: return '%';
        case 3:  return '.';
        case 12: return '*';
        case 7:  return '+';
        case 8:  return 'o';
        case 2:  return '~';
        default: return '?';
    }
}

std::vector<std::string> dungeon_ascii(const Dungeon& d, bool show_objects) {
    std::vector<std::string> lines;
    lines.reserve(kDungeonH);
    for (int y = 0; y < kDungeonH; y++) {
        std::string row;
        row.reserve(kDungeonW);
        for (int x = 0; x < kDungeonW; x++) {
            uint8_t code = d.obj_code(x, y);
            if (show_objects && code)
                row.push_back(tile_symbol(code));
            else
                row.push_back(tile_symbol(d.tile(x, y)));
        }
        lines.push_back(row);
    }
    return lines;
}

bool dungeon_is_solid_tile(uint8_t code) {
    // Walls: tile 6 and tile 13.  Ground truth (emulator opacity sweep, Round 27):
    // patch a tile code X directly ahead and vary the cell *behind* it.  For
    // X in {0,1,2,3,7,8,12} the view changes (the ray passes through); only for
    // X in {6,13} is the view unchanged (the cell behind is occluded).  The
    // rendered view for 6/13 is the full-screen wall sprite DP036, matching.
    // The earlier {3,12} guess is disproved: DNG06's spawn cell is tile 3, and
    // {3,12} leaves it with 0 of 1201 cells reachable (DNG00: 8/684).
    //
    // The engine keys the view on the *masked* low nibble (the view routine
    // reads the map byte, `and al,0xf`, then `cmp al,6 / cmp al,0xd`; G-005), so
    // mask here too.  No shipped map has a code whose solidity differs under the
    // mask, so this is a faithful no-op today.
    switch (code & 0x0f) {
        case 6:
        case 13:
            return true;
        default:
            return false;
    }
}

bool dungeon_passable(const Dungeon& d, int x, int y) {
    if (x < 0 || y < 0 || x >= kDungeonW || y >= kDungeonH) return false;
    return !dungeon_is_solid_tile(d.tile(x, y));
}

bool dungeon_reachable(const Dungeon& d, int x0, int y0, int x1, int y1) {
    if (!dungeon_passable(d, x0, y0) || !dungeon_passable(d, x1, y1))
        return false;
    std::vector<char> seen((size_t)kDungeonTiles, 0);
    std::vector<int> stack;
    stack.push_back(y0 * kDungeonW + x0);
    seen[(size_t)y0 * kDungeonW + x0] = 1;
    const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
    while (!stack.empty()) {
        int c = stack.back();
        stack.pop_back();
        int x = c % kDungeonW, y = c / kDungeonW;
        if (x == x1 && y == y1) return true;
        for (int k = 0; k < 4; k++) {
            int nx = x + dx[k], ny = y + dy[k];
            if (!dungeon_passable(d, nx, ny)) continue;
            int nc = ny * kDungeonW + nx;
            if (seen[(size_t)nc]) continue;
            seen[(size_t)nc] = 1;
            stack.push_back(nc);
        }
    }
    return false;
}

int dungeon_facing_dx(int facing) { return facing == 1 ? 1 : facing == 3 ? -1 : 0; }
int dungeon_facing_dy(int facing) { return facing == 2 ? 1 : facing == 0 ? -1 : 0; }

void dungeon_turn(DungeonPos& p, bool right) {
    p.facing = (p.facing + (right ? 1 : 3)) & 3;
}

bool dungeon_can_step(const Dungeon& d, const DungeonPos& p) {
    return dungeon_passable(d, p.x + dungeon_facing_dx(p.facing),
                               p.y + dungeon_facing_dy(p.facing));
}

bool dungeon_step(const Dungeon& d, DungeonPos& p) {
    if (!dungeon_can_step(d, p)) return false;
    p.x += dungeon_facing_dx(p.facing);
    p.y += dungeon_facing_dy(p.facing);
    return true;
}

bool dungeon_place(const Dungeon& d, DungeonPos& p, int x, int y) {
    if (dungeon_passable(d, x, y)) { p.x = x; p.y = y; return true; }
    for (int r = 1; r < kDungeonW + kDungeonH; r++) {
        for (int dy = -r; dy <= r; dy++) {
            for (int dx = -r; dx <= r; dx++) {
                if (std::abs(dx) != r && std::abs(dy) != r) continue;
                if (dungeon_passable(d, x + dx, y + dy)) {
                    p.x = x + dx; p.y = y + dy; return true;
                }
            }
        }
    }
    return false;
}

bool dungeon_place_entry(const Dungeon& d, DungeonPos& p) {
    p.facing = d.entry_facing() & 3;
    return dungeon_place(d, p, d.entry_x(), d.entry_y());
}

bool dungeon_place_cell(const Dungeon& d, DungeonPos& p, int cell, int facing) {
    if (cell < 0 || cell >= kDungeonW * kDungeonH) return false;
    p.facing = facing & 3;
    return dungeon_place(d, p, cell % kDungeonW, cell / kDungeonW);
}

bool dungeon_teleport_cell(DungeonPos& p, int cell) {
    if (cell < 0 || cell >= kDungeonW * kDungeonH) return false;
    p.x = cell % kDungeonW;
    p.y = cell / kDungeonW;
    return true;
}

bool dungeon_follow_door(const std::string& dir, uint16_t dest_location,
                         int cell, uint16_t& location, Dungeon& d,
                         DungeonPos& pos) {
    const std::string name = dungeon_map_name(dest_location);
    if (name.empty()) return false;
    Dungeon next;
    std::string err;
    if (!load_dungeon(dir + "/DNGS/" + name, next, &err)) return false;
    DungeonPos moved = pos;   // place on a copy; commit only on success
    if (!dungeon_place_cell(next, moved, cell, pos.facing)) return false;
    d = next;
    pos = moved;
    location = dest_location;
    return true;
}

// --- Stairs, holes and portals -------------------------------------------

// LOF.EXE ds:0xa546: portal destinations, 4 bytes per dungeon number, laid out
// as [dest dungeon, 0, dest cell lo, dest cell hi].  The handler indexes it with
// `si = [0x37fe] << 2` and reads ds:[si-0x5aba] / ds:[si-0x5ab8].
//
// The index is the **second digit of the DNGS file name** (location % 10), not
// the group digit: the file name is built as `DNGS\DNG%01ld%01ld` (image
// 0x46645), and `[0x37fe]` is compared against 7 both here and for the
// stairs/hole wording, which only makes sense if it is the level digit -- the
// shipped levels have group digits 0,1,2,3,5,6 (DNG66) and level digits 0..7.
// The data settles it: both shipped portals sit on level 3 (DNG03, DNG43), and
// entry 3 gives DNG13 cell 757, a walkable cell.  Indexing by the group digit
// instead gives DNG03 *no* destination (entry 0 is all zero) and DNG43 a
// destination inside a wall (entry 4 -> DNG13 cell 104, tile 6).
static const uint8_t kPortalTable[8][4] = {
    {0, 0, 0x00, 0x00},   // dungeon 0
    {0, 0, 0xfd, 0x03},   // dungeon 1
    {0, 0, 0x00, 0x00},   // dungeon 2
    {1, 0, 0xf5, 0x02},   // dungeon 3
    {1, 0, 0x68, 0x00},   // dungeon 4
    {1, 0, 0x92, 0x04},   // dungeon 5
    {1, 0, 0x6e, 0x00},   // dungeon 6
    {0, 0, 0xc5, 0x04},   // dungeon 7
};

bool dungeon_is_stair(uint8_t code) {
    uint8_t n = code & kTileStairMask;
    return n == kTileStairUp || n == kTileStairDown;
}

bool dungeon_is_up_stair(uint8_t code) {
    return (code & kTileStairMask) == kTileStairUp;
}

bool dungeon_is_down_stair(uint8_t code) {
    return (code & kTileStairMask) == kTileStairDown;
}

bool dungeon_is_portal(uint8_t code) {
    return dungeon_is_stair(code) && (code >> 4) == kTilePortal;
}

// `location` is the full save location (`group*10 + level`).  A portal is
// looked up with the level digit, everything else takes its destination dungeon
// from the tile's high nibble.
int dungeon_stair_dungeon(uint8_t code, int location) {
    if (dungeon_is_portal(code)) {
        int level = location % 10;
        return (level >= 0 && level < 8) ? kPortalTable[level][0] : 0;
    }
    return code >> 4;
}

bool dungeon_stair_target(const Dungeon& d, int x, int y, int location,
                          DungeonLink* out) {
    uint8_t code = d.tile(x, y);
    if (!dungeon_is_stair(code)) return false;
    DungeonLink l;
    l.dungeon = dungeon_stair_dungeon(code, location);
    if (dungeon_is_portal(code)) {
        int level = location % 10;
        if (level < 0 || level >= 8) return false;
        l.cell = kPortalTable[level][2] | (kPortalTable[level][3] << 8);
    } else {
        l.cell = d.dest_cell(x, y);
    }
    if (l.cell < 0) return false;
    if (out) *out = l;
    return true;
}

// The engine prints "hole" instead of "stairs" on level 7 of a dungeon group:
// the handler tests the DNGS file-name level digit (`[0x37fe]`) against 7.
bool dungeon_is_hole_level(int location) { return (location % 10) == 7; }

const char* dungeon_stair_message(uint8_t code, int location, bool* portal) {
    if (portal) *portal = dungeon_is_portal(code);
    if (dungeon_is_portal(code)) return "You enter ";
    bool hole = dungeon_is_hole_level(location);
    if (dungeon_is_up_stair(code))
        return hole ? "You climb up the hole." : "You climb up the stairs.";
    return hole ? "You climb down the hole." : "You climb down the stairs.";
}

// Colour per tile code (EGA 16-colour indices).
static uint8_t tile_colour(uint8_t v) {
    switch (v) {
        case 0:  return 8;    // dark gray  - void/open
        case 6:  return 4;    // red        - wall
        case 13: return 4;    // red        - wall variant
        case 3:  return 7;    // light gray - floor/doorway
        case 12: return 1;    // blue       - water
        case 7:  return 10;   // green
        case 8:  return 6;    // brown
        case 2:  return 13;   // magenta
        default: return 15;   // white
    }
}

void render_dungeon_map(Screen& scr, const Dungeon& d, int px, int py, int cell) {
    if (cell < 1) cell = 1;
    int w = kDungeonW * cell, h = kDungeonH * cell;
    int ox = (scr.width - w) / 2, oy = (scr.height - h) / 2;
    for (int y = 0; y < kDungeonH; y++) {
        for (int x = 0; x < kDungeonW; x++) {
            uint8_t col = tile_colour(d.tile(x, y));
            // object cells get a lighter marker so features stand out
            if (d.obj_code(x, y)) col = 14;
            for (int dy = 0; dy < cell; dy++)
                for (int dx = 0; dx < cell; dx++)
                    scr.put(ox + x * cell + dx, oy + y * cell + dy, col);
        }
    }
    // player marker: a bright plus at (px,py)
    int cx = ox + px * cell + cell / 2, cy = oy + py * cell + cell / 2;
    for (int k = -cell; k <= cell; k++) {
        scr.put(cx + k, cy, 15);
        scr.put(cx, cy + k, 15);
    }
}

void render_dungeon_map(Screen& scr, const Dungeon& d, const DungeonPos& p,
                        int cell) {
    render_dungeon_map(scr, d, p.x, p.y, cell);
    if (cell < 1) cell = 1;
    int w = kDungeonW * cell, h = kDungeonH * cell;
    int ox = (scr.width - w) / 2, oy = (scr.height - h) / 2;
    int cx = ox + p.x * cell + cell / 2, cy = oy + p.y * cell + cell / 2;
    int dx = dungeon_facing_dx(p.facing), dy = dungeon_facing_dy(p.facing);
    for (int k = 1; k <= cell; k++) {
        scr.put(cx + dx * k, cy + dy * k, 15);
        scr.put(cx + dx * k - dy, cy + dy * k - dx, 14);
        scr.put(cx + dx * k + dy, cy + dy * k + dx, 14);
    }
}

// --- First-person viewport ------------------------------------------------

// Blit the engine's pre-rendered view sprite.  load_ega_image + blit_image are
// the same routines the rest of the drop-in uses for EGA art.
bool render_dungeon_view(Screen& scr, const std::string& sprite_path,
                         int vx, int vy) {
    EgaImage img;
    if (!load_ega_image(sprite_path, img, nullptr)) return false;
    blit_image(scr, img, vx, vy);
    return true;
}

std::string dungeon_view_dir(int dungeon) {
    return "D" + std::to_string(dungeon % 10);
}

bool dungeon_ahead_cell(const Dungeon& d, const DungeonPos& p, int& ax,
                        int& ay) {
    int x = p.x + dungeon_facing_dx(p.facing);
    int y = p.y + dungeon_facing_dy(p.facing);
    if (x < 0 || y < 0 || x >= kDungeonW || y >= kDungeonH) return false;
    ax = x;
    ay = y;
    return true;
}

int dungeon_base_view_index(const Dungeon& d, const DungeonPos& p) {
    return dungeon_base_view_index(d, p, /*bank_has_passage_sprites=*/true);
}

int dungeon_base_view_index(const Dungeon& d, const DungeonPos& p,
                            bool bank_has_passage_sprites) {
    int x, y;
    if (!dungeon_ahead_cell(d, p, x, y)) return 1;   // off-map -> wall ahead
    // Only the LOW nibble of the ahead tile code selects the base view; the
    // high nibble is an object/feature class the base blit ignores.  Measured by
    // sweeping the ahead cell of DNG05's spawn (Round 28; see dungeon.h).
    switch (d.tile(x, y) & 0x0f) {
        // DP058..DP060 ship only in the dungeon banks (D1..D5); D0 has its own
        // sprite set, so these would name a missing file there.  Fall through to
        // the always-present DP001 (the same G-004 fallback the open nibbles use).
        case 0x4: if (bank_has_passage_sprites) return 58; return 1;
        case 0x5: if (bank_has_passage_sprites) return 59; return 1;
        case 0x6: case 0xd:
            if (bank_has_passage_sprites) return 60;
            // D0 has no DP060.  It has two flat wall faces, and which one is
            // drawn depends on the depth column: measured on DNG10, a wall with
            // more wall behind it gives DP001 (cell 10 f3: 0.9499), a wall that
            // ends the map gives DP036 (cells 37/38 f0: 0.9648 both).  DP036 is
            // the map-border backdrop.  Notes-a Round 50b.
            if (x + dungeon_facing_dx(p.facing) < 0 ||
                y + dungeon_facing_dy(p.facing) < 0 ||
                x + dungeon_facing_dx(p.facing) >= kDungeonW ||
                y + dungeon_facing_dy(p.facing) >= kDungeonH)
                return 36;
            return 1;
        case 0xa: return 14;             // doorway
        // 0x1/0x3/0xb/0xc render DP001 (a textured wall/room view).
        case 0x1: case 0x3: case 0xb: case 0xc: return 1;
        // 0x0/0x2/0x7/0x8/0x9 are open cells whose view the engine draws from
        // the `EGA/CHAR2/CHnn` room bank (Round 38), not a composed DPnnn view.
        // The room-index -> CHnn table is not decoded yet, so fall back to the
        // wall view DP001 (G-004).
        default: return 1;
    }
}

bool dungeon_view_opaque(uint8_t code) {
    // The renderer marches the view column past these nibbles and stops on any
    // other.  Measured by the depth sweep (notes-a.md Round 47): the open
    // nibbles 0/2/7/8/9 let the march continue; 3 is opaque even though it is
    // walkable (so the movement wall set is NOT the view set).
    switch (code & 0x0f) {
        case 0x0: case 0x2: case 0x7: case 0x8: case 0x9: return false;
        default: return true;
    }
}

// The horizon-band side pieces of the outdoor (D0) view, and the base sprite
// they belong to.  MEASURED on DNG10 cell 745 (25,20): base `DP001` plus these
// three pieces reproduces the 120x119 window pixel-exactly (notes-a.md Rounds
// 24-25), and the pieces are overlays, so index 0 must leave the base visible.
// When the base resolves to anything else - e.g. looking back at the town, which
// draws a different bank entirely - the pieces do not appear, which is also
// measured.
//
// MEASURED, not tuned: the set is exact (0 px wrong) on cell 745 and each piece
// is necessary (ablation: base alone 0.9076, all three 0.9926 before the object
// overlay below).  The residual 106 px that looked like "overfit" were a
// MISSING DRAW, not a wrong base - they are the map object sprite, see
// kOutdoorObjectPath.  With it the window is pixel-exact.
//
// Still unverified: whether this band is the same for every outdoor cell.  Cells
// 636/673 (which have no object) score 0.9463/0.9265 rather than 1.0, so some
// other per-cell factor remains; that is tracked as G-013.  Do not widen the
// set without a second confirmed cell.
struct ViewPiece {
    int index;  // DPnnn number
    int x;      // window-relative origin
    int y;
};
static const int kOutdoorBaseViewIndex = 1;  // DP001
static const ViewPiece kOutdoorPieces[] = {
    {12, 0, 49},
    {4, 15, 46},
    {5, 90, 50},
};

// A map object standing in the viewport, drawn from the `EGA/UP` bank rather
// than the dungeon bank.  MEASURED on DNG10 cell 745 (25,20), which has an
// object record and where the base+pieces composite was still 106 px wrong;
// `UP/UP22` at (70,53) fixes exactly those 106 px and breaks none, taking the
// window to 0 px wrong.  Cells 673/636 have no object and get worse if it is
// drawn (0.9463->0.9400), so it is tied to the object record, not a generic
// overlay.  Object records are `[obj_code, obj_low]`; byte0 != 0 marks a cell
// as carrying one.
static const char kOutdoorObjectPath[] = "/EGA/UP/UP22";
static const int kOutdoorObjectX = 70;
static const int kOutdoorObjectY = 53;

std::vector<DungeonView::Overlay> dungeon_view_overlays(const std::string& dir,
                                                        int dungeon, int index) {
    std::vector<DungeonView::Overlay> out;
    if (dungeon_view_dir(dungeon) != "D0" || index != kOutdoorBaseViewIndex)
        return out;
    for (const ViewPiece& piece : kOutdoorPieces)
        out.push_back({dungeon_view_path(dir, dungeon, piece.index),
                       kViewportX + piece.x, kViewportY + piece.y});
    return out;
}

DungeonView dungeon_view_for(const std::string& dir, int dungeon,
                             const Dungeon& d, const DungeonPos& p) {
    DungeonView v;
    // DP058..DP060 are dungeon-bank art; the outdoor bank (D0) has its own set,
    // so ask for the fallback there rather than a path that does not exist.
    const bool bank_has_passage =
        dungeon_view_dir(dungeon) != "D0";
    int base_index = dungeon_base_view_index(d, p, bank_has_passage);
    // Default: the depth-1 tileset sprite at the viewport origin.
    v.path = dungeon_view_path(dir, dungeon, base_index);
    v.x = kViewportX;
    v.y = kViewportY;

    // The outdoor (D0) view is a composite: the base sprite plus the measured
    // horizon-band side pieces, whenever the base resolves to the sprite those
    // pieces were measured on.
    v.overlays = dungeon_view_overlays(dir, dungeon, base_index);

    // A map object in the party's own cell is drawn into the window from the UP
    // bank.  Gated on the cell actually carrying an object record: without that
    // gate the sprite is drawn on cells that have none and the match degrades.
    if (dungeon_view_dir(dungeon) == "D0" && base_index == kOutdoorBaseViewIndex &&
        d.object(p.x, p.y) != 0)
        v.overlays.push_back({dir + kOutdoorObjectPath,
                              kViewportX + kOutdoorObjectX,
                              kViewportY + kOutdoorObjectY});

    // March the column of cells ahead to the first view-opaque cell.
    int dx = dungeon_facing_dx(p.facing), dy = dungeon_facing_dy(p.facing);
    int cx = p.x, cy = p.y, depth = 0;
    for (int step = 1; step <= kDungeonW + kDungeonH; step++) {
        cx += dx; cy += dy;
        depth = step;
        bool off = (cx < 0 || cy < 0 || cx >= kDungeonW || cy >= kDungeonH);
        if (off || dungeon_view_opaque(d.tile(cx, cy))) break;
    }

    // Depth-2 and depth->=3 blit a scenario encounter sprite instead.  These are
    // pinned for the D5/Temple scenario only (the `NSC<dd>` encounter bank is
    // scenario-specific and its producer is BLK-A005); other scenarios keep the
    // depth-1 tileset sprite above.
    if (dungeon == 5) {
        if (depth == 2) {
            v.path = dir + "/EGA/CHAR2/CH40";
            v.x = kViewportX; v.y = kViewportY;
        } else if (depth >= 3) {
            v.path = dir + "/EGA/CHAR2/CH25";
            v.x = kViewportX; v.y = kViewportY;
        }
    }
    return v;
}

// Blit an EGA image at (dx,dy) clipped to the viewport window, so a sprite whose
// origin is left of the window (e.g. CH25 at x=-14) cannot spill onto the HUD.
// With `transparent`, index 0 leaves the destination untouched: the horizon-band
// side pieces are overlays and their background must not erase the base view.
static void blit_clipped_to_window(Screen& scr, const EgaImage& img, int dx,
                                   int dy, bool transparent = false) {
    int w = img.width_bytes, h = img.height;
    size_t pl = (size_t)w * h;
    for (int y = 0; y < h; y++) {
        int sy = dy + y;
        if (sy < kViewportY || sy >= kViewportY + kViewportH) continue;
        const uint8_t* p0 = img.planes.data() + (size_t)y * w;
        const uint8_t* p1 = img.planes.data() + pl + (size_t)y * w;
        const uint8_t* p2 = img.planes.data() + 2 * pl + (size_t)y * w;
        const uint8_t* p3 = img.planes.data() + 3 * pl + (size_t)y * w;
        for (int xb = 0; xb < w; xb++) {
            uint8_t b0 = p0[xb], b1 = p1[xb], b2 = p2[xb], b3 = p3[xb];
            for (int bit = 0; bit < 8; bit++) {
                int sx = dx + xb * 8 + bit;
                if (sx < kViewportX || sx >= kViewportX + kViewportW) continue;
                int s = 7 - bit;
                uint8_t val = (uint8_t)(((b0 >> s) & 1) | (((b1 >> s) & 1) << 1) |
                                        (((b2 >> s) & 1) << 2) | (((b3 >> s) & 1) << 3));
                if (transparent && val == 0) continue;
                scr.put(sx, sy, val);
            }
        }
    }
}

bool render_dungeon_play_view(Screen& scr, const std::string& dir, int dungeon,
                              const Dungeon& d, const DungeonPos& p) {
    DungeonView v = dungeon_view_for(dir, dungeon, d, p);
    EgaImage img;
    if (!load_ega_image(v.path, img, nullptr)) return false;
    blit_clipped_to_window(scr, img, v.x, v.y);
    for (const DungeonView::Overlay& ov : v.overlays) {
        EgaImage piece;
        if (load_ega_image(ov.path, piece, nullptr))
            blit_clipped_to_window(scr, piece, ov.x, ov.y, /*transparent=*/true);
    }
    return true;
}

std::string dungeon_view_path(const std::string& dir, int dungeon, int index) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "/EGA/%s/DP%03d",
                  dungeon_view_dir(dungeon).c_str(), index);
    return dir + buf;
}

std::string dungeon_room_art_path(const std::string& dir, int ch) {
    // The room routine branches on the number: `cmp ax, 0x16 / jge big`, so a
    // number below 22 always selects the CHAR3 bank (verified: NN=5 opens
    // ega\char3\ch05).  Numbers >= 22 take the bank digit from the runtime table
    // cs:[0x2a3e] indexed by cs:[0x240e]; that index is context-supplied and is
    // 5 (-> '2', CHAR2) for the DNG05 spawn, so 2 is used here as the measured
    // value.  `ch` is the encounter/area sprite number, `NSC<dd>[id].+0x48` for
    // the position-selected record.  See docs/notes-a.md Rounds 42-45.
    const int bank = (ch < kRoomArtSmallThreshold) ? 3 : 2;
    char buf[32];
    std::snprintf(buf, sizeof(buf), "/EGA/CHAR%d/CH%02d", bank, ch);
    return dir + buf;
}

std::string dungeon_encounter_sprite(const std::string& dir, int scenario,
                                     int location) {
    int ch = dungeon_room_art_number(dir, scenario, location);
    if (ch < 0) return {};
    return dungeon_room_art_path(dir, ch);
}

int dungeon_room_art_number(const std::string& dir, int scenario, int location) {
    if (location < 0) return -1;
    char buf[32];
    std::snprintf(buf, sizeof(buf), "/SYST/NSC%02d", scenario);
    FILE* f = std::fopen((dir + buf).c_str(), "rb");
    if (!f) return -1;
    uint8_t count = 0;
    if (std::fread(&count, 1, 1, f) != 1) { std::fclose(f); return -1; }
    if (location >= count) { std::fclose(f); return -1; }
    long off = 1L + (long)location * kRoomArtDiskStride + kRoomArtField;
    if (std::fseek(f, off, SEEK_SET) != 0) { std::fclose(f); return -1; }
    uint8_t v = 0;
    if (std::fread(&v, 1, 1, f) != 1) { std::fclose(f); return -1; }
    std::fclose(f);
    return v;
}

bool render_dungeon_default_view(Screen& scr, const std::string& dir,
                                 int dungeon) {
    return render_dungeon_view(scr, dungeon_view_path(dir, dungeon, 1));
}

bool render_dungeon_screen(Screen& scr, const std::string& dir, int dungeon,
                           int index) {
    // The play screen's static frame is the same artwork the main menu uses,
    // EGA/UP/MSCR; the first-person window is the view sprite at (10,11).
    // Verified against the DOSBox oracle (ref/oracle/dungeon_slot1.png): the
    // frame region matches at 92.1% and the composite at 92.8%.
    EgaImage frame;
    if (!load_ega_image(dir + "/EGA/UP/MSCR", frame, nullptr)) return false;
    blit_image(scr, frame, 0, 0);
    if (!render_dungeon_view(scr, dungeon_view_path(dir, dungeon, index),
                             kViewportX, kViewportY))
        return false;
    // The outdoor base view is a composite, so `--play D0/DP001` must draw the
    // same side pieces the play loop does.
    for (const DungeonView::Overlay& ov :
         dungeon_view_overlays(dir, dungeon, index)) {
        EgaImage img;
        if (load_ega_image(ov.path, img, nullptr))
            blit_clipped_to_window(scr, img, ov.x, ov.y, /*transparent=*/true);
    }
    return true;
}

bool render_dungeon_play(Screen& scr, const std::string& dir, int dungeon,
                         const Dungeon& d, const DungeonPos& p) {
    EgaImage frame;
    if (!load_ega_image(dir + "/EGA/UP/MSCR", frame, nullptr)) return false;
    blit_image(scr, frame, 0, 0);
    return render_dungeon_play_view(scr, dir, dungeon, d, p);
}

// Colour indices of the synthetic fallback viewport.  The oracle's window uses
// only light grey (7) for walls and black (0) for ceiling / floor.
static constexpr uint8_t kViewWall = 7;    // wall face
static constexpr uint8_t kViewFloor = 0;   // ceiling / floor / open space

// Cast a single column.  Walks the grid from the party position along the
// view ray until it leaves the level or hits a solid cell, and returns the wall
// distance in tiles.  Uses the DDA (Amanatides & Woo) algorithm so sampling is
// uniform and walls land on cell boundaries.  Off-map cells count as solid, so
// a ray fired at open space still terminates at the level edge.
static float view_cast(const Dungeon& d, float px, float py, float dx, float dy,
                       float max_dist) {
    int mapx = (int)px, mapy = (int)py;
    float ddx = (dx == 0) ? 1e30f : 1.f / (dx > 0 ? dx : -dx);
    float ddy = (dy == 0) ? 1e30f : 1.f / (dy > 0 ? dy : -dy);
    int stepx = (dx < 0) ? -1 : 1;
    int stepy = (dy < 0) ? -1 : 1;
    float sidex = (dx < 0) ? (px - mapx) : (mapx + 1 - px);
    float sidey = (dy < 0) ? (py - mapy) : (mapy + 1 - py);
    float sdx = sidex * ddx;   // distance to the next x grid line
    float sdy = sidey * ddy;
    float dist = 0;
    for (int guard = 0; guard < 4 * kDungeonW; guard++) {
        if (sdx < sdy) { dist = sdx; mapx += stepx; sdx += ddx; }
        else { dist = sdy; mapy += stepy; sdy += ddy; }
        if (dist > max_dist) return max_dist;
        bool off = (mapx < 0 || mapy < 0 || mapx >= kDungeonW || mapy >= kDungeonH);
        if (off || dungeon_is_solid_tile(d.tile(mapx, mapy))) return dist;
    }
    return max_dist;
}

void render_dungeon_viewport(Screen& scr, const Dungeon& d, const DungeonPos& p,
                             int vx, int vy, int vw, int vh) {
    if (vw <= 0 || vh <= 0) return;
    for (int y = 0; y < vh; y++)
        for (int x = 0; x < vw; x++) scr.put(vx + x, vy + y, kViewFloor);

    // 60-degree horizontal field of view.  The camera plane is the facing
    // direction rotated 90 degrees and scaled by tan(fov/2).
    const float plane = 0.57735f;
    float dirx = (float)dungeon_facing_dx(p.facing);
    float diry = (float)dungeon_facing_dy(p.facing);
    float planx = -diry * plane, plany = dirx * plane;
    float px = p.x + 0.5f, py = p.y + 0.5f;
    float max_dist = (float)(kDungeonW + kDungeonH);

    for (int x = 0; x < vw; x++) {
        float cam = 2.f * x / (float)vw - 1.f;   // -1 .. +1 across the window
        float rx = dirx + planx * cam, ry = diry + plany * cam;
        float dist = view_cast(d, px, py, rx, ry, max_dist);
        // A wall one tile away is as tall as the window; nearer walls clamp to
        // it (a solid wall right ahead fills the window with a flat grey sheet,
        // exactly as the oracle shows when the Temple save loads).
        int wall_h = (int)(vh / dist);
        if (wall_h > vh) wall_h = vh;
        if (wall_h < 1) wall_h = 1;
        int top = (vh - wall_h) / 2, bot = top + wall_h;
        for (int y = top; y < bot; y++) scr.put(vx + x, vy + y, kViewWall);
    }
}


bool load_dungeon(const std::string& path, Dungeon& out, std::string* err) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) {
        if (err) *err = "cannot open " + path;
        return false;
    }
    std::vector<uint8_t> raw;
    uint8_t buf[4096];
    size_t got;
    while ((got = std::fread(buf, 1, sizeof buf, f)) > 0)
        raw.insert(raw.end(), buf, buf + got);
    std::fclose(f);
    return decode_dungeon(raw, out, err);
}

std::string dungeon_file_name(int dungeon, int level) {
    if (dungeon < 0) dungeon = 0;
    if (dungeon > 9) dungeon = 9;
    if (level < 0) level = 0;
    if (level > 9) level = 9;
    return std::string("DNG") + (char)('0' + dungeon) + (char)('0' + level);
}

// ---------------------------------------------------------------------------
// BLK-A005 view slot geometry.  Tables transcribed from the engine's data
// segment (cs-relative offsets in the comment); the DP number follows from the
// drawer's tile offset and the id->DP(12+id) descriptor mapping.
namespace {
constexpr int kSlotCount = 28;
// drawers A/B: X 0x25DA, Y 0x2612, tile 0x264A
const int kXab[kSlotCount] = {10,10,100,10,40,100,10,40,85,115,10,25,55,85,115,10,25,55,80,105,120,10,20,40,60,80,100,120};
const int kYab[kSlotCount] = {11,11,11,41,41,41,41,41,41,41,57,57,57,57,57,57,57,57,57,57,57,61,61,61,61,61,61,61};
const int kTab[kSlotCount] = {36,46,47,41,37,40,52,48,49,53,43,38,38,38,42,56,54,50,51,55,57,45,39,39,39,39,39,44};
// drawer C: X 0x26E6, Y 0x271E, tile 0x2756
const int kXc[kSlotCount] = {10,0,0,10,41,101,0,0,0,0,10,25,55,85,115,0,0,0,0,0,0,10,20,40,60,80,100,120};
const int kYc[kSlotCount] = {11,0,0,41,41,41,0,0,0,0,57,57,57,57,57,0,0,0,0,0,0,61,61,61,61,61,61,61};
const int kTc[kSlotCount] = {2,0,0,6,3,7,0,0,0,0,8,4,4,4,9,0,0,0,0,0,0,10,5,5,5,5,5,11};
}  // namespace

int dungeon_slot_count() { return kSlotCount; }

int dungeon_slot_x(ViewDrawer d, int slot) {
    if (slot < 0 || slot >= kSlotCount) return 0;
    return d == ViewDrawer::C ? kXc[slot] : kXab[slot];
}

int dungeon_slot_y(ViewDrawer d, int slot) {
    if (slot < 0 || slot >= kSlotCount) return 0;
    return d == ViewDrawer::C ? kYc[slot] : kYab[slot];
}

int dungeon_slot_dp(ViewDrawer d, int slot) {
    if (slot < 0 || slot >= kSlotCount) return -1;
    if (d == ViewDrawer::C) {
        int t = kTc[slot];
        return t == 0 ? -1 : t + 0x3c;
    }
    int t = kTab[slot];
    if (d == ViewDrawer::B) { t -= 0x16; return t >= 1 ? t : -1; }
    return t;  // drawer A
}

std::vector<ViewTile> dungeon_view_pieces(const uint8_t* codes, bool side_flag) {
    std::vector<ViewTile> out;
    auto add = [&](ViewDrawer d, int slot) {
        int dp = dungeon_slot_dp(d, slot);
        if (dp >= 0) out.push_back({dp, dungeon_slot_x(d, slot), dungeon_slot_y(d, slot)});
    };
    // Per-cell primary + adjacent slots (cells 0x34..0x42 = index 0..14), decoded
    // from the dispatch block (image 0x3CAE3..).  code 7/8 pick the near/far
    // drawer by the side flag (7 = the side that swaps A<->B with facing).
    static const int P[15]   = {1, 0, 2, 6, 7, 4, 8, 9, 15, 16, 17, 12, 18, 19, 20};
    static const int Adj[15] = {0, 0, 0, 0, 3, 0, 5, 0, 0, 10, 11, 0, 13, 14, 0};
    const ViewDrawer left  = side_flag ? ViewDrawer::B : ViewDrawer::A;
    const ViewDrawer right = side_flag ? ViewDrawer::A : ViewDrawer::B;
    for (int i = 14; i >= 0; --i) {
        int code = codes[i];
        if (code == 6)        { add(ViewDrawer::A, P[i]); if (Adj[i]) add(ViewDrawer::A, Adj[i]); }
        else if (code == 7)   { add(left,  P[i]);         if (Adj[i]) add(left,  Adj[i]); }
        else if (code == 8)   { add(right, P[i]);         if (Adj[i]) add(right, Adj[i]); }
        else if (code == 0xa) { add(ViewDrawer::B, P[i]); if (Adj[i]) add(ViewDrawer::B, Adj[i]); }
        else if (code == 0xc) { if (Adj[i]) add(ViewDrawer::C, Adj[i]); }
    }
    // Far-row depth loop (3A8D:0x2190): cells 0x44..0x49 (index 16..21) -> slots 22..27.
    for (int i = 16; i <= 21; ++i) {
        int slot = 22 + (i - 16), code = codes[i];
        if (code == 6 || code == 7)        add(ViewDrawer::A, slot);
        else if (code == 8 || code == 0xa) add(ViewDrawer::B, slot);
        else if (code == 0xc)              add(ViewDrawer::C, slot);
    }
    return out;
}

bool render_dungeon_tiled(Screen& scr, const std::string& dir, int dungeon,
                          const uint8_t* codes, bool side_flag) {
    EgaImage base;
    if (!load_ega_image(dungeon_view_path(dir, dungeon, 1), base)) return false;
    blit_image(scr, base, kViewportX, kViewportY, false);
    for (const ViewTile& t : dungeon_view_pieces(codes, side_flag)) {
        EgaImage img;
        if (load_ega_image(dungeon_view_path(dir, dungeon, t.dp), img))
            blit_image(scr, img, t.x, t.y, true);
    }
    return true;
}

uint8_t dungeon_march_code(uint8_t tile, uint16_t obj, bool side_flag) {
    if (tile == 0xd) return 6;
    if (tile == 4 || tile == 5) return 0;
    if (tile == 9) return 0xa;
    if (tile == 3) {
        if (obj == 1) return 6;
        if (obj == 2 || obj == 4) return side_flag ? 8 : 7;
        return 3;
    }
    return tile;
}

void dungeon_view_codes(const Dungeon& d, const DungeonPos& p, uint8_t codes[22]) {
    // Per-facing cell offsets (static table, decoded at 0x4C92:0x9A0C); a view
    // cell samples the map cell at party_cell + offset.
    static const int kRay[4][22] = {
        {-37, -36, -35, -74, -73, -72, -71, -70, -111, -110, -109, -108, -107, -106, -105, -147, -146, -145, -144, -143, -142, -141},
        {-35,   1,  37, -70, -34,   2,  38,  74, -105,  -69,  -33,    3,   39,   75,  111, -104,  -68,  -32,    4,   40,   76,  112},
        { 37,  36,  35,  74,  73,  72,  71,  70,  111,  110,  109,  108,  107,  106,  105,  147,  146,  145,  144,  143,  142,  141},
        { 35,  -1, -37,  70,  34,  -2, -38, -74,  105,   69,   33,   -3,  -39,  -75, -111,  104,   68,   32,   -4,  -40,  -76, -112},
    };
    int facing = ((p.facing % 4) + 4) % 4;
    int party = p.y * kDungeonW + p.x;
    bool side = (facing == 1 || facing == 3);
    for (int i = 0; i < 22; i++) {
        int cell = party + kRay[facing][i];
        if (cell < 0 || cell >= kDungeonTiles) { codes[i] = 0; continue; }
        codes[i] = dungeon_march_code(d.tile(cell % kDungeonW, cell / kDungeonW),
                                      d.object(cell % kDungeonW, cell / kDungeonW),
                                      side);
    }
}

}  // namespace lofport
