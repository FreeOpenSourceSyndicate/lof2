// lof — native drop-in replacement for LOF.EXE (Legend of Faerghail game).
//
// Does NOT run LOF.EXE.  Reads the game's data files and reproduces the engine.
// Currently implemented: the boot sequence up to and including the title
// screen; the remaining screens are filled in as the data formats are decoded.
#include "gfx.h"
#include "data.h"
#include "screens.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

using namespace lofport;

static std::string game_dir(const char* argv0) {
    std::string p = argv0 ? argv0 : ".";
    size_t s = p.find_last_of('/');
    if (s != std::string::npos) return p.substr(0, s);
    return ".";
}

// --dump-data: print the parsed system data, proving the readers work on the
// real files without executing the originals.
static void dump_data(const std::string& dir) {
    auto cls = load_snsc(dir + "/SYST/SNSC");
    printf("SYST/SNSC: %zu classes\n", cls.size());
    for (size_t i = 0; i < cls.size(); i++)
        printf("  [%2zu] %-16s / %s\n", i, cls[i].singular.c_str(), cls[i].plural.c_str());
    for (int n = 0; n <= 7; n++) {
        char p[32]; snprintf(p, sizeof p, "/SYST/NSC%02d", n);
        auto r = load_nsc(dir + p);
        if (!r.empty()) {
            printf("SYST/NSC%02d: %zu entries; first: %s / %s\n", n, r.size(),
                   r[0].singular.c_str(), r[0].plural.c_str());
        }
    }
    for (int n = 0; n <= 7; n++) {
        char p[32]; snprintf(p, sizeof p, "/SYST/TXT%d", n);
        auto t = load_txt(dir + p);
        if (!t.empty())
            printf("SYST/TXT%d: %zu strings; first: \"%s\"\n", n, t.size(), t[0].c_str());
    }
}

int main(int argc, char** argv) {
    std::string dir = game_dir(argv[0]);
    bool headless = false, have_shot = false, dump = false;
    std::string shot;
    long shot_frame = -1;

    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if (a == "--dir" && i + 1 < argc) dir = argv[++i];
        else if (a == "--headless") headless = true;
        else if (a == "--dump-data") dump = true;
        else if (a == "--shot" && i + 1 < argc) { shot = argv[++i]; have_shot = true; }
        else if (a == "--shot-frame" && i + 1 < argc) shot_frame = atol(argv[++i]);
    }

    if (dump) { dump_data(dir); return 0; }

    Screen scr;
    scr.resize(320, 200);
    scr.clear(0);

    // --screen NAME renders a pre-decoded full-screen image (EGA/IN/NAME) and
    // exits; used to reproduce/verify the credits screen and other stills.
    std::string screen;
    for (int i = 1; i < argc; i++)
        if (std::string(argv[i]) == "--screen" && i + 1 < argc) screen = argv[++i];
    if (!screen.empty()) {
        std::string err;
        // NAME -> EGA/IN/NAME (full screens); SUB/NAME -> EGA/SUB/NAME.
        std::string rel = screen.find('/') == std::string::npos ? "IN/" + screen : screen;
        if (!show_ega(scr, dir + "/EGA/" + rel, false, &err)) {
            fprintf(stderr, "lof: %s\n", err.c_str());
            return 1;
        }
        if (have_shot && !save_png(shot, scr)) { fprintf(stderr, "lof: cannot write %s\n", shot.c_str()); return 1; }
        if (!headless) present_sdl(scr, "Legend of Faerghail");
        return 0;
    }

    // LOF's intro sequence (from the asset table at image offset 0x35EC8):
    // title graphic (EGA/IN/TITL) -> credits still (EGA/IN/CRED) -> main menu,
    // which is the menu background (EGA/UP/MSCR) with the menu frame
    // (EGA/UP/MENU) composited at (7,4).  Screen positions were recovered by
    // template-matching the decoded assets against DOSBox captures.
    std::string err;
    std::vector<Screen> frames;

    Screen title;
    title.resize(320, 200);
    title.clear(0);
    if (show_ega(title, dir + "/EGA/IN/TITL", true, &err)) frames.push_back(title);

    Screen cred;
    cred.resize(320, 200);
    cred.clear(0);
    if (show_ega(cred, dir + "/EGA/IN/CRED", false, &err)) frames.push_back(cred);

    Screen menu;
    menu.resize(320, 200);
    menu.clear(0);
    if (blit_ega(menu, dir + "/EGA/UP/MSCR", 0, 0, &err) &&
        blit_ega(menu, dir + "/EGA/UP/MENU", 7, 4, &err))
        frames.push_back(menu);

    if (frames.empty()) { fprintf(stderr, "lof: %s\n", err.c_str()); return 1; }
    if (shot_frame >= 0 && (size_t)shot_frame < frames.size()) scr = frames[shot_frame];
    else scr = frames.back();

    if (have_shot && !save_png(shot, scr)) {
        fprintf(stderr, "lof: cannot write %s\n", shot.c_str());
        return 1;
    }
    if (!headless) present_sdl_frames(frames, "Legend of Faerghail");
    return 0;
}
