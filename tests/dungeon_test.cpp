// Self-test for the DNGS/DNGxx dungeon-map reader.
//
// The synthetic part always runs (no game data required) and checks the RLE
// expansion and the layer slicing.  When the game data is present it also
// checks real maps (DNG05 = the Temple, DNG10 = the wilderness) against values
// recovered from the shipped files.
#include "dungeon.h"

#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

using namespace lofport;

// Build a synthetic map: header [1,2,3,4], layer0 = x+y, layer1 = 0x10, layer2
// = 0x20, then RLE-compress it with escape 0xff and re-read it.
static void synthetic() {
    std::vector<uint8_t> raw;
    raw.push_back(1); raw.push_back(2); raw.push_back(3); raw.push_back(4);
    for (int layer = 0; layer < 3; layer++)
        for (int y = 0; y < kDungeonDim; y++)
            for (int x = 0; x < kDungeonDim; x++)
                raw.push_back(layer == 0 ? (uint8_t)(x + y)
                                         : (uint8_t)(layer == 1 ? 0x10 : 0x20));
    assert(raw.size() == kDungeonDecoded);

    // RLE: escape 0xff, then (count, value).  A run of 0x10 bytes compresses to
    // one triple; emit everything else literally.
    std::vector<uint8_t> enc;
    enc.push_back(0xff);
    size_t i = 0;
    while (i < raw.size()) {
        size_t j = i;
        while (j < raw.size() && raw[j] == raw[i] && j - i < 255) j++;
        if (j - i >= 3) {
            enc.push_back(0xff);
            enc.push_back((uint8_t)(j - i));
            enc.push_back(raw[i]);
        } else {
            for (size_t k = i; k < j; k++) {
                if (raw[k] == 0xff) { enc.push_back(0xff); enc.push_back(1); enc.push_back(0xff); }
                else enc.push_back(raw[k]);
            }
        }
        i = j;
    }
    const char* path = "/tmp/lof_dungeon_test.dng";
    FILE* f = fopen(path, "wb");
    assert(f);
    fwrite(enc.data(), 1, enc.size(), f);
    fclose(f);

    DungeonMap m = load_dungeon(path);
    assert(m.ok);
    assert(m.level() == 1 && m.style() == 2 && m.music() == 3 && m.spawn() == 4);
    for (int y = 0; y < kDungeonDim; y++)
        for (int x = 0; x < kDungeonDim; x++) {
            assert(m.walls[y][x] == (uint8_t)(x + y));
            assert(m.objects[y][x] == 0x10);
            assert(m.entities[y][x] == 0x20);
        }
    // Out-of-range reads as 0.
    assert(m.at(-1, 0) == 0 && m.at(kDungeonDim, 0) == 0);

    // A truncated / malformed file is rejected.
    std::vector<uint8_t> bad = enc;
    bad.resize(enc.size() / 2);
    f = fopen(path, "wb");
    assert(f);
    fwrite(bad.data(), 1, bad.size(), f);
    fclose(f);
    assert(!load_dungeon(path).ok);
    remove(path);

    printf("dungeon_test: synthetic ok\n");
}

static int real_data(const char* dir) {
    char p[128];

    // DNG05 — the Temple (save slot 1).  Header [0,2,0,125]; 780 wall cells.
    snprintf(p, sizeof p, "%s/DNGS/DNG05", dir);
    DungeonMap t = load_dungeon(p);
    if (!t.ok) { printf("dungeon_test: no game data (%s)\n", p); return 0; }
    assert(t.header[0] == 0 && t.header[1] == 2 && t.header[2] == 0 && t.header[3] == 125);
    int nz = 0;
    for (int y = 0; y < kDungeonDim; y++)
        for (int x = 0; x < kDungeonDim; x++) if (t.walls[y][x]) nz++;
    assert(nz == 780);
    assert(t.walls[0][0] == 6 && t.walls[0][17] == 6);   // solid border row
    assert(t.walls[1][17] == 3);                         // the doorway tile
    assert(t.walls[2][17] == 7);                         // the 'O' marker

    // DNG10 — the wilderness (save slot 2).  Header [0,0,2,233].
    snprintf(p, sizeof p, "%s/DNGS/DNG10", dir);
    DungeonMap w = load_dungeon(p);
    assert(w.ok);
    assert(w.header[0] == 0 && w.header[1] == 0 && w.header[2] == 2 && w.header[3] == 233);

    // Every shipped map decodes to the fixed size and has a distinct first
    // header byte of 0 except DNG13 (a level-4 map).
    snprintf(p, sizeof p, "%s/DNGS/DNG13", dir);
    DungeonMap l4 = load_dungeon(p);
    assert(l4.ok && l4.level() == 4 && l4.style() == 2);

    printf("dungeon_test: real DNGS ok\n");
    return 0;
}

int main(int argc, char** argv) {
    synthetic();
    const char* dir = argc > 1 ? argv[1] : "game";
    int rc = real_data(dir);
    if (rc == 0) printf("dungeon_test: ok\n");
    return rc;
}
