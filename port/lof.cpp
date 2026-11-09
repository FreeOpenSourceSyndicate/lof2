// lof — native drop-in replacement for LOF.EXE (Legend of Faerghail game).
//
// Does NOT run LOF.EXE.  Reads the game's data files and reproduces the engine.
// Currently implemented: the boot sequence up to and including the title
// screen; the remaining screens are filled in as the data formats are decoded.
#include "gfx.h"
#include "data.h"
#include "screens.h"
#include "font.h"
#include "char.h"

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

// --dump-spec: decode SYST/SPEC0..7 (workstream B's reader) so the parsed
// scenario specials are visible from the shipped front-end.
static void dump_spec_files(const std::string& dir) {
    for (int n = 0; n <= 7; n++) {
        char p[32]; snprintf(p, sizeof p, "/SYST/SPEC%d", n);
        SpecTable t = load_spec(dir + p);
        if (!t.ok) continue;
        char label[32]; snprintf(label, sizeof label, "SYST/SPEC%d", n);
        dump_spec(stdout, t, label);
    }
}

int main(int argc, char** argv) {
    std::string dir = game_dir(argv[0]);
    bool headless = false, have_shot = false, dump = false, dump_spec = false;
    std::string shot;
    long shot_frame = -1;

    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if (a == "--dir" && i + 1 < argc) dir = argv[++i];
        else if (a == "--headless") headless = true;
        else if (a == "--dump-data") dump = true;
        else if (a == "--dump-spec") dump_spec = true;
        else if (a == "--shot" && i + 1 < argc) { shot = argv[++i]; have_shot = true; }
        else if (a == "--shot-frame" && i + 1 < argc) shot_frame = atol(argv[++i]);
    }

    if (dump) { dump_data(dir); return 0; }
    if (dump_spec) { dump_spec_files(dir); return 0; }

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

    // --play SUB/DPNNN composes the in-game play screen and exits: the static
    // HUD background (EGA/UP/MSCR) with the first-person viewport tile
    // (EGA/SUB/DPNNN, e.g. D0/DP001) blitted into the 120x119 window at (10,11),
    // then the party-panel names (from the save named by --save) drawn in the
    // compact small-caps face.  The window geometry, the background and the
    // panel layout were recovered by matching the decoded assets against a
    // DOSBox capture of save slot 2 (ref/oracle/play.png).
    std::string play, save;
    for (int i = 1; i < argc; i++) {
        if (std::string(argv[i]) == "--play" && i + 1 < argc) play = argv[++i];
        else if (std::string(argv[i]) == "--save" && i + 1 < argc) save = argv[++i];
    }
    if (!play.empty()) {
        std::string err;
        Screen s;
        s.resize(320, 200);
        s.clear(0);
        if (!blit_ega(s, dir + "/EGA/UP/MSCR", 0, 0, &err)) { fprintf(stderr, "lof: %s\n", err.c_str()); return 1; }
        if (!blit_ega(s, dir + "/EGA/" + play, 10, 11, &err)) { fprintf(stderr, "lof: %s\n", err.c_str()); return 1; }
        // Party panel: the six character names in a 3x2 grid, drawn with CHR2's
        // 5x5 small-caps face.  Positions recovered from the oracle.
        if (!save.empty()) {
            Font small;
            if (small.load(dir + "/EGA/UP/CHR2", &err)) {
                SaveGame sg = load_save(save);
                std::vector<std::string> profs = profession_names();
                static const int kColX[3] = {148, 208, 268};
                static const int kRowY[2] = {9, 37};
                int slot = 0;
                for (const SaveChar& c : sg.chars) {
                    if (slot >= 6) break;
                    if (c.present == 0 || c.name.empty()) continue;
                    int x = kColX[slot % 3], y = kRowY[slot / 3];
                    small.draw_small(s, x, y, c.name, 15);
                    if (c.profession < profs.size())
                        small.draw_small(s, x, y + 6, profs[c.profession], 0);
                    slot++;
                }
            }
        }
        if (have_shot && !save_png(shot, s)) { fprintf(stderr, "lof: cannot write %s\n", shot.c_str()); return 1; }
        if (!headless) present_sdl(s, "Legend of Faerghail");
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

    // Main menu.  The static artwork is EGA/UP/MSCR (the full 320x200 background,
    // which already contains the menu frame -- no separate frame image is blitted).
    // The engine then paints the town scene into the left viewport and draws the
    // menu text with its own font.  Positions and strings were recovered by
    // template-matching the decoded assets and font glyphs against a DOSBox
    // capture of the menu.
    Screen menu;
    menu.resize(320, 200);
    menu.clear(0);
    if (blit_ega(menu, dir + "/EGA/UP/MSCR", 0, 0, &err)) {
        // Left viewport (118x119).  The town scene EGA/TWN/TWN03 is scrolled so
        // that its column 0 lands at screen x=26; the 14 columns that scroll off
        // the left edge (TWN03 columns 106..119) wrap back in at x=12..25.  The
        // wrapped strip is painted one scanline higher, matching the engine's
        // two-tile blit (verified pixel-for-pixel against a DOSBox capture).
        EgaImage twn;
        if (load_ega_image(dir + "/EGA/TWN/TWN03", twn, nullptr)) {
            int th = twn.height;
            size_t pl = (size_t)twn.width_bytes * th;
            auto px = [&](int x, int y) -> uint8_t {
                int xb = x >> 3, s = 7 - (x & 7);
                const uint8_t* p = twn.planes.data() + (size_t)y * twn.width_bytes + xb;
                return (uint8_t)(((p[0] >> s) & 1) | (((p[pl] >> s) & 1) << 1) |
                                 (((p[2 * pl] >> s) & 1) << 2) | (((p[3 * pl] >> s) & 1) << 3));
            };
            for (int sx = 26; sx < 130; sx++)           // main tile at (26,11)
                for (int y = 0; y < th && 11 + y < 130; y++)
                    menu.put(sx, 11 + y, px(sx - 26, y));
            for (int sx = 10; sx < 26; sx++)            // wrapped strip at (10,12)
                for (int y = 0; y < th && 12 + y < 130; y++)
                    menu.put(sx, 12 + y, px(104 + (sx - 10), y));
        }
        // Menu text.  The greeting and "(C)ontinue" are drawn white (index 15)
        // on the gray (7) panel, while the option list is black (0).  Verified
        // against the DOSBox capture.  Entries are on 8px rows at x=139 with a
        // blank row after "(R)est", and a 6px glyph advance.
        Font font;
        if (font.load(dir + "/EGA/UP/CHR1", &err)) {
            font.advance = 6;
            font.draw(menu, 139, 92, "Welcome, hardy", 15);
            font.draw(menu, 139, 100, "adventurers!", 15);
            static const struct { int y; const char* text; } kEntries[] = {
                {116, "(P)ick-pocket"}, {124, "(R)est"},
                {140, "(L)ook for new character"}, {148, "(E)rase a character"},
                {156, "(A)dd a new character"}, {164, "(D)ismiss a character"},
                {172, "(O)pen file"},
            };
            for (const auto& e : kEntries)
                font.draw(menu, 139, e.y, e.text, 0);
            font.draw(menu, 139, 188, "(C)ontinue", 15);
        }
        frames.push_back(menu);
    }

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
