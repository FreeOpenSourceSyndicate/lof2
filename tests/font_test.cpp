// Self-test for the native font renderer.  It builds a Font in memory (no game
// data required) and checks the glyph cell mapping and the draw path.
#include "font.h"

#include <cassert>
#include <cstdio>

using namespace lofport;

static void set_glyph(Font& f, int band, int cell, int row, uint8_t bits) {
    f.glyph[band][cell][row] = bits;
}

int main() {
    Font f;
    f.ok = true;

    // Upper case 'A' -> band 0, cell 4.  Lower case 'a' -> band 1, cell 4.
    set_glyph(f, 0, 4, 0, 0x80);   // leftmost pixel of the first row
    set_glyph(f, 1, 4, 1, 0x01);   // rightmost pixel of the second row
    // Digit '0' -> band 0, cell 30.
    set_glyph(f, 0, 30, 2, 0x40);
    // '(' -> band 1, cell 2.
    set_glyph(f, 1, 2, 3, 0x02);

    Screen scr;
    scr.resize(64, 16);
    scr.clear(0);

    int x = f.draw(scr, 0, 0, "Aa0(", 7);
    assert(x == 32);                        // four cells at the default 8px advance
    assert(scr.pixels[0 * 64 + 0] == 7);    // 'A' pixel
    assert(scr.pixels[1 * 64 + 8 + 7] == 7); // 'a' pixel
    assert(scr.pixels[2 * 64 + 16 + 1] == 7); // '0' pixel (0x40 -> bit 1)
    assert(scr.pixels[3 * 64 + 24 + 6] == 7); // '(' pixel (0x02 -> bit 6)
    // A blank cell must not set anything.
    assert(scr.pixels[0 * 64 + 1] == 0);

    // The engine advances text by 6 pixels per cell (found by matching the menu
    // capture), so check a narrower advance places the second glyph correctly.
    Screen sn;
    sn.resize(32, 8);
    sn.clear(0);
    f.advance = 6;
    assert(f.draw(sn, 0, 0, "AA", 7) == 12);
    assert(sn.pixels[0] == 7);      // first 'A' at x=0
    assert(sn.pixels[6] == 7);      // second 'A' at x=6

    // Unknown characters advance but draw nothing.
    Font f2;
    f2.ok = true;
    Screen s2;
    s2.resize(16, 8);
    s2.clear(0);
    assert(f2.draw(s2, 0, 0, "\x01", 7) == 8);
    for (uint8_t v : s2.pixels) assert(v == 0);

    // draw() on an unloaded font is a no-op but still advances.
    Font empty;
    Screen s3;
    s3.resize(16, 8);
    s3.clear(0);
    assert(empty.draw(s3, 0, 0, "A", 7) == 8);
    for (uint8_t v : s3.pixels) assert(v == 0);

    printf("font_test: ok\n");
    return 0;
}
