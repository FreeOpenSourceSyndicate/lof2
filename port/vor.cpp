// vor — native drop-in replacement for VOR.EXE (Legend of Faerghail intro).
//
// Reproduces the original intro/protection program without running VOR.EXE:
// it loads the title graphic from EGA/IN/DRAG, shows it full screen, waits for
// a key, and exits.  START.BAT runs `vor` and then `lof`.
#include "gfx.h"

#include <cstdio>
#include <cstring>
#include <string>

using namespace lofport;

// The game data directory: the directory holding this executable, or --dir.
static std::string game_dir(const char* argv0) {
    std::string p = argv0 ? argv0 : ".";
    size_t s = p.find_last_of('/');
    if (s != std::string::npos) return p.substr(0, s);
    return ".";
}

int main(int argc, char** argv) {
    std::string dir = game_dir(argv[0]);
    bool headless = false, have_shot = false;
    std::string shot;

    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if (a == "--dir" && i + 1 < argc) dir = argv[++i];
        else if (a == "--headless") headless = true;
        else if (a == "--shot" && i + 1 < argc) { shot = argv[++i]; have_shot = true; }
    }

    Screen scr;
    scr.resize(320, 200);
    scr.clear(0);

    EgaImage img;
    std::string err;
    std::string path = dir + "/EGA/IN/DRAG";
    if (!load_ega_image(path, img, &err)) {
        fprintf(stderr, "vor: %s\n", err.c_str());
        return 1;
    }
    blit_image(scr, img, 0, 0);

    if (have_shot && !save_png(shot, scr)) {
        fprintf(stderr, "vor: cannot write %s\n", shot.c_str());
        return 1;
    }
    if (!headless) present_sdl(scr, "Legend of Faerghail");
    return 0;
}
