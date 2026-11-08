#include "gfx.h"

#include <cstdio>
#include <cstring>

#ifdef USE_SDL
#include <SDL.h>
#endif

namespace lofport {

// Decode an EGA 6-bit attribute colour selector to RGB, exactly like the
// hardware: bits 0-2 are the secondary (1x) guns, bits 3-5 the primary (2x).
void rgb_from_attr(uint8_t attr, uint8_t* rgb) {
    attr &= 0x3F;
    rgb[0] = (uint8_t)(((attr >> 2) & 1) * 170 + ((attr >> 5) & 1) * 85);  // R
    rgb[1] = (uint8_t)(((attr >> 1) & 1) * 170 + ((attr >> 4) & 1) * 85);  // G
    rgb[2] = (uint8_t)(((attr >> 0) & 1) * 170 + ((attr >> 3) & 1) * 85);  // B
}

// The palette the game programs at start-up via INT 10h AH=10h AL=02h.  LOF
// remaps attribute indices 0..7 to the low-intensity colour set and 8..15 to
// the same eight hues at high intensity (index 6 becomes yellow, not brown, and
// index 8 dark green, not dark gray), so the stock EGA/BGI palette renders the
// menu, town and intro in the wrong colours.
//
// The eight selectors are the low-intensity hues black/blue/green/cyan/
// red/magenta/brown/gray; index i>=8 selects the same hue at double intensity.
// These are the values LOF writes (the tracer at INT 10h AH=10h AL=02h shows
// data 00 01 02 03 04 05 06 07 10 11 12 13 14 15 16 17).
static uint8_t game_attr(int i) { return (uint8_t)(i < 8 ? i : i + 8); }

Palette::Palette() {
    for (int i = 0; i < 16; i++) rgb_from_attr(game_attr(i), rgb[i]);
}

// Image header (11 bytes): escape byte, 0x1234 magic, width (word),
// height (word), decompressed-size+10 (word), 2 reserved bytes.  The RLE
// stream starts right after the 11-byte header.  (Verified against LOF.EXE's
// loader at image offset 0x1D5E and by the font strip decoding cleanly.)
static const size_t kHeader = 11;

bool load_ega_image(const std::string& path, EgaImage& out, std::string* err) {
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) { if (err) *err = "cannot open " + path; return false; }
    std::vector<uint8_t> d;
    {
        fseek(f, 0, SEEK_END);
        long n = ftell(f);
        fseek(f, 0, SEEK_SET);
        if (n <= 0) { fclose(f); if (err) *err = "empty " + path; return false; }
        d.resize(n);
        if (fread(d.data(), 1, n, f) != (size_t)n) {
            fclose(f); if (err) *err = "short read " + path; return false;
        }
    }
    fclose(f);

    if (d.size() < kHeader || d[1] != 0x34 || d[2] != 0x12) {
        if (err) *err = path + ": bad 0x1234 magic";
        return false;
    }
    uint8_t esc = d[0];
    int w = d[3] | (d[4] << 8);
    int h = d[5] | (d[6] << 8);

    // RLE: literals copy through; `esc` introduces (count, value).
    std::vector<uint8_t> planes;
    size_t want = (size_t)w * h * 4;
    planes.reserve(want);
    size_t i = kHeader;
    while (i < d.size() && planes.size() < want) {
        uint8_t b = d[i++];
        if (b == esc) {
            if (i + 1 >= d.size()) break;
            uint8_t cnt = d[i];
            uint8_t val = d[i + 1];
            i += 2;
            size_t room = want - planes.size();
            planes.insert(planes.end(), cnt < room ? cnt : room, val);
        } else {
            planes.push_back(b);
        }
    }
    // Some images (e.g. IN/DRAG, IN/CRED) end early with the remaining planes
    // implicitly zero; pad so the blit never reads past the buffer.
    planes.resize(want, 0);

    out.width_bytes = w;
    out.height = h;
    out.planes.swap(planes);
    return true;
}

void blit_mask(Screen& scr, const EgaImage& img, int dx, int dy, uint8_t idx) {
    int w = img.width_bytes, h = img.height;
    size_t pl = (size_t)w * h;
    for (int y = 0; y < h; y++) {
        int sy = dy + y;
        if (sy < 0 || sy >= scr.height) continue;
        const uint8_t* p0 = img.planes.data() + (size_t)y * w;
        const uint8_t* p1 = img.planes.data() + pl + (size_t)y * w;
        const uint8_t* p2 = img.planes.data() + 2 * pl + (size_t)y * w;
        const uint8_t* p3 = img.planes.data() + 3 * pl + (size_t)y * w;
        for (int xb = 0; xb < w; xb++) {
            uint8_t b0 = p0[xb], b1 = p1[xb], b2 = p2[xb], b3 = p3[xb];
            for (int bit = 0; bit < 8; bit++) {
                int s = 7 - bit;
                uint8_t v = (uint8_t)(((b0 >> s) & 1) | (((b1 >> s) & 1) << 1) |
                                      (((b2 >> s) & 1) << 2) | (((b3 >> s) & 1) << 3));
                if (v) scr.put(dx + xb * 8 + bit, sy, idx);
            }
        }
    }
}

void blit_image(Screen& scr, const EgaImage& img, int dx, int dy) {
    int w = img.width_bytes, h = img.height;
    size_t pl = (size_t)w * h;
    for (int y = 0; y < h; y++) {
        int sy = dy + y;
        if (sy < 0 || sy >= scr.height) continue;
        const uint8_t* p0 = img.planes.data() + (size_t)y * w;
        const uint8_t* p1 = img.planes.data() + pl + (size_t)y * w;
        const uint8_t* p2 = img.planes.data() + 2 * pl + (size_t)y * w;
        const uint8_t* p3 = img.planes.data() + 3 * pl + (size_t)y * w;
        for (int xb = 0; xb < w; xb++) {
            uint8_t b0 = p0[xb], b1 = p1[xb], b2 = p2[xb], b3 = p3[xb];
            for (int bit = 0; bit < 8; bit++) {
                int s = 7 - bit;
                uint8_t v = (uint8_t)(((b0 >> s) & 1) | (((b1 >> s) & 1) << 1) |
                                      (((b2 >> s) & 1) << 2) | (((b3 >> s) & 1) << 3));
                scr.put(dx + xb * 8 + bit, sy, v);
            }
        }
    }
}

void present_sdl(const Screen& scr, const std::string& title) {
    std::vector<Screen> one{scr};
    present_sdl_frames(one, title);
}

// Show a sequence of screens in one SDL window.  Each screen stays up until the
// user presses a key (advance) or closes the window (stop).  Used to reproduce
// the intro: title graphic -> credits still -> menu.
void present_sdl_frames(const std::vector<Screen>& frames, const std::string& title) {
#ifdef USE_SDL
    if (frames.empty()) return;
    const int W = frames[0].width, H = frames[0].height;
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return;
    }
    SDL_Window* win = SDL_CreateWindow(title.c_str(), SDL_WINDOWPOS_CENTERED,
                                       SDL_WINDOWPOS_CENTERED, W, H,
                                       SDL_WINDOW_SHOWN);
    SDL_Renderer* ren = win ? SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED) : nullptr;
    if (ren && !ren) ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_SOFTWARE);
    SDL_Texture* tex = ren ? SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888,
                                               SDL_TEXTUREACCESS_STREAMING, W, H) : nullptr;

    std::vector<uint32_t> fb((size_t)W * H);
    bool done = false;
    for (size_t f = 0; f < frames.size() && !done; f++) {
        const Screen& scr = frames[f];
        for (size_t i = 0; i < fb.size(); i++) {
            const uint8_t* c = scr.pal.rgb[scr.pixels[i] & 15];
            fb[i] = 0xFF000000u | ((uint32_t)c[2] << 16) | ((uint32_t)c[1] << 8) | c[0];
        }
        bool advance = false;
        while (!advance && !done) {
            SDL_Event e;
            while (SDL_PollEvent(&e)) {
                if (e.type == SDL_QUIT) done = true;
                else if (e.type == SDL_KEYDOWN) advance = true;
            }
            if (tex) {
                SDL_UpdateTexture(tex, nullptr, fb.data(), W * 4);
                SDL_RenderClear(ren);
                SDL_RenderCopy(ren, tex, nullptr, nullptr);
                SDL_RenderPresent(ren);
            }
            SDL_Delay(16);
        }
    }
    if (tex) SDL_DestroyTexture(tex);
    if (ren) SDL_DestroyRenderer(ren);
    if (win) SDL_DestroyWindow(win);
    SDL_Quit();
#else
    (void)frames; (void)title;
#endif
}

// Minimal PNG writer (zlib "stored" deflate blocks, no external dependency).
static uint32_t crc32(const uint8_t* p, size_t n) {
    static uint32_t table[256];
    static bool init = false;
    if (!init) {
        for (uint32_t i = 0; i < 256; i++) {
            uint32_t c = i;
            for (int k = 0; k < 8; k++) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : (c >> 1);
            table[i] = c;
        }
        init = true;
    }
    uint32_t c = 0xFFFFFFFFu;
    for (size_t i = 0; i < n; i++) c = table[(c ^ p[i]) & 0xFF] ^ (c >> 8);
    return c ^ 0xFFFFFFFFu;
}

static void put_be32(std::vector<uint8_t>& v, uint32_t x) {
    v.push_back((uint8_t)(x >> 24)); v.push_back((uint8_t)(x >> 16));
    v.push_back((uint8_t)(x >> 8)); v.push_back((uint8_t)x);
}

static void chunk(std::vector<uint8_t>& out, const char* type, const std::vector<uint8_t>& data) {
    put_be32(out, (uint32_t)data.size());
    size_t start = out.size();
    out.insert(out.end(), type, type + 4);
    out.insert(out.end(), data.begin(), data.end());
    put_be32(out, crc32(out.data() + start, out.size() - start));
}

bool save_png(const std::string& path, const Screen& scr) {
    int w = scr.width, h = scr.height;
    std::vector<uint8_t> raw;
    raw.reserve((size_t)(w * 3 + 1) * h);
    for (int y = 0; y < h; y++) {
        raw.push_back(0);  // filter: none
        for (int x = 0; x < w; x++) {
            const uint8_t* c = scr.pal.rgb[scr.pixels[(size_t)y * w + x] & 15];
            raw.push_back(c[0]); raw.push_back(c[1]); raw.push_back(c[2]);
        }
    }
    // Wrap in zlib stored blocks.
    std::vector<uint8_t> z;
    z.push_back(0x78); z.push_back(0x01);
    size_t pos = 0;
    while (pos < raw.size()) {
        size_t n = std::min<size_t>(65535, raw.size() - pos);
        bool last = (pos + n == raw.size());
        z.push_back(last ? 1 : 0);
        uint16_t len = (uint16_t)n, nlen = (uint16_t)~len;
        z.push_back((uint8_t)(len & 0xFF)); z.push_back((uint8_t)(len >> 8));
        z.push_back((uint8_t)(nlen & 0xFF)); z.push_back((uint8_t)(nlen >> 8));
        z.insert(z.end(), raw.begin() + pos, raw.begin() + pos + n);
        pos += n;
    }
    uint32_t adler = 1, s1 = 1, s2 = 0;
    for (uint8_t b : raw) { s1 = (s1 + b) % 65521; s2 = (s2 + s1) % 65521; }
    adler = (s2 << 16) | s1;
    put_be32(z, adler);

    std::vector<uint8_t> out = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    std::vector<uint8_t> ihdr;
    put_be32(ihdr, w); put_be32(ihdr, h);
    ihdr.push_back(8); ihdr.push_back(2); ihdr.push_back(0);
    ihdr.push_back(0); ihdr.push_back(0);
    chunk(out, "IHDR", ihdr);
    chunk(out, "IDAT", z);
    chunk(out, "IEND", {});

    FILE* f = fopen(path.c_str(), "wb");
    if (!f) return false;
    fwrite(out.data(), 1, out.size(), f);
    fclose(f);
    return true;
}

}  // namespace lofport
