// Native re-implementation of the Legend of Faerghail EGA graphics layer.
//
// This does NOT run the original executables.  It reads the game's data files
// (the EGA/*.image files) and renders them itself, so the drop-in works even if
// LOF.EXE / VOR.EXE are absent.
#ifndef LOF_PORT_GFX_H
#define LOF_PORT_GFX_H

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

namespace lofport {

// Decode an EGA 6-bit attribute colour selector (0..63) to RGB exactly like the
// hardware: bits 0-2 are the secondary (1x) guns, bits 3-5 the primary (2x).
void rgb_from_attr(uint8_t attr, uint8_t* rgb);

// A 16-entry palette.  Defaults to the standard EGA colour set (the values the
// BIOS programs at mode set, e.g. index 6 = brown, 8 = dark gray).
struct Palette {
    uint8_t rgb[16][3];
    Palette();
    // Program the palette from 16 attribute selectors (as the game does via
    // INT 10h AH=10h AL=02h).
    void set_from_attr(const uint8_t* attr16) {
        for (int i = 0; i < 16; i++) rgb_from_attr(attr16[i], rgb[i]);
    }
};

// A 320x200 (or up to 640x350) 16-colour indexed framebuffer.
struct Screen {
    int width = 320;
    int height = 200;
    Palette pal;
    std::vector<uint8_t> pixels;   // width*height palette indices (0..15)

    void resize(int w, int h) {
        width = w; height = h; pixels.assign((size_t)w * h, 0);
    }
    void clear(uint8_t idx = 0) {
        std::fill(pixels.begin(), pixels.end(), idx);
    }
    inline void put(int x, int y, uint8_t idx) {
        if (x >= 0 && y >= 0 && x < width && y < height)
            pixels[(size_t)y * width + x] = idx;
    }
};

// Decoded 4-plane EGA image (see tools/ega.py for the format description).
struct EgaImage {
    int width_bytes = 0;   // bytes per row
    int height = 0;
    std::vector<uint8_t> planes;   // 4 concatenated bitplanes, each width_bytes*height
};

// Load and RLE-decompress an EGA/*.image file. Returns false on error.
bool load_ega_image(const std::string& path, EgaImage& out, std::string* err = nullptr);

// The 16-entry EGA/CGA-compatible RGB palette used when the game does not
// reprogram the attribute controller.
extern const uint8_t kEgaPalette[16][3];

// Draw `img` onto `scr` at (dx,dy) using colour `idx` for every set pixel.
// (The game's blitter draws single-colour masks; the full-colour compositing of
// multi-plane images is handled by blit_image below.)
void blit_mask(Screen& scr, const EgaImage& img, int dx, int dy, uint8_t idx);

// Draw the whole multi-plane image as colour indices at (dx,dy).
void blit_image(Screen& scr, const EgaImage& img, int dx, int dy);

// Present the screen in an SDL window (blocking until closed) when built with
// SDL2; otherwise a no-op.  `title` is the window caption.
void present_sdl(const Screen& scr, const std::string& title);

// Write the screen to a PNG file (used by --shot for headless verification).
bool save_png(const std::string& path, const Screen& scr);

}  // namespace lofport

#endif
