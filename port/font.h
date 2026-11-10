// Native re-implementation of the Legend of Faerghail bitmap font.
//
// The game ships its text font as two 320x24 EGA strips, EGA/UP/CHR1 and
// EGA/UP/CHR2.  Each strip is 40 glyph cells of 8x8 pixels stacked in three
// 8-pixel bands:
//
//   band 0 : upper case A..Z in cells 4..29, digits 0..9 in cells 30..39
//   band 1 : lower case a..z in cells 4..29, punctuation/symbols
//   band 2 : further symbols
//
// The glyphs are read straight out of the game's data file at run time, so no
// font data is embedded in the binary.
#ifndef LOF_PORT_FONT_H
#define LOF_PORT_FONT_H

#include "gfx.h"

#include <cstdint>
#include <string>

namespace lofport {

// A decoded font strip (one EGA/UP/CHR? file): 3 bands x 40 cells x 8x8 bits,
// one bit per pixel (1 = ink).
struct Font {
    bool ok = false;
    int advance = 8;           // horizontal step between cells (engine uses 6)
    uint8_t glyph[3][40][8];   // [band][cell][row], bit 7 = leftmost pixel

    // Load a font strip from an EGA/UP/CHRn file.
    bool load(const std::string& path, std::string* err = nullptr);

    // Draw `text` at (x,y) in colour `idx`.  Unknown characters render as a
    // blank cell.  Returns the x coordinate just past the last glyph.
    int draw(Screen& scr, int x, int y, const std::string& text, uint8_t idx) const;

    // Draw `text` with the game's compact "small caps" face: the *top-left 5x5*
    // of the band-0 cell `toupper(ch) - 64`, advanced 5 px per glyph.  This is
    // the font the engine uses for the party-panel names on the play screen
    // (loaded from CHR2); lower case renders as upper case.
    int draw_small(Screen& scr, int x, int y, const std::string& text, uint8_t idx) const;

    // Blit one 5x5 glyph by (band, cell), advancing 5 px.  The play-screen
    // status row draws CHR2 cells directly rather than through a text string.
    int draw_small_cell(Screen& scr, int x, int y, int band, int cell, uint8_t idx) const;
};

}  // namespace lofport

#endif
