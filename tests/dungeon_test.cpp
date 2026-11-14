// Self-test for the DNGS dungeon decoder (workstream A).
//
// The synthetic case is built in memory so the test needs no game data.  When
// game data is present (game/DNGS/DNG00 ...) the decoder is also run against a
// real file and its geometry / a few known cells are asserted.
#include "dungeon.h"

#include <cassert>
#include <cstdio>
#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <set>
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
    // The open-cell corridor cases (0x0/0x2/0x7/0x8/0x9) have no single
    // full-size sprite: the engine composes that view (BLK-A003), so the
    // drop-in falls back to DP001.  Pinned here as the documented approximation.
    for (int nib : {0x0, 0x2, 0x7, 0x8, 0x9})
        assert(view_for(nib) == 1);
    // Off-map ahead always falls back to the wall view.
    p = DungeonPos{5, 0, 0};
    assert(dungeon_base_view_index(d, p) == 1);

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

int main(int argc, char** argv) {
    test_synthetic();
    test_render();
    test_reachable();
    test_render_facing();
    test_render_layers();
    test_viewport();
    test_view_sprite((argc > 1) ? argv[1] : "game/DNGS");
    test_view_inventory((argc > 1) ? argv[1] : "game/DNGS");
    test_default_views((argc > 1) ? argv[1] : "game/DNGS");
    test_walk();
    test_level_connected((argc > 1) ? argv[1] : "game/DNGS");
    std::string dir = (argc > 1) ? argv[1] : "game/DNGS";
    test_real(dir);
    test_object_layout(dir);
    test_file_name(dir);
    test_connectivity_stats(dir);
    test_object_doors(dir);
    test_walk_real(dir);
    test_view_themes(dir);
    test_dungeon_screen(dir);
    test_base_view(dir);
    std::printf("dungeon_test: ok\n");
    return 0;
}
