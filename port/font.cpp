#include "font.h"

#include <cstdio>

namespace lofport {

// CHR1/CHR2 are 320x24 = 40 cells of 8x8, stored as three 8-pixel bands.
bool Font::load(const std::string& path, std::string* err) {
    EgaImage img;
    if (!load_ega_image(path, img, err)) return false;
    if (img.width_bytes * 8 < 320 || img.height < 24) {
        if (err) *err = path + ": font strip too small";
        return false;
    }
    for (int b = 0; b < 3; b++) {
        for (int c = 0; c < 40; c++) {
            for (int y = 0; y < 8; y++) {
                int sy = b * 8 + y;
                uint8_t bits = 0;
                for (int x = 0; x < 8; x++) {
                    int sx = c * 8 + x;
                    uint8_t v = img.planes[sy * img.width_bytes + (sx >> 3)];
                    bits = (uint8_t)((bits << 1) | ((v >> (7 - (sx & 7))) & 1));
                }
                glyph[b][c][y] = bits;
            }
        }
    }
    ok = true;
    return true;
}

// Glyph cells: band 0 holds upper case in cells 0..25 and digits in cells
// 26..35; band 1 holds lower case in cells 0..25.  The punctuation cells were
// recovered by matching rendered glyphs against a DOSBox capture of the menu.
static bool glyph_cell(char ch, int& band, int& cell) {
    if (ch >= 'A' && ch <= 'Z') { band = 0; cell = ch - 'A'; return true; }
    if (ch >= 'a' && ch <= 'z') { band = 1; cell = ch - 'a'; return true; }
    if (ch >= '0' && ch <= '9') { band = 0; cell = 26 + (ch - '0'); return true; }
    switch (ch) {
        case '(': band = 1; cell = 38; return true;
        case ')': band = 1; cell = 39; return true;
        case ',': band = 1; cell = 32; return true;
        case '-': band = 1; cell = 33; return true;
        case '!': band = 0; cell = 39; return true;
        default: return false;
    }
}

int Font::draw(Screen& scr, int x, int y, const std::string& text, uint8_t idx) const {
    for (char ch : text) {
        int band, cell;
        if (ok && glyph_cell(ch, band, cell)) {
            for (int gy = 0; gy < 8; gy++) {
                uint8_t bits = glyph[band][cell][gy];
                for (int gx = 0; gx < 8; gx++)
                    if (bits & (0x80 >> gx)) scr.put(x + gx, y + gy, idx);
            }
        }
        x += advance;
    }
    return x;
}

}  // namespace lofport
