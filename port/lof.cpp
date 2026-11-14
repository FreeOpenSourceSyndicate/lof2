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

    // Lower-left status box.  It is drawn with the *full* CHR1 face at a 6-px
    // pitch, five label lines at y = 136/144/160/176/184:
    //
    //   line 1  the scenario title (the region the save's location belongs to)
    //   line 2  the party facing (N/E/S/W)
    //   line 4  the in-game time of day
    //   line 6  "Spirits"  + the party's morale
    //   line 7  "Rations"  + the party's rations
    //
    // Only the first two are recoverable from a save file: the title is the
    // location's scenario (the ones digit indexes `scenario_names()`) and the
    // facing is the save's +0x9a6 digit byte.  The time of day, morale and
    // rations are *live* engine state (the clock, the party's mood and the
    // per-character ration count the engine accumulates at run time) that a
    // save alone does not carry, so the port draws their labels only, which is
    // exactly what the engine itself shows before those values are set.
    Font box;
    if (box.load(dir + "/EGA/UP/CHR1", &err)) {
        std::vector<std::string> scen = scenario_names();
        std::vector<std::string> face = facing_names();
        int loc = (int)save_location(sg);
        int sindex = loc % 10;   // scenario the location belongs to
        std::string title = (sindex >= 0 && sindex < (int)scen.size())
                                ? scen[sindex] : "    Scenario !!      ";
        box.draw_text(s, 6, 136, title, 15);
        std::string heading = (sg.facing < face.size())
                                  ? face[sg.facing] : "    Direction !!     ";
        box.draw_text(s, 6, 144, heading, 15);
        box.draw_text(s, 6, 176, "Spirits  ", 15);
        box.draw_text(s, 6, 184, "Rations  ", 15);
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

// One text row of a town/building screen: y position, string, and colour
// (15 = white, 0 = black).
struct TownTextRow { int y; const char* text; uint8_t idx; };

// Compose a town-style screen: the HUD background (EGA/UP/MSCR), a town scene
// tile (EGA/TWN/<tile>) in the 120x119 viewport at (10,11), then the engine's
// message + option text in CHR1's full face at a 6-px pitch, x=139.  Every
// town building shares this layout; only the viewport tile and the text rows
// differ.  The viewport tile is the building's exterior art: TWN00 is the
// town-menu street, TWN01/02/04 are the Emporium/Bank/House exteriors.
static bool compose_building_screen(const std::string& dir, const char* tile,
                                    const TownTextRow* rows, size_t n,
                                    Screen& s, std::string& err) {
    s.resize(320, 200);
    s.clear(0);
    if (!blit_ega(s, dir + "/EGA/UP/MSCR", 0, 0, &err)) return false;
    if (!blit_ega(s, dir + "/EGA/TWN/" + tile, 10, 11, &err)) return false;
    Font font;
    if (!font.load(dir + "/EGA/UP/CHR1", &err)) return false;
    for (size_t i = 0; i < n; i++)
        font.draw_text(s, 139, rows[i].y, rows[i].text, rows[i].idx);
    return true;
}

// Compose the in-game town screen (the building menu you get on entering a
// town): MSCR + the street viewport EGA/TWN/TWN00 + the message and building
// list.  Recovered by template-matching the decoded assets and font glyphs
// against the emulator's town frame (tests/golden_test.sh scenario `town`):
// every glyph matches a CHR1 cell exactly.
static bool compose_town_screen(const std::string& dir, Screen& town, std::string& err) {
    static const TownTextRow kText[] = {
        { 92, "You enter the proud town of", 15},
        {100, "Thyn. Which building do you", 15},
        {108, "want to visit?", 15},
        {132, "(E)mporium", 0},
        {140, "(B)ank", 0},
        {156, "(I)nn", 0},
        {164, "(H)ouse of trades", 0},
        {172, "(T)emple", 0},
        {188, "(C)ontinue", 15},
    };
    return compose_building_screen(dir, "TWN00", kText,
                                   sizeof(kText) / sizeof(kText[0]), town, err);
}

// Compose the Emporium screen: MSCR + the shop exterior EGA/TWN/TWN01 + the
// shopkeeper's greeting and the buy/sell/repair/gather menu.  Decoded from the
// emulator's `shop` golden frame (tests/golden_test.sh scenario `shop`); the
// greeting ("Well hi, wealthy heroes, what / would you like, huh?") is drawn
// white (15) on the light-gray MSCR box, the options black (0).  The strings
// are the engine's own (LOF.EXE ds:0x5B71/0x5B8B/0x5BA5/...); the greeting
// wraps after "what" (the engine's own row break, not a reflow).
static bool compose_emporium_screen(const std::string& dir, Screen& s, std::string& err) {
    static const TownTextRow kText[] = {
        { 92, "Well hi, wealthy heroes, what", 15},
        {100, "would you like, huh?", 15},
        {116, "(B)uy wares", 0},
        {124, "(S)ell wares", 0},
        {140, "(R)epair wares", 0},
        {156, "(G)ather gold", 0},
        {172, "(C)ontinue", 15},
    };
    return compose_building_screen(dir, "TWN01", kText,
                                   sizeof(kText) / sizeof(kText[0]), s, err);
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

// Draw the dungeon level inside the viewport rectangle (walk mode): the 36x36
// tiles at `cell` px each, coloured by code (walls grey, floor dark grey, void
// black), with the party marked and its facing shown as an arrow.  Uses agent
// A's Dungeon decoder; kept inside the 120x119 window so the HUD and party
// panel stay visible around it.
static void draw_viewport_map(Screen& s, const Dungeon& d, const DungeonPos& p) {
    int cell = kViewportH / kDungeonH;
    if (cell < 1) cell = 1;
    int w = kDungeonW * cell, h = kDungeonH * cell;
    int ox = kViewportX + (kViewportW - w) / 2;
    int oy = kViewportY + (kViewportH - h) / 2;
    for (int y = 0; y < kDungeonH; y++) {
        for (int x = 0; x < kDungeonW; x++) {
            uint8_t t = d.tile(x, y);
            uint8_t c = dungeon_is_solid_tile(t) ? 7 : (t == 0 ? 0 : 8);
            for (int dy = 0; dy < cell; dy++)
                for (int dx = 0; dx < cell; dx++)
                    s.put(ox + x * cell + dx, oy + y * cell + dy, c);
        }
    }
    int cx = ox + p.x * cell + cell / 2, cy = oy + p.y * cell + cell / 2;
    int dx = dungeon_facing_dx(p.facing), dy = dungeon_facing_dy(p.facing);
    for (int k = 0; k <= cell; k++) {
        s.put(cx + dx * k, cy + dy * k, 15);
        s.put(cx + dx * k - dy, cy + dy * k - dx, 14);
        s.put(cx + dx * k + dy, cy + dy * k + dx, 14);
    }
}

// Translate an input key to a walk action and apply it.  Accepts either the
// BIOS scancode the walk model documents (0x48 up, 0x50 down, 0x4b left,
// 0x4d right) or the SDL2 arrow keysym (0x40000052 up, 0x40000051 down,
// 0x40000050 left, 0x4000004f right) that Window::poll_key reports.  Returns
// true if the key was a movement key.
static bool walk_apply_key(Dungeon& d, DungeonPos& p, int key) {
    switch (key) {
        case 0x48: case 0x40000052: dungeon_step(d, p); return true;   // up
        case 0x50: case 0x40000051:                                    // down
            dungeon_turn(p, true); dungeon_turn(p, true); return true;
        case 0x4b: case 0x40000050: dungeon_turn(p, false); return true;  // left
        case 0x4d: case 0x4000004f: dungeon_turn(p, true); return true;   // right
        default: return false;
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
    Dungeon dng;
    DungeonPos pos;
    bool have_dng = false;
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
                    // Load the save's dungeon so the play state is walkable.
                    have_dng = load_dungeon(dir + "/DNGS/" + dungeon_map_name(save_location(sg)),
                                            dng, &err) && dungeon_place(dng, pos, 0, 0);
                }
            } else if (on_menu && key == 'Q') {
                break;
            } else if (frame + 1 < intro.size()) {
                frame++;
            }
        } else if (state == PLAY) {
            if (key == 'Q') break;
            // Arrow keys walk the party; recompose the viewport map after a move.
            if (have_dng && walk_apply_key(dng, pos, key))
                draw_viewport_map(play_screen, dng, pos);
        }
    }
    win.close();
    if (!shot.empty()) {
        const Screen& s = have_play ? play_screen : intro[frame];
        if (!save_png(shot, s)) { fprintf(stderr, "lof: cannot write %s\n", shot.c_str()); return false; }
    }
    return true;
}

// --walk: load the save's dungeon, place the party and replay a scancode
// sequence through the walk model, then render the play screen with the
// level map in the viewport.  Scancodes: up 0x48 step, down 0x50 turn 180,
// left 0x4b turn left, right 0x4d turn right (the wilderness key map).  Prints
// the resulting position so a headless test can assert the party moved.
static bool walk_loop(const std::string& dir, const std::string& save,
                      const std::vector<int>& keys, Screen& out, DungeonPos& pos) {
    SaveGame sg = load_save(save);
    if (!sg.ok) { fprintf(stderr, "lof: --walk needs a valid --save\n"); return false; }
    std::string mapname = dungeon_map_name(save_location(sg));
    std::string path = dir + "/DNGS/" + mapname;
    Dungeon d;
    std::string err;
    if (!load_dungeon(path, d, &err)) {
        fprintf(stderr, "lof: %s: %s\n", path.c_str(), err.c_str());
        return false;
    }
    dungeon_place(d, pos, 0, 0);
    for (int k : keys) walk_apply_key(d, pos, k);
    std::string vd = dungeon_view_dir(save_location(sg));
    if (!compose_play_screen(dir, vd.empty() ? "D0/DP001" : vd + "/DP001", sg, true, out, err))
        return false;
    draw_viewport_map(out, d, pos);
    return true;
}

int main(int argc, char** argv) {
    std::string dir = game_dir(argv[0]);
    bool headless = false, have_shot = false, dump = false, dump_spec = false, dump_dng = false;
    bool ui = false, walk = false;
    std::string shot;
    long shot_frame = -1;
    std::string walk_save, walk_keys;

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
        else if (a == "--walk") walk = true;
        else if (a == "--save" && i + 1 < argc) walk_save = argv[++i];
        else if (a == "--walk-keys" && i + 1 < argc) walk_keys = argv[++i];
    }

    if (dump) { dump_data(dir); return 0; }
    if (dump_spec) { dump_spec_files(dir); return 0; }
    if (dump_dng) { dump_dungeon(dir); return 0; }

    // --walk: drive the native walk model over the save's dungeon and render
    // the play screen with the level map in the viewport.  This is the front-end
    // half of the play loop; it reads only game data (DNGS/ + GAMES/), never an
    // executable.
    if (walk) {
        std::string save = walk_save.empty() ? dir + "/GAMES/GAME1" : walk_save;
        std::vector<int> keys;
        std::string cur;
        for (size_t j = 0; j <= walk_keys.size(); j++) {
            if (j == walk_keys.size() || walk_keys[j] == ',') {
                if (!cur.empty()) keys.push_back((int)strtol(cur.c_str(), nullptr, 0));
                cur.clear();
            } else cur.push_back(walk_keys[j]);
        }
        Screen s;
        DungeonPos pos;
        if (!walk_loop(dir, save, keys, s, pos)) return 1;
        printf("walk: x=%d y=%d facing=%d\n", pos.x, pos.y, pos.facing);
        if (have_shot && !save_png(shot, s)) { fprintf(stderr, "lof: cannot write %s\n", shot.c_str()); return 1; }
        if (!headless) present_sdl(s, "Legend of Faerghail");
        return 0;
    }

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
                // comma-separated key codes, decimal or 0x-hex, e.g.
                // "32,32,79" (space,space,'O') or "79,0x40000052" ('O', up-arrow)
                std::string s = argv[++i], cur;
                for (size_t j = 0; j <= s.size(); j++) {
                    if (j == s.size() || s[j] == ',') {
                        if (!cur.empty()) ui_keys.push_back((int)strtol(cur.c_str(), nullptr, 0));
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
    bool town_flag = false;
    bool emporium_flag = false;
    for (int i = 1; i < argc; i++) {
        if (std::string(argv[i]) == "--play" && i + 1 < argc) play = argv[++i];
        else if (std::string(argv[i]) == "--save" && i + 1 < argc) save = argv[++i];
        else if (std::string(argv[i]) == "--town") town_flag = true;
        else if (std::string(argv[i]) == "--emporium") emporium_flag = true;
    }
    if (town_flag || emporium_flag) {
        std::string err;
        Screen s;
        bool ok = emporium_flag ? compose_emporium_screen(dir, s, err)
                                : compose_town_screen(dir, s, err);
        if (!ok) {
            fprintf(stderr, "lof: %s\n", err.c_str());
            return 1;
        }
        if (have_shot && !save_png(shot, s)) { fprintf(stderr, "lof: cannot write %s\n", shot.c_str()); return 1; }
        if (!headless) present_sdl(s, "Legend of Faerghail");
        return 0;
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
