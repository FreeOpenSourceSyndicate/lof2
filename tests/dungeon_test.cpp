// Self-test for the DNGS dungeon decoder (workstream A).
//
// The synthetic case is built in memory so the test needs no game data.  When
// game data is present (game/DNGS/DNG00 ...) the decoder is also run against a
// real file and its geometry / a few known cells are asserted.
#include "dungeon.h"
#include "char.h"

#include <cassert>
#include <cstdio>
#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <set>
#include <fstream>
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
    plain[0] = 0; plain[1] = 2;               // flag = 0, entry_facing = 2
    plain[2] = 0x12; plain[3] = 0x34;         // entry_pos = 0x1234 (big-endian)
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

    // Malformed / truncated streams must be rejected, never crash: a lone
    // escape byte, an escape run cut off before its value, and a stream that
    // over-decodes past the fixed 3892-byte geometry.
    std::vector<uint8_t> one = {0xFF};
    assert(!decode_dungeon(one, d, nullptr));
    std::vector<uint8_t> cut = {0xFF, 0xFF, 0x05};        // escape,count,no value
    assert(!decode_dungeon(cut, d, nullptr));
    std::vector<uint8_t> big = {0xFF, 0xFF, 0x20, 0x06};  // escape,count,value
    for (int k = 0; k < 250; k++) {                       // + 250*16 bytes
        big.push_back(0xFF);
        big.push_back(0x10);
        big.push_back(0x06);
    }
    assert(!decode_dungeon(big, d, nullptr));             // decodes too long
}

static void test_real(const std::string& dir) {
    // DNG00 is the only level with a non-uniform border (its outer ring is 12,
    // the interior mostly 0); DNG26 is almost entirely 6.
    struct Expect { const char* name; int facing; uint16_t pos;
                    int x, y, tile; };
    Expect cases[] = {
        {"DNG00", 3, 718, 0, 0, 12},
        {"DNG00", 3, 718, 35, 35, 12},
        {"DNG00", 3, 718, 17, 17, 0},
        {"DNG01", 1, 255, 0, 0, 6},
        {"DNG01", 1, 255, 17, 17, 0},
        {"DNG26", 3, 1091, 0, 0, 6},
        {"DNG26", 3, 1091, 5, 6, 6},
    };
    int checked = 0;
    for (auto& e : cases) {
        std::string path = dir + "/" + e.name;
        Dungeon d;
        std::string err;
        if (!load_dungeon(path, d, &err)) continue;   // no data -> skip
        assert((int)d.tiles.size() == kDungeonTiles);
        assert((int)d.objects.size() == kDungeonTiles);
        assert(d.h1 == e.facing);
        assert(d.field == e.pos);
        assert(d.entry_facing() == e.facing);
        assert(d.entry_x() == e.pos % kDungeonW);
        assert(d.entry_y() == e.pos / kDungeonW);
        assert(d.tile(e.x, e.y) == e.tile);
        checked++;
    }
    // The header entry cell/facing is the party's start: it must equal the save
    // (DNG05 = GAME1 125/2, DNG10 = GAME2 745/0), and the start is not a wall.
    for (auto& e : {std::pair<const char*, int>{"DNG05", 125},
                    std::pair<const char*, int>{"DNG10", 745}}) {
        Dungeon d;
        std::string err;
        if (load_dungeon(dir + "/" + e.first, d, &err)) {
            assert(d.field == e.second);
            assert(d.tile(d.entry_x(), d.entry_y()) != 3 &&
                   d.tile(d.entry_x(), d.entry_y()) != 12);   // start is not wall
            DungeonPos p;
            assert(dungeon_place_entry(d, p));
            assert(p.x == d.entry_x() && p.y == d.entry_y());
            assert(p.facing == d.h1);
        }
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
    d.tiles.assign(kDungeonTiles, 3);       // floor (tile 3 is walkable)
    d.objects.assign(kDungeonTiles, 0);
    d.tiles[0] = 6;                         // wall at (0,0)
    d.tiles[5] = 12;                        // water at (5,0)
    assert(!dungeon_passable(d, 0, 0));
    assert(dungeon_passable(d, 1, 0));
    assert(!dungeon_passable(d, -1, 0));
    assert(!dungeon_passable(d, kDungeonW, 0));

    // ascii: wall symbol for (0,0), floor for the rest of row 0.
    auto lines = dungeon_ascii(d);
    assert((int)lines.size() == kDungeonH);
    assert(lines[0][0] == '%');             // 6  -> '%'
    assert(lines[0][5] == '*');             // 12 -> '*'
    assert(lines[0][1] == '.');             // 3  -> '.'

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
    d.tiles.assign(kDungeonTiles, 3);       // floor
    d.objects.assign(kDungeonTiles, 0);
    // A vertical wall at x=10 splits the level into two rooms.
    for (int y = 0; y < kDungeonH; y++) d.tiles[y * kDungeonW + 10] = 6;
    assert(dungeon_reachable(d, 1, 1, 9, 30));    // same room
    assert(!dungeon_reachable(d, 1, 1, 11, 1));   // across the wall
    assert(dungeon_reachable(d, 11, 1, 35, 35));  // other room
    // Punch a doorway and the two rooms connect.
    d.tiles[5 * kDungeonW + 10] = 3;
    assert(dungeon_reachable(d, 1, 1, 11, 1));
    // A solid tile is never a valid endpoint.
    assert(!dungeon_reachable(d, 1, 1, 10, 1));
    assert(dungeon_is_solid_tile(6) && dungeon_is_solid_tile(13));
    assert(!dungeon_is_solid_tile(0) && !dungeon_is_solid_tile(3));
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
    d.tiles.assign(kDungeonTiles, 3);
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
    d.tiles.assign(kDungeonTiles, 3);       // floor
    d.objects.assign(kDungeonTiles, 0);
    d.tiles[2 * kDungeonW + 3] = 6;         // wall at (3,2)
    d.objects[4 * kDungeonW + 5] = 0x0100;  // door feature at (5,4)
    Screen scr; scr.resize(320, 200); scr.clear(0);
    render_dungeon_map(scr, d, 1, 1, 6);
    int ox = (320 - kDungeonW * 6) / 2, oy = (200 - kDungeonH * 6) / 2;
    auto at = [&](int x, int y) {
        return scr.pixels[(size_t)(oy + y * 6 + 3) * scr.width + ox + x * 6 + 3];
    };
    uint8_t floor_c = at(1, 1), wall_c = at(3, 2), door_c = at(5, 4);
    assert(floor_c != wall_c && wall_c != door_c && floor_c != door_c);
    assert(wall_c == 4);                    // code 6 -> red
    assert(door_c == 14);                   // feature marker
    assert(dungeon_is_solid_tile(6) && dungeon_is_solid_tile(13));
    assert(!dungeon_is_solid_tile(3) && !dungeon_is_solid_tile(0));
    // The engine masks the tile code to its low nibble before the wall test
    // (view routine `and al,0xf`; G-005), so the high nibble (an object/feature
    // class) must not change solidity.
    assert(dungeon_is_solid_tile(0x16) && dungeon_is_solid_tile(0x2d));
    assert(!dungeon_is_solid_tile(0x10) && !dungeon_is_solid_tile(0x0f));
}

// First-person viewport.  A solid wall directly ahead fills the window with
// the flat wall colour (the oracle's Temple spawn view); an open corridor
// exposes the dark floor and the side walls, so the column histogram changes.
static void test_viewport() {
    Dungeon d;
    d.tiles.assign(kDungeonTiles, 3);       // floor everywhere
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
    d.tiles[9 * kDungeonW + 10] = 6;        // wall at (10,9)
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
    d2.tiles.assign(kDungeonTiles, 3);
    d2.objects.assign(kDungeonTiles, 0);
    d2.tiles[9 * kDungeonW + 10] = 6;
    Screen near; near.resize(320, 200);
    render_dungeon_viewport(near, d2, DungeonPos{10, 10, 0});
    d2.tiles[9 * kDungeonW + 10] = 3;
    d2.tiles[6 * kDungeonW + 10] = 6;        // wall at (10,6), 4 cells ahead
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
    // Path helper: the view dir is the map id's ones digit (DNG11 -> EGA/D1,
    // DNG00 -> EGA/D0), matching the engine's `DNGS\DNG%01ld%01ld` + `EGA\D<d>`.
    std::string root = dir + "/..";
    assert(dungeon_view_path(root, 5, 1) == root + "/EGA/D5/DP001");
    assert(dungeon_view_path(root, 0, 12) == root + "/EGA/D0/DP012");
    assert(dungeon_view_path(root, 10, 1) == root + "/EGA/D0/DP001");
    assert(dungeon_view_path(root, 11, 36) == root + "/EGA/D1/DP036");
    assert(dungeon_view_dir(15) == "D5");
    assert(render_dungeon_default_view(scr, root, 5));
    std::printf("dungeon_test: view sprite blit verified (%dx%d)\n", w, img.height);
}

// A map object in the party's own cell is drawn from the UP bank, not the
// dungeon bank.  Measured on DNG10 cell 745: without it the outdoor composite is
// 106 px wrong, with it the window is exact.  The gate is the object record, so
// a cell with no object must not get the sprite (drawing it there measurably
// degrades cells 636/673).  See notes-a.md Round 51.
static void test_outdoor_object(const std::string& dir) {
    namespace fs = std::filesystem;
    std::string root = dir + "/..";
    if (!fs::is_directory(root + "/EGA/UP")) {
        std::printf("dungeon_test: no EGA/UP, skipping object check\n");
        return;
    }
    Dungeon d;
    d.tiles.assign(kDungeonTiles, 0);        // open column, no walls
    d.objects.assign(kDungeonTiles, 0);
    DungeonPos p{25, 20, 0};

    // No object -> base + band pieces only.
    auto n_noobj = dungeon_view_for(root, 10, d, p).overlays.size();
    // An object in the party's own cell -> one extra overlay, from EGA/UP.
    d.objects[20 * kDungeonW + 25] = 0x0100;
    auto v = dungeon_view_for(root, 10, d, p);
    assert(v.overlays.size() == n_noobj + 1);
    const std::string& last = v.overlays.back().path;
    assert(last.size() > 8 && last.substr(last.size() - 8) == "/UP/UP22");
    assert(v.overlays.back().x == kViewportX + 70);
    assert(v.overlays.back().y == kViewportY + 53);

    // A dungeon bank (not D0) never gets the outdoor overlay set at all.
    auto vd = dungeon_view_for(root, 5, d, p);
    for (const auto& o : vd.overlays)
        assert(o.path.find("/UP/UP22") == std::string::npos);
    std::printf("dungeon_test: outdoor object overlay gated on the object record\n");
}

// The seven full-size (120x119) view sprites of a theme, and one duplicate
// pair among them, pinned from the decode work (docs/notes-a.md Round 19).
// Guards the tileset inventory against a bad re-extract.
static void test_view_inventory(const std::string& dir) {
    namespace fs = std::filesystem;
    std::string vd = dir + "/../EGA/D5";
    if (!fs::is_directory(vd)) {
        std::printf("dungeon_test: no D5 dir, skipping inventory check\n");
        return;
    }
    std::vector<std::string> full;
    for (auto& e : fs::directory_iterator(vd)) {
        std::string n = e.path().filename().string();
        if (n.rfind("DP", 0) != 0 || n.size() != 5) continue;
        EgaImage im;
        if (load_ega_image(e.path().string(), im, nullptr) &&
            im.width_bytes * 8 == kViewportW && im.height == kViewportH)
            full.push_back(n);
    }
    std::sort(full.begin(), full.end());
    assert(full.size() == 7);   // DP001 DP014 DP036 DP058 DP059 DP060 DP067
    std::printf("dungeon_test: %zu full-size view sprites in D5\n", full.size());
}

// Every map ships a canonical DP001 full view in its ones-digit view dir;
// blitting it must succeed and cover the whole window (the engine's entry view).
static void test_default_views(const std::string& dir) {
    std::string root = dir + "/..";
    const int ids[] = {0, 1, 2, 3, 4, 5, 6, 7, 10, 11, 15, 17, 23, 66};
    int n = 0;
    for (int id : ids) {
        Screen scr; scr.resize(320, 200); scr.clear(9);
        if (!render_dungeon_default_view(scr, root, id)) continue;
        int drawn = 0;
        for (int y = 0; y < kViewportH; y++)
            for (int x = 0; x < kViewportW; x++)
                if (scr.pixels[(size_t)(kViewportY + y) * scr.width +
                               kViewportX + x] != 9) drawn++;
        assert(drawn > 0);   // something was actually drawn
        n++;
    }
    if (n) std::printf("dungeon_test: %d canonical DP001 views blit ok\n", n);
}

// The dungeon view sprites are shipped as themes: EGA/D1..D7 are seven texture
// themes over ONE identical slot set (same DPnnn indices, different pixels),
// and EGA/D0 is a distinct, larger wilderness set.  The engine's selection rule
// maps (position, facing, structure) to a slot index independent of the theme
// (it ships one contiguous ega\d0\dp000..dp069 table).  Pins that structure.
static void test_view_themes(const std::string& dir) {
    namespace fs = std::filesystem;
    std::string root = dir + "/..";
    std::set<int> theme_slots;
    int themes = 0, wilderness = 0;
    for (int d = 1; d <= 7; d++) {
        std::string vd = root + "/EGA/" + dungeon_view_dir(d);
        if (!fs::is_directory(vd)) continue;
        std::set<int> slots;
        for (auto& e : fs::directory_iterator(vd)) {
            std::string n = e.path().filename().string();
            if (n.rfind("DP", 0) == 0 && n.size() == 5) {
                try { slots.insert(std::stoi(n.substr(2))); } catch (...) {}
            }
        }
        if (slots.empty()) continue;
        if (!theme_slots.empty()) assert(slots == theme_slots);  // themes share slots
        theme_slots = slots;
        themes++;
    }
    std::string wd = root + "/EGA/D0";
    if (fs::is_directory(wd)) {
        for (auto& e : fs::directory_iterator(wd)) {
            std::string n = e.path().filename().string();
            if (n.rfind("DP", 0) == 0 && n.size() == 5) wilderness++;
        }
    }
    if (themes >= 2)
        assert(theme_slots.size() == 51);   // the shared theme slot set
    if (themes && wilderness)
        assert((size_t)wilderness > theme_slots.size());  // D0 is larger
    if (themes)
        std::printf("dungeon_test: %d theme dirs share %zu slots, D0 has %d\n",
                    themes, theme_slots.size(), wilderness);
}

// Full play-screen composition: the static frame (EGA/UP/MSCR) plus the view
// sprite at (10,11).  The composite must equal the same two blits done by hand,
// and must differ from the frame alone (i.e. the window really was overwritten).
static void test_dungeon_screen(const std::string& dir) {
    std::string root = dir + "/..";
    Screen got; got.resize(320, 200); got.clear(9);
    if (!render_dungeon_screen(got, root, 5, 1)) {
        std::printf("dungeon_test: no MSCR/DP001, skipping screen check\n");
        return;
    }
    EgaImage frame, view;
    assert(load_ega_image(root + "/EGA/UP/MSCR", frame, nullptr));
    assert(load_ega_image(root + "/EGA/D5/DP001", view, nullptr));
    Screen ref; ref.resize(320, 200); ref.clear(9);
    blit_image(ref, frame, 0, 0);
    blit_image(ref, view, kViewportX, kViewportY);
    assert(got.pixels == ref.pixels);
    Screen frame_only; frame_only.resize(320, 200); frame_only.clear(9);
    blit_image(frame_only, frame, 0, 0);
    assert(got.pixels != frame_only.pixels);   // the window was replaced
    std::printf("dungeon_test: dungeon screen composite verified\n");
}

// The base first-person view is selected by the tile code of the cell directly
// ahead (see dungeon_base_view_index / docs/notes-a.md Round 24).  Synthetic
// cases pin the measured map; the real DNG05 entry cell pins the two regimes the
// oracle confirms (wall ahead -> DP001, corridor ahead -> DP037).
static void test_base_view(const std::string& dir) {
    Dungeon d;
    d.tiles.assign(kDungeonTiles, 6);
    d.objects.assign(kDungeonTiles, 0);
    DungeonPos p{5, 5, 0};                 // facing N -> ahead is (5,4)

    int ax = -1, ay = -1;
    assert(dungeon_ahead_cell(d, p, ax, ay) && ax == 5 && ay == 4);
    p.facing = 1;                          // E -> (6,5)
    assert(dungeon_ahead_cell(d, p, ax, ay) && ax == 6 && ay == 5);
    p.x = 5; p.facing = 3;                 // W -> (4,5)
    assert(dungeon_ahead_cell(d, p, ax, ay) && ax == 4 && ay == 5);
    p.x = 0;                               // W off-map
    assert(!dungeon_ahead_cell(d, p, ax, ay));

    // Measured tile-code -> base view map (D5 Temple tileset, Round 28).  The
    // base view is selected by the LOW nibble of the ahead tile code; the high
    // nibble (an object/feature class) is ignored.
    p = DungeonPos{5, 5, 0};
    auto view_for = [&](int code) {
        d.tiles[4 * kDungeonW + 5] = code;
        return dungeon_base_view_index(d, p);
    };
    // Every high-nibble variant of a low nibble must give the same view.
    for (int hi = 0; hi <= 0xf0; hi += 0x10) {
        assert(view_for(hi | 0x4) == 58);   // side passage
        assert(view_for(hi | 0x5) == 59);   // side passage variant
        assert(view_for(hi | 0x6) == 60);   // wall face (== DP036)
        assert(view_for(hi | 0xd) == 60);   // wall face variant
        assert(view_for(hi | 0xa) == 14);   // doorway
        // 0x1/0x3/0xb/0xc render the textured wall/room view DP001.
        assert(view_for(hi | 0x1) == 1);
        assert(view_for(hi | 0x3) == 1);
        assert(view_for(hi | 0xb) == 1);
        assert(view_for(hi | 0xc) == 1);
    }
    // The open-cell corridor cases (0x0/0x2/0x7/0x8/0x9) are drawn from the
    // `EGA/CHAR2/CHnn` room bank (Round 38); the room-index -> CHnn table is not
    // decoded yet, so the drop-in falls back to DP001.  Pinned as documented.
    for (int nib : {0x0, 0x2, 0x7, 0x8, 0x9})
        assert(view_for(nib) == 1);

    // Depth model (Round 29): the real engine draws the view from the distance
    // to the first *view-opaque* cell, which is not the movement wall set (tile
    // 3 is passable but view-opaque).  dungeon_base_view_index only sees the
    // ahead cell, so it must stay depth-independent: changing the cell behind
    // the ahead cell must not change its answer.  This pins that it remains the
    // documented approximation rather than silently becoming depth-aware.
    p = DungeonPos{5, 5, 0};
    d.tiles[4 * kDungeonW + 5] = 0;            // ahead open (corridor case)
    int v_ahead_open = dungeon_base_view_index(d, p);
    d.tiles[3 * kDungeonW + 5] = 6;            // depth-2 becomes a wall
    assert(dungeon_base_view_index(d, p) == v_ahead_open);
    d.tiles[3 * kDungeonW + 5] = 0;
    assert(dungeon_base_view_index(d, p) == v_ahead_open);
    // Off-map ahead always falls back to the wall view.
    p = DungeonPos{5, 0, 0};
    assert(dungeon_base_view_index(d, p) == 1);

    // The outdoor bank (D0) has no DP058..DP060, so the passage/wall nibbles
    // fall back.  A wall that *ends the map* draws DP036 instead of DP001:
    // measured on DNG10, cells 37/38 facing N (wall at y=0) score 0.9648 against
    // D0/DP036, while cell 10 facing W (wall with more wall behind it) scores
    // 0.9499 against D0/DP001.  notes-a.md Round 50b.
    {
        Dungeon o;
        o.tiles.assign(kDungeonTiles, 6);
        o.objects.assign(kDungeonTiles, 0);
        DungeonPos q{3, 1, 0};                    // facing N, ahead (3,0)
        assert(dungeon_base_view_index(o, q, false) == 36);
        o.tiles[0 * kDungeonW + 3] = 0x0d;        // the 0xd wall variant too
        assert(dungeon_base_view_index(o, q, false) == 36);
        q = DungeonPos{10, 2, 0};                 // wall at (10,1), map continues
        o.tiles[1 * kDungeonW + 10] = 6;
        assert(dungeon_base_view_index(o, q, false) == 1);
        // The dungeon banks keep the decompiled DP060 for the same nibble.
        assert(dungeon_base_view_index(o, q, true) == 60);
        // A wall at the map edge on any axis triggers DP036.
        q = DungeonPos{0, 20, 3};                 // facing W, ahead (-1,20)
        assert(!dungeon_ahead_cell(o, q, ax, ay));
        o.tiles[20 * kDungeonW + 0] = 6;          // party cell, ahead off-map
        q = DungeonPos{35, 20, 1};                // facing E, ahead (36,20)
        assert(dungeon_base_view_index(o, q, false) == 1);  // off-map -> 1
        o.tiles[20 * kDungeonW + 34] = 6;
        q = DungeonPos{34, 20, 1};                // wall at x=35, behind off-map
        assert(dungeon_base_view_index(o, q, false) == 36);
    }

    // Real DNG05: entry (17,3) facing 2 (S) -> ahead (17,4) is tile 3 -> wall;
    // turned to face N the ahead cell (17,2) is tile 7 -> the corridor case,
    // which has no single sprite and so also falls back to the wall view.
    Dungeon r;
    if (load_dungeon(dir + "/DNG05", r)) {
        DungeonPos e; dungeon_place_entry(r, e);
        assert(e.x == 17 && e.y == 3 && e.facing == 2);
        assert(r.tile(17, 4) == 3 && dungeon_base_view_index(r, e) == 1);
        e.facing = 0;
        assert(r.tile(17, 2) == 7 && dungeon_base_view_index(r, e) == 1);
        // Facing E the ahead cell (18,3) is open room (tile 0) -> wall view too.
        e.facing = 1;
        assert(r.tile(18, 3) == 0 && dungeon_base_view_index(r, e) == 1);
        std::printf("dungeon_test: base-view selection verified (DNG05 entry)\n");
    }

    // render_dungeon_play picks the same sprite render_dungeon_screen would.
    std::string root = dir + "/..";
    Screen a, b; a.resize(320, 200); a.clear(9); b.resize(320, 200); b.clear(9);
    DungeonPos q{5, 5, 0};
    d.tiles[4 * kDungeonW + 5] = 6;
    if (render_dungeon_play(a, root, 5, d, q)) {
        assert(render_dungeon_screen(b, root, 5, 60));
        assert(a.pixels == b.pixels);
    }

    // The outdoor bank (D0) ships no DP058..DP060, so asking for them would name
    // a file that is not on disk.  Ground truth (docs/notes-a.md Round 50): the
    // engine draws DP001 for a wall ahead in D0, not a flat wall face.  The
    // fallback is therefore a bank property, not a filesystem probe, so this
    // stays hermetic on a synthetic dungeon.
    {
        Dungeon w;
        w.tiles.assign(kDungeonW * kDungeonH, 0);
        DungeonPos at{5, 5, 0};
        w.tiles[4 * kDungeonW + 5] = 6;            // wall directly ahead
        assert(dungeon_base_view_index(w, at, /*bank_has_passage_sprites=*/true) == 60);
        assert(dungeon_base_view_index(w, at, /*bank_has_passage_sprites=*/false) == 1);
        w.tiles[4 * kDungeonW + 5] = 4;            // side passage
        assert(dungeon_base_view_index(w, at, true) == 58);
        assert(dungeon_base_view_index(w, at, false) == 1);
        w.tiles[4 * kDungeonW + 5] = 5;            // side passage variant
        assert(dungeon_base_view_index(w, at, true) == 59);
        assert(dungeon_base_view_index(w, at, false) == 1);
        // The nibbles that are bank-independent keep their answer in both banks.
        w.tiles[4 * kDungeonW + 5] = 0xa;          // doorway -> DP014 everywhere
        assert(dungeon_base_view_index(w, at, true) == 14);
        assert(dungeon_base_view_index(w, at, false) == 14);
        w.tiles[4 * kDungeonW + 5] = 3;            // textured wall -> DP001
        assert(dungeon_base_view_index(w, at, true) == 1);
        assert(dungeon_base_view_index(w, at, false) == 1);

        // Every index the D0 path can now return must exist on disk; DP058..060
        // are the ones that do not.
        std::FILE* probe = std::fopen((root + "/EGA/D0/DP060").c_str(), "rb");
        if (probe) {
            std::fclose(probe);
            // Data present: assert the two banks really do differ.
            assert(!std::fopen((root + "/EGA/D0/DP058").c_str(), "rb"));
            assert(std::fopen((root + "/EGA/D1/DP058").c_str(), "rb"));
        }
        std::printf("dungeon_test: outdoor bank falls back for DP058..DP060\n");
    }
}

// `EGA/CHAR<B>/CHnn` is the engine's position-selected encounter/area sprite
// for the first-person window (Rounds 38-45): the engine picks an `SYST/NSC<dd>`
// record from the party's map cell and blits `NSC<dd>[id].+0x48` ->
// `ega\char<B>\ch<NN>`.  Zeroing `CHAR2/CH25` on disk turns the window black, so
// the view is a `CHAR<B>/CHnn` blit, not a procedural composite.  This pins the
// path helper, the number resolver, and that the sprite loads at the viewport.
static void test_room_art(const std::string& dir) {
    std::string root = dir + "/..";
    // The routine branches on the number: < 0x16 selects CHAR3 (which is the
    // bank that actually holds CH00..CH21), >= 0x16 takes the runtime bank
    // (CHAR2 for the DNG05 spawn, CH22..CH74).  Verified: NN=5 opens
    // ega\char3\ch05; NN=25 opens ega\char2\ch25.
    assert(dungeon_room_art_path(root, 25) == root + "/EGA/CHAR2/CH25");
    assert(dungeon_room_art_path(root, 40) == root + "/EGA/CHAR2/CH40");
    assert(dungeon_room_art_path(root, 1) == root + "/EGA/CHAR3/CH01");
    assert(dungeon_room_art_path(root, 7) == root + "/EGA/CHAR3/CH07");
    assert(dungeon_room_art_path(root, 21) == root + "/EGA/CHAR3/CH21");
    assert(dungeon_room_art_path(root, 22) == root + "/EGA/CHAR2/CH22");

    // The measured DNG05 sprite numbers must load and render, and the render
    // must be non-empty (the blit put pixels down) and stable (re-rendering the
    // same sprite gives the same pixels).  CH25 is the Temple spawn's sprite
    // (NSC05 Lizard-warrior record).
    Screen a, b;
    a.resize(320, 200); a.clear(0);
    if (render_dungeon_view(a, dungeon_room_art_path(root, 25),
                            kViewportX, kViewportY)) {
        int ink = 0;
        for (uint8_t v : a.pixels) ink += (v != 0);
        assert(ink > 1000);                 // the corridor art is dense
        b.resize(320, 200); b.clear(0);
        assert(render_dungeon_view(b, dungeon_room_art_path(root, 25),
                                   kViewportX, kViewportY));
        assert(a.pixels == b.pixels);
        std::printf("dungeon_test: room-art view verified (CHAR2/CH25)\n");
    }

    // Round 41 (Task 2b): the sprite number is read from the scenario's
    // NPC/monster table SYST/NSC<dd>, at +0x48 of the record selected by the
    // party's location id.  The Temple spawn (scenario 05, location 16) must
    // resolve to 25, and its record's neighbours to the values verified against
    // the emulator's open trace (records 0..9 Lizard priest -> 26, records
    // 10..17 Lizard warrior -> 25).
    assert(dungeon_room_art_number(root, 5, 16) == 25);
    assert(dungeon_room_art_number(root, 5, 0) == 26);
    assert(dungeon_room_art_number(root, 5, 10) == 25);
    assert(dungeon_room_art_number(root, 5, 35) == 40);   // Ogre
    // Out-of-range indices are rejected, not read past the record table.
    assert(dungeon_room_art_number(root, 5, 68) == -1);
    assert(dungeon_room_art_number(root, 5, 9999) == -1);
    assert(dungeon_room_art_number(root, 5, -1) == -1);
    // A missing scenario file is reported, not guessed.
    assert(dungeon_room_art_number(root, 99, 0) == -1);

    // The convenience resolver maps (scenario, location) straight to the sprite
    // path the play view blits, applying the bank rule.
    assert(dungeon_encounter_sprite(root, 5, 16) == root + "/EGA/CHAR2/CH25");
    assert(dungeon_encounter_sprite(root, 5, 0) == root + "/EGA/CHAR2/CH26");
    assert(dungeon_encounter_sprite(root, 5, 18) == root + "/EGA/CHAR3/CH05");
    assert(dungeon_encounter_sprite(root, 99, 0).empty());
    assert(dungeon_encounter_sprite(root, 5, 9999).empty());
    std::printf("dungeon_test: room-art number from SYST/NSC05 verified\n");
}

static void test_walk() {
    Dungeon d;
    d.tiles.assign(kDungeonTiles, 3);       // floor
    d.objects.assign(kDungeonTiles, 0);
    // A wall directly north of (5,5); open everywhere else.
    d.tiles[4 * kDungeonW + 5] = 6;

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

// A hand-built maze should be fully connected.  DNG01 is one connected room
// under the {6,13} wall set, so assert it here as a real-data regression on the
// wall classification.
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
    assert(hit == total);   // DNG01 is one connected room under {6,13}
}

// The {6,13} wall set is the emulator-measured opacity ground truth (Round 27):
// tiles 6/13 occlude the cell behind them, all others are see-through.  Under
// it many levels are NOT one connected region — they are several rooms (DNG17
// is four).  The guard is therefore that the model does not SHATTER the levels:
// the largest component must dominate each level and most levels must still be
// a single room.  A wrong wall set (e.g. marking the floor tile 0 solid) turns
// every level into hundreds of fragments and this fails.
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
    assert(connected >= levels - 19);           // >= ~34% fully connected (measured 11/29)
    assert(total_big * 100 >= total_open * 93); // mean largest-component >= 0.93
}

// Object-layer semantics: the object/feature layer marks special cells (doors,
// items, stairs) with a non-zero code.  Under the {6,13} wall set the features
// sit on PASSABLE cells (they mark a doorway/object on a walkable square), not
// on the wall tiles.  Pins the "object layer = features on walkable cells"
// reading for two structurally different levels (DNG01, DNG17).
static void test_object_doors(const std::string& dir) {
    const char* names[] = {"DNG01", "DNG17"};
    int checked = 0;
    for (const char* nm : names) {
        Dungeon d;
        if (!load_dungeon(dir + "/" + std::string(nm), d)) continue;
        int feat = 0, on_wall = 0;
        for (int y = 0; y < kDungeonH; y++)
            for (int x = 0; x < kDungeonW; x++) {
                if (!d.obj_code(x, y)) continue;
                feat++;
                if (!dungeon_passable(d, x, y)) on_wall++;
            }
        assert(feat > 0);
        // Features mark walkable cells; almost none sit inside a wall tile.
        assert(on_wall * 10 < feat);
        checked++;
    }
    if (checked) std::printf("dungeon_test: %d levels checked for door features\n", checked);
}

// The object word -> SPEC record link.  The engine does not index SPEC by the
// raw object code: it collects the level's *distinct* object words from cells
// whose tile code is exactly 3 and whose word exceeds 0x0a, in row-major order
// (capped at 70), and the word's position in that table is the SPEC record
// index.  Ground truth: image 0x1F0C0 (build), 0x1F46E (lookup), 0x1F520
// (index * 0xee).  The consequence that makes this checkable is that the table
// can never be longer than the level's SPEC record count -- an assertion that
// fails loudly for every level if the rule is wrong.
static void test_event_keys(const std::string& dir) {
    const char* names[] = {"DNG00", "DNG01", "DNG02", "DNG03",
                           "DNG04", "DNG05", "DNG06", "DNG07"};
    int checked = 0;
    for (const char* nm : names) {
        Dungeon d;
        if (!load_dungeon(dir + "/" + std::string(nm), d)) continue;
        const std::vector<uint16_t>& keys = d.event_keys();
        assert((int)keys.size() <= kEventKeyMax);
        // Distinct by construction.
        std::set<uint16_t> uniq(keys.begin(), keys.end());
        assert(uniq.size() == keys.size());
        // Every key is a real object word on a tile-3 cell.
        for (uint16_t w : keys) {
            assert(w > kEventMinWord);
            bool found = false;
            for (int y = 0; y < kDungeonH && !found; y++)
                for (int x = 0; x < kDungeonW && !found; x++)
                    if (d.is_event_cell(x, y) && d.object(x, y) == w) found = true;
            assert(found);
        }
        // `event_index` agrees with the table for every event cell, and a cell
        // that is not an event cell reports -1.
        for (int y = 0; y < kDungeonH; y++)
            for (int x = 0; x < kDungeonW; x++) {
                int idx = d.event_index(x, y);
                if (!d.is_event_cell(x, y)) { assert(idx == -1); continue; }
                assert(idx >= 0 && idx < (int)keys.size());
                assert(keys[(size_t)idx] == d.object(x, y));
            }
        // The trigger guard, as the engine writes it (0x2E664 tile test, 0x2E679
        // word test): the post-move handler *calls* the dispatcher when the
        // party's cell has tile == 3 and its word is not 0x0a, and an event only
        // *fires* if the dispatcher's table lookup then succeeds (word > 0x0a).
        // So the number of cells where `event_index() >= 0` must equal the number
        // of cells the engine's guard admits, and both must equal the number of
        // tile-3 cells on these levels (the word test never excludes any).  Pin
        // the counts: they change if the trigger rule regresses.
        int fires = 0, guarded = 0, tile3 = 0;
        for (int y = 0; y < kDungeonH; y++)
            for (int x = 0; x < kDungeonW; x++) {
                if (d.tile(x, y) == kEventTile) tile3++;
                if (d.tile(x, y) == kEventTile && d.object(x, y) != 0x0a) guarded++;
                if (d.event_index(x, y) >= 0) fires++;
            }
        assert(fires == guarded);
        assert(tile3 == guarded);       // every tile-3 cell clears the word test
        static const int kExpectedCells[] = {78, 46, 22, 50, 53, 74, 95, 100};
        assert(fires == kExpectedCells[checked]);
        // The decisive check: the table indexes SPEC records, so it can never
        // hold more keys than the level has records.  SPECn's first byte is the
        // record count (238-byte records follow).
        //
        // `nm + 3` is the two-digit level id; SPEC is selected by its **ones**
        // digit (see char.h `spec_scenario_for_location`).  This used to be a
        // silent no-op: it built "SPEC00" from both digits, the file never
        // existed, and the `if (fopen)` guard swallowed the whole check.
        const int level = (nm[3] - '0') * 10 + (nm[4] - '0');
        std::string spec_file = dir + "/../SYST/SPEC" +
                                std::to_string(spec_scenario_for_location(level));
        FILE* f = std::fopen(spec_file.c_str(), "rb");
        assert(f && "SPEC file for this level must exist");
        int nrec = std::fgetc(f);
        std::fclose(f);
        assert(nrec > 0);
        assert((int)keys.size() <= nrec);
        // Pin the table size per level.  These counts are the rule's signature:
        // dropping the tile-3 filter gives 19/32/22/24/42/39/47/18 instead, so
        // this assertion fails if the rule regresses.
        static const int kExpected[] = {13, 28, 12, 17, 35, 32, 44, 14};
        assert(keys.size() == (size_t)kExpected[checked]);
        checked++;
    }
    if (checked) std::printf("dungeon_test: %d levels checked for event keys\n", checked);
}

// Every shipped level's event table must fit the SPEC file the engine actually
// loads for it.  This is the whole-table form of the mapping rule, and it is
// what distinguishes the two readings of `DNG<dd>`:
//
//   ones digit  -> SPEC<ones>   : 29/29 levels fit (largest index < record count)
//   tens digit  -> SPEC<tens>   : 5 levels overflow (DNG01/04/05/06 need indexes
//                                 27/34/31/43 but SPEC0 has 24 records)
//
// The engine's own trace pins which one it is: for DNG10 it opens SYST/SPEC0.
// Pin the aggregate too, so a rule change has to break something visible.
static void test_event_spec_mapping(const std::string& dir) {
    namespace fs = std::filesystem;
    int levels = 0, fit = 0, overflow = 0;
    for (int tens = 0; tens < 8; tens++) {
        for (int ones = 0; ones < 8; ones++) {
            const int level = tens * 10 + ones;
            char nm[16];
            std::snprintf(nm, sizeof nm, "DNG%02d", level);
            const std::string dpath = dir + "/" + nm;
            if (!fs::exists(dpath)) continue;
            Dungeon d;
            if (!load_dungeon(dpath, d)) continue;
            levels++;

            auto rec_count = [&](int scenario) {
                const std::string p = spec_path(dir + "/..", scenario);
                FILE* f = std::fopen(p.c_str(), "rb");
                if (!f) return -1;
                int n = std::fgetc(f);
                std::fclose(f);
                return n;
            };

            int max_idx = -1;
            for (int y = 0; y < kDungeonH; y++)
                for (int x = 0; x < kDungeonW; x++)
                    max_idx = std::max(max_idx, d.event_index(x, y));

            // The rule the engine uses: ones digit.
            const int scen = spec_scenario_for_location(level);
            assert(scen == ones);
            const int nrec = rec_count(scen);
            assert(nrec > 0 && "every level's SPEC file must exist");
            if (max_idx >= 0) {
                assert(max_idx < nrec);
                fit++;
            }
            // The rejected reading must actually be rejected for at least the
            // levels where it overflows, i.e. this is not a vacuous distinction.
            const int alt = rec_count(tens);
            if (alt > 0 && max_idx >= alt) overflow++;
        }
    }
    assert(levels == 29);
    assert(fit == levels);
    assert(overflow >= 4 && "the tens-digit reading must fail on real levels");
    std::printf("dungeon_test: SPEC mapping ok (%d levels fit, %d overflow "
                "under the tens-digit reading)\n", fit, overflow);
}

// The recruit event a player actually meets, resolved end to end.
//
// A player reported: leaving the start and walking forward reaches "A large
// warrior asks if he may join you (Yes/No)."; yes -> "Siegurd joins the
// companionship.", no -> "He goes, disappointed.".  That fixes the meaning of
// type 1 and gives a record to test the selector against.
//
// The selector is the word's *index* in the level's event table, not the word
// itself: the table-build loop (0x1F0C2) appends a word only when it is not
// already present and writes it to a slot from a monotonic counter, so the
// first-occurrence position is the record number.  DNG10 -- the level the
// shipped saves start in -- holds 0x2100 at index 1, and SPEC0[1] is the
// warrior record.  Asserting the text, not just the index, keeps this honest:
// if the mapping drifts the strings stop matching.
static void test_recruit_record(const std::string& dir) {
    Dungeon d;
    std::string err;
    if (!load_dungeon(dir + "/DNG10", d, &err)) {
        std::printf("dungeon_test: skipping recruit check (%s)\n", err.c_str());
        return;
    }
    const std::vector<uint16_t>& keys = d.event_keys();
    assert(keys.size() > 1);
    assert(keys[1] == 0x2100 && "DNG10 key-table slot 1 changed");

    // `dir` is the DNGS directory; SPEC lives beside it under SYST.
    SpecTable spec = load_spec(spec_path(dir + "/..", 10));
    assert(spec.ok);
    assert(spec.records.size() > 1);
    const SpecRecord& r = spec.records[1];
    assert(r.type == 1 && "slot 1 of SPEC0 must be the recruit event");
    assert(r.text_b.find("large warrior") != std::string::npos);
    assert(r.text_b.find("Yes/No") != std::string::npos);
    assert(r.text_a.find("disappointed") != std::string::npos);

    // The cell that resolves to slot 1 must actually exist on the level, so the
    // index is reachable in play rather than a table entry with no cell.
    int cells = 0;
    for (int y = 0; y < kDungeonH; y++)
        for (int x = 0; x < kDungeonW; x++)
            if (d.event_index(x, y) == 1) cells++;
    assert(cells >= 1 && "no cell resolves to the recruit record");
    std::printf("dungeon_test: recruit record ok (%d cells -> SPEC0[1] "
                "\"A large warrior...\")\n", cells);
}

// The recruit record names the joining character through `class_eb` (+0xeb).
//
// The reader at 0x1F85B scales that byte by 0x4e -- the SNSC record stride --
// and indexes the buffer at [0x37b0], which the loader at 0x1B150 fills by
// allocating 0x444 (1092 = 14 * 78, the whole SYST/SNSC file) and reading
// "SYST\SNSC" into it.  So the field is an SNSC class index, and the class's
// name is the character who joins.
//
// The player's report is the oracle: the warrior they met joins as "Siegurd",
// and SPEC0[1] is the only recruit record with class_eb == 0 while SNSC[0] is
// "Siegurd".  Asserting the name (not just the index) keeps this honest.
static void test_recruit_class(const std::string& dir) {
    const std::string syst = dir + "/..";
    SpecTable spec = load_spec(spec_path(syst, 10));
    std::vector<SnsRecord> classes = load_snsc(syst + "/SYST/SNSC");
    if (!spec.ok || spec.records.size() < 2 || classes.size() < 1) {
        std::printf("dungeon_test: skipping recruit-class check (no data)\n");
        return;
    }
    const SpecRecord& r = spec.records[1];
    assert(r.type == 1);
    assert(r.has_class() && "the warrior record must name a class");
    assert(r.class_eb < classes.size());
    assert(classes[(size_t)r.class_eb].singular == "Siegurd" &&
           "SPEC0[1] must recruit SNSC[0], which is Siegurd");

    // A class index that is out of range must be reported as absent, not read
    // past the table: the records that recruit nobody store 0xff (== -1).
    int none = 0, named = 0;
    for (const SpecRecord& rec : spec.records) {
        if (rec.type != 1) continue;
        if (!rec.has_class()) { none++; continue; }
        named++;
        assert(rec.class_eb < classes.size());
    }
    assert(named >= 1 && none >= 1 &&
           "SPEC0 should hold both recruiting and non-recruiting type-1 records");
    std::printf("dungeon_test: recruit class ok (SPEC0[1] -> SNSC[%u] \"%s\")\n",
                (unsigned)r.class_eb, classes[(size_t)r.class_eb].singular.c_str());
}

// The dispatch table does *not* hold per-type handlers.
//
// `record+0xe2` (the type) is turned into an index by `sub ax,7` and used at
// 0x1F5B8 as `jmp word cs:[bx+0x25ea]`.  The 17 words live at image 0x1F5CA
// (`cs = 0x1CFE`, the segment of the `jmp` itself), and they contain exactly two
// distinct IPs, 0x25DE and 0x25E4 -- the two arms that set the local flag
// `[bp-8]` to 0 and 1 respectively.  There are no per-type routines.
//
// Reading the table at `0x25EA` *within the image* (which an earlier round did)
// lands at 0x21B9A: that is code, `push cx; push ax; lcall ...`, and the
// "handlers" it yields are mid-instruction.  Both traps are asserted here.
static void test_dispatch_table(const std::string& dir) {
    const std::string exe = dir + "/../LOF.EXE";
    std::ifstream in(exe, std::ios::binary);
    if (!in) {
        std::printf("dungeon_test: skipping dispatch-table check (no LOF.EXE)\n");
        return;
    }
    std::vector<uint8_t> file((std::istreambuf_iterator<char>(in)),
                              std::istreambuf_iterator<char>());
    if (file.size() < 0x40) {
        std::printf("dungeon_test: skipping dispatch-table check (short EXE)\n");
        return;
    }
    const size_t hdr = (size_t)(file[8] | (file[9] << 8)) * 16;  // MZ header
    auto img = [&](size_t o) { return file[hdr + o]; };
    auto rd16 = [&](size_t o) {   // a word at an *image* offset
        return (uint16_t)(file[hdr + o] | (file[hdr + o + 1] << 8));
    };

    // The `jmp word cs:[bx+0x25ea]` sits at image 0x1F5B8; its own segment is
    // what `cs:` resolves to, and `target()` (tools/lofdis.py, LOAD_SEG=0x1010)
    // gives 0x1CFE for it.  The table is therefore at 0x1CFE*16 + 0x25EA.
    const size_t kTable = 0x1CFE * 16 + 0x25EA;
    assert(kTable == 0x1F5CA);
    assert(img(0x1F5B8) == 0x2E && img(0x1F5B9) == 0xFF &&
           "0x1F5B8 must be `jmp word cs:[bx+disp16]` (2E FF A7 ...)");
    assert(rd16(0x1F5BB) == 0x25EA && "the jmp's displacement must be 0x25EA");

    // Every entry resolves to one of exactly two arms.
    const uint16_t kArmZero = 0x25DE, kArmOne = 0x25E4;
    for (int i = 0; i < 17; i++) {
        const uint16_t ip = rd16(kTable + (size_t)i * 2);
        assert((ip == kArmZero || ip == kArmOne) &&
               "a dispatch entry must be one of the two flag arms");
    }
    // Arm 0 stores 0, arm 1 stores 1, and both rejoin the shared tail.
    const size_t kZero = 0x1CFE * 16 + kArmZero;   // 0x1F5BE
    const size_t kOne = 0x1CFE * 16 + kArmOne;     // 0x1F5C4
    assert(kZero == 0x1F5BE && kOne == 0x1F5C4);
    assert(img(kZero) == 0xC6 && img(kZero + 1) == 0x46 && img(kZero + 2) == 0xF8 &&
           "arm 0 must be `mov byte [bp-8], imm8`");
    assert(img(kZero + 3) == 0x00 && "arm 0 must store 0");
    assert(img(kOne) == 0xC6 && img(kOne + 1) == 0x46 && img(kOne + 2) == 0xF8 &&
           "arm 1 must be `mov byte [bp-8], imm8`");
    assert(img(kOne + 3) == 0x01 && "arm 1 must store 1");

    // Trap 1: 0x21B9A (the table read image-relative) is code, not a table.
    assert(img(0x21B9A) == 0x51 && img(0x21B9B) == 0x50 &&
           "0x21B9A must be `push cx; push ax`, i.e. code");

    // Trap 2: which types land on which arm.  This is the flag the once-only
    // gate tests, so pin the mapping rather than trusting the derivation.
    auto arm_for = [&](int type) -> int {
        const int i = type - 7;
        if (i < 0 || i > 16) return 1;   // the unsigned `ja` path stores 1
        return rd16(kTable + (size_t)i * 2) == kArmZero ? 0 : 1;
    };
    for (int t : {7, 8, 11, 12, 13, 15, 18, 23}) assert(arm_for(t) == 0);
    for (int t : {9, 10, 14, 16, 17, 19, 20, 21, 22}) assert(arm_for(t) == 1);
    for (int t = 0; t <= 6; t++) assert(arm_for(t) == 1);

    std::printf("dungeon_test: dispatch table ok (17 words at 0x1F5CA, two arms, "
                "no per-type handlers)\n");
}

// `record+0x0e` (`text_a`) is an *outcome* line, not a second prompt.
//
// The engine prints `text_b` (+0x50) as the prompt, reads an answer and compares
// it case-insensitively against the record's keyword (+0x00).  `+0x0e` is
// printed afterwards, on both the match and the mismatch branch, with `+0x4b`
// gating each branch separately:
//
//   match    (0x20A0E je 0x20A72): +0x4b == 0 and +0x0e non-empty -> print
//   mismatch (0x20A42 je 0x20A47): +0x4b == 0 and +0x0e non-empty -> print
//
// so +0x4b == 0 means "both answers get a line", and +0x4b == 1 means neither
// does.  The proof that this is an outcome line and not a prompt is in the data:
// every record carrying `text_a` but no `text_b` is a pure outcome -- "Wrong.",
// "The door opens with a creak.", "Pity, I'd like to have known how he is." --
// and SPEC0[1].text_a is "He goes, disappointed.", the documented ground truth
// for declining the warrior.
//
// What is NOT proven here: that this line is specifically the *decline* line.
// The recruit's accept path (TXT0 speech + "joins the companionship.", string at
// file offset 0x4a2ff) is not yet traced, so the port must not claim it.
static void test_answer_text(const std::string& dir) {
    const std::string syst = dir + "/..";
    SpecTable spec = load_spec(spec_path(syst, 10));
    if (!spec.ok || spec.records.size() < 2) {
        std::printf("dungeon_test: skipping answer-text check (no data)\n");
        return;
    }
    const SpecRecord& recruit = spec.records[1];
    assert(recruit.type == 1);
    assert(recruit.text_a == "He goes, disappointed." &&
           "SPEC0[1].text_a must be the decline line");
    assert(recruit.text_b.find("asks if he may join you") != std::string::npos);

    // The prompt is +0x50 and the answer is +0x0e across the whole release, so
    // an "answer-only" record exists and never carries a prompt.
    int answer_only = 0;
    for (int scenario = 0; scenario < 10; scenario++) {
        SpecTable t = load_spec(spec_path(syst, scenario));
        if (!t.ok) continue;
        for (const SpecRecord& r : t.records)
            if (!r.text_a.empty() && r.text_b.empty()) answer_only++;
    }
    assert(answer_only > 0 && "the release must contain answer-only records");
    std::printf("dungeon_test: answer text ok (SPEC0[1].text_a = \"%s\")\n",
                recruit.text_a.c_str());
}

// The answer branches, as a tested model.
//
// The dispatcher compares the typed answer with the record keyword
// case-insensitively and takes one of two branches.  Getting the polarity wrong
// silently swaps "the effect fires" with "it does not", which is the kind of bug
// that looks like working code, so the model is asserted directly.
static void test_answer_branches(const std::string& dir) {
    SpecRecord r;
    r.keyword = "Y";
    r.text_a = "He goes, disappointed.";
    r.text_b = "A large warrior asks if he may join you (Yes/No).";
    r.flag_4b = 0;

    // Empty keyword: the record never reads an answer and never prints +0x0e,
    // but it goes straight to the type switch (0x209CE: jmp 0x20AB8), so its
    // per-type effect *does* fire.
    SpecRecord silent = r;
    silent.keyword.clear();
    assert(classify_answer(silent, "Y") == AnswerEffect::NoAnswerWanted);
    assert(!answer_prints_outcome(silent, classify_answer(silent, "Y")));
    assert(answer_fires_effect(classify_answer(silent, "Y")));

    // Match: the effect fires.  Mismatch: it must not.
    assert(classify_answer(r, "Y") == AnswerEffect::Match);
    assert(answer_fires_effect(classify_answer(r, "Y")));
    assert(classify_answer(r, "N") == AnswerEffect::Mismatch);
    assert(!answer_fires_effect(classify_answer(r, "N")));

    // The compare is case-insensitive, which is what makes the ground truth
    // (a player typing lowercase 'y' to the warrior) work.
    assert(classify_answer(r, "y") == AnswerEffect::Match);
    assert(keyword_matches("Y", "y"));
    assert(keyword_matches("Fire", "fIrE"));
    assert(!keyword_matches("Y", "yes") && "prefixes must not match");
    assert(!keyword_matches("Y", ""));
    assert(!keyword_matches("", "Y"));

    // +0x4b == 0: both branches print the outcome line.
    assert(answer_prints_outcome(r, AnswerEffect::Match));
    assert(answer_prints_outcome(r, AnswerEffect::Mismatch));
    // +0x4b == 1: neither branch does.
    SpecRecord quiet = r;
    quiet.flag_4b = 1;
    assert(!answer_prints_outcome(quiet, AnswerEffect::Match));
    assert(!answer_prints_outcome(quiet, AnswerEffect::Mismatch));
    // An empty +0x0e prints nothing either way.
    SpecRecord no_line = r;
    no_line.text_a.clear();
    assert(!answer_prints_outcome(no_line, AnswerEffect::Match));

    // Against the shipped record: the warrior wants "Y" and its outcome line is
    // the documented decline text.
    SpecTable spec = load_spec(spec_path(dir + "/..", 10));
    if (spec.ok && spec.records.size() > 1) {
        const SpecRecord& warrior = spec.records[1];
        assert(classify_answer(warrior, "y") == AnswerEffect::Match);
        assert(classify_answer(warrior, "n") == AnswerEffect::Mismatch);
        assert(answer_prints_outcome(warrior, AnswerEffect::Mismatch));
        assert(warrior.text_a == "He goes, disappointed.");
    }
    std::printf("dungeon_test: answer branches ok (match fires the effect, "
                "mismatch does not, +0x4b gates the outcome line)\n");
}

// `+0xeb` is an SNSC class index, but the block that reads it at 0x1F85B is
// dead code, and SPEC0[1] does not go through it.
//
//   1F85B: cmp byte es:[bx+0xea], 0
//   1F861: jge 0x1F88F          ; +0xea >= 0  -> SKIP the whole +0xeb reader
//   1F863: cmp byte es:[bx+0xeb], 0
//   1F869: jl  0x1F88F          ; +0xeb < 0   -> SKIP it too
//   1F86D: imul byte es:[bx+0xeb]  <- only reached when +0xea < 0 <= +0xeb
//
// SPEC0[1] (the large warrior) has +0xea == 42, so it takes the first branch and
// never touches +0xeb there.  Across the whole release no record satisfies
// `+0xea < 0 && +0xeb >= 0`, so the 0x1F86D body cannot execute at all.  An
// earlier round read 0x1F85B as "the recruit reader" and derived "Siegurd" from
// it; that inference is unsound.
//
// The *live* +0xeb reader is at 0x20F02, in the join path: it scales +0xeb by
// 0x4e (the SNSC stride), indexes the SNSC image at [0x37b0] and calls
// 0x1D99A, which zeroes a 0x19a-byte character slot and copies the record in.
// So +0xeb is a class index -- that part of the earlier finding holds -- but it
// is consumed by the join routine, not by the 0x1F85B test.
static void test_recruit_field_guard(const std::string& dir) {
    const std::string syst = dir + "/..";
    SpecTable spec = load_spec(spec_path(syst, 10));
    if (!spec.ok || spec.records.size() < 2) {
        std::printf("dungeon_test: skipping recruit-guard check (no data)\n");
        return;
    }
    // The guard is on a *signed* byte: SPEC0[1].+0xea is 42, not negative.
    const uint8_t ea = (uint8_t)spec.records[1].raw[0xea];
    assert((int8_t)ea >= 0 &&
           "SPEC0[1].+0xea must be >= 0, i.e. it skips the 0x1F85B reader");
    assert((int8_t)ea == 42 && "SPEC0[1].+0xea is 42");

    // No record in the release can reach 0x1F86D: it needs +0xea < 0 <= +0xeb.
    int reachable = 0, total = 0;
    for (int scenario = 0; scenario < 10; scenario++) {
        SpecTable t = load_spec(spec_path(syst, scenario));
        if (!t.ok) continue;
        for (const SpecRecord& r : t.records) {
            total++;
            const int ea_s = (int8_t)(uint8_t)r.raw[0xea];
            const int eb_s = (int8_t)(uint8_t)r.raw[0xeb];
            if (ea_s < 0 && eb_s >= 0) reachable++;
        }
    }
    assert(total == 665 && "the release has 665 SPEC records");
    assert(reachable == 0 &&
           "no record may reach 0x1F86D; the +0xeb reader there is dead code");
    std::printf("dungeon_test: recruit guard ok (SPEC0[1].+0xea=%d, 0/%d records "
                "reach the 0x1F85B +0xeb reader)\n", (int)(int8_t)ea, total);
}

// The long text block a record can name: `index_c9` (+0xc9, 0xff = none).
//
// The field is an entry index into `SYST/TXT<n>`, where <n> is the *same
// scenario number as the SPEC file*, not the index itself.  The open helper at
// 0x1D7C2 formats "SYST\TXT%d" from the scenario number and then seeks
// `4 * index_c9` into the offset table.  Reading the field as the file number
// is the natural mistake (it is what this port did first) and it "works" for the
// one record where the two coincide, so the check is that every index resolves
// in its own scenario's TXT and that at least one does *not* resolve in another.
//
// An entry is a run of NUL-separated lines; an empty line is a paragraph break,
// not the end of the entry.  The recruit record is the useful witness: SPEC0[1]
// carries the warrior's speech ("My journey will take me to Cyldane...") as its
// block, which is the text a player sees after answering the question.
static void test_text_block_index(const std::string& dir) {
    // `dir` is the DNGS directory; SYST sits beside it.
    const std::string syst = dir + "/..";
    SpecTable spec = load_spec(spec_path(syst, 10));
    if (!spec.ok || spec.records.size() < 2) {
        std::printf("dungeon_test: skipping text-block check (no SPEC0)\n");
        return;
    }
    const SpecRecord& recruit = spec.records[1];
    assert(recruit.has_index() && "the recruit record should name a text block");

    std::vector<std::string> own = load_txt(syst + "/SYST/TXT0");
    assert(!own.empty());
    assert(recruit.index_c9 < own.size() &&
           "index_c9 must resolve inside its own scenario's TXT file");

    // The block is the warrior's speech, and it is multi-line: reading the
    // whole entry (not just up to the first NUL) is what makes it readable.
    const std::string& body = own[(size_t)recruit.index_c9];
    assert(body.find("Cyldane") != std::string::npos &&
           "SPEC0[1]'s text block should be the warrior's speech");
    assert(body.find('\0') != std::string::npos &&
           "the block must span more than one NUL-separated line");

    // Reading index_c9 as the *file* number must not be silently acceptable:
    // SPEC0's indices reach 12 and only TXT0..TXT7 exist.
    bool some_index_exceeds_files = false;
    for (const SpecRecord& r : spec.records)
        if (r.has_index() && r.index_c9 > 7) some_index_exceeds_files = true;
    assert(some_index_exceeds_files &&
           "SPEC0 should have an index that is out of range as a file number, "
           "which is what pins the field to be an entry index");
    std::printf("dungeon_test: text block ok (index_c9=%u -> TXT0 entry, "
                "%zu lines)\n", (unsigned)recruit.index_c9,
                (size_t)std::count(body.begin(), body.end(), '\0') + 1);
}

// End-to-end walk model over a real level: place the party, then walk and turn
// with the real decoder output and assert the invariants the play loop relies
// on (never inside a solid cell, a successful step always changes the cell, a
// turn only changes facing, facing deltas agree with the N/E/S/W table).
static void test_walk_real(const std::string& dir) {
    Dungeon d;
    std::string err;
    if (!load_dungeon(dir + "/" + dungeon_file_name(0, 5), d, &err)) {
        std::printf("dungeon_test: no DNG05, skipping real-walk check\n");
        return;
    }
    DungeonPos p;
    assert(dungeon_place(d, p, kDungeonW / 2, kDungeonH / 2));
    assert(dungeon_passable(d, p.x, p.y));
    int steps = 0, turns = 0;
    for (int i = 0; i < 2000; i++) {
        if (dungeon_can_step(d, p)) {
            int ox = p.x, oy = p.y;
            assert(dungeon_step(d, p));
            assert(p.x != ox || p.y != oy);            // a step moves
            assert(p.x == ox + dungeon_facing_dx(p.facing));
            assert(p.y == oy + dungeon_facing_dy(p.facing));
            assert(dungeon_passable(d, p.x, p.y));     // never in a wall
            steps++;
            if (steps > 300) break;
        } else {
            dungeon_turn(p, true);                     // rotate until open
            assert(p.facing >= 0 && p.facing <= 3);
            if (++turns % 4 == 0) break;               // boxed in: stop
        }
    }
    // A naive straight-then-turn walker gets boxed into a room quickly in a
    // real maze (it does not backtrack), so only assert it can walk at all —
    // the value here is the per-step invariants checked above.
    assert(steps > 15);                                // the level is walkable
    std::printf("dungeon_test: real walk %d steps, %d turns from (%d,%d)\n",
                steps, turns, p.x, p.y);
}

// The first-person view is selected by marching the column of cells ahead of
// the party to the first *view-opaque* cell (dungeon_view_opaque /
// dungeon_view_for, docs/notes-a.md Round 47).  This pins the depth model on
// synthetic maps and on the real DNG05 entry column (which the DOSBox oracle
// confirms: spawn -> DP001, after a step -> CHAR2/CH25 at the origin).
static void test_view_depth(const std::string& dir) {
    // View-opacity is by LOW nibble and is NOT the movement wall set: 3 is
    // walkable but view-opaque, and 0/2/7/8/9 are open.
    for (int hi = 0; hi <= 0xf0; hi += 0x10) {
        assert(!dungeon_view_opaque(hi | 0x0));
        assert(!dungeon_view_opaque(hi | 0x2));
        assert(!dungeon_view_opaque(hi | 0x7));
        assert(!dungeon_view_opaque(hi | 0x8));
        assert(!dungeon_view_opaque(hi | 0x9));
        assert(dungeon_view_opaque(hi | 0x1));
        assert(dungeon_view_opaque(hi | 0x3));    // passable but opaque
        assert(dungeon_view_opaque(hi | 0x6));
        assert(dungeon_view_opaque(hi | 0xa));
        assert(dungeon_view_opaque(hi | 0xd));
    }

    // Synthetic corridor: party at (5,5) facing S.  The ahead cells (5,6),
    // (5,7), ... are open until one is made opaque; the depth picks the sprite
    // and the depth-1 case falls back to the tileset DPnnn path.
    Dungeon d;
    d.tiles.assign(kDungeonTiles, 0);
    d.objects.assign(kDungeonTiles, 0);
    DungeonPos p{5, 5, 2};
    auto view = [&]() { return dungeon_view_for("/root", 5, d, p); };

    // Wall at depth 1 -> a D5/DPnnn tileset sprite at the viewport origin.
    d.tiles[6 * kDungeonW + 5] = 6;
    DungeonView v = view();
    assert(v.path == "/root/EGA/D5/DP060" && v.x == 10 && v.y == 11);

    // Wall at depth 2 -> CHAR2/CH40 at the viewport origin.  Every view tile
    // is a full 120x119 cell, so all depths blit plainly at (10,11); the old
    // (58,27)/(-14,28) offsets compensated for the truncated byte-11 decode.
    d.tiles[6 * kDungeonW + 5] = 0;
    d.tiles[7 * kDungeonW + 5] = 6;
    v = view();
    assert(v.path == "/root/EGA/CHAR2/CH40" && v.x == 10 && v.y == 11);

    // Wall at depth 3 and deeper -> CHAR2/CH25 at the viewport origin.
    d.tiles[7 * kDungeonW + 5] = 0;
    d.tiles[8 * kDungeonW + 5] = 3;              // 3 is view-opaque
    v = view();
    assert(v.path == "/root/EGA/CHAR2/CH25" && v.x == 10 && v.y == 11);
    d.tiles[8 * kDungeonW + 5] = 0;
    d.tiles[20 * kDungeonW + 5] = 6;
    v = view();
    assert(v.path == "/root/EGA/CHAR2/CH25" && v.x == 10 && v.y == 11);

    // An all-open column runs off the map: treated as an opaque wall at the
    // edge (the engine's map is bounded).
    d.tiles.assign(kDungeonTiles, 0);
    v = view();
    assert(v.path == "/root/EGA/CHAR2/CH25");

    // Only the D5 scenario maps depths 2/>=3 onto the encounter bank; other
    // scenarios keep the depth-1 tileset sprite (the bank is scenario-specific).
    d.tiles[6 * kDungeonW + 5] = 0;
    d.tiles[7 * kDungeonW + 5] = 6;
    DungeonView v1 = dungeon_view_for("/root", 1, d, p);
    assert(v1.path == "/root/EGA/D1/DP001" && v1.x == 10 && v1.y == 11);

    // Real DNG05: the entry (17,3) facing S sees the wall at (17,4) at depth 1
    // (DP001); one step south (17,4) leaves the column open to the wall at
    // (17,10) -> depth >=3 -> CH25.  Matches the oracle captures.
    Dungeon r;
    if (load_dungeon(dir + "/DNG05", r)) {
        DungeonPos e; dungeon_place_entry(r, e);
        assert(e.x == 17 && e.y == 3 && e.facing == 2);
        DungeonView sv = dungeon_view_for(dir + "/..", 5, r, e);
        assert(sv.path == dir + "/../EGA/D5/DP001" && sv.x == 10 && sv.y == 11);
        DungeonPos q = e; q.y = 4;               // step south into the corridor
        DungeonView qv = dungeon_view_for(dir + "/..", 5, r, q);
        assert(qv.path == dir + "/../EGA/CHAR2/CH25" && qv.x == 10 && qv.y == 11);
        std::printf("dungeon_test: depth-march view verified (DNG05 entry)\n");
    }
}

// Stairs, holes and portals: the tile-code model, the big-endian destination
// record, and -- against the real levels -- the two invariants that make the
// model falsifiable (every stair has a destination; every destination lands
// next to the matching opposite stair).
static void test_stairs(const std::string& dir) {
    // Tile-code classification.
    assert(!dungeon_is_stair(0x06));
    assert(!dungeon_is_stair(0x00));
    assert(dungeon_is_stair(0x15) && dungeon_is_down_stair(0x15));
    assert(dungeon_is_stair(0x04) && dungeon_is_up_stair(0x04));
    assert(dungeon_is_stair(0x84) && dungeon_is_up_stair(0x84));
    assert(dungeon_is_stair(0xf4) && dungeon_is_up_stair(0xf4) &&
           dungeon_is_portal(0xf4));
    assert(!dungeon_is_portal(0x04) && !dungeon_is_portal(0x15));
    // High nibble is the destination dungeon (location only matters for 0xF).
    assert(dungeon_stair_dungeon(0x15, 0) == 1);
    assert(dungeon_stair_dungeon(0x04, 5) == 0);
    assert(dungeon_stair_dungeon(0x65, 1) == 6);
    assert(dungeon_stair_dungeon(0x84, 1) == 8);

    // The portal table (ds:0xa546) is indexed by the DNGS file-name **level**
    // digit (location % 10), not by the dungeon group.  Both shipped portals
    // live on level 3 (DNG03 and DNG43), so both must resolve through entry 3.
    assert(dungeon_stair_dungeon(0xf4, 13) == 1);   // DNG13 -> entry 3
    assert(dungeon_stair_dungeon(0xf4, 43) == 1);   // DNG43 -> entry 3
    assert(dungeon_stair_dungeon(0xf4, 5) == 1);    // DNG05 -> entry 5
    assert(dungeon_stair_dungeon(0xf4, 0) == 0);    // entry 0 is all zero

    // Messages.  The stairs/hole wording is selected by the level digit:
    // the handler compares `[0x37fe] == 7`, and [0x37fe] is the level digit.
    bool portal = true;
    assert(std::string(dungeon_stair_message(0x15, 2)) ==
           "You climb down the stairs.");
    assert(std::string(dungeon_stair_message(0x15, 7)) ==
           "You climb down the hole.");
    assert(std::string(dungeon_stair_message(0x04, 2)) ==
           "You climb up the stairs.");
    assert(std::string(dungeon_stair_message(0x04, 7)) ==
           "You climb up the hole.");
    assert(!dungeon_is_hole_level(2) && !dungeon_is_hole_level(16) &&
           dungeon_is_hole_level(7) && dungeon_is_hole_level(37) &&
           dungeon_is_hole_level(17));
    assert(std::string(dungeon_stair_message(0xf4, 3, &portal)) ==
           "You enter ");
    assert(portal);

    // Destination record is big-endian: the first disk byte is the HIGH byte of
    // the cell index.  Cell 644 = 0x0284 is stored as disk bytes (0x02, 0x84),
    // which the decoder assembles into the word 0x8402.
    Dungeon d;
    d.tiles.assign(kDungeonTiles, 0);
    d.objects.assign(kDungeonTiles, 0);
    d.tiles[5 * kDungeonW + 5] = 0x15;
    d.objects[5 * kDungeonW + 5] = 0x8402;      // disk order: byte0=0x02, byte1=0x84
    assert(d.dest_cell(5, 5) == 644);
    DungeonLink l;
    assert(dungeon_stair_target(d, 5, 5, 0, &l));
    assert(l.dungeon == 1 && l.cell == 644);

    // A non-stair cell has no link even if it carries a record, and a stair
    // with no record has none either.
    d.tiles[5 * kDungeonW + 5] = 0x06;
    assert(!dungeon_stair_target(d, 5, 5, 0, &l));
    d.tiles[5 * kDungeonW + 5] = 0x15;
    d.objects[5 * kDungeonW + 5] = 0;
    assert(!dungeon_stair_target(d, 5, 5, 0, &l));

    // Portal target comes from the table, not the record.
    d.tiles[5 * kDungeonW + 5] = 0xf4;
    assert(dungeon_stair_target(d, 5, 5, 5, &l));
    assert(l.dungeon == 1 && l.cell == 0x0492);

    // --- Real levels ------------------------------------------------------
    // 29 levels DNG<d><l>; load every one that exists.
    std::vector<Dungeon> lv;
    std::vector<std::pair<int, int>> id;
    for (int dd = 0; dd <= 6; dd++) {
        for (int ll = 0; ll <= 7; ll++) {
            Dungeon m;
            if (!load_dungeon(dir + "/" + dungeon_file_name(dd, ll), m)) continue;
            lv.push_back(m);
            id.push_back({dd, ll});
        }
    }
    if (lv.size() < 29) {
        std::printf("dungeon_test: stairs (data-driven part skipped: %zu/29 "
                    "levels)\n", lv.size());
        return;
    }

    auto find = [&](int dd, int ll) -> const Dungeon* {
        for (size_t k = 0; k < id.size(); k++)
            if (id[k].first == dd && id[k].second == ll) return &lv[k];
        return nullptr;
    };

    int stairs = 0, portals = 0, plain_dest = 0, resolved = 0, reciprocal = 0;
    int portal_resolved = 0;
    for (size_t k = 0; k < lv.size(); k++) {
        const Dungeon& m = lv[k];
        int group = id[k].first, level = id[k].second;
        int cur = group * 10 + level;   // full save location, as the engine has it
        for (int y = 0; y < kDungeonH; y++) {
            for (int x = 0; x < kDungeonW; x++) {
                uint8_t code = m.tile(x, y);
                if (!dungeon_is_stair(code)) continue;
                stairs++;
                DungeonLink l;
                bool is_portal = dungeon_is_portal(code);
                if (is_portal) portals++;
                if (!dungeon_stair_target(m, x, y, cur, &l)) continue;
                const Dungeon* dst = find(l.dungeon, level);
                if (is_portal) {
                    // A portal destination must be a real, walkable cell: the
                    // group-digit reading puts DNG43's portal inside a wall.
                    if (dst && l.cell >= 0 && l.cell < kDungeonTiles &&
                        !dungeon_is_solid_tile(dst->tile(l.cell % kDungeonW,
                                                l.cell / kDungeonW)))
                        portal_resolved++;
                    continue;
                }
                plain_dest++;
                if (!dst || l.cell < 0 || l.cell >= kDungeonTiles) continue;
                resolved++;
                // The arrival cell sits next to the matching opposite stair:
                // an up stair lands beside a down stair and vice versa.
                bool want_up = dungeon_is_down_stair(code);
                int cx = l.cell % kDungeonW, cy = l.cell / kDungeonW;
                const int nb[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
                for (const auto& n : nb) {
                    uint8_t t = dst->tile(cx + n[0], cy + n[1]);
                    if (want_up ? dungeon_is_up_stair(t)
                                : dungeon_is_down_stair(t)) {
                        reciprocal++;
                        break;
                    }
                }
            }
        }
    }
    // --- The record invariant that separates the two readings --------------
    // Stair records are big-endian cell indices, so their first byte is 0..4.
    // Feature records are `(0, code)` or `(1, code)`.  A first byte of 2..4
    // therefore occurs ONLY on stair cells across all 29 levels -- that is what
    // proves the big-endian reading rather than merely fitting it.  Also check
    // `dest_cell()` refuses non-stair cells and `has_feature()` the converse.
    int stair_hi_2plus = 0, feature_hi_1 = 0, feature_total = 0, bad = 0;
    for (const Dungeon& m : lv) {
        for (int y = 0; y < kDungeonH; y++) {
            for (int x = 0; x < kDungeonW; x++) {
                uint8_t b0 = m.obj_low(x, y);
                if (dungeon_is_stair(m.tile(x, y))) {
                    if (b0 >= 2) stair_hi_2plus++;
                    if (m.has_feature(x, y)) bad++;
                    continue;
                }
                if (b0 >= 2) bad++;
                if (m.dest_cell(x, y) != -1) bad++;
                if (m.object(x, y)) {
                    feature_total++;
                    if (b0 == 1) feature_hi_1++;
                }
            }
        }
    }
    assert(stair_hi_2plus > 0);
    assert(bad == 0);
    assert(feature_total > 3000 && feature_hi_1 > 200);
    std::printf("dungeon_test: object records ok (%d stair records with a high "
                "byte >= 2, %d feature records, %d with a high byte of 1)\n",
                stair_hi_2plus, feature_total, feature_hi_1);

    // 57 stair cells ship across the 29 levels, 2 of them portals.
    assert(stairs == 57);
    assert(portals == 2);
    // Every ordinary stair carries a destination record (55/55) and every one
    // lands in a real level, beside the matching opposite stair (55/55).  These
    // two are the invariants that make the model falsifiable: a wrong nibble
    // reading or a little-endian destination would break them immediately.
    assert(plain_dest == 55);
    assert(resolved == 55);
    assert(reciprocal == resolved);
    // Both portals resolve through the ds:0xa546 table.
    assert(portal_resolved == 2);
    std::printf("dungeon_test: stairs verified (%d cells, %d ordinary, "
                "%d reciprocal, %d portals)\n", stairs, plain_dest, reciprocal,
                portals);
}

// A type-7 door teleports the party: SPEC2[0] is the only shipped type-7 record
// (ones=1, tens=1 -> location 11 = DNG11, cell 572).  The target fields must
// decode to location 11, and following it must load DNG11 and stand the party on
// cell 572; a bad destination must leave the party and the map untouched.
static void test_door_follow(const std::string& dir) {
    const std::string root = dir + "/..";

    SpecTable spec = load_spec(root + "/SYST/SPEC2");
    assert(spec.ok && !spec.records.empty());
    const SpecRecord& r = spec.records[0];
    assert(r.type == 7);
    DoorTarget t = door_target(r);
    assert(t.location == 1 && t.map == 1 && t.cell == 572);
    assert(t.destination_location() == 11);   // tens*10 + ones

    Dungeon d, ref;
    std::string err;
    assert(load_dungeon(dir + "/DNG05", d, &err));
    assert(load_dungeon(dir + "/DNG11", ref, &err));
    uint16_t location = 5;
    DungeonPos pos;
    assert(dungeon_place_cell(d, pos, 0, 2));
    const int facing = pos.facing;
    assert(dungeon_follow_door(root, t.destination_location(), t.cell,
                               location, d, pos));
    assert(location == 11);
    assert(d.tiles == ref.tiles);             // the map really is DNG11
    DungeonPos expect;
    assert(dungeon_place_cell(ref, expect, t.cell, facing));
    assert(pos.x == expect.x && pos.y == expect.y && pos.facing == expect.facing);

    // A bad destination changes nothing.
    Dungeon d2;
    assert(load_dungeon(dir + "/DNG05", d2, &err));
    uint16_t loc2 = 5;
    DungeonPos p2;
    assert(dungeon_place_cell(d2, p2, 0, 2));
    const int x2 = p2.x, y2 = p2.y, f2 = p2.facing;
    assert(!dungeon_follow_door(root, 77, 0, loc2, d2, p2));
    assert(loc2 == 5 && p2.x == x2 && p2.y == y2 && p2.facing == f2);

    std::printf("dungeon_test: door follow ok (SPEC2[0] -> DNG11 cell %d)\n", t.cell);
}

// A type-11 event teleports within the current map: the handler writes the
// record's +0xe4 straight into the party-cell global.  The seven shipped
// type-11 records all carry a valid cell; SPEC3[54] ("You dive under the
// rock...") -> cell 1168 = (16,32), and the facing is unchanged.
static void test_teleport(const std::string& dir) {
    const std::string root = dir + "/..";
    int checked = 0;
    for (int sc = 0; sc < 10; ++sc) {
        SpecTable t = load_spec(root + "/SYST/SPEC" + std::to_string(sc));
        if (!t.ok) continue;
        for (const SpecRecord& r : t.records) {
            if (r.type != 11) continue;
            assert(r.counter >= 0 && r.counter < kDungeonW * kDungeonH);
            ++checked;
        }
    }
    assert(checked == 7 && "the release must carry seven type-11 records");

    SpecTable s3 = load_spec(root + "/SYST/SPEC3");
    assert(s3.ok && s3.records.size() > 54);
    const SpecRecord& r = s3.records[54];
    assert(r.type == 11 && r.counter == 1168);
    DungeonPos pos;
    pos.x = 5; pos.y = 6; pos.facing = 3;
    assert(dungeon_teleport_cell(pos, r.counter));
    assert(pos.x == 16 && pos.y == 32 && pos.facing == 3);  // facing unchanged

    DungeonPos bad = pos;
    assert(!dungeon_teleport_cell(bad, kDungeonW * kDungeonH));
    assert(bad.x == pos.x && bad.y == pos.y && bad.facing == pos.facing);

    std::printf("dungeon_test: teleport ok (%d type-11 records; SPEC3[54] -> cell 1168 = 16,32)\n",
                checked);
}

static void test_view_slots() {
    assert(dungeon_slot_count() == 28);
    // drawer A: DP == table T (cross-checked against the emulator's blits)
    assert(dungeon_slot_dp(ViewDrawer::A, 0) == 36);
    assert(dungeon_slot_dp(ViewDrawer::A, 10) == 43);   // DP043 @ (10,57)
    assert(dungeon_slot_dp(ViewDrawer::A, 11) == 38);   // DP038 @ (25,57)
    assert(dungeon_slot_dp(ViewDrawer::A, 16) == 54);   // DP054
    assert(dungeon_slot_dp(ViewDrawer::A, 17) == 50);   // DP050
    assert(dungeon_slot_dp(ViewDrawer::A, 22) == 39);   // DP039
    assert(dungeon_slot_x(ViewDrawer::A, 10) == 10 && dungeon_slot_y(ViewDrawer::A, 10) == 57);
    assert(dungeon_slot_x(ViewDrawer::A, 22) == 20 && dungeon_slot_y(ViewDrawer::A, 22) == 61);
    // drawer B: DP = T - 0x16
    assert(dungeon_slot_dp(ViewDrawer::B, 22) == 39 - 0x16);
    assert(dungeon_slot_dp(ViewDrawer::B, 10) == 43 - 0x16);
    // drawer C: its own tables, DP = T + 0x3c, unused slots -> -1
    assert(dungeon_slot_dp(ViewDrawer::C, 0) == 2 + 0x3c);
    assert(dungeon_slot_x(ViewDrawer::C, 0) == 10 && dungeon_slot_y(ViewDrawer::C, 0) == 11);
    assert(dungeon_slot_dp(ViewDrawer::C, 1) == -1);
    // out-of-range guards
    assert(dungeon_slot_dp(ViewDrawer::A, -1) == -1);
    assert(dungeon_slot_dp(ViewDrawer::A, 28) == -1);
    printf("view_slots: ok (28 slots; A/B/C DP math cross-checked vs blits)\n");
}

static void test_view_pieces() {
    uint8_t codes[22] = {0};
    assert(dungeon_view_pieces(codes, false).empty());          // all open
    // code 6 at cell 0x42 (index 14) -> slot 20 drawer A -> DP057 @ (120,57)
    codes[14] = 6;
    auto p = dungeon_view_pieces(codes, false);
    assert(p.size() == 1 && p[0].dp == 57 && p[0].x == 120 && p[0].y == 57);
    // code 0xa -> slot 20 drawer B -> DP057-0x16=35
    codes[14] = 0xa;
    p = dungeon_view_pieces(codes, false);
    assert(p.size() == 1 && p[0].dp == 35 && p[0].x == 120 && p[0].y == 57);
    codes[14] = 0;
    // code 6 at cell 0x41 (index 13) -> slots 19 + adjacent 14 (drawer A)
    codes[13] = 6;
    p = dungeon_view_pieces(codes, false);
    assert(p.size() == 2 && p[0].dp == 55 && p[1].dp == 42);
    codes[13] = 0;
    // code 7/8 swap near/far drawer by the side flag
    codes[14] = 7;
    assert(dungeon_view_pieces(codes, false)[0].dp == 57);
    assert(dungeon_view_pieces(codes, true)[0].dp == 35);
    codes[14] = 8;
    assert(dungeon_view_pieces(codes, false)[0].dp == 35);
    assert(dungeon_view_pieces(codes, true)[0].dp == 57);
    codes[14] = 0;
    // far-row depth loop: cell 0x44 (index 16) code 6 -> slot 22 drawer A -> DP039 @ (20,61)
    codes[16] = 6;
    p = dungeon_view_pieces(codes, false);
    assert(p.size() == 1 && p[0].dp == 39 && p[0].x == 20 && p[0].y == 61);
    printf("view_pieces: ok (dispatch A/B/C + side-flag swap + far-row)\n");
}

static void test_view_tiled(const std::string& gameDir) {
    if (!std::filesystem::exists(gameDir + "/EGA/D5/DP001")) {
        printf("view_tiled: skipped (no game data)\n");
        return;
    }
    Screen scr;
    scr.resize(320, 200);
    scr.clear(0);
    uint8_t codes[22] = {0};
    codes[14] = 6;  // far wall -> slot 20 drawer A (DP057 @ 120,57)
    codes[16] = 6;  // far row -> slot 22 drawer A (DP039 @ 20,61)
    assert(render_dungeon_tiled(scr, gameDir, 5, codes, false));
    int nz = 0;
    for (int y = 11; y < 130; y++)
        for (int x = 10; x < 130; x++)
            if (scr.pixels[(size_t)y * 320 + x]) nz++;
    assert(nz > 1000);  // base + pieces actually drew
    printf("view_tiled: ok (%d non-zero viewport px; base+pieces composed)\n", nz);
}

static void test_view_march() {
    assert(dungeon_march_code(6, 0, false) == 6);
    assert(dungeon_march_code(7, 0, false) == 7);
    assert(dungeon_march_code(8, 0, false) == 8);
    assert(dungeon_march_code(12, 0, false) == 0xc);
    assert(dungeon_march_code(0, 0, false) == 0);
    assert(dungeon_march_code(4, 0, false) == 0);   // 4/5 -> open
    assert(dungeon_march_code(5, 0, false) == 0);
    assert(dungeon_march_code(9, 0, false) == 0xa);
    assert(dungeon_march_code(0xd, 0, false) == 6);
    assert(dungeon_march_code(3, 1, false) == 6);
    assert(dungeon_march_code(3, 2, false) == 7 && dungeon_march_code(3, 2, true) == 8);
    assert(dungeon_march_code(3, 4, false) == 7);
    printf("view_march: ok (tile->wall-code rules)\n");
}

int main(int argc, char** argv) {
    test_synthetic();
    test_view_slots();
    test_view_pieces();
    test_view_tiled((argc > 1)
                        ? std::filesystem::path(argv[1]).parent_path().string()
                        : std::string("game"));
    test_view_march();
    test_render();
    test_reachable();
    test_render_facing();
    test_render_layers();
    test_viewport();
    test_view_sprite((argc > 1) ? argv[1] : "game/DNGS");
    test_view_inventory((argc > 1) ? argv[1] : "game/DNGS");
    test_outdoor_object((argc > 1) ? argv[1] : "game/DNGS");
    test_default_views((argc > 1) ? argv[1] : "game/DNGS");
    test_walk();
    test_level_connected((argc > 1) ? argv[1] : "game/DNGS");
    std::string dir = (argc > 1) ? argv[1] : "game/DNGS";
    test_real(dir);
    test_object_layout(dir);
    test_file_name(dir);
    test_connectivity_stats(dir);
    test_object_doors(dir);
    test_event_keys(dir);
    test_event_spec_mapping(dir);
    test_recruit_record(dir);
    test_text_block_index(dir);
    test_dispatch_table(dir);
    test_answer_text(dir);
    test_recruit_field_guard(dir);
    test_answer_branches(dir);
    test_recruit_class(dir);
    test_walk_real(dir);
    test_view_themes(dir);
    test_dungeon_screen(dir);
    test_base_view(dir);
    test_room_art(dir);
    test_view_depth(dir);
    test_stairs(dir);
    test_door_follow(dir);
    test_teleport(dir);
    std::printf("dungeon_test: ok\n");
    return 0;
}
