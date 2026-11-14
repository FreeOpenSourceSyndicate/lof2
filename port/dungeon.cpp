// Dungeon map decoder for Legend of Faerghail (DNGS/DNGnn).
// See port/dungeon.h and docs/notes-a.md.
#include "dungeon.h"

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
    switch (code) {
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
    int x, y;
    if (!dungeon_ahead_cell(d, p, x, y)) return 1;   // off-map -> wall ahead
    // Only the LOW nibble of the ahead tile code selects the base view; the
    // high nibble is an object/feature class the base blit ignores.  Ground
    // truth: swept all 14 low nibbles that occur in DNGS/ (see dungeon.h),
    // including every high-nibble variant of 0x4 and 0x5.
    switch (d.tile(x, y) & 0x0f) {
        case 0x4: return 58;          // side passage
        case 0x5: return 59;          // side passage variant
        case 0x6: case 0xd: return 60;  // open room
        case 0xa: return 14;          // doorway
        // 0x0/0x2/0x7/0x8/0x9 are the "corridor/opening straight ahead" case.
        // The engine builds that view as a composite of a 64x59 wall-at-depth-2
        // piece (DP037) centred plus smaller side pieces, so no single full-size
        // sprite reproduces it; fall back to the wall view (see notes-a.md).
        default: return 1;            // wall directly ahead (DP001)
    }
}

std::string dungeon_view_path(const std::string& dir, int dungeon, int index) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "/EGA/%s/DP%03d",
                  dungeon_view_dir(dungeon).c_str(), index);
    return dir + buf;
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
    return render_dungeon_view(scr, dungeon_view_path(dir, dungeon, index),
                               kViewportX, kViewportY);
}

bool render_dungeon_play(Screen& scr, const std::string& dir, int dungeon,
                         const Dungeon& d, const DungeonPos& p) {
    return render_dungeon_screen(scr, dir, dungeon,
                                 dungeon_base_view_index(d, p));
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

}  // namespace lofport
