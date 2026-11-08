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

// Standard EGA 16-colour palette (RGB triples), 0..15 — the default palette the
// BIOS programs at mode set (index 6 = brown, 8 = dark gray).
const uint8_t kEgaPalette[16][3] = {
    {0x00, 0x00, 0x00}, {0x00, 0x00, 0xAA}, {0x00, 0xAA, 0x00}, {0x00, 0xAA, 0xAA},
    {0xAA, 0x00, 0x00}, {0xAA, 0x00, 0xAA}, {0xAA, 0x55, 0x00}, {0xAA, 0xAA, 0xAA},
    {0x55, 0x55, 0x55}, {0x55, 0x55, 0xFF}, {0x55, 0xFF, 0x55}, {0x55, 0xFF, 0xFF},
    {0xFF, 0x55, 0x55}, {0xFF, 0x55, 0xFF}, {0xFF, 0xFF, 0x55}, {0xFF, 0xFF, 0xFF},
};

Palette::Palette() {
    for (int i = 0; i < 16; i++) {
        rgb[i][0] = kEgaPalette[i][0];
        rgb[i][1] = kEgaPalette[i][1];
        rgb[i][2] = kEgaPalette[i][2];
    }
}

// The image header is: escape byte, 0x1234 magic, width (word), height (word).
static const size_t kHeader = 7;

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
    planes.reserve((size_t)w * h * 4);
    size_t i = kHeader;
    while (i < d.size()) {
        uint8_t b = d[i++];
        if (b == esc) {
            if (i + 1 >= d.size()) break;
            uint8_t cnt = d[i];
            uint8_t val = d[i + 1];
            i += 2;
            planes.insert(planes.end(), cnt, val);
        } else {
            planes.push_back(b);
        }
    }

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
#ifdef USE_SDL
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return;
    }
    SDL_Window* win = SDL_CreateWindow(title.c_str(), SDL_WINDOWPOS_CENTERED,
                                       SDL_WINDOWPOS_CENTERED, scr.width, scr.height,
                                       SDL_WINDOW_SHOWN);
    SDL_Renderer* ren = win ? SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED) : nullptr;
    if (ren && !ren) ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_SOFTWARE);
    SDL_Texture* tex = ren ? SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888,
                                               SDL_TEXTUREACCESS_STREAMING,
                                               scr.width, scr.height) : nullptr;

    std::vector<uint32_t> fb((size_t)scr.width * scr.height);
    for (size_t i = 0; i < fb.size(); i++) {
        const uint8_t* c = scr.pal.rgb[scr.pixels[i] & 15];
        fb[i] = 0xFF000000u | ((uint32_t)c[2] << 16) | ((uint32_t)c[1] << 8) | c[0];
    }
    bool done = false;
    while (!done) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) done = true;
            else if (e.type == SDL_KEYDOWN) done = true;
        }
        if (tex) {
            SDL_UpdateTexture(tex, nullptr, fb.data(), scr.width * 4);
            SDL_RenderClear(ren);
            SDL_RenderCopy(ren, tex, nullptr, nullptr);
            SDL_RenderPresent(ren);
        }
        SDL_Delay(16);
    }
    if (tex) SDL_DestroyTexture(tex);
    if (ren) SDL_DestroyRenderer(ren);
    if (win) SDL_DestroyWindow(win);
    SDL_Quit();
#else
    (void)scr; (void)title;
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
