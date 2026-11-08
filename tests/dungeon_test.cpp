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

int main(int argc, char** argv) {
    test_synthetic();
    test_render();
    test_reachable();
    test_render_facing();
    test_walk();
    std::string dir = (argc > 1) ? argv[1] : "game/DNGS";
    test_real(dir);
    test_object_layout(dir);
    std::printf("dungeon_test: ok\n");
    return 0;
}
