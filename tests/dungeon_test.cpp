// Self-test for the DNGS dungeon decoder (workstream A).
//
// The synthetic case is built in memory so the test needs no game data.  When
// game data is present (game/DNGS/DNG00 ...) the decoder is also run against a
// real file and its geometry / a few known cells are asserted.
#include "dungeon.h"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

using namespace lofport;

// Build a DNGS byte stream that RLE-decodes to `plain` using escape `esc`.
static std::vector<uint8_t> encode_rle(const std::vector<uint8_t>& plain,
                                       uint8_t esc) {
    std::vector<uint8_t> out;
    out.push_back(esc);
    size_t i = 0;
    while (i < plain.size()) {
        // literal run up to the next repeat of >=3 equal bytes
        size_t j = i;
        while (j < plain.size()) {
            if (j + 2 < plain.size() && plain[j] == plain[j + 1] &&
                plain[j] == plain[j + 2])
                break;
            j++;
        }
        for (size_t k = i; k < j; k++) out.push_back(plain[k]);
        if (j >= plain.size()) break;
        uint8_t v = plain[j];
        size_t run = 0;
        while (j + run < plain.size() && plain[j + run] == v && run < 255) run++;
        out.push_back(esc);
        out.push_back((uint8_t)run);
        out.push_back(v);
        i = j + run;
    }
    return out;
}

static void test_synthetic() {
    std::vector<uint8_t> plain(kDungeonDecoded, 6);
    // Zero the object region so only the one record we set is non-zero.
    for (size_t k = kDungeonHeader + kDungeonTiles; k < plain.size(); k++)
        plain[k] = 0;
    plain[0] = 0; plain[1] = 2;
    plain[2] = 0x34; plain[3] = 0x12;         // field = 0x1234
    plain[kDungeonHeader + 0] = 12;           // tile (0,0)
    plain[kDungeonHeader + 1] = 3;            // tile (1,0)
    plain[kDungeonHeader + kDungeonW * 5 + 7] = 0;  // tile (7,5)
    // object (7,5) = 0x0201
    size_t ob = kDungeonHeader + kDungeonTiles + 2 * (5 * kDungeonW + 7);
    plain[ob] = 0x01; plain[ob + 1] = 0x02;

    for (uint8_t esc : {(uint8_t)0xFF, (uint8_t)0xFD, (uint8_t)0xAA}) {
        std::vector<uint8_t> raw = encode_rle(plain, esc);
        Dungeon d;
        std::string err;
        bool ok = decode_dungeon(raw, d, &err);
        assert(ok);
        assert(d.h0 == 0 && d.h1 == 2 && d.field == 0x1234);
        assert((int)d.tiles.size() == kDungeonTiles);
        assert((int)d.objects.size() == kDungeonTiles);
        assert(d.tile(0, 0) == 12);
        assert(d.tile(1, 0) == 3);
        assert(d.tile(7, 5) == 0);
        assert(d.tile(20, 20) == 6);          // untouched cell
        assert(d.object(7, 5) == 0x0201);
        assert(d.object(0, 0) == 0);
        // Out-of-range accessors are safe and return 0.
        assert(d.tile(-1, 0) == 0 && d.tile(0, -1) == 0);
        assert(d.tile(kDungeonW, 0) == 0 && d.object(0, kDungeonH) == 0);
    }

    // A short stream must be rejected.
    std::vector<uint8_t> bad = {0xFF, 0x00, 0x01, 0x02};
    Dungeon d;
    assert(!decode_dungeon(bad, d, nullptr));
    std::vector<uint8_t> empty;
    assert(!decode_dungeon(empty, d, nullptr));
}

static void test_real(const std::string& dir) {
    // DNG00 is the only level with a non-uniform border (its outer ring is 12,
    // the interior mostly 0); DNG26 is almost entirely 6.
    struct Expect { const char* name; int h1; uint16_t field;
                    int x, y, tile; };
    Expect cases[] = {
        {"DNG00", 3, 52738, 0, 0, 12},
        {"DNG00", 3, 52738, 35, 35, 12},
        {"DNG00", 3, 52738, 17, 17, 0},
        {"DNG01", 1, 65280, 0, 0, 6},
        {"DNG01", 1, 65280, 17, 17, 0},
        {"DNG26", 3, 17156, 0, 0, 6},
        {"DNG26", 3, 17156, 5, 6, 6},
    };
    int checked = 0;
    for (auto& e : cases) {
        std::string path = dir + "/" + e.name;
        Dungeon d;
        std::string err;
        if (!load_dungeon(path, d, &err)) continue;   // no data -> skip
        assert((int)d.tiles.size() == kDungeonTiles);
        assert((int)d.objects.size() == kDungeonTiles);
        assert(d.h1 == e.h1);
        assert(d.field == e.field);
        assert(d.tile(e.x, e.y) == e.tile);
        checked++;
    }
    if (checked) std::printf("dungeon_test: verified %d real cells\n", checked);
    else std::printf("dungeon_test: no game data, skipping real-file checks\n");
}

// DNGS file naming: DNG<dungeon><level>, matching the game's own
// `DNGS\DNG%01ld%01ld` format string.  Every file present in game/DNGS must
// round-trip through the helper, and every one must decode to the fixed
// 36x36 geometry (the header+body size is invariant across all 29 files).
static void test_file_name(const std::string& dir) {
    assert(dungeon_file_name(0, 0) == "DNG00");
    assert(dungeon_file_name(1, 7) == "DNG17");
    assert(dungeon_file_name(6, 6) == "DNG66");
    assert(dungeon_file_name(12, -1) == "DNG90");   // clamped to single digits
    int checked = 0;
    for (int dg = 0; dg < 7; dg++) {
        for (int lv = 0; lv < 8; lv++) {
            std::string p = dir + "/" + dungeon_file_name(dg, lv);
            FILE* f = std::fopen(p.c_str(), "rb");
            if (!f) continue;
            std::fclose(f);
            Dungeon d;
            assert(load_dungeon(p, d));
            assert(dungeon_file_name(dg, lv) ==
                   std::string("DNG") + (char)('0' + dg) + (char)('0' + lv));
            // Fixed geometry for every shipped level.
            assert((int)d.tiles.size() == kDungeonTiles);
            assert((int)d.objects.size() == kDungeonTiles);
            checked++;
        }
    }
    if (checked) std::printf("dungeon_test: %d levels load by (dungeon,level)\n",
                             checked);
}

static void test_render() {
    Dungeon d;
    d.tiles.assign(kDungeonTiles, 6);
    d.objects.assign(kDungeonTiles, 0);
    d.tiles[0] = 12;                        // wall at (0,0)
    d.tiles[5] = 3;                         // wall at (5,0)
    assert(!dungeon_passable(d, 0, 0));
    assert(dungeon_passable(d, 1, 0));
    assert(!dungeon_passable(d, -1, 0));
    assert(!dungeon_passable(d, kDungeonW, 0));

    // ascii: wall symbol for (0,0), floor for the rest of row 0.
    auto lines = dungeon_ascii(d);
    assert((int)lines.size() == kDungeonH);
    assert(lines[0][0] == '*');             // 12 -> '*'
    assert(lines[0][5] == '%');             // 3  -> '%'
    assert(lines[0][1] == '.');             // 6  -> '.'

    Screen scr;
    scr.resize(320, 200);
    scr.clear(0);
    render_dungeon_map(scr, d, 1, 1, 4);
    // The player marker is drawn bright (15) somewhere near the top-left.
    bool found = false;
    for (uint8_t v : scr.pixels) if (v == 15) { found = true; break; }
    assert(found);
}

static void test_reachable() {
    Dungeon d;
    d.tiles.assign(kDungeonTiles, 6);
    d.objects.assign(kDungeonTiles, 0);
    // A vertical wall at x=10 splits the level into two rooms.
    for (int y = 0; y < kDungeonH; y++) d.tiles[y * kDungeonW + 10] = 3;
    assert(dungeon_reachable(d, 1, 1, 9, 30));    // same room
    assert(!dungeon_reachable(d, 1, 1, 11, 1));   // across the wall
    assert(dungeon_reachable(d, 11, 1, 35, 35));  // other room
    // Punch a doorway and the two rooms connect.
    d.tiles[5 * kDungeonW + 10] = 6;
    assert(dungeon_reachable(d, 1, 1, 11, 1));
    // A solid tile is never a valid endpoint.
    assert(!dungeon_reachable(d, 1, 1, 10, 1));
    assert(dungeon_is_solid_tile(3) && dungeon_is_solid_tile(12));
    assert(!dungeon_is_solid_tile(0) && !dungeon_is_solid_tile(6));
}

// The 2592-byte tail is 1296 two-byte records, NOT three 1296-byte planes.
// Decisive evidence: read as words, the feature bytes are spatially coherent
// (adjacent non-zero cells), whereas reading the tail as fixed 1296-byte
// planes gives near-random scatter.  This test pins that decision down.
static int coherence(const std::vector<uint8_t>& g) {
    int adj = 0;
    for (int y = 0; y < kDungeonH; y++)
        for (int x = 0; x < kDungeonW; x++) {
            if (!g[y * kDungeonW + x]) continue;
            if (x + 1 < kDungeonW && g[y * kDungeonW + x + 1]) adj++;
            if (y + 1 < kDungeonH && g[(y + 1) * kDungeonW + x]) adj++;
        }
    return adj;
}

static void test_object_layout(const std::string& dir) {
    FILE* f = std::fopen((dir + "/DNG17").c_str(), "rb");
    if (!f) { std::printf("dungeon_test: no DNG17, skipping layout check\n"); return; }
    std::vector<uint8_t> raw; uint8_t buf[4096]; size_t got;
    while ((got = std::fread(buf, 1, sizeof buf, f)) > 0) raw.insert(raw.end(), buf, buf + got);
    std::fclose(f);
    std::vector<uint8_t> d = dngs_rle_decode(raw);
    assert(d.size() == (size_t)kDungeonDecoded);
    const uint8_t* tail = d.data() + kDungeonHeader + kDungeonTiles;
    std::vector<uint8_t> hi(kDungeonTiles), blk(tail, tail + kDungeonTiles);
    for (int i = 0; i < kDungeonTiles; i++) hi[i] = tail[2 * i + 1];
    assert(coherence(hi) > 4 * coherence(blk));  // words are the right reading
}

static void test_render_facing() {
    Dungeon d;
    d.tiles.assign(kDungeonTiles, 6);
    d.objects.assign(kDungeonTiles, 0);
    Screen scr; scr.resize(320, 200);
    DungeonPos p{18, 18, 1};  // facing east
    render_dungeon_map(scr, d, p, 4);
    // The facing arrow must paint some pixels east of the centre marker.
    int cx = (320 - kDungeonW * 4) / 2 + 18 * 4 + 2;
    int cy = (200 - kDungeonH * 4) / 2 + 18 * 4 + 2;
    int east = 0;
    for (int k = 1; k <= 4; k++)
        east += scr.pixels[cy * scr.width + (cx + k)] == 15;
    assert(east > 0);
}

// The top-down map must distinguish the three cell kinds the model defines:
// floor, wall and a door/feature.  Render a level with one of each and check
// the sampled centre pixels carry three different colours.
static void test_render_layers() {
    Dungeon d;
    d.tiles.assign(kDungeonTiles, 6);       // floor
    d.objects.assign(kDungeonTiles, 0);
    d.tiles[2 * kDungeonW + 3] = 3;         // wall at (3,2)
    d.objects[4 * kDungeonW + 5] = 0x0100;  // door feature at (5,4)
    Screen scr; scr.resize(320, 200); scr.clear(0);
    render_dungeon_map(scr, d, 1, 1, 6);
    int ox = (320 - kDungeonW * 6) / 2, oy = (200 - kDungeonH * 6) / 2;
    auto at = [&](int x, int y) {
        return scr.pixels[(size_t)(oy + y * 6 + 3) * scr.width + ox + x * 6 + 3];
    };
    uint8_t floor_c = at(1, 1), wall_c = at(3, 2), door_c = at(5, 4);
    assert(floor_c != wall_c && wall_c != door_c && floor_c != door_c);
    assert(wall_c == 4);                    // code 3 -> red
    assert(door_c == 14);                   // feature marker
    assert(dungeon_is_solid_tile(3) && dungeon_is_solid_tile(12));
    assert(!dungeon_is_solid_tile(6) && !dungeon_is_solid_tile(0));
}

// First-person viewport.  A solid wall directly ahead fills the window with
// the flat wall colour (the oracle's Temple spawn view); an open corridor
// exposes the dark floor and the side walls, so the column histogram changes.
static void test_viewport() {
    Dungeon d;
    d.tiles.assign(kDungeonTiles, 6);       // floor everywhere
    d.objects.assign(kDungeonTiles, 0);
    Screen scr; scr.resize(320, 200);

    auto count = [&](uint8_t c) {
        int n = 0;
        for (int y = 0; y < kViewportH; y++)
            for (int x = 0; x < kViewportW; x++)
                if (scr.pixels[(size_t)(kViewportY + y) * scr.width +
                               kViewportX + x] == c) n++;
        return n;
    };

    // Wall one cell north of the party, facing it: the whole window is wall.
    d.tiles[9 * kDungeonW + 10] = 3;        // wall at (10,9)
    DungeonPos p{10, 10, 0};                // at (10,10), facing N
    scr.clear(15);
    render_dungeon_viewport(scr, d, p);
    assert(count(7) == kViewportW * kViewportH);   // all wall (index 7)
    assert(count(0) == 0);

    // Turn around: the cell south is open, so the window is mostly floor with
    // a distant wall band — it must NOT be a flat sheet any more.
    p.facing = 2;                            // S
    scr.clear(15);
    render_dungeon_viewport(scr, d, p);
    int wall = count(7), open = count(0);
    assert(open > kViewportW * kViewportH / 2);    // mostly open
    assert(wall > 0);                              // still some wall visible
    assert(wall < kViewportW * kViewportH);

    // A wall one cell away fills more of the window than one four cells away.
    Dungeon d2;
    d2.tiles.assign(kDungeonTiles, 6);
    d2.objects.assign(kDungeonTiles, 0);
    d2.tiles[9 * kDungeonW + 10] = 3;
    Screen near; near.resize(320, 200);
    render_dungeon_viewport(near, d2, DungeonPos{10, 10, 0});
    d2.tiles[9 * kDungeonW + 10] = 6;
    d2.tiles[6 * kDungeonW + 10] = 3;        // wall at (10,6), 4 cells ahead
    Screen far; far.resize(320, 200);
    render_dungeon_viewport(far, d2, DungeonPos{10, 10, 0});
    auto wallcount = [](const Screen& s) {
        int n = 0;
        for (int y = 0; y < kViewportH; y++)
            for (int x = 0; x < kViewportW; x++)
                if (s.pixels[(size_t)(kViewportY + y) * s.width + kViewportX + x] == 7)
                    n++;
        return n;
    };
    assert(wallcount(near) > wallcount(far));
}

// The engine's real model: blit a pre-rendered view sprite (EGA/D<n>/DPnnn)
// into the window.  When the game data is present, the blit must reproduce the
// decoded sprite byte-for-byte at (kViewportX,kViewportY).
static void test_view_sprite(const std::string& dir) {
    std::string path = dir + "/../EGA/D5/DP001";
    EgaImage img;
    if (!load_ega_image(path, img, nullptr)) {
        std::printf("dungeon_test: no view sprite, skipping blit check\n");
        return;
    }
    Screen scr; scr.resize(320, 200); scr.clear(9);
    assert(render_dungeon_view(scr, path));
    int w = img.width_bytes * 8;
    size_t pl = (size_t)img.width_bytes * img.height;
    for (int y = 0; y < img.height; y++)
        for (int x = 0; x < w; x++) {
            int xb = x >> 3, s = 7 - (x & 7);
            const uint8_t* p = img.planes.data() + (size_t)y * img.width_bytes + xb;
            uint8_t want = (uint8_t)(((p[0] >> s) & 1) | (((p[pl] >> s) & 1) << 1) |
                                     (((p[2 * pl] >> s) & 1) << 2) |
                                     (((p[3 * pl] >> s) & 1) << 3));
            assert(scr.pixels[(size_t)(kViewportY + y) * scr.width +
                              kViewportX + x] == want);
        }
    // A missing sprite is reported, not drawn.
    assert(!render_dungeon_view(scr, dir + "/../EGA/D5/NOPE"));
    std::printf("dungeon_test: view sprite blit verified (%dx%d)\n", w, img.height);
}

static void test_walk() {
    Dungeon d;
    d.tiles.assign(kDungeonTiles, 6);
    d.objects.assign(kDungeonTiles, 0);
    // A wall directly north of (5,5); open everywhere else.
    d.tiles[4 * kDungeonW + 5] = 3;

    DungeonPos p{5, 5, 0};  // facing N, wall ahead
    assert(!dungeon_can_step(d, p));
    assert(!dungeon_step(d, p) && p.x == 5 && p.y == 5);
    dungeon_turn(p, true);                 // -> E
    assert(p.facing == 1 && dungeon_can_step(d, p));
    assert(dungeon_step(d, p) && p.x == 6 && p.y == 5);
    dungeon_turn(p, false);                // -> N
    assert(p.facing == 0);
    dungeon_turn(p, false);                // -> W
    assert(p.facing == 3 && dungeon_facing_dx(3) == -1);
    assert(dungeon_step(d, p) && p.x == 5);

    // place() snaps off a wall to the nearest open cell.
    DungeonPos q;
    assert(dungeon_place(d, q, 5, 4));
    assert(dungeon_passable(d, q.x, q.y));
    assert(!(q.x == 5 && q.y == 4));       // that cell is the wall
}

// A hand-built maze should be fully connected.  With the {3,12} wall set,
// 24/29 levels are; DNG01 is one of them, so assert it here as a real-data
// regression on the wall classification.
static void test_level_connected(const std::string& dir) {
    Dungeon d;
    std::string err;
    if (!load_dungeon(dir + "/DNG01", d, &err)) {
        std::printf("dungeon_test: no DNG01, skipping connectivity check\n");
        return;
    }
    int sx = -1, sy = -1;
    for (int y = 0; y < kDungeonH && sx < 0; y++)
        for (int x = 0; x < kDungeonW; x++)
            if (dungeon_passable(d, x, y)) { sx = x; sy = y; break; }
    assert(sx >= 0);
    int total = 0, hit = 0;
    for (int y = 0; y < kDungeonH; y++)
        for (int x = 0; x < kDungeonW; x++)
            if (dungeon_passable(d, x, y)) {
                total++;
                hit += dungeon_reachable(d, sx, sy, x, y);
            }
    assert(hit == total);   // DNG01 is fully connected under {3,12}
}

// The {3,12} wall set keeps the levels almost fully connected, which a
// hand-built maze should be.  This guards the passability model across ALL
// shipped levels: the mean largest-component fraction must stay high and the
// vast majority of levels must be one connected region.  If a future change to
// dungeon_is_solid_tile() wrongly makes floor tiles solid (or walls passable),
// the levels shear into fragments and this fails.
static void test_connectivity_stats(const std::string& dir) {
    int levels = 0, connected = 0, total_open = 0, total_big = 0;
    for (int dg = 0; dg < 7; dg++) {
        for (int lv = 0; lv < 8; lv++) {
            Dungeon d;
            if (!load_dungeon(dir + "/" + dungeon_file_name(dg, lv), d)) continue;
            levels++;
            // Flood-fill components of the passable graph, 4-neighbour.
            std::vector<int> comp((size_t)kDungeonTiles, -1);
            int biggest = 0, open = 0;
            std::vector<int> stack;
            for (int s = 0; s < kDungeonTiles; s++) {
                int sx = s % kDungeonW, sy = s / kDungeonW;
                if (comp[(size_t)s] >= 0 || !dungeon_passable(d, sx, sy)) continue;
                int n = 0;
                stack.push_back(s);
                comp[(size_t)s] = s;
                while (!stack.empty()) {
                    int c = stack.back(); stack.pop_back();
                    n++;
                    int x = c % kDungeonW, y = c / kDungeonW;
                    const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
                    for (int k = 0; k < 4; k++) {
                        int nx = x + dx[k], ny = y + dy[k];
                        if (!dungeon_passable(d, nx, ny)) continue;
                        int nc = ny * kDungeonW + nx;
                        if (comp[(size_t)nc] >= 0) continue;
                        comp[(size_t)nc] = s;
                        stack.push_back(nc);
                    }
                }
                if (n > biggest) biggest = n;
                open += n;
            }
            total_open += open;
            total_big += biggest;
            if (open > 0 && biggest * 1000 >= open * 999) connected++;
        }
    }
    if (!levels) { std::printf("dungeon_test: no levels, skipping connectivity stats\n"); return; }
    std::printf("dungeon_test: connectivity %d/%d fully connected, "
                "mean largest=%.3f\n", connected, levels,
                total_open ? (double)total_big / total_open : 0.0);
    assert(connected >= levels - 6);            // >= ~80% fully connected
    assert(total_big * 100 >= total_open * 98); // mean largest-component >= 0.98
}

// Object-layer semantics: features sit predominantly on WALL cells that are
// open straight through (classic doorways), not scattered on floor.  Pins the
// "object layer = doors/features" reading for two structurally different
// levels (DNG01 corridor-heavy, DNG17 room-heavy).
static void test_object_doors(const std::string& dir) {
    const char* names[] = {"DNG01", "DNG17"};
    int checked = 0;
    for (const char* nm : names) {
        Dungeon d;
        if (!load_dungeon(dir + "/" + std::string(nm), d)) continue;
        int feat = 0, door = 0;
        for (int y = 0; y < kDungeonH; y++)
            for (int x = 0; x < kDungeonW; x++) {
                if (!d.obj_code(x, y)) continue;
                feat++;
                if (!dungeon_passable(d, x, y)) {   // feature on a wall cell
                    bool h = dungeon_passable(d, x - 1, y) &&
                             dungeon_passable(d, x + 1, y);
                    bool v = dungeon_passable(d, x, y - 1) &&
                             dungeon_passable(d, x, y + 1);
                    if (h || v) door++;
                }
            }
        assert(feat > 0);
        // Most wall features are doorways (open through), not blind stubs.
        assert(door * 2 > feat);
        checked++;
    }
    if (checked) std::printf("dungeon_test: %d levels checked for door features\n", checked);
}

int main(int argc, char** argv) {
    test_synthetic();
    test_render();
    test_reachable();
    test_render_facing();
    test_render_layers();
    test_viewport();
    test_view_sprite((argc > 1) ? argv[1] : "game/DNGS");
    test_walk();
    test_level_connected((argc > 1) ? argv[1] : "game/DNGS");
    std::string dir = (argc > 1) ? argv[1] : "game/DNGS";
    test_real(dir);
    test_object_layout(dir);
    test_file_name(dir);
    test_connectivity_stats(dir);
    test_object_doors(dir);
    std::printf("dungeon_test: ok\n");
    return 0;
}
