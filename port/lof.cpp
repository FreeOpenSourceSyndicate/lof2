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
#include "dungeon.h"

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

// Compose the in-game play screen: the static HUD background (EGA/UP/MSCR)
// with the first-person viewport tile (EGA/SUB/DPNNN) blitted into the 120x119
// window at (10,11), then the party panel.  `play` is the SUB-relative tile
// path (e.g. "D0/DP001"); `sg` supplies the party when `have_sg` is set.  The
// window geometry, background and panel layout were recovered by matching the
// decoded assets against a DOSBox capture of save slot 2 (ref/oracle/play.png).
static bool compose_play_screen(const std::string& dir, const std::string& play,
                                const SaveGame& sg, bool have_sg, Screen& s,
                                std::string& err) {
    s.resize(320, 200);
    s.clear(0);
    if (!blit_ega(s, dir + "/EGA/UP/MSCR", 0, 0, &err)) return false;
    if (!blit_ega(s, dir + "/EGA/" + play, 10, 11, &err)) return false;
    // Party panel: the six character names in a 3x2 grid, drawn with CHR2's
    // 5x5 small-caps face.  Positions recovered from the oracle.
    if (!have_sg) return true;
    Font small;
    if (!small.load(dir + "/EGA/UP/CHR2", &err)) return false;
    std::vector<std::string> profs = profession_names();
    std::vector<std::string> status = status_short_names();
    static const int kColX[3] = {148, 208, 268};
    static const int kRowY[2] = {9, 37};
    int slot = 0;
    for (const SaveChar& c : sg.chars) {
        if (slot >= 6) break;
        if (c.present == 0 || c.name.empty()) continue;
        int x = kColX[slot % 3], y = kRowY[slot / 3];
        small.draw_small(s, x, y, c.name, 15);
        if (c.profession < profs.size()) {
            small.draw_small(s, x, y + 6, profs[c.profession], 0);
        } else {
            // The engine's profession table has 12 entries; an index past the
            // end prints two '?' glyphs (band 1 cell 36).
            small.draw_small_cell(s, x,     y + 6, 1, 36, 0);
            small.draw_small_cell(s, x + 5, y + 6, 1, 36, 0);
        }
        // Status row: the character's stat (dark gray) at x, then the status
        // word ("GOOD" for the whole party) in white at x+21.  The stat is the
        // save record's +0x1e byte, which the engine prints with "%d"
        // (disassembly 2EEC2) using CHR2 digit cells 27..36, followed by the
        // '%' glyph, which the engine's glyph table places in band 1 cell 37.
        int v = c.panel_stat;
        char num[8];
        int n = snprintf(num, sizeof num, "%d", v);
        for (int k = 0; k < n; k++)
            small.draw_small_cell(s, x + 5 * k, y + 12, 0, 27 + (num[k] - '0'), 8);
        small.draw_small_cell(s, x + 5 * n, y + 12, 1, 37, 8);
        if (!status.empty())
            small.draw_small(s, x + 21, y + 12, status[0], 15);
        slot++;
    }
    return true;
}

// Compose the main menu screen: the static artwork EGA/UP/MSCR (the full
// 320x200 background, which already contains the menu frame), the town scene
// painted into the left viewport, and the menu text in the engine's own font.
// Positions and strings were recovered by template-matching the decoded assets
// and font glyphs against a DOSBox capture of the menu.
static bool compose_menu_screen(const std::string& dir, Screen& menu, std::string& err) {
    menu.resize(320, 200);
    menu.clear(0);
    if (!blit_ega(menu, dir + "/EGA/UP/MSCR", 0, 0, &err)) return false;
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
    // Menu text.  The greeting and "(C)ontinue" are drawn white (index 15) on
    // the gray (7) panel, while the option list is black (0).  Entries are on
    // 8px rows at x=139 with a blank row after "(R)est", 6px glyph advance.
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
    return true;
}

// --dump-dungeon: decode a few DNGS/DNGxx levels and print the 36x36 floor
// plan (and object layer) so the recovered map format is visible from the
// shipped front-end.
static void dump_dungeon(const std::string& dir) {
    static const struct { int d, l; } kLevels[] = {{0,5},{0,0},{1,0},{1,3}};
    for (const auto& lv : kLevels) {
        std::string name = dungeon_file_name(lv.d, lv.l);
        std::string p = dir + "/DNGS/" + name;
        Dungeon m;
        std::string err;
        if (!load_dungeon(p, m, &err)) {
            printf("%s: <missing or malformed: %s>\n", name.c_str(), err.c_str());
            continue;
        }
        printf("%s: h0=%u h1=%u field=%u\n", name.c_str(), m.h0, m.h1, m.field);
        for (const std::string& row : dungeon_ascii(m, false))
            printf("  %s\n", row.c_str());
    }
}

// The interactive front-end loop.  `intro` is the title/credits/menu sequence;
// any key advances it.  At the menu the recovered hot-keys apply: 'O' loads
// `save_path` and switches to the in-game play screen, 'Q'/Esc quits.  Frames
// are presented through an SDL window; without SDL2 there is no input source,
// so the loop exits immediately (headless builds still exercise the composition
// code through the scripted entry points --screen/--play/--shot).
static bool run_ui(const std::string& dir, const std::vector<Screen>& intro,
                   const std::string& save_path, const std::vector<int>& keys,
                   const std::string& shot) {
    Window win;
    win.open("Legend of Faerghail", 320, 200);
    win.script(keys);
    std::string err;
    enum { INTRO, PLAY } state = INTRO;
    size_t frame = 0;
    Screen play_screen;
    bool have_play = false;
    for (;;) {
        if (state == INTRO) win.present(intro[frame]);
        else if (have_play) win.present(play_screen);
        int key = -1; bool quit = false;
        if (!win.wait_key(&key, &quit)) break;   // no SDL: nothing to drive it
        if (quit) break;
        if (state == INTRO) {
            bool on_menu = (frame + 1 == intro.size());
            if (on_menu && key == 'O') {
                SaveGame sg = load_save(save_path);
                std::string vd = sg.ok ? dungeon_view_dir(save_location(sg)) : std::string();
                if (!vd.empty() &&
                    compose_play_screen(dir, vd + "/DP001", sg, true, play_screen, err)) {
                    have_play = true;
                    state = PLAY;
                }
            } else if (on_menu && key == 'Q') {
                break;
            } else if (frame + 1 < intro.size()) {
                frame++;
            }
        } else if (state == PLAY) {
            if (key == 'Q') break;
        }
    }
    win.close();
    if (!shot.empty()) {
        const Screen& s = have_play ? play_screen : intro[frame];
        if (!save_png(shot, s)) { fprintf(stderr, "lof: cannot write %s\n", shot.c_str()); return false; }
    }
    return true;
}

int main(int argc, char** argv) {
    std::string dir = game_dir(argv[0]);
    bool headless = false, have_shot = false, dump = false, dump_spec = false, dump_dng = false;
    bool ui = false;
    std::string shot;
    long shot_frame = -1;

    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if (a == "--dir" && i + 1 < argc) dir = argv[++i];
        else if (a == "--headless") headless = true;
        else if (a == "--dump-data") dump = true;
        else if (a == "--dump-spec") dump_spec = true;
        else if (a == "--dump-dungeon") dump_dng = true;
        else if (a == "--shot" && i + 1 < argc) { shot = argv[++i]; have_shot = true; }
        else if (a == "--shot-frame" && i + 1 < argc) shot_frame = atol(argv[++i]);
        else if (a == "--ui") ui = true;
    }

    if (dump) { dump_data(dir); return 0; }
    if (dump_spec) { dump_spec_files(dir); return 0; }
    if (dump_dng) { dump_dungeon(dir); return 0; }

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

    // --ui runs the interactive front-end: the intro (title -> credits ->
    // menu) followed by a keyboard-driven main loop.  It reproduces the menu
    // keys recovered from the engine: at the menu, 'O' (0x18) loads a save
    // (default GAMES/GAME2) and enters the in-game play screen, 'Q'/Esc quits,
    // any other key advances the intro.  This is the state machine the earlier
    // slideshow lacked; it is scriptable so it can be regression-tested.
    if (ui) {
        std::string ui_save = dir + "/GAMES/GAME2";
        std::vector<int> ui_keys;
        for (int i = 1; i < argc; i++) {
            if (std::string(argv[i]) == "--save" && i + 1 < argc) ui_save = argv[++i];
            else if (std::string(argv[i]) == "--ui-keys" && i + 1 < argc) {
                // comma-separated ASCII codes, e.g. "32,32,79" (space,space,'O')
                std::string s = argv[++i], cur;
                for (size_t j = 0; j <= s.size(); j++) {
                    if (j == s.size() || s[j] == ',') {
                        if (!cur.empty()) ui_keys.push_back(atoi(cur.c_str()));
                        cur.clear();
                    } else cur.push_back(s[j]);
                }
            }
        }
        std::string err;
        Screen title;
        title.resize(320, 200);
        title.clear(0);
        show_ega(title, dir + "/EGA/IN/TITL", true, &err);
        Screen cred;
        cred.resize(320, 200);
        cred.clear(0);
        show_ega(cred, dir + "/EGA/IN/CRED", false, &err);
        Screen menu;
        if (!compose_menu_screen(dir, menu, err)) {
            fprintf(stderr, "lof: %s\n", err.c_str());
            return 1;
        }
        std::vector<Screen> intro = {title, cred, menu};
        if (!run_ui(dir, intro, ui_save, ui_keys, shot)) return 1;
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
        // "auto" picks the view tile from the save's location (EGA/D<ones>/
        // DP001), so a save slot can be rendered without naming the directory.
        SaveGame sg;
        bool have_sg = false;
        if (!save.empty()) {
            sg = load_save(save);
            have_sg = sg.ok;
        }
        if (play == "auto") {
            if (!have_sg) { fprintf(stderr, "lof: --play auto needs a valid --save\n"); return 1; }
            std::string vd = dungeon_view_dir(save_location(sg));
            if (vd.empty()) { fprintf(stderr, "lof: save has no view directory\n"); return 1; }
            play = vd + "/DP001";
        }
        Screen s;
        if (!compose_play_screen(dir, play, sg, have_sg, s, err)) {
            fprintf(stderr, "lof: %s\n", err.c_str());
            return 1;
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

    // Main menu.
    Screen menu;
    if (compose_menu_screen(dir, menu, err)) frames.push_back(menu);

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
