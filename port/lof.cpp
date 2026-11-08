// lof — native drop-in replacement for LOF.EXE (Legend of Faerghail game).
//
// Does NOT run LOF.EXE.  Reads the game's data files and reproduces the engine.
// Currently implemented: the boot sequence up to and including the title
// screen; the remaining screens are filled in as the data formats are decoded.
#include "gfx.h"

#include <cstdio>
#include <cstring>
#include <string>

using namespace lofport;

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

    // LOF's boot screen draws the EGA/IN/TITL logo.  Its position and the rest
    // of the boot sequence are still being reverse-engineered; render the logo
    // centred for now so the drop-in already produces the opening image.
    EgaImage titl;
    std::string err;
    if (load_ega_image(dir + "/EGA/IN/TITL", titl, &err)) {
        int x = (scr.width - titl.width_bytes * 8) / 2;
        int y = (scr.height - titl.height) / 2;
        blit_image(scr, titl, x, y);
    } else {
        fprintf(stderr, "lof: %s\n", err.c_str());
    }

    if (have_shot && !save_png(shot, scr)) {
        fprintf(stderr, "lof: cannot write %s\n", shot.c_str());
        return 1;
    }
    if (!headless) present_sdl(scr, "Legend of Faerghail");
    return 0;
}
