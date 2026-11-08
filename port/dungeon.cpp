// Dungeon map decoder for Legend of Faerghail (DNGS/DNGnn).
// See port/dungeon.h and docs/notes-a.md.
#include "dungeon.h"

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

static char tile_symbol(uint8_t v) {
    switch (v) {
        case 0:  return '#';
        case 6:  return '.';
        case 3:  return '%';
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

bool dungeon_passable(const Dungeon& d, int x, int y) {
    if (x < 0 || y < 0 || x >= kDungeonW || y >= kDungeonH) return false;
    uint8_t t = d.tile(x, y);
    return t != 3 && t != 12;
}

// Colour per tile code (EGA 16-colour indices).
static uint8_t tile_colour(uint8_t v) {
    switch (v) {
        case 0:  return 8;    // dark gray  - open floor
        case 6:  return 7;    // light gray - floor
        case 3:  return 4;    // red        - wall
        case 12: return 1;    // blue       - wall/water
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
