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
        case '.': band = 1; cell = 34; return true;
        case '?': band = 1; cell = 35; return true;
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

// The compact face used for the play-screen party panel.  Cell = toupper - 64
// (so 'A'..'Z' -> 1..26, '@'/'[' -> 0/27, matching ASCII order); only the
// top-left 5x5 of the 8x8 band-0 glyph is drawn, and glyphs advance 5 px.
int Font::draw_small(Screen& scr, int x, int y, const std::string& text, uint8_t idx) const {
    for (char ch : text) {
        char u = (ch >= 'a' && ch <= 'z') ? (char)(ch - 32) : ch;
        int cell = (unsigned char)u - 64;
        if (ok && cell >= 0 && cell < 40) {
            for (int gy = 0; gy < 5; gy++) {
                uint8_t bits = glyph[0][cell][gy];
                for (int gx = 0; gx < 5; gx++)
                    if (bits & (0x80 >> gx)) scr.put(x + gx, y + gy, idx);
            }
        }
        x += 5;
    }
    return x;
}

int Font::draw_small_cell(Screen& scr, int x, int y, int band, int cell, uint8_t idx) const {
    if (ok && band >= 0 && band < 3 && cell >= 0 && cell < 40) {
        for (int gy = 0; gy < 5; gy++) {
            uint8_t bits = glyph[band][cell][gy];
            for (int gx = 0; gx < 5; gx++)
                if (bits & (0x80 >> gx)) scr.put(x + gx, y + gy, idx);
        }
    }
    return x + 5;
}

// Full 8x8 face at a 6-px pitch.  The engine's text cells are 8 px wide but it
// advances 6, so each cell's leftmost 6 columns are drawn (the glyphs are 5 px
// wide with a 1 px gap, so column 6 is blank anyway).  Verified pixel-exact
// against the DOSBox status box in ref/oracle/play.png.
int Font::draw_text(Screen& scr, int x, int y, const std::string& text, uint8_t idx) const {
    for (char ch : text) {
        int band, cell;
        if (ok && glyph_cell(ch, band, cell)) {
            for (int gy = 0; gy < 8; gy++) {
                uint8_t bits = glyph[band][cell][gy];
                for (int gx = 0; gx < 6; gx++)
                    if (bits & (0x80 >> gx)) scr.put(x + gx, y + gy, idx);
            }
        }
        x += 6;
    }
    return x;
}

int Font::draw_text_cell(Screen& scr, int x, int y, int band, int cell, uint8_t idx) const {
    if (ok && band >= 0 && band < 3 && cell >= 0 && cell < 40) {
        for (int gy = 0; gy < 8; gy++) {
            uint8_t bits = glyph[band][cell][gy];
            for (int gx = 0; gx < 6; gx++)
                if (bits & (0x80 >> gx)) scr.put(x + gx, y + gy, idx);
        }
    }
    return x + 6;
}

}  // namespace lofport
