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

int main(int argc, char** argv) {
    test_synthetic();
    test_render();
    std::string dir = (argc > 1) ? argv[1] : "game/DNGS";
    test_real(dir);
    std::printf("dungeon_test: ok\n");
    return 0;
}
