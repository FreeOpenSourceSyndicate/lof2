// Dungeon map decoder for Legend of Faerghail (DNGS/DNGnn).
// See port/dungeon.h and docs/notes-a.md.
#include "dungeon.h"

#include "gfx.h"

#include <cstdio>

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

    out.h0 = d[0];
    out.h1 = d[1];
    out.field = (uint16_t)(d[2] | (d[3] << 8));

    out.tiles.assign(d.begin() + kDungeonHeader,
                     d.begin() + kDungeonHeader + kDungeonTiles);
    const uint8_t* obj = d.data() + kDungeonHeader + kDungeonTiles;
    out.objects.resize(kDungeonTiles);
    for (int i = 0; i < kDungeonTiles; i++)
        out.objects[i] = (uint16_t)(obj[2 * i] | (obj[2 * i + 1] << 8));
    return true;
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

}  // namespace lofport
