// vor — native drop-in replacement for VOR.EXE (Legend of Faerghail intro).
//
// Reproduces the original intro/protection program without running VOR.EXE:
// it loads the title graphic from EGA/IN/DRAG, shows it full screen, waits for
// a key, and exits.  START.BAT runs `vor` and then `lof`.
#include "gfx.h"
#include "screens.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

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
    long shot_frame = -1;

    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if (a == "--dir" && i + 1 < argc) dir = argv[++i];
        else if (a == "--headless") headless = true;
        else if (a == "--shot" && i + 1 < argc) { shot = argv[++i]; have_shot = true; }
        else if (a == "--shot-frame" && i + 1 < argc) shot_frame = atol(argv[++i]);
    }

    // VOR shows the title graphic (EGA/IN/DRAG), then the credits still
    // (EGA/IN/CRED); LOF.EXE shows the same credits image again afterwards.
    std::string err;
    std::vector<Screen> frames;
    for (const char* name : {"DRAG", "CRED"}) {
        Screen s;
        s.resize(320, 200);
        s.clear(0);
        if (!show_ega(s, dir + "/EGA/IN/" + name, false, &err)) {
            fprintf(stderr, "vor: %s\n", err.c_str());
            return 1;
        }
        frames.push_back(std::move(s));
    }

    Screen scr = frames.front();
    if (shot_frame >= 0 && (size_t)shot_frame < frames.size()) scr = frames[shot_frame];

    if (have_shot && !save_png(shot, scr)) {
        fprintf(stderr, "vor: cannot write %s\n", shot.c_str());
        return 1;
    }
    if (!headless) present_sdl_frames(frames, "Legend of Faerghail");
    return 0;
}
