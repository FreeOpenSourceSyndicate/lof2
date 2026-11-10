#include "dungeon.h"

#include <cstdio>
#include <vector>

namespace lofport {

namespace {

std::vector<uint8_t> read_file(const std::string& path) {
    std::vector<uint8_t> v;
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return v;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n > 0) {
        v.resize(n);
        if (fread(v.data(), 1, n, f) != (size_t)n) v.clear();
    }
    fclose(f);
    return v;
}

// Expand the RLE stream.  A literal byte is copied; the escape byte is followed
// by (count, value) and emits `value` `count` times.
std::vector<uint8_t> rle_decode(const std::vector<uint8_t>& d, uint8_t esc) {
    std::vector<uint8_t> out;
    size_t i = 1, n = d.size();
    while (i < n) {
        uint8_t b = d[i++];
        if (b == esc) {
            if (i + 1 >= n) break;
            uint8_t cnt = d[i], val = d[i + 1];
            i += 2;
            out.insert(out.end(), cnt, val);
        } else {
            out.push_back(b);
        }
    }
    return out;
}

}  // namespace

DungeonMap load_dungeon(const std::string& path) {
    DungeonMap m;
    std::vector<uint8_t> d = read_file(path);
    if (d.size() < 2) return m;
    std::vector<uint8_t> raw = rle_decode(d, d[0]);
    if (raw.size() != kDungeonDecoded) return m;

    for (int i = 0; i < 4; i++) m.header[i] = raw[i];
    const uint8_t* p = raw.data() + 4;
    for (int y = 0; y < kDungeonDim; y++)
        for (int x = 0; x < kDungeonDim; x++) {
            m.walls[y][x] = p[0 * kDungeonLayer + y * kDungeonDim + x];
            m.objects[y][x] = p[1 * kDungeonLayer + y * kDungeonDim + x];
            m.entities[y][x] = p[2 * kDungeonLayer + y * kDungeonDim + x];
        }
    m.ok = true;
    return m;
}

}  // namespace lofport
