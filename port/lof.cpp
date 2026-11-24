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
#include "combat.h"

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

// The game's software mouse cursor: a 16x16 arrow the engine draws itself over
// the current screen at the polled mouse position.  The engine never asks the
// BIOS to draw it (it only calls INT 33h fn0 "reset" and fn3 "poll"), so the
// shape is a fixed 4-plane sprite the routine at image 0x2758 blits.  It is the
// "16x16 top-left patch" that the DOSBox reference captures show: those frames
// were taken with the mouse parked at (0,0).  The emulator never draws it
// (its mouse never moves), so its frames lack the arrow.  Values were recovered
// byte-exact by dumping the decoded scratch buffer (LOF_PXDUMP) at the blit.
static const uint8_t kCursorSprite[16][16] = {
    {0xf,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0x8,0x8,0x8,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0x8,0x3,0x3,0x8,0,0,0,0,0,0,0,0,0,0,0},
    {0,0x8,0x8,0x3,0x3,0x8,0,0,0,0,0,0,0,0,0,0},
    {0,0,0x8,0x8,0x3,0x3,0,0,0,0,0,0x8,0,0,0,0},
    {0,0,0x8,0x8,0x8,0x3,0,0,0,0x8,0x3,0,0,0,0,0},
    {0x3,0,0,0,0,0x8,0x8,0,0,0x3,0x8,0,0,0,0,0},
    {0xf,0,0,0,0,0,0x8,0x3,0x8,0,0,0,0,0,0,0},
    {0xf,0x8,0x3,0x8,0,0,0x8,0x8,0x3,0,0,0,0,0,0,0},
    {0xf,0x3,0x8,0x8,0x3,0x3,0x8,0x8,0x8,0x3,0,0,0,0,0,0},
    {0x3,0xf,0x8,0x8,0x8,0x8,0x8,0,0,0x8,0x3,0,0,0,0,0},
    {0,0xf,0x3,0x8,0x8,0x8,0x8,0,0,0,0x8,0x3,0,0,0,0},
    {0,0x3,0xf,0x3,0x8,0x8,0x8,0x8,0,0,0,0x8,0x3,0,0,0},
    {0,0,0x3,0xf,0x3,0x8,0x8,0x8,0,0,0,0,0x8,0x3,0,0},
    {0,0,0,0x3,0xf,0xf,0x3,0x8,0,0,0,0,0,0x8,0x3,0},
    {0,0,0,0,0,0x3,0xf,0xf,0xf,0x3,0,0,0,0,0x8,0x3},
};
// Mask: 1 where the sprite writes a pixel (value != 0), 0 where it is
// transparent (the engine's masked blit leaves those pixels untouched).
static const uint8_t kCursorMask[16][16] = {
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0},
    {0,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0},
    {0,0,1,1,1,1,0,0,0,0,0,1,0,0,0,0},
    {0,0,1,1,1,1,0,0,0,1,1,0,0,0,0,0},
    {1,0,0,0,0,1,1,0,0,1,1,0,0,0,0,0},
    {1,0,0,0,0,0,1,1,1,0,0,0,0,0,0,0},
    {1,1,1,1,0,0,1,1,1,0,0,0,0,0,0,0},
    {1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0},
    {1,1,1,1,1,1,1,0,0,1,1,0,0,0,0,0},
    {0,1,1,1,1,1,1,0,0,0,1,1,0,0,0,0},
    {0,1,1,1,1,1,1,1,0,0,0,1,1,0,0,0},
    {0,0,1,1,1,1,1,1,0,0,0,0,1,1,0,0},
    {0,0,0,1,1,1,1,1,0,0,0,0,0,1,1,0},
    {0,0,0,0,0,1,1,1,1,1,0,0,0,0,1,1},
};

// Draw the mouse cursor at (x,y), masking out its transparent pixels.  The
// engine draws it after every other element, so it sits on top of the screen.
static void draw_cursor(Screen& s, int x, int y) {
    for (int cy = 0; cy < 16; cy++)
        for (int cx = 0; cx < 16; cx++)
            if (kCursorMask[cy][cx]) s.put(x + cx, y + cy, kCursorSprite[cy][cx]);
}

// Draw the shared in-game chrome that every explore/combat screen carries over
// the HUD background: the six-member party roster (names in CHR2's 5x5
// small-caps face, professions, the +0x1e stat number and the status word) and
// the lower-left status box (scenario, facing, Spirits, Rations).  Recovered by
// matching a DOSBox capture; the roster panel is pixel-exact.  The play screen
// and the encounter screen both draw it (the encounter oracle's panel is
// byte-identical to the play oracle's).
// The shared 3x2 party grid (cols 148/208/268, rows 9/37): each slot shows the
// name (small-caps white), the profession (black), the +0x1e stat (dark gray,
// "%d" + '%') and the status word (white).  Used by the play screen AND the
// town building screens (the character sheet shares the same grid).
static bool draw_party_grid(Screen& s, const std::string& dir,
                            const SaveGame& sg, std::string& err) {
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
        // word in white at x+21.  The stat is the save record's +0x1e byte,
        // which the engine prints with "%d" (disassembly 2EEC2) using CHR2
        // digit cells 27..36, followed by the '%' glyph, which the engine's
        // glyph table places in band 1 cell 37.
        int v = c.panel_stat;
        char num[8];
        int n = snprintf(num, sizeof num, "%d", v);
        for (int k = 0; k < n; k++)
            small.draw_small_cell(s, x + 5 * k, y + 12, 0, 27 + (num[k] - '0'), 8);
        small.draw_small_cell(s, x + 5 * n, y + 12, 1, 37, 8);
        // The word is the character's own status code: image 0x2EF30 reads
        // es:[bx+si+0x1f], scales it by 4 and indexes the far-pointer table at
        // ds:0x0e70 (Good/Mad/Ill/Poisn/Blnd./Stun./Stone/Dead).  It is not a
        // party-wide constant.
        size_t si = c.raw[kCharStatus1f];
        if (si < status.size())
            small.draw_small(s, x + 21, y + 12, status[si], 15);
        // The fixed HP gauge under the "HT" label: a 50x3 white (15) bar at
        // (x, y+18).  Oracle (ref/oracle/house.png, round_result.png) shows it
        // full-width regardless of the character's HP, for every occupied slot.
        for (int by = 0; by < 3; by++)
            for (int bx = 0; bx < 50; bx++)
                s.put(x + bx, y + 18 + by, 15);
        slot++;
    }
    return true;
}

static bool draw_party_and_status(Screen& s, const std::string& dir,
                                  const SaveGame& sg, bool have_sg, std::string& err) {
    if (!have_sg) return true;
    if (!draw_party_grid(s, dir, sg, err)) return false;

    // Lower-left status box.  It is drawn with the *full* CHR1 face at a 6-px
    // pitch, five label lines at y = 136/144/160/176/184:
    //
    //   line 1  the scenario title (the region the save's location belongs to)
    //   line 2  the party facing (N/E/S/W)
    //   line 4  the in-game time of day
    //   line 6  "Spirits"  + the party's morale
    //   line 7  "Rations"  + the party's rations
    //
    // The title is the location's scenario (the ones digit indexes
    // `scenario_names()`) and the facing is the save's +0x9a6 digit byte.
    // The time-of-day word is the save's +0x9b6 index into the engine table
    // ds:0x0f04 (eight 21-char centred strings "Midnight".."Evening"); the
    // morale word is the save's +0x9ba index into ds:0x0f24 ("Good".."Bad").
    // Both port tables match the engine strings exactly.  Measured from the
    // DOSBox oracle: the time-of-day value is drawn white (index 15, same as the
    // labels); the morale and rations values are light gray (index 7).
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
        if (sg.time_of_day >= 0) {
            std::vector<std::string> tod = time_of_day_names();
            if (sg.time_of_day < (int)tod.size())
                box.draw_text(s, 6, 160, tod[sg.time_of_day], 15);
        }
        box.draw_text(s, 6, 176, "Spirits  ", 15);
        // The morale word follows the "Spirits" label.  The engine indexes the
        // ds:0x0f24 table (image 0x2184C) by the save's +0x9ba word; the value is
        // drawn light gray (index 7) starting one cell after the label's trailing
        // spaces, at x=60.
        if (sg.morale >= 0) {
            std::vector<std::string> morale = morale_names();
            if (sg.morale < (int)morale.size())
                box.draw_text(s, 60, 176, morale[sg.morale], 7);
        }
        box.draw_text(s, 6, 184, "Rations  ", 15);
        // The rations figure is the sum of every present character's +0x70 byte
        // (image 0x21743 accumulates es:0x70(%bx) over the six party slots and
        // prints it with "%ld" at x=60).  A save's copy is the value at save time;
        // the engine consumes rations as the party travels.
        {
            // Six slots only.  Walking every parsed record used to include the
            // world-state bytes as a bogus 7th "character" and printed 38 where
            // the oracle prints 19 (the reader bug fixed alongside this).
            long total = 0;
            for (size_t i = 0; i < sg.chars.size() && i < kSaveCharCount; i++)
                if (sg.chars[i].present) total += sg.chars[i].raw[kCharRations70];
            box.draw_text(s, 60, 184, std::to_string(total), 7);
        }
    }
    return true;
}

// Compose the in-game play screen: the static HUD background (EGA/UP/MSCR) with
// the first-person viewport tile (EGA/SUB/DPNNN) blitted into the 120x119 window
// at (10,11), then the shared party/status chrome.  `play` is the SUB-relative
// tile path (e.g. "D0/DP001"); `sg` supplies the party when `have_sg` is set.
// The window geometry, background and panel layout were recovered by matching
// the decoded assets against a DOSBox capture of save slot 2 (ref/oracle/play.png).
// Parse a `--play` view name ("D0/DP001") into its dungeon number and sprite
// index.  Returns false when the name is not of that shape, in which case no
// composite overlays apply.
static bool parse_view_name(const std::string& play, int& dungeon, int& index) {
    if (play.size() < 3 || play[0] != 'D') return false;
    char* end = nullptr;
    long d = strtol(play.c_str() + 1, &end, 10);
    if (end == play.c_str() + 1 || d < 0) return false;
    const char* dp = strstr(end, "/DP");
    if (!dp) return false;
    long i = strtol(dp + 3, &end, 10);
    if (end == dp + 3 || i < 0) return false;
    dungeon = (int)d;
    index = (int)i;
    return true;
}

static bool compose_play_screen(const std::string& dir, const std::string& play,
                                const SaveGame& sg, bool have_sg, Screen& s,
                                std::string& err, bool with_view = true) {
    s.resize(320, 200);
    s.clear(0);
    if (!blit_ega(s, dir + "/EGA/UP/MSCR", 0, 0, &err)) return false;
    if (with_view && !blit_ega(s, dir + "/EGA/" + play, 10, 11, &err)) return false;
    // The outdoor view is a composite, so a named view must carry its side
    // pieces here too (see dungeon_view_overlays).
    int dungeon = 0, index = 0;
    if (with_view && parse_view_name(play, dungeon, index)) {
        for (const DungeonView::Overlay& ov :
             dungeon_view_overlays(dir, dungeon, index)) {
            if (!blit_ega(s, ov.path, ov.x, ov.y, &err, /*transparent=*/true))
                return false;
        }
    }
    return draw_party_and_status(s, dir, sg, have_sg, err);
}

// The character-sheet screen programs a non-standard EGA attribute palette: the
// intro/menu screens use the BIOS stock set, but the detail view reprograms the
// attribute controller (INT 10h AH=10h AL=02h) with the selectors the game
// installs at start-up (00..07, 10..17).  Under it index 8 is dark green
// (0,85,0) and index 15 pale green (170,255,170).  Recovered from the emulator's
// attribute-register dump at the sheet frame and verified against the DOSBox
// capture ref/oracle/charsheet.png (the party grid matches it index-for-index).
static const uint8_t kCharSheetAttr[16] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
};

// Compose the in-game character sheet (`--charsheet`): the play-screen chrome
// (MSCR + the shared 3x2 party grid) with the selected member's portrait blitted
// into the 120x119 viewport, the location/status box in the sheet's green, and
// the stat block on the right panel.  The portrait is a single sprite,
// EGA/CHAR3/CH<profession> (the emulator's open trace for the sheet opens only
// `ega\char3\ch09` for the Paladin Decker; it matches the oracle 1.0000 at
// (10,11)).  The stat text uses the engine's own format strings (image
// 0x2676e): "%-10s:" name, " a " + lower-case alignment, race-adjective +
// profession, "Rnk %02d", "Hit point %04d / %04d", "Magic.... %04d / %04d",
// "Str/Con/Dex/Int/Wis %02d", "State...  " + status, "Experience %011ld",
// "Weight... %04d / %04d" (weights divided by 10).  Text is the CHR1 full face
// at a 6-px pitch: name/options in black (0), the rest dark green (8).
static bool compose_charsheet_screen(const std::string& dir, const SaveGame& sg,
                                     bool have_sg, int member, Screen& s,
                                     std::string& err) {
    s.resize(320, 200);
    s.clear(0);
    s.pal.set_from_attr(kCharSheetAttr);
    if (!blit_ega(s, dir + "/EGA/UP/MSCR", 0, 0, &err)) return false;
    if (have_sg && !draw_party_grid(s, dir, sg, err)) return false;

    const SaveChar* c = nullptr;
    if (have_sg && member >= 0 && member < (int)sg.chars.size())
        c = &sg.chars[member];
    else if (have_sg && !sg.chars.empty())
        c = &sg.chars[0];

    if (c) {
        CharSheet ch = decode_char(c->raw.data());
        // Portrait: the profession selects the class portrait bank.
        char ppath[64];
        snprintf(ppath, sizeof ppath, "%s/EGA/CHAR3/CH%02d", dir.c_str(),
                 (int)c->raw[kCharProf1b]);
        blit_ega(s, ppath, kViewportX, kViewportY, &err);
        err.clear();

        Font box;
        if (!box.load(dir + "/EGA/UP/CHR1", &err)) return false;

        // Location/status box, same rows as the play screen but in green (8).
        std::vector<std::string> scen = scenario_names();
        std::vector<std::string> face = facing_names();
        int loc = (int)save_location(sg);
        int sindex = loc % 10;
        std::string title = (sindex >= 0 && sindex < (int)scen.size())
                                ? scen[sindex] : "    Scenario !!      ";
        box.draw_text(s, 6, 136, title, 8);
        std::string heading = (sg.facing < face.size()) ? face[sg.facing]
                                                        : "    Direction !!     ";
        box.draw_text(s, 6, 144, heading, 8);
        if (sg.time_of_day >= 0) {
            std::vector<std::string> tod = time_of_day_names();
            if (sg.time_of_day < (int)tod.size())
                box.draw_text(s, 6, 160, tod[sg.time_of_day], 8);
        }
        box.draw_text(s, 6, 176, "Spirits  ", 8);
        box.draw_text(s, 6, 184, "Rations  ", 8);

        // Stat block.
        std::vector<std::string> align = alignment_lower_names();
        std::vector<std::string> adj = race_adjective_names();
        std::vector<std::string> prof = profession_names();
        std::vector<std::string> status = status_short_names();
        uint8_t ai = c->raw[0x19];
        uint8_t ri = c->raw[0x1a];
        uint8_t pi = c->raw[0x1b];

        char line[128];
        // Name (black) then ": a <alignment>" (green).
        snprintf(line, sizeof line, "%-10s:", c->name.c_str());
        box.draw_text(s, 139, 92, line, 0);
        std::string al = (ai < align.size()) ? align[ai] : "??";
        box.draw_text(s, 205, 92, " a " + al, 8);
        // Race-adjective + profession, the same table the party grid uses.  (The
        // engine selects the profession string from a table indexed by
        // record[+0x18]; the plain profession table matches the grid, so that is
        // what is used here.  A gender-variant table is visible in the
        // disassembly but its exact selection is not reproduced — see BLK-M.)
        std::string race = (ri < adj.size()) ? adj[ri] : "??";
        std::string pr = (pi < prof.size()) ? prof[pi] : "??";
        box.draw_text(s, 139, 100, race + pr, 8);

        // Labels are green (8), values (and their separators) black (0), both at
        // the engine's column positions.
        box.draw_text(s, 139, 116, "Rnk ", 8);
        snprintf(line, sizeof line, "%02d  ", ch.level17);
        box.draw_text(s, 163, 116, line, 0);
        box.draw_text(s, 187, 116, "Hit point ", 8);
        snprintf(line, sizeof line, "%04d / %04d", ch.hp_cur, ch.hp_max);
        box.draw_text(s, 247, 116, line, 0);
        box.draw_text(s, 187, 124, "Magic.... ", 8);
        snprintf(line, sizeof line, "%04d / %04d", ch.mag_cur69, ch.mag_max68);
        box.draw_text(s, 247, 124, line, 0);
        box.draw_text(s, 139, 132, "Str ", 8);
        snprintf(line, sizeof line, "%02d", ch.str45);
        box.draw_text(s, 163, 132, line, 0);
        box.draw_text(s, 139, 140, "Con ", 8);
        snprintf(line, sizeof line, "%02d  ", ch.con44);
        box.draw_text(s, 163, 140, line, 0);
        {
            std::string st = (c->raw[kCharStatus1f] < status.size())
                                 ? status[c->raw[kCharStatus1f]] : "??";
            box.draw_text(s, 187, 140, "State...  ", 8);
            box.draw_text(s, 247, 140, st, 0);
        }
        box.draw_text(s, 139, 148, "Dex ", 8);
        snprintf(line, sizeof line, "%02d  ", ch.dex46);
        box.draw_text(s, 163, 148, line, 0);
        box.draw_text(s, 187, 148, "Experience ", 8);
        snprintf(line, sizeof line, "%011ld", (long)ch.exp);
        box.draw_text(s, 253, 148, line, 0);
        box.draw_text(s, 139, 156, "Int ", 8);
        snprintf(line, sizeof line, "%02d", ch.int47);
        box.draw_text(s, 163, 156, line, 0);
        box.draw_text(s, 139, 164, "Wis ", 8);
        snprintf(line, sizeof line, "%02d  ", ch.wis48);
        box.draw_text(s, 163, 164, line, 0);
        box.draw_text(s, 187, 164, "Weight... ", 8);
        snprintf(line, sizeof line, "%04d / %04d", ch.weight_cur / 10,
                 ch.weight_max / 10);
        box.draw_text(s, 247, 164, line, 0);

        box.draw_text(s, 139, 172, "(E)xchange gold/rations", 0);
        box.draw_text(s, 139, 180, "(S)how next page", 0);
        box.draw_text(s, 139, 188, "(C)ontinue", 0);
    }
    return true;
}


// One enemy group on the encounter screen: the monster record id in the
// scenario's SYST/NSC<dd> table (which selects the plural name and, via the
// record's +0x48 field, the CHAR0 sprite) plus the group's size and rank.
struct EncounterGroup {
    int rec = 12;             // NSC00 record (12 = Mercenary, the pinned oracle)
    uint32_t count = 4;
    uint32_t rank = 2;
};

// Compose the wandering-monster encounter screen: the same HUD background, the
// party/status chrome, and a first-person viewport holding the encountered
// monster's sprite.  The engine reads the monster table SYST/NSC<scenario>
// during the encounter, picks one record per enemy group, and blits its creature
// sprite (NSC[record].+0x48 -> EGA/CHAR0/CH<nn>, e.g. Mercenary = record 12 ->
// CH24); each group also prints a "N <Plural> (Rnk R)" line, one per 8-px row
// from y=108, under the intro line.  The CHAR0 bank is the engine's own choice
// here: the emulator's open trace for the oracle encounter shows
// `ega\char0\ch24` (LOF_I21=1).
//
// The sprite is a full 120x119 cell blitted plainly at the viewport origin
// (10,11).  Verified against the emulator's encounter frames: the viewport is
// 1.0000 for both the Mercenary (ref/oracle/combat.png, CHAR0/CH24) and a
// second encounter (CHAR0/CH26); the whole-screen residue is only the
// party/status values a save does not carry, as on the play screen.
static bool compose_encounter_screen(const std::string& dir, const char* intro,
                                     const std::vector<EncounterGroup>& groups,
                                     const std::vector<SnsRecord>& roster, int sprite_ch,
                                     const SaveGame& sg, bool have_sg,
                                     Screen& s, std::string& err) {
    s.resize(320, 200);
    s.clear(0);
    if (!blit_ega(s, dir + "/EGA/UP/MSCR", 0, 0, &err)) return false;

    // Monster sprite: the creature's 120x119 first-person cell
    // (EGA/CHAR0/CH<nn>) blitted at the viewport origin.
    char spath[64];
    snprintf(spath, sizeof spath, "%s/EGA/CHAR0/CH%02d", dir.c_str(), sprite_ch);
    if (!blit_ega(s, spath, kViewportX, kViewportY, &err)) return false;

    if (!draw_party_and_status(s, dir, sg, have_sg, err)) return false;

    // Encounter text, in CHR1's full face at a 6-px pitch, x=139: the intro
    // line and "What will you do ?" in colour 8, each group line's count/name
    // in colour 8 with its " (Rnk R)" tail black (0), and the four action
    // choices black.  Group lines start at y=108, one per 8-px row.
    Font font;
    if (!font.load(dir + "/EGA/UP/CHR1", &err)) return false;
    font.draw_text(s, 139, 92, intro, 8);
    for (size_t g = 0; g < groups.size(); g++) {
        const EncounterGroup& grp = groups[g];
        std::string plural = (grp.rec >= 0 && grp.rec < (int)roster.size())
                                 ? roster[grp.rec].plural : "Monsters";
        std::string line = combat_group_line(grp.count, plural, grp.rank);
        int y = 108 + 8 * (int)g;
        size_t rnk = line.find(" (Rnk");
        if (rnk == std::string::npos) {
            font.draw_text(s, 139, y, line, 8);
        } else {
            font.draw_text(s, 139, y, line.substr(0, rnk), 8);
            font.draw_text(s, 139 + 6 * (int)rnk, y, line.substr(rnk), 0);
        }
    }
    font.draw_text(s, 139, 132, combat_prompt_names().at(0), 8);
    const std::vector<std::string>& actions = combat_action_names();
    for (size_t i = 0; i < actions.size(); i++)
        font.draw_text(s, 139, 156 + 8 * (int)i, actions[i], 0);
    return true;
}

// The give-artefact (amulet) page shown when leaving town for the first time:
// the play-screen chrome (MSCR + the town viewport + the party panel) with the
// engine's narration in the right-hand text block (CHR1, x=139).  The text is
// the engine's own: "On starting your journey, you receive an amulet from a
// town guard. A gift from the Count." (word-wrapped to three 29-cell lines),
// "You find an artefact:", "The Amulet", "Who should receive that (1-6)?",
// "(L)eave it".  Line rows are 92/100/108/116 then 132/148/164 -- verified by
// diffing the archived capture ref/emu/amulet_newgame.png against MSCR (the
// text region is uniform MSCR index 7 except the glyphs).  The narration and
// the two prompts are light green, the artefact name and "(L)eave it" black.
// The viewport is TWN00 (the town scene; the capture overlays a town-guard
// sprite, a follow-up).  Verified by ink mask (palette-independent) in
// tests/port_amulet_test.py.
static bool compose_amulet_screen(const std::string& dir, const SaveGame& sg,
                                  bool have_sg, Screen& s, std::string& err) {
    s.resize(320, 200);
    s.clear(0);
    if (!blit_ega(s, dir + "/EGA/UP/MSCR", 0, 0, &err)) return false;
    if (!blit_ega(s, dir + "/EGA/TWN/TWN00", kViewportX, kViewportY, &err))
        return false;
    if (!draw_party_and_status(s, dir, sg, have_sg, err)) return false;
    Font font;
    if (!font.load(dir + "/EGA/UP/CHR1", &err)) return false;
    font.draw_text(s, 139, 92,  "On starting your journey, you", 10);
    font.draw_text(s, 139, 100, "receive an amulet from a town", 10);
    font.draw_text(s, 139, 108, "guard. A gift from the Count.", 10);
    font.draw_text(s, 139, 116, "You find an artefact:", 10);
    font.draw_text(s, 139, 132, "The Amulet", 0);
    font.draw_text(s, 139, 148, "Who should receive that (1-6)?", 10);
    font.draw_text(s, 139, 164, "(L)eave it", 0);
    return true;
}

// Compose the per-round combat decision screen.  It is the encounter screen
// (identical viewport and left panel) with the right-hand text block replaced
// by the round-decision list.  The row layout is the engine's own printf
// sequence at image 0x18734 (see docs/notes-maintainer-decision.md):
//
//   "%ld"        the 1-based party index, colour 8, x=139, rows 116+8*n
//   "%-10s "     the name, colour 8, x=145
//   rank abbrev  the 4-entry far-pointer table at ds:0x0eb0 selected by the
//                character's +0x24 byte (KIL/ATK/DEF/RET), black, x=211
//   action word  the 5-entry table at ds:0x0ec0 selected by +0x0f
//                (Defends/Attacks/Stalks/Use object/Cast Spell), colour 8, x=235
//   trailing     the engine prints "@ %ld" (ds:0x6fe6/0x6fec) right after the
//                10-cell action column, i.e. at x=295, with (+0x0d + 1) when
//                +0x16 == 0xff, else (+0x16 + 1).  '@' and ' ' have no glyph in
//                the text face but still advance 6 px each, so the number inks
//                at x=307 (measured ink x308..311), colour 8.
//
// The header ("Please make your decision" / "for the next round!") is at y=92
// and 100; the column header ("Name       HP   W P B Attack" + rule) at y=108
// and 116 is only drawn on the round-result screen, not here.  The footer is
// "(A)ttack!" at y=172 and "(Q)uick combat" at y=180, black.
static bool compose_decision_screen(const std::string& dir, int sprite_ch,
                                    const SaveGame& sg, bool have_sg, Screen& s,
                                    std::string& err) {
    s.resize(320, 200);
    s.clear(0);
    if (!blit_ega(s, dir + "/EGA/UP/MSCR", 0, 0, &err)) return false;
    char spath[64];
    snprintf(spath, sizeof spath, "%s/EGA/CHAR0/CH%02d", dir.c_str(), sprite_ch);
    if (!blit_ega(s, spath, kViewportX, kViewportY, &err)) return false;
    if (!draw_party_and_status(s, dir, sg, have_sg, err)) return false;

    Font font;
    if (!font.load(dir + "/EGA/UP/CHR1", &err)) return false;
    font.draw_text(s, 139, 92, "Please make your decision", 8);
    font.draw_text(s, 139, 100, "for the next round!", 8);

    const std::vector<std::string> abbrev = combat_abbrev_names();
    const std::vector<std::string> actions = action_names();
    for (size_t i = 0; i < sg.chars.size() && i < 6; i++) {
        const SaveChar& c = sg.chars[i];
        // Row filter, image 0x18798/0x1879E: slot present and +0x1f < 5.
        if (!c.present || c.raw[kCharStatus1f] >= 5) continue;
        int y = 116 + 8 * (int)i;
        font.draw_text(s, 139, y, std::to_string(i + 1), 8);
        font.draw_text(s, 145, y, c.name, 8);
        uint8_t rank = c.raw[0x24];
        if (rank < abbrev.size())
            font.draw_text(s, 211, y, abbrev[rank], 0);
        // Image 0x18839: `cmp byte es:[bx+0x49],0 / je <action word>` — any
        // non-zero +0x49 replaces the action column with the fixed string
        // ds:0x6ff2 ("is out of action.") and skips the number.
        if (c.raw[kCharOutOfAction49] != 0) {
            font.draw_text(s, 235, y, "is out of action.", 8);
            continue;
        }
        uint8_t act = c.raw[0x0f];
        if (act < actions.size())
            font.draw_text(s, 235, y, actions[act], 8);
        // The action word is a "%-10s " field, so the trailing "@ %ld" starts
        // 10 cells (60 px) past x=235; '@' and ' ' advance 6 px uninked each,
        // leaving the number at x=307.
        const int kTrailX = 295 + 2 * 6;
        if (c.raw[0x16] == 0xff) {
            // Image 0x186f3: only when signed(+0x0d) is in 0..2 and +0x0f != 0.
            int8_t d = (int8_t)c.raw[0x0d];
            if (d >= 0 && d <= 2 && c.raw[0x0f] != 0)
                font.draw_text(s, kTrailX, y, std::to_string(d + 1), 8);
        } else {
            font.draw_text(s, kTrailX, y, std::to_string(c.raw[0x16] + 1), 8);
        }
    }
    font.draw_text(s, 139, 172, "(A)ttack!", 0);
    font.draw_text(s, 139, 180, "(Q)uick combat", 0);
    return true;
}

// One row of the round-result table: the values the engine's combat resolver
// produces for a party member.  These are the engine's four per-round arrays
// (docs/notes-b.md, "The four per-character arrays"): W = party_seg[i*2+0x5608],
// P = [i*2+0x5609], B = [i+0x224], Attack = [i+0x3d48] unless char +0x49 != 0,
// which is the out-of-action "-".
struct RoundRow {
    int  hp_delta = 0;
    bool w = false, p = false, b = false;
    int  attack = 1;
};

// Compose the round-result screen ("The result of this round:").  It is the
// encounter chrome (MSCR + the creature cell + the party/status panel) with the
// melee table in the right-hand text block.  Layout is the engine's own draw
// sequence at image 0x1867D..0x189F6 (docs/notes-b.md, "round-result printer"):
//
//   "The result of this round:"             newline 1, colour 8
//   "Name       HP   W P B Attack"          newline 1, colour 0
//   "---------- ---- - - - -------"         newline 1, colour 0
//   per present member i, starting at the row after the rule (y=124+8i):
//     "%-10s "  the name                    colour 8, x=139 (11 cells -> x=205)
//     "%4ld "   the HP change               colour 8, right-aligned in 4 cells
//     "* "/"- " W, P, B                     colour 8, x=235/247/259
//     "Passed"/"Failed"/"-"                 colour 8, x=271
//   "(C)ontinue"                            colour 0, y=180
//
// The three leading arguments of the drawer (lcall 0x18d,0x16a8) are
// (offset, segment, newline); the "style" values in the listing are the
// newline flag, not a colour or a face.  The colour is a separate global set by
// lcall 0x18d,0x175a (image 0x302A, table cs:0x174c).  Rows advance because the
// row's last draw ("Passed"/"Failed"/"-") sets newline=1.
static bool compose_round_result_screen(const std::string& dir, int sprite_ch,
                                        const SaveGame& sg, bool have_sg,
                                        const std::vector<RoundRow>& rows,
                                        Screen& s, std::string& err) {
    s.resize(320, 200);
    s.clear(0);
    if (!blit_ega(s, dir + "/EGA/UP/MSCR", 0, 0, &err)) return false;
    char spath[64];
    snprintf(spath, sizeof spath, "%s/EGA/CHAR0/CH%02d", dir.c_str(), sprite_ch);
    if (!blit_ega(s, spath, kViewportX, kViewportY, &err)) return false;
    if (!draw_party_and_status(s, dir, sg, have_sg, err)) return false;

    Font font;
    if (!font.load(dir + "/EGA/UP/CHR1", &err)) return false;
    font.draw_text(s, 139, 92, "The result of this round:", 8);
    font.draw_text(s, 139, 108, "Name       HP   W P B Attack", 0);
    font.draw_text(s, 139, 116, "---------- ---- - - - -------", 0);

    size_t r = 0;
    for (size_t i = 0; i < sg.chars.size() && i < 6; i++) {
        if (!sg.chars[i].present) continue;
        const RoundRow row = r < rows.size() ? rows[r] : RoundRow();
        r++;
        int y = 124 + 8 * (int)i;
        font.draw_text(s, 139, y, sg.chars[i].name, 8);
        // "%4ld " field: right-aligned in four 6-px cells.
        std::string hp = std::to_string(row.hp_delta);
        int hx = 205 + 6 * (4 - (int)hp.size());
        if (hx < 205) hx = 205;
        font.draw_text(s, hx, y, hp, 8);
        font.draw_text(s, 235, y, row.w ? "*" : "-", 8);
        font.draw_text(s, 247, y, row.p ? "*" : "-", 8);
        font.draw_text(s, 259, y, row.b ? "*" : "-", 8);
        font.draw_text(s, 271, y,
                       row.attack == 0 ? "Failed"
                                       : (row.attack == 1 ? "Passed" : "-"), 8);
    }
    font.draw_text(s, 139, 180, "(C)ontinue", 0);
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
    if (!blit_ega(menu, dir + "/EGA/TWN/TWN03", kViewportX, kViewportY, &err)) return false;
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
                                    Screen& s, std::string& err,
                                    const SaveGame* sg = nullptr) {
    s.resize(320, 200);
    s.clear(0);
    if (!blit_ega(s, dir + "/EGA/UP/MSCR", 0, 0, &err)) return false;
    if (!blit_ega(s, dir + "/EGA/TWN/" + tile, 10, 11, &err)) return false;
    Font font;
    if (!font.load(dir + "/EGA/UP/CHR1", &err)) return false;
    for (size_t i = 0; i < n; i++)
        font.draw_text(s, 139, rows[i].y, rows[i].text, rows[i].idx);
    // The 3x2 party grid sits in the top-right (y 9..49), clear of the text
    // (y >= 88); the building screens show it exactly like the play screen.
    if (sg && !draw_party_grid(s, dir, *sg, err)) return false;
    return true;
}

// The town name shown on entering a town is selected by the engine from the
// loaded save, not hardcoded.  The town routine (image 0x53E0) tests the 32-bit
// cell es:[0x460e] (via [ds:0xaf48]) and prints "Thyn." when it is nonzero and
// "Cyldane." when it is zero -- the only two names in the EXE (ds:0x5A39 /
// ds:0x5A40).  The save loader (image 0x1C952) fills that cell with the
// sign-extended 16-bit word at save offset 0x9a4, so the selector is exactly
// "is the word at 0x9a4 nonzero".  With no readable save the boot initialiser
// (image 0x179) leaves the cell at 1, i.e. "Thyn.".
static const char* town_name_from_save(const std::string& save_path) {
    FILE* f = save_path.empty() ? nullptr : fopen(save_path.c_str(), "rb");
    if (!f) return "Thyn";
    uint8_t b[2] = {0, 0};
    bool got = fseek(f, 0x9a4, SEEK_SET) == 0 && fread(b, 1, 2, f) == 2;
    fclose(f);
    if (!got) return "Thyn";
    return (b[0] | (b[1] << 8)) ? "Thyn" : "Cyldane";
}

// Compose the in-game town screen (the building menu you get on entering a
// town): MSCR + the street viewport EGA/TWN/TWN00 + the message and building
// list.  Recovered by template-matching the decoded assets and font glyphs
// against the emulator's town frame (tests/golden_test.sh scenario `town`):
// every glyph matches a CHR1 cell exactly.  `name` is the town name the engine
// selects from the save (see town_name_from_save).
static bool compose_town_screen(const std::string& dir, const char* name,
                                Screen& town, std::string& err,
                                const SaveGame* sg = nullptr) {
    std::string row = std::string(name) + ". Which building do you";
    TownTextRow kText[] = {
        { 92, "You enter the proud town of", 15},
        {100, row.c_str(), 15},
        {108, "want to visit?", 15},
        {132, "(E)mporium", 0},
        {140, "(B)ank", 0},
        {156, "(I)nn", 0},
        {164, "(H)ouse of trades", 0},
        {172, "(T)emple", 0},
        {188, "(C)ontinue", 15},
    };
    return compose_building_screen(dir, "TWN00", kText,
                                   sizeof(kText) / sizeof(kText[0]), town, err, sg);
}

// Compose the Emporium screen: MSCR + the shop exterior EGA/TWN/TWN01 + the
// shopkeeper's greeting and the buy/sell/repair/gather menu.  Decoded from the
// emulator's `shop` golden frame (tests/golden_test.sh scenario `shop`); the
// greeting ("Well hi, wealthy heroes, what / would you like, huh?") is drawn
// white (15) on the light-gray MSCR box, the options black (0).  The strings
// are the engine's own (LOF.EXE ds:0x5B71/0x5B8B/0x5BA5/...); the greeting
// wraps after "what" (the engine's own row break, not a reflow).
static bool compose_emporium_screen(const std::string& dir, Screen& s, std::string& err,
                            const SaveGame* sg = nullptr) {
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
                                   sizeof(kText) / sizeof(kText[0]), s, err, sg);
}

// Compose the Bank screen: MSCR + the bank exterior EGA/TWN/TWN02 + the
// teller's greeting and the account menu.  Strings are the engine's own
// (LOF.EXE ds:0x631C/0x634D/...); decoded from the emulator's Bank frame.
static bool compose_bank_screen(const std::string& dir, Screen& s, std::string& err,
                        const SaveGame* sg = nullptr) {
    static const TownTextRow kText[] = {
        { 92, "Greetings, brave heroes, what", 15},
        {100, "can we do for you?", 15},
        {124, "(D)eposit/open account", 0},
        {132, "(W)ithdraw/close account", 0},
        {140, "(E)xamine accounts", 0},
        {156, "(J)oint deposit", 0},
        {172, "(C)ontinue", 15},
    };
    return compose_building_screen(dir, "TWN02", kText,
                                   sizeof(kText) / sizeof(kText[0]), s, err, sg);
}

// Compose the House of trades screen: MSCR + the hall exterior EGA/TWN/TWN04 +
// the Board's greeting and the training menu.  Strings are the engine's own
// (LOF.EXE ds:0x6855/0x686F/0x688C/...); decoded from the emulator's frame.
static bool compose_house_screen(const std::string& dir, Screen& s, std::string& err,
                         const SaveGame* sg = nullptr) {
    static const TownTextRow kText[] = {
        { 92, "The Grand Board of Trades", 15},
        {100, "greets you in Thyn.", 15},
        {108, "Welcome!", 15},
        {124, "(T)raining", 0},
        {132, "(L)earn magic spells", 0},
        {140, "(A)nother language", 0},
        {156, "(J)oint deposit", 0},
        {172, "(C)ontinue", 15},
    };
    return compose_building_screen(dir, "TWN04", kText,
                                   sizeof(kText) / sizeof(kText[0]), s, err, sg);
}

// Compose the Temple screen: MSCR + the temple exterior EGA/TWN/TWN05 in the
// viewport + the priest's greeting and the temple menu.  The exterior tile is a
// full 120x119 cell blitted at the viewport origin, pixel-exact against the
// emulator's `temple_bldg` frame.  Strings are the engine's own
// (LOF.EXE ds:0x5086/0x5095/0x50a3/0x50b4/0x50ce/0x50de).
static bool compose_temple_screen(const std::string& dir, Screen& s, std::string& err,
                          const SaveGame* sg = nullptr) {
    s.resize(320, 200);
    s.clear(0);
    if (!blit_ega(s, dir + "/EGA/UP/MSCR", 0, 0, &err)) return false;
    if (!blit_ega(s, dir + "/EGA/TWN/TWN05", kViewportX, kViewportY, &err)) return false;
    Font font;
    if (!font.load(dir + "/EGA/UP/CHR1", &err)) return false;
    static const TownTextRow kText[] = {
        { 92, "welcome to our", 15},
        {100, "Great Temple!", 15},
        {116, "(H)eal character", 0},
        {124, "(R)evive a dead character", 0},
        {140, "(J)oint deposit", 0},
        {156, "(C)ontinue", 15},
    };
    for (const auto& r : kText)
        font.draw_text(s, 139, r.y, r.text, r.idx);
    if (sg && !draw_party_grid(s, dir, *sg, err)) return false;
    return true;
}

// The Temple's (H)eal / (R)evive options open a character-selection sub-prompt
// over the *same* viewport (the priest's question replaces the greeting).  The
// question strings are the engine's own (LOF.EXE ds:0x5106 "Whose wounds should"
// / 0x511a "we heal (1-6)?" for heal; ds:0x50e9 "Who should be revived" /
// 0x50fe "(1-6)?" for revive), drawn white (15) on the light-gray box; the only
// option row is "(C)ontinue" (black, 0) at y=116.  Verified against the emulator
// `temple_heal`/`temple_revive` frames (ref/oracle/temple_heal.png,
// temple_revive.png).
static bool compose_temple_prompt(const std::string& dir, const char* line0,
                                  const char* line1, Screen& s, std::string& err,
                                  const SaveGame* sg = nullptr) {
    s.resize(320, 200);
    s.clear(0);
    if (!blit_ega(s, dir + "/EGA/UP/MSCR", 0, 0, &err)) return false;
    if (!blit_ega(s, dir + "/EGA/TWN/TWN05", kViewportX, kViewportY, &err)) return false;
    Font font;
    if (!font.load(dir + "/EGA/UP/CHR1", &err)) return false;
    font.draw_text(s, 139, 92, line0, 15);
    font.draw_text(s, 139, 100, line1, 15);
    font.draw_text(s, 139, 116, "(C)ontinue", 0);
    if (sg && !draw_party_grid(s, dir, *sg, err)) return false;
    return true;
}

// The Inn's (R)est option: the innkeeper's question over the *same* viewport
// (the Inn screen is the main menu, so the viewport is the menu's scrolled TWN03
// blit, x0=16).  The question and the (S)/(W)/(J) options are the engine's own
// (LOF.EXE ds:0x49d5 "Would you like a simple" / 0x49ed "nights rest, or do you
// also" / 0x4a09 "wish to have all rations" / 0x4a22 "replenished?" /
// 0x4a2f "(S)imple nights stay" / 0x4a44 "(W)ith replenished rations"), drawn
// white (15) for the question and "(C)ontinue" and black (0) for the three
// option rows.  Verified against the emulator `inn_rest` frame
// (ref/oracle/inn_rest.png).
static bool compose_inn_rest_screen(const std::string& dir, Screen& s, std::string& err,
                                    const SaveGame* sg = nullptr) {
    s.resize(320, 200);
    s.clear(0);
    if (!blit_ega(s, dir + "/EGA/UP/MSCR", 0, 0, &err)) return false;
    if (!blit_ega(s, dir + "/EGA/TWN/TWN03", kViewportX, kViewportY, &err)) return false;
    Font font;
    if (!font.load(dir + "/EGA/UP/CHR1", &err)) return false;
    static const TownTextRow kText[] = {
        { 92, "Would you like a simple", 15},
        {100, "nights rest, or do you also", 15},
        {108, "wish to have all rations", 15},
        {116, "replenished?", 15},
        {132, "(S)imple nights stay", 0},
        {140, "(W)ith replenished rations", 0},
        {156, "(J)oint deposit", 0},
        {172, "(C)ontinue", 15},
    };
    for (const auto& r : kText)
        font.draw_text(s, 139, r.y, r.text, r.idx);
    if (sg && !draw_party_grid(s, dir, *sg, err)) return false;
    return true;
}

// The Inn's "who pays" prompt, shown after choosing (S)imple nights stay: the
// engine prints "A nights stay and meal costs <n>g," (wrapped) then "who pays
// (1-6)?" over the same MSCR + TWN03 chrome as --inn-rest.  The price is the
// engine's computed value ("A nights stay and meal " + "costs " + n + "g,",
// strings 0x412bf/0x412d7); its formula is not yet derived, so it is a
// parameter (default 2 = the 1-member oracle `ref/emu/inn_rest_pay.png`).
static bool compose_inn_rest_pay_screen(const std::string& dir, int cost, Screen& s,
                                        std::string& err,
                                        const SaveGame* sg = nullptr) {
    s.resize(320, 200);
    s.clear(0);
    if (!blit_ega(s, dir + "/EGA/UP/MSCR", 0, 0, &err)) return false;
    if (!blit_ega(s, dir + "/EGA/TWN/TWN03", kViewportX, kViewportY, &err)) return false;
    Font font;
    if (!font.load(dir + "/EGA/UP/CHR1", &err)) return false;
    font.draw_text(s, 139, 92, "A nights stay and meal costs", 15);
    char buf[24];
    std::snprintf(buf, sizeof buf, "%dg,", cost);
    font.draw_text(s, 139, 100, buf, 15);
    font.draw_text(s, 139, 108, "who pays (1-6)?", 15);
    font.draw_text(s, 139, 124, "(C)ontinue", 0);
    if (sg && !draw_party_grid(s, dir, *sg, err)) return false;
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

// Raise the dungeon event for the cell the party just entered, if any.
//
// The engine's post-move handler (0x2E664) calls the SPEC dispatcher (0x1F31E)
// when the party's cell has tile 3 and a word the table build accepts; the
// dispatcher then resolves the word to a record index through its key table and
// reads `index * 0xee` from `SYST/SPEC<ones digit of the level>`.  This mirrors
// that lookup and runs the part of the effect tail (0x1F5EC) that is proven.
//
// The tail prints text_b (record +0x50) *before* it looks at the type: the
// print at 0x1F713 is reached from both the plain-text path (0x1F5C4) and every
// arm of the 17-entry jump table (0x1F5B8, types 7..23), so the message is shown
// for all of them.  Printing it only for "text" types would swallow the prompt
// of every interactive event -- e.g. the wilderness door at 0x1F6DE, whose
// question would never appear.
//
// The *effects* are not reproduced.  Which path a type takes is decided by
// `sub ax,7; cmp ax,0x10; ja 0x1F5C4`: types 0..6 and >=24 fall through to the
// plain-text path, types 7..23 are dispatched through the jump table.  Of the
// plain-text group only 0..6 are fully reproduced (print text_b and stop); 24
// and 25 have extra tail work (25 re-prints the question and loops it, at
// 0x1F76D), and the jump-table group needs the per-type handlers.  Those are
// reported by name so the omission is visible in a transcript rather than
// silently printing nothing.
//
// Two tail details are also still unimplemented and deliberately not guessed:
// the conditional extra line from record +0xc9 (0x1F6D6; -1 means "none", and
// 16 of 586 text records set it), and the +0x0e line gated on the
// `0x2f0e:0x1af4` result, which is a strlen (0x30BD4) and so only tests whether
// text_b is empty.  The record is *not* consumed: the engine clears the cell's
// tile/word at many per-type sites, not in one generic place, so consuming
// unconditionally here would diverge.
// The party-wide "already done" union the once-only check reads.
//
// The dispatcher builds `[0x37ee]`, a 12-byte array, by OR-ing character byte
// `+0x38 + i` across the six party slots (loops at 0x1F38C / 0x1F396; the
// stride is 0x19a, the save slot size).  It is *not* a global latch and not a
// bitfield -- each byte is its own 0..255 index, which is why the record's
// `+0xe6` argument is bounded by 12 rather than 96.
static std::array<uint8_t, 12> party_done_flags(const SaveGame& sg) {
    std::array<uint8_t, 12> out{};
    for (const SaveChar& c : sg.chars) {
        if (!c.present) continue;
        for (size_t i = 0; i < out.size(); i++)
            if (c.raw[0x38 + i]) out[i] = 1;
    }
    return out;
}

// Types whose type word selects `[bp-8] == 0` in the dispatch table at 0x1F5CA.
// Only these are subject to the once-only check below; everything else (types
// 0..6 and 9,10,14,16,17,19..22) gets `[bp-8] == 1` and is never suppressed.
static bool type_takes_zero_flag(uint16_t type) {
    switch (type) {
        case 7: case 8: case 11: case 12: case 13: case 15: case 18: case 23:
            return true;
        default:
            return false;
    }
}

// The player's typed reply (--answer) for records that carry a keyword; null
// when none was supplied.
static const std::string* g_event_answer = nullptr;

// `answer` is the player's typed reply for records that carry a keyword
// (+0x00); nullptr means no reply was supplied.
static DoorTarget g_door;     // type-7 destination, see door_target()
static CoffinLatch g_coffin;  // type-16 two-visit latch, see CoffinLatch

static bool fire_cell_event(const std::string& dir, uint16_t& location,
                            Dungeon& d, DungeonPos& pos,
                            SpecTable* spec, bool* spec_loaded,
                            SaveGame* sg,
                            const std::string* answer = nullptr,
                            CombatRng* rng = nullptr) {
    const int idx = d.event_index(pos.x, pos.y);
    if (idx < 0) return false;
    if (!*spec_loaded) {
        *spec_loaded = true;
        *spec = load_spec(spec_path(dir, location));
        if (!spec->ok)
            fprintf(stderr, "lof: cannot read %s\n",
                    spec_path(dir, location).c_str());
    }
    if (!spec->ok || idx >= (int)spec->records.size()) {
        fprintf(stderr, "lof: event index %d has no SPEC record\n", idx);
        return false;
    }
    const SpecRecord& r = spec->records[(size_t)idx];

    // Once-only records are skipped entirely -- no text and no effect -- when
    // the party has already been through them:
    //   1F603: cmp word es:[bx+0xe4], 1   ; the record is "once only"
    //   1F60B: cmp byte [bp-8], 0         ; and its type takes the 0 flag
    //   1F611: cmp word es:[bx+0xe6], 0xc ; and the index is inside the array
    //   1F622: cmp byte es:[bx+0x37ee],0 ; and the party has done it
    //   1F62A: jmp 0x212F6                ; -> the shared epilogue, silently
    // 0x212F6 is `pop si; pop di; mov sp,bp; pop bp; retf`, i.e. a plain return.
    if (r.counter == 1 && type_takes_zero_flag((uint16_t)r.type) &&
        r.arg_e6 < 12 && sg != nullptr) {
        if (party_done_flags(*sg)[(size_t)r.arg_e6]) return false;
    }

    // A type-16 record (lava crossing / sarcophagus) alternates: the visit that
    // finds the latch set prints the coffin line instead of the record's text
    // (0x1F639 -> 0x21276 skips it).
    if (g_coffin.visit(r.type)) {
        printf("lof: The companions arise out of the coffin.\n");
        return false;
    }

    if (!r.text_b.empty()) printf("lof: %s\n", r.text_b.c_str());

    // A record with a keyword (+0x00) reads a reply before anything fires
    // (0x209C8..0x20A95): only a matching answer reaches the per-type effect
    // switch.  The outcome line (+0x0e) is printed on both branches, gated by
    // +0x4b (see `answer_prints_outcome`).
    const AnswerEffect effect = classify_answer(r, answer ? *answer : "");
    if (effect == AnswerEffect::NoAnswerWanted ? false : answer == nullptr) {
        printf("lof:   (answer wanted)\n");
        return false;
    }
    if (answer_prints_outcome(r, effect) && !r.text_a.empty())
        printf("lof: %s\n", r.text_a.c_str());
    if (!answer_fires_effect(effect)) return false;

    // A record may also name a long text block: `index_c9` (0xff = none) is an
    // entry index into `SYST/TXT<same scenario number as the SPEC file>`.  The
    // open helper at 0x1D7C2 formats "SYST\TXT%d" from the scenario number, not
    // from `index_c9`, and then seeks `4 * index_c9` into the offset table --
    // verified across all 46 records that set the field: every index is in
    // range for its own scenario's TXT file and out of range for the others.
    //
    // An entry is a run of NUL-separated *lines*; an empty line is a paragraph
    // break, not the end of the entry (the offset/length pair delimits it).  The
    // engine pages this over the message box; printing it line by line keeps the
    // whole block instead of dropping it, which is what a bare text_b does for
    // the 20 records that carry no short text at all (e.g. SPEC0[19]).
    // ...but the tail skips the block for the types whose handler has already
    // spoken, and the test is on the type word, not on "did we print anything":
    //   cmp word es:[bx+0xe2], 0x19 ; je 0x1F713   (type 25)
    //   cmp word es:[bx+0xe2], 1    ; je 0x1F713   (type 1,  at 0x1F6DE)
    //   cmp word es:[bx+0xe2], 8    ; je 0x1F713   (type 8,  at 0x1F6E6)
    // and the block is skipped whenever the `[bp+6]` argument is non-zero
    // (0x1F6D0).  A recruit's speech belongs to the accept path, not to the
    // prompt, so printing it on entry would show the player the "yes" text
    // before they answered.  Types 1, 8 and 25 therefore print no block here.
    const bool type_skips_block = r.type == 1 || r.type == 8 || r.type == 25;
    if (r.has_index() && !type_skips_block) {
        const int scenario = spec_scenario_for_location(location);
        const std::string txt_path =
            dir + "/SYST/TXT" + std::to_string(scenario);
        std::vector<std::string> blocks = load_txt(txt_path);
        if (r.index_c9 >= blocks.size()) {
            printf("lof:   (text block %d missing from %s)\n",
                   (int)r.index_c9, txt_path.c_str());
        } else {
            const std::string& body = blocks[(size_t)r.index_c9];
            size_t start = 0;
            while (start <= body.size()) {
                size_t end = body.find('\0', start);
                if (end == std::string::npos) end = body.size();
                if (end > start)
                    printf("lof: %s\n", body.substr(start, end - start).c_str());
                start = end + 1;
            }
        }
    }

    // Recruits: `+0xeb` is an SNSC class index (0xff = none) and names the
    // character who would join.  The reader that matters is at 0x20F02 in the
    // join path -- it scales `+0xeb` by 0x4e (the SNSC stride), indexes the SNSC
    // image at [0x37b0] and calls 0x1D99A, which zeroes a 0x19a-byte character
    // slot and copies the record into it.
    //
    // Correction: an earlier version of this comment cited the reader at
    // 0x1F85B.  That block is guarded by `cmp byte es:[bx+0xea],0 ; jge` and by
    // `cmp byte es:[bx+0xeb],0 ; jl`, so it only runs for +0xea < 0 <= +0xeb --
    // a combination that occurs in 0 of the 665 shipped records.  SPEC0[1] has
    // +0xea == 42 and never reaches it.  The class index is still correct
    // (pinned by test_recruit_class and test_recruit_field_guard); only the
    // justification was wrong.
    //
    // The join itself is not implemented: the accept path clears the slot, copies
    // the SNSC record, appends the record's `+0xc9` speech and prints
    // " joins the companionship." (image string at file offset 0x4a2ff).  This
    // branch only reports the class the event names.
    if (r.type == 1 && r.has_class()) {
        std::vector<SnsRecord> classes = load_snsc(dir + "/SYST/SNSC");
        if (r.class_eb < classes.size()) {
            const std::string& who = classes[(size_t)r.class_eb].singular;
            // The join path (0x20EF6..) calls 0x1D99A with the SNSC record and a
            // free slot; 0x1D99A zeroes the 0x19a-byte slot and copies
            // `strlen(record)` bytes into `slot[1..]` -- i.e. the class *name*
            // (the record's 19-byte NUL-padded field stops the copy).  The
            // engine's own line is " joins the companionship." (image 0x44E05);
            // the sibling " finds no place in the team to join the companions."
            // covers a full party.  The port prints the success line and does
            // not yet mutate the roster (G-018: which free slot, the party
            // capacity, and where the recruit's *stats* come from).
            printf("lof:   %s joins the companionship.\n", who.c_str());
        } else {
            printf("lof:   (recruit class %u missing from SYST/SNSC)\n",
                   (unsigned)r.class_eb);
        }
    }

    // Type 7: a doorway.  The handler (0x20AD7..0x20B13) writes the record's
    // destination straight into the engine's party-position globals -- +0xe4 ->
    // [0x37fa] (location ones), +0xe6 -> [0x460e] (location tens), +0xe8 ->
    // [0x3824] (cell) -- and raises [0x3b15].  The save loader proves the global
    // roles and the flag's only reader (0x2C1F6) merely skips the normal
    // walk-buffer commit; so the door is a teleport to (tens*10+ones, cell).
    // See DoorTarget and GUESSWORK G-017 (resolved).
    if (r.type == 7) {
        g_door = door_target(r);
        const uint16_t dest = g_door.destination_location();
        printf("lof:   (door -> location %d, cell %d)\n",
               (int)dest, g_door.cell);
        if (dungeon_follow_door(dir, dest, g_door.cell, location, d, pos))
            return true;
        fprintf(stderr, "lof: cannot follow door to %s cell %d\n",
                dungeon_map_name(dest).c_str(), g_door.cell);
        return false;
    }

    // Type 11: teleport within the current map.  The handler
    // (0x20CD7..0x20CE4) writes the record's +0xe4 straight into the party-cell
    // global [0x3824]; the map/location is unchanged and there is no answer
    // tail.  Data confirms +0xe4 is a cell (the seven shipped type-11 records
    // carry 314..1168).  See notes-a.md "Per-type effects".
    if (r.type == 11) {
        const int cell = r.counter;   // +0xe4
        if (!dungeon_teleport_cell(pos, cell)) {
            fprintf(stderr, "lof: type-11 teleport to bad cell %d\n", cell);
            return false;
        }
        printf("lof:   (teleport -> cell %d)\n", cell);
        return true;
    }

    // Type 18: a slide + trap.  Same cell write as type 11 (0x20CE0), then
    // every present member loses `rand(+0xe6)+1` HP (0x20D2F..0x20D8E).  See
    // notes-a.md "Per-type effects".
    if (r.type == 18) {
        const int cell = r.counter;            // +0xe4
        if (dungeon_teleport_cell(pos, cell))
            printf("lof:   (trap slide -> cell %d)\n", cell);
        else
            fprintf(stderr, "lof: type-18 trap to bad cell %d\n", cell);
        if (sg != nullptr && rng != nullptr) {
            const int dmg = apply_trap_damage(*sg, r.arg_e6, *rng);  // +0xe6
            printf("lof:   (trap: %d damage across the party)\n", dmg);
        }
        return true;
    }

    // Plain-text group with no extra tail work: the event is fully reproduced.
    if (r.type <= 6) return false;
    printf("lof:   (event type %u at %d,%d: effect not implemented)\n",
           (unsigned)r.type, pos.x, pos.y);
    return false;
}

// One movement key press.  Turning keys just turn; a forward key steps, and if
// the cell stepped onto is a stair the party follows it to the destination
// level.  `location` is the save's level id (tens*10 + ones, the same encoding
// `save_location()` produces): a stair's high nibble is the destination dungeon
// while the level digit is unchanged, so the new id is
// `link.dungeon * 10 + location % 10`.
//
// Returns true if the key was a movement key and has been applied (so the
// caller must recompose the screen).  *level_changed is set when the party
// followed a stair, which also means the view directory changed; `msg` then
// receives the stair message.
static bool walk_key(const std::string& dir, uint16_t& location, Dungeon& dng,
                     DungeonPos& pos, int key, bool* level_changed,
                     std::string& msg) {
    if (level_changed) *level_changed = false;
    if (key == 0x50 || key == 0x40000051) {          // down: turn about
        dungeon_turn(pos, true);
        dungeon_turn(pos, true);
        return true;
    }
    if (key == 0x4b || key == 0x40000050) {          // left
        dungeon_turn(pos, false);
        return true;
    }
    if (key == 0x4d || key == 0x4000004f) {          // right
        dungeon_turn(pos, true);
        return true;
    }
    if (key != 0x48 && key != 0x40000052) return false;   // not a movement key

    int ahead_x = pos.x + dungeon_facing_dx(pos.facing);
    int ahead_y = pos.y + dungeon_facing_dy(pos.facing);
    uint8_t tile = dng.tile(ahead_x, ahead_y);
    if (!dungeon_is_stair(tile)) {
        dungeon_step(dng, pos);
        return true;
    }

    DungeonLink link;
    if (!dungeon_stair_target(dng, ahead_x, ahead_y, location, &link)) {
        dungeon_step(dng, pos);
        return true;
    }
    // The engine picks the wording before it moves the party, so it reads the
    // *current* level digit.
    const char* stair_msg = dungeon_stair_message(tile, location);
    uint16_t next_loc = (uint16_t)(link.dungeon * 10 + location % 10);
    Dungeon next;
    std::string err;
    if (!load_dungeon(dir + "/DNGS/" + dungeon_map_name(next_loc), next, &err)) {
        fprintf(stderr, "lof: %s\n", err.c_str());
        dungeon_step(dng, pos);
        return true;
    }
    dng = next;
    dungeon_place_cell(dng, pos, link.cell, pos.facing);
    location = next_loc;
    msg = stair_msg;
    if (level_changed) *level_changed = true;
    return true;
}

// The interactive front-end loop.  `intro` is the title/credits/menu sequence;
// any key advances it.  At the menu the recovered hot-keys apply: 'O' loads
// `save_path` and switches to the in-game play screen, 'Q'/Esc quits.  Frames
// are presented through an SDL window; without SDL2 there is no input source,
// so the loop exits immediately (headless builds still exercise the composition
// code through the scripted entry points --screen/--play/--shot).
// Build the combat rows for a save's party.  The per-character inputs are the
// record bytes the melee routine reads (port/combat.h); the two values the
// engine derives elsewhere are taken as parameters: `hit_score` is the
// prelude's ((attack+power+base)/3 [+25%]) / +0x55 built from the combat rows'
// +0x04/+0x05/+0x07/+0x08 bytes (the builder that fills them is not derived),
// and the weapon damage byte is 0 because the engine picks the wielded item
// from one of two hand slots at random (0x13A43) and that choice is not
// derived.  With no weapon the engine takes the fallback path at 0x14D80,
// which is exactly what melee_round() implements.
// The encounter's defender row: NSC00 record 12 (the pinned Mercenary), whose
// combat record the defender-row builder (0x27A1E) copies field by field.
static PartyRow encounter_defender_row(const std::string& dir, int scen, int rec,
                                       int hp) {
    char p[32];
    snprintf(p, sizeof p, "/SYST/NSC%02d", scen);
    auto recs = load_nsc(dir + p);
    if (rec >= 1 && rec <= (int)recs.size())
        return defender_row(recs[rec - 1], 0, hp, hp);
    return PartyRow{};
}

static std::vector<Combatant> build_party_combatants(const SaveGame& sg,
                                                     int hit_override,
                                                     const PartyRow& def_row) {
    std::vector<Combatant> v;
    for (size_t i = 0; i < sg.chars.size() && i < 6; i++) {
        const SaveChar& c = sg.chars[i];
        if (!c.present) continue;
        const uint8_t* r = c.raw.data();
        Combatant k;
        k.marker = 2;
        k.index = (int)i;
        k.name = c.name;
        k.group = 0;
        k.hp = r[0x22] | (r[0x23] << 8);
        k.max_hp = r[0x20] | (r[0x21] << 8);
        k.action = r[0x0f];
        k.target = r[0x0e];
        auto slots = spell_slots(c);
        k.spell = slots.empty() ? 0 : slots[0].index;
        k.exp = (uint32_t)r[0x6c] | ((uint32_t)r[0x6d] << 8) |
                ((uint32_t)r[0x6e] << 16) | ((uint32_t)r[0x6f] << 24);
        // The prelude's hit score, from this member's combat row and the
        // defender row (defaults: the measured Mercenary row).  `--hit-score`
        // overrides it for experiments.
        PartyRow atk = party_row(decode_char(r), (uint8_t)i);
        k.melee.hit_score = hit_override < 0
                                ? hit_score(atk, def_row, r[0x1a], k.melee.divisor55)
                                : hit_override;
        // The engine's weapon scan: class-1 items in either hand (fallback
        // class 5), picked with rand(2) per round (image 0x1398B/0x13A43).
        WeaponSlots w = weapon_slots(decode_char(r));
        for (int h = 0; h < 2; ++h) {
            k.melee.weapon_slot[h] = w.slot[h];
            k.melee.weapon_damage_slot[h] = w.damage[h];
            k.melee.weapon_f0[h] = w.f0[h];
        }
        k.melee.speed56 = r[0x56];
        k.melee.attacks5d = r[0x5d];
        k.melee.divisor55 = r[0x55] ? r[0x55] : 1;
        k.melee.skill27 = r[0x27];
        k.melee.str45 = r[0x45];
        k.melee.mod5e = r[0x5e];
        k.melee.mod5f = r[0x5f];
        k.melee.level17 = r[0x17];
        int pct = r[0x4e] | (r[0x4f] << 8);
        k.melee.pct4e = pct > 0 ? pct : 100;
        k.melee.xp_mult = 1;  // victim-side table byte; not derived
        v.push_back(k);
    }
    return v;
}

// The defenders for a synthetic encounter: `count` identical rows of `hp`.
static std::vector<Combatant> build_defenders(int count, int hp) {
    std::vector<Combatant> v;
    for (int i = 0; i < count; i++) {
        Combatant d;
        d.marker = 1;
        d.index = 12;  // SYST/NSC00 record 12 (Mercenary), the pinned encounter
        d.name = "Mercenary";
        d.group = 0;
        d.hp = d.max_hp = hp;
        d.action = kActAttack;
        d.melee.hit_score = 100;
        d.melee.speed56 = 1;
        d.melee.divisor55 = 1;
        d.melee.xp_mult = 1;
        v.push_back(d);
    }
    return v;
}

// Place the party where the save left it: the save's own cell/facing
// (`kSavePartyPos`/`kSaveFacing`) when present, else the level's entry cell.
// The DNG header only matches the save when the save was taken at the entry, so
// the header is the fallback, not the primary source.
static void place_party_from_save(const Dungeon& d, const SaveGame& sg,
                                  DungeonPos& pos) {
    if (sg.party_pos != kSavePartyPosInvalid &&
        dungeon_place_cell(d, pos, sg.party_pos,
                           sg.facing == kSaveFacingInvalid ? 0 : sg.facing))
        return;
    dungeon_place_entry(d, pos);
}

static bool run_ui(const std::string& dir, const std::vector<Screen>& intro,
                   const std::string& save_path, const std::vector<int>& keys,
                   const std::string& shot, int enc_count = 2, int enc_hp = 8) {
    Window win;
    win.open("Legend of Faerghail", 320, 200);
    win.script(keys);
    std::string err;
    // PLAY -> DECISION -> RESULT -> PLAY is the engine's combat entry: the
    // decision prompt (image 0x1810E) then the round resolution (0x17982) and
    // its table.  `C` in play opens a round against the pinned Mercenary group.
    enum { INTRO, PLAY, DECISION, RESULT, TOWN, EMPORIUM, BANK, HOUSE, TEMPLE,
           AMULET } state = INTRO;
    Screen combat_screen;
    Screen town_screen;
    std::vector<Combatant> combat_party, combat_monsters;
    CombatRng combat_rng;
    combat_rng.seed = 1;
    size_t frame = 0;
    Screen play_screen;
    bool have_play = false;
    Dungeon dng;
    DungeonPos pos;
    bool have_dng = false;
    SaveGame play_sg;
    int play_dungeon = 0;
    SpecTable play_spec;
    bool play_spec_loaded = false;
    // Compose the in-game screen (HUD + party panel, no viewport tile) and blit
    // the depth-selected first-person view for the party's current position.
    auto redraw_play = [&]() {
        if (!compose_play_screen(dir, "", play_sg, true, play_screen, err,
                                 /*with_view=*/false))
            return;
        if (!render_dungeon_play_view(play_screen, dir, play_dungeon, dng, pos))
            draw_viewport_map(play_screen, dng, pos);
    };
    for (;;) {
        if (state == INTRO) win.present(intro[frame]);
        else if (state == DECISION || state == RESULT) win.present(combat_screen);
        else if (state == TOWN || state == EMPORIUM || state == BANK ||
                 state == HOUSE || state == TEMPLE || state == AMULET)
            win.present(town_screen);
        else if (have_play) win.present(play_screen);
        int key = -1; bool quit = false;
        if (!win.wait_key(&key, &quit)) break;   // no SDL: nothing to drive it
        if (quit) break;
        if (state == INTRO) {
            bool on_menu = (frame + 1 == intro.size());
            if (on_menu && key == 'O') {
                SaveGame sg = load_save(save_path);
                if (sg.ok) {
                    play_sg = sg;
                    play_dungeon = save_location(sg);
                    if (load_dungeon(dir + "/DNGS/" + dungeon_map_name(play_dungeon),
                                     dng, &err)) {
                        place_party_from_save(dng, sg, pos);
                        have_dng = true;
                        redraw_play();
                        have_play = true;
                        state = PLAY;
                    } else {
                        // No map for this location: the engine stays in town and
                        // shows the building menu instead of a first-person view.
                        std::string e2;
                        if (compose_town_screen(dir, town_name_from_save(save_path),
                                                town_screen, e2, &play_sg)) {
                            have_play = false;
                            state = TOWN;
                        }
                    }
                }
            } else if (on_menu && key == 'Q') {
                break;
            } else if (frame + 1 < intro.size()) {
                frame++;
            }
        } else if (state == PLAY) {
            if (key == 'Q') break;
            if (key == 'C' && have_play) {
                combat_party = build_party_combatants(
                    play_sg, -1, encounter_defender_row(dir, 0, 12, 8));
                combat_monsters = build_defenders(enc_count, enc_hp);
                for (const Combatant& m : combat_party)
                    fprintf(stderr, "lof: combat start: %s hp %d exp %u\n",
                            m.name.c_str(), m.hp, m.exp);
                if (!compose_decision_screen(dir, 24, play_sg, true, combat_screen,
                                             err)) {
                    fprintf(stderr, "lof: %s\n", err.c_str());
                    break;
                }
                state = DECISION;
                continue;
            }
            // Arrow keys walk the party; recompose the viewport after a move.
            // A stair changes the level, so the view directory (and thus the
            // whole screen) has to be recomposed too.  `walk_key` applies every
            // movement key, including plain turns and blocked steps.
            if (have_play && have_dng) {
                uint16_t loc = (uint16_t)play_dungeon;
                std::string stair_msg;
                bool changed = false;
                if (walk_key(dir, loc, dng, pos, key, &changed, stair_msg)) {
                    if (changed) { play_dungeon = loc; play_spec_loaded = false; }
                    if (!stair_msg.empty()) printf("lof: %s\n", stair_msg.c_str());
                    if (fire_cell_event(dir, loc, dng, pos, &play_spec,
                                        &play_spec_loaded, &play_sg,
                                        g_event_answer, &combat_rng)) {
                        play_dungeon = loc;      // a door changed the level
                        play_spec_loaded = false;
                    }
                    redraw_play();
                }
            }
        } else if (state == TOWN) {
            if (key == 'Q') break;
            std::string e2;
            if (key == 'E') { compose_emporium_screen(dir, town_screen, e2, &play_sg); state = EMPORIUM; }
            else if (key == 'B') { compose_bank_screen(dir, town_screen, e2, &play_sg); state = BANK; }
            else if (key == 'H') { compose_house_screen(dir, town_screen, e2, &play_sg); state = HOUSE; }
            else if (key == 'T') { compose_temple_screen(dir, town_screen, e2, &play_sg); state = TEMPLE; }
            else if (key == 'I') { state = INTRO; frame = intro.size() - 1; }  // Inn = boot menu
            else if (key == 'C') {
                compose_amulet_screen(dir, play_sg, play_sg.ok, town_screen, e2);
                state = AMULET;
            }
        } else if (state == EMPORIUM || state == BANK || state == HOUSE ||
                   state == TEMPLE) {
            // Any key returns to the town hub.
            std::string e2;
            compose_town_screen(dir, town_name_from_save(save_path), town_screen, e2,
                                &play_sg);
            state = TOWN;
        } else if (state == AMULET) {
            // The leave-town step.  (L)eave it, (C)ontinue or a member digit
            // 1-6 all head out; a digit also hands the amulet to that member.
            // The destination is the region's outdoor map, DNG<tens>0 (the
            // emulator opens DNGS\DNG10 here), entered at the party's saved
            // cell/facing -- the position is preserved across the town visit.
            bool leave = (key == 'L' || key == 'C' || (key >= '1' && key <= '6'));
            if (leave) {
                if (key >= '1' && key <= '6') {
                    int idx = key - '1';
                    if (idx < (int)play_sg.chars.size() &&
                        play_sg.chars[idx].raw[kCharPresent] != 0)
                        give_item(play_sg.chars[idx], kItemAmulet);
                }
                uint16_t outdoor = (uint16_t)(play_sg.loc_tens * 10);
                std::string e3;
                if (load_dungeon(dir + "/DNGS/" + dungeon_map_name(outdoor), dng,
                                 &e3)) {
                    play_dungeon = outdoor;
                    // The party is now on the outdoor map: its location ones
                    // digit becomes 0 (so the status box titles the region,
                    // loc % 10 -> scenario 0).  Cell/facing are preserved.
                    play_sg.loc_ones = (uint8_t)(outdoor % 10);
                    place_party_from_save(dng, play_sg, pos);
                    have_dng = true;
                    redraw_play();
                    have_play = true;
                    state = PLAY;
                    continue;
                }
            }
            // No outdoor map (or a non-leaving key): return to the town hub.
            std::string e2;
            compose_town_screen(dir, town_name_from_save(save_path), town_screen, e2,
                                &play_sg);
            state = TOWN;
        } else if (state == DECISION) {
            // A digit 1-6 selects that (present) member to cast its slot-0
            // memorised spell this round, if it has one; pressing it again
            // reverts to attack.  Any other key commits the round.
            if (key >= '1' && key <= '6') {
                size_t idx = (size_t)(key - '1');
                if (idx < combat_party.size() && combat_party[idx].spell > 0) {
                    Combatant& m = combat_party[idx];
                    if (m.action == kActCastSpell) {
                        m.action = kActAttack;
                        fprintf(stderr, "lof: %s will attack\n", m.name.c_str());
                    } else {
                        m.action = kActCastSpell;
                        const SpellRecord* sp = spell_by_index(m.spell);
                        fprintf(stderr, "lof: %s will cast %s\n", m.name.c_str(),
                                sp ? sp->name.c_str() : "?");
                    }
                    continue;  // don't commit; wait for another key
                }
            }
            // Any other key commits the round: resolve it, then show the table.
            RoundResult rr = resolve_round(combat_rng, combat_party, combat_monsters);
            // Persist the round's HP/XP into the save so the next round starts
            // from the real state (the play screen does not show HP, but combat
            // must accumulate rather than reset to full HP every round).
            for (const Combatant& m : combat_party) {
                if (m.index < 0 || m.index >= (int)play_sg.chars.size()) continue;
                uint8_t* r = play_sg.chars[m.index].raw.data();
                r[0x22] = (uint8_t)(m.hp & 0xff);
                r[0x23] = (uint8_t)((m.hp >> 8) & 0xff);
                r[0x6c] = (uint8_t)(m.exp & 0xff);
                r[0x6d] = (uint8_t)((m.exp >> 8) & 0xff);
                r[0x6e] = (uint8_t)((m.exp >> 16) & 0xff);
                r[0x6f] = (uint8_t)((m.exp >> 24) & 0xff);
                if (m.action == kActCastSpell && m.spell > 0) {
                    int before = play_sg.chars[m.index].raw[kCharSkill142 + 1];
                    consume_spell_slot(play_sg.chars[m.index], 0);
                    fprintf(stderr, "lof: %s cast: charges %d -> %d\n",
                            m.name.c_str(), before,
                            play_sg.chars[m.index].raw[kCharSkill142 + 1]);
                }
                fprintf(stderr, "lof: round end: %s hp %d exp %u\n",
                        m.name.c_str(), m.hp, m.exp);
            }
            std::vector<RoundRow> rows;
            for (const CombatRoundRow& cr : rr.rows) {
                RoundRow row;
                row.hp_delta = cr.hp_delta;
                row.w = cr.w;
                row.p = cr.p;
                row.b = cr.b;
                row.attack = cr.attack;
                rows.push_back(row);
            }
            if (!compose_round_result_screen(dir, 24, play_sg, true, rows,
                                             combat_screen, err)) {
                fprintf(stderr, "lof: %s\n", err.c_str());
                break;
            }
            state = RESULT;
        } else if (state == RESULT) {
            // The engine returns to the play screen for the next round.
            redraw_play();
            state = PLAY;
        }
    }
    win.close();
    if (!shot.empty()) {
        const Screen& s = (state == DECISION || state == RESULT) ? combat_screen
                          : (state == TOWN || state == EMPORIUM || state == BANK ||
                             state == HOUSE || state == TEMPLE || state == AMULET)
                                ? town_screen
                                : (have_play ? play_screen : intro[frame]);
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
                      const std::string& save_out, const std::vector<int>& keys,
                      Screen& out, DungeonPos& pos) {
    SaveGame sg = load_save(save);
    if (!sg.ok) { fprintf(stderr, "lof: --walk needs a valid --save\n"); return false; }
    // The engine refuses a save whose trailer does not fold correctly (it shows
    // its "caught trying to fraud" page).  We load it anyway but say so: one of
    // the archived saves (GAME5) is tampered, and silently repairing it on write
    // would hide that.
    if (!sg.sealed)
        fprintf(stderr, "lof: warning: %s fails its checksum (tampered); "
                        "loading anyway, writing will re-seal it\n", save.c_str());
    uint16_t location = save_location(sg);
    std::string path = dir + "/DNGS/" + dungeon_map_name(location);
    Dungeon d;
    std::string err;
    if (!load_dungeon(path, d, &err)) {
        fprintf(stderr, "lof: %s: %s\n", path.c_str(), err.c_str());
        return false;
    }
    place_party_from_save(d, sg, pos);
    SpecTable spec;
    bool spec_loaded = false;
    // A walk-local RNG for the type-18 trap roll, seeded with the engine's
    // default INT 1Ah tick (LOF_TICK).  The port does not replay the whole RNG
    // stream, so trap HP is a model (see apply_trap_damage).
    CombatRng walk_rng;
    walk_rng.seed = 0x0B;
    for (int k : keys) {
        std::string msg;
        bool changed = false;
        uint16_t before = location;
        walk_key(dir, location, d, pos, k, &changed, msg);
        if (!msg.empty()) printf("lof: %s\n", msg.c_str());
        if (location != before) spec_loaded = false;   // stair: new level, new SPEC
        if (fire_cell_event(dir, location, d, pos, &spec, &spec_loaded, &sg,
                            g_event_answer, &walk_rng))
            spec_loaded = false;                       // door: new map, new SPEC
    }
    // Persist the walk.  The party position is written back into the save's
    // world block and the file is re-sealed, so a later load resumes here.
    if (!save_out.empty()) {
        save_set_party(sg, (uint16_t)(pos.y * kDungeonW + pos.x), pos.facing);
        if (!write_save_file(save_out, build_save(sg))) {
            fprintf(stderr, "lof: cannot write %s\n", save_out.c_str());
            return false;
        }
    }
    // Compose the HUD + party panel with no viewport tile, then blit the
    // depth-selected first-person view sprite into the window (its offset
    // depends on the wall depth, so it is not a fixed DP001 blit).
    if (!compose_play_screen(dir, "", sg, true, out, err, /*with_view=*/false))
        return false;
    if (!render_dungeon_play_view(out, dir, location, d, pos)) {
        // No view sprite on disk: fall back to the synthetic map viewport.
        draw_viewport_map(out, d, pos);
    }
    return true;
}

int main(int argc, char** argv) {
    std::string dir = game_dir(argv[0]);
    bool headless = false, have_shot = false, dump = false, dump_spec = false, dump_dng = false;
    bool ui = false, walk = false;
    std::string shot;
    long shot_frame = -1;
    std::string walk_save, walk_keys, walk_out, answer_text;
    // --cursor X,Y overlays the engine's software mouse cursor at (X,Y) on the
    // rendered screen.  The DOSBox reference captures were taken with the mouse
    // parked at (0,0), so `--cursor 0,0` reproduces them exactly.
    int cur_x = 0, cur_y = 0;
    bool have_cursor = false;

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
        else if (a == "--answer" && i + 1 < argc) {
            answer_text = argv[++i];
            g_event_answer = &answer_text;
        }
        else if (a == "--save-out" && i + 1 < argc) walk_out = argv[++i];
        else if (a == "--cursor" && i + 1 < argc) {
            const char* v = argv[++i];
            char* end = nullptr;
            cur_x = (int)strtol(v, &end, 10);
            if (end && *end == ',') cur_y = (int)strtol(end + 1, nullptr, 10);
            have_cursor = true;
        }
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
        if (!walk_loop(dir, save, walk_out, keys, s, pos)) return 1;
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
        int ui_enc_count = 2, ui_enc_hp = 8;
        for (int i = 1; i < argc; i++) {
            if (std::string(argv[i]) == "--save" && i + 1 < argc) ui_save = argv[++i];
            else if (std::string(argv[i]) == "--ui-encounter" && i + 1 < argc) {
                std::string v = argv[++i];
                size_t c = v.find(':');
                ui_enc_count = atoi(v.substr(0, c).c_str());
                ui_enc_hp = c == std::string::npos ? 8 : atoi(v.substr(c + 1).c_str());
            }
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
        if (!run_ui(dir, intro, ui_save, ui_keys, shot, ui_enc_count, ui_enc_hp)) return 1;
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
    bool charsheet_flag = false;
    int charsheet_member = 0;      // 0-based party slot for --charsheet
    bool town_flag = false;
    bool emporium_flag = false;
    bool bank_flag = false;
    bool house_flag = false;
    bool temple_flag = false;
    bool inn_rest_flag = false;
    bool inn_rest_pay_flag = false;
    int inn_rest_pay_cost = 2;
    int inn_rest_do = 0;            // 1-based payer for --inn-rest-do (0 = off)
    bool inn_rest_rations = false;  // --inn-rest-rations: the "replenished" option
    int temple_heal_do = 0;         // 1-based char to heal (--temple-heal-do)
    int temple_heal_payer = 0;      // 1-based payer (0 = the healed char)
    int temple_revive_do = 0;       // 1-based char to revive (--temple-revive-do)
    int temple_revive_payer = 0;    // 1-based payer (0 = the revived char)
    int temple_revive_roll = 1;     // engine rand(100)+1; CLI takes it as input
    int bank_deposit_do = 0;        // 1-based char to deposit for (0 = off)
    uint32_t bank_deposit_amt = 0;  // amount (0 = all)
    int bank_withdraw_do = 0;       // 1-based char to withdraw for (0 = off)
    uint32_t bank_withdraw_amt = 0; // amount (0 = all)
    int joint_deposit_do = 0;       // 1-based recipient for --joint-deposit
    int buy_do = 0;                 // 1-based char for --buy
    int buy_item_idx = 0;           // item index to buy
    int use_do = 0;                 // 1-based char for --use
    int use_item_idx = 0;           // item index to use
    int cast_do = 0;                // 1-based target char for --cast
    int cast_spell = 0;             // 1-based spell index to cast
    int spells_do = 0;              // 1-based char for --spells
    int train_do = 0;               // 1-based char for --train
    int train_hp = 5;               // HP gain roll for --train
    bool temple_heal_flag = false;
    bool temple_revive_flag = false;
    bool encounter_flag = false;
    bool amulet_flag = false;
    int amulet_give = 0;          // 1-based character to receive the amulet (0 = none)
    std::string amulet_out;       // where --amulet-give writes the updated save
    bool decision_flag = false;
    bool round_result_flag = false;
    bool fight_flag = false;
    int fight_hp = 8, fight_count = 2;
    int fight_hit_score = -1;
    // The typed reply for a dungeon record that carries a keyword (+0x00).
    int fight_monster_scen = 0, fight_monster_rec = 12;  // pinned Mercenary
    uint32_t fight_seed = 1;
    std::vector<EncounterGroup> encounter_groups;
    std::vector<RoundRow> round_rows;
    for (int i = 1; i < argc; i++) {
        if (std::string(argv[i]) == "--play" && i + 1 < argc) play = argv[++i];
        else if (std::string(argv[i]) == "--save" && i + 1 < argc) save = argv[++i];
        else if (std::string(argv[i]) == "--town") town_flag = true;
        else if (std::string(argv[i]) == "--charsheet") {
            charsheet_flag = true;
            if (i + 1 < argc && argv[i + 1][0] >= '0' && argv[i + 1][0] <= '9')
                charsheet_member = atoi(argv[++i]) - 1;  // 1-based on the CLI
        }
        else if (std::string(argv[i]) == "--emporium") emporium_flag = true;
        else if (std::string(argv[i]) == "--bank") bank_flag = true;
        else if (std::string(argv[i]) == "--house") house_flag = true;
        else if (std::string(argv[i]) == "--temple") temple_flag = true;
        else if (std::string(argv[i]) == "--inn-rest") inn_rest_flag = true;
        else if (std::string(argv[i]) == "--inn-rest-pay") {
            inn_rest_pay_flag = true;
            if (i + 1 < argc && argv[i + 1][0] >= '0' && argv[i + 1][0] <= '9')
                inn_rest_pay_cost = atoi(argv[++i]);
        }
        else if (std::string(argv[i]) == "--inn-rest-do" && i + 1 < argc) inn_rest_do = atoi(argv[++i]);
        else if (std::string(argv[i]) == "--inn-rest-rations") inn_rest_rations = true;
        else if (std::string(argv[i]) == "--temple-heal-do" && i + 1 < argc) temple_heal_do = atoi(argv[++i]);
        else if (std::string(argv[i]) == "--temple-heal-payer" && i + 1 < argc) temple_heal_payer = atoi(argv[++i]);
        else if (std::string(argv[i]) == "--temple-revive-do" && i + 1 < argc) temple_revive_do = atoi(argv[++i]);
        else if (std::string(argv[i]) == "--temple-revive-payer" && i + 1 < argc) temple_revive_payer = atoi(argv[++i]);
        else if (std::string(argv[i]) == "--temple-revive-roll" && i + 1 < argc) temple_revive_roll = atoi(argv[++i]);
        else if (std::string(argv[i]) == "--bank-deposit" && i + 2 < argc) {
            bank_deposit_do = atoi(argv[++i]);
            bank_deposit_amt = (uint32_t)strtoul(argv[++i], nullptr, 10);
        }
        else if (std::string(argv[i]) == "--bank-withdraw" && i + 2 < argc) {
            bank_withdraw_do = atoi(argv[++i]);
            bank_withdraw_amt = (uint32_t)strtoul(argv[++i], nullptr, 10);
        }
        else if (std::string(argv[i]) == "--joint-deposit" && i + 1 < argc) joint_deposit_do = atoi(argv[++i]);
        else if (std::string(argv[i]) == "--buy" && i + 2 < argc) {
            buy_do = atoi(argv[++i]);
            buy_item_idx = atoi(argv[++i]);
        }
        else if (std::string(argv[i]) == "--use" && i + 2 < argc) {
            use_do = atoi(argv[++i]);
            use_item_idx = atoi(argv[++i]);
        }
        else if (std::string(argv[i]) == "--cast" && i + 2 < argc) {
            cast_do = atoi(argv[++i]);
            cast_spell = atoi(argv[++i]);
        }
        else if (std::string(argv[i]) == "--spells" && i + 1 < argc) spells_do = atoi(argv[++i]);
        else if (std::string(argv[i]) == "--train" && i + 2 < argc) {
            train_do = atoi(argv[++i]);
            train_hp = atoi(argv[++i]);
        }
        else if (std::string(argv[i]) == "--temple-heal") temple_heal_flag = true;
        else if (std::string(argv[i]) == "--temple-revive") temple_revive_flag = true;
        else if (std::string(argv[i]) == "--decision") decision_flag = true;
        else if (std::string(argv[i]) == "--amulet") amulet_flag = true;
        else if (std::string(argv[i]) == "--amulet-give" && i + 1 < argc) {
            amulet_give = atoi(argv[++i]);
            amulet_flag = true;
        }
        else if (std::string(argv[i]) == "--amulet-out" && i + 1 < argc) amulet_out = argv[++i];
        else if (std::string(argv[i]) == "--fight") {
            fight_flag = true;
            // Optional "hp[:count]" defender spec.
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                unsigned hp = 8, count = 2;
                int n = sscanf(argv[++i], "%u:%u", &hp, &count);
                if (n >= 1) fight_hp = (int)hp;
                if (n >= 2) fight_count = (int)count;
            }
        }
        else if (std::string(argv[i]) == "--seed") {
            if (i + 1 < argc) fight_seed = (uint32_t)strtoul(argv[++i], nullptr, 0);
        }
        else if (std::string(argv[i]) == "--monster") {
            if (i + 1 < argc) sscanf(argv[++i], "%d:%d", &fight_monster_scen,
                                     &fight_monster_rec);
        }
        else if (std::string(argv[i]) == "--answer") {
            if (i + 1 < argc) {
                answer_text = argv[++i];
                g_event_answer = &answer_text;
            }
        }
        else if (std::string(argv[i]) == "--hit-score") {
            if (i + 1 < argc) fight_hit_score = atoi(argv[++i]);
        }
        else if (std::string(argv[i]) == "--round-result") {
            round_result_flag = true;
            // Optional "delta:w:p:b:a" row specs (w/p/b 0|1, a 0=Failed
            // 1=Passed 2=out-of-action), one per present member.  Consecutive
            // specs are consumed; anything that is not a spec ends the list.
            while (i + 1 < argc) {
                int delta = 0, w = 0, p = 0, b = 0, a = 1;
                char junk = 0;
                int n = sscanf(argv[i + 1], "%d:%d:%d:%d:%d%c",
                               &delta, &w, &p, &b, &a, &junk);
                if (n < 1 || n > 5) break;
                i++;
                RoundRow row;
                row.hp_delta = delta;
                row.w = w != 0; row.p = p != 0; row.b = b != 0;
                row.attack = (a < 0 || a > 2) ? 1 : a;
                round_rows.push_back(row);
            }
        }
        else if (std::string(argv[i]) == "--encounter") {
            encounter_flag = true;
            // Optional "rec[:count[:rank]]" spec; repeatable for several groups.
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                EncounterGroup g;
                int n = sscanf(argv[++i], "%d:%u:%u", &g.rec, &g.count, &g.rank);
                if (n < 2) g.count = 4;
                if (n < 3) g.rank = 2;
                encounter_groups.push_back(g);
            }
        }
    }
    // Bare --encounter renders the pinned oracle: two Mercenary groups.
    if (encounter_flag && encounter_groups.empty()) {
        encounter_groups.push_back({12, 4, 2});
        encounter_groups.push_back({12, 6, 1});
    }
    // Bare --round-result renders the pinned oracle frame
    // (ref/oracle/round_result.png): the six GAME2 members, Handy's weapon
    // damaged and its action failed, Cassandra down 7 HP.
    if (round_result_flag && round_rows.empty()) {
        round_rows.push_back({0, false, false, false, 1});
        round_rows.push_back({0, false, false, false, 1});
        round_rows.push_back({0, true,  false, false, 0});
        round_rows.push_back({0, false, false, false, 1});
        round_rows.push_back({-7, false, false, false, 1});
        round_rows.push_back({0, false, false, false, 1});
    }
    if (inn_rest_do > 0) {
        SaveGame sg;
        bool have_sg = false;
        if (!save.empty()) { sg = load_save(save); have_sg = sg.ok; }
        if (!have_sg) { fprintf(stderr, "lof: --inn-rest-do needs a valid --save\n"); return 1; }
        int idx = inn_rest_do - 1;
        if (idx < 0 || idx >= (int)sg.chars.size() || sg.chars[idx].present == 0) {
            fprintf(stderr, "lof: --inn-rest-do %d: no present character\n", inn_rest_do);
            return 1;
        }
        if (amulet_out.empty()) { fprintf(stderr, "lof: --inn-rest-do needs --amulet-out\n"); return 1; }
        int cost = rest_cost(sg, inn_rest_rations);
        if (!pay_gold(sg.chars[idx], cost)) {
            fprintf(stderr, "lof: character %d cannot pay %d gold\n", inn_rest_do, cost);
            return 1;
        }
        rest_party(sg, inn_rest_rations);
        if (!write_save_file(amulet_out, build_save(sg))) {
            fprintf(stderr, "lof: cannot write %s\n", amulet_out.c_str());
            return 1;
        }
        fprintf(stderr, "lof: rested; character %d paid %d gold%s\n", inn_rest_do, cost,
                inn_rest_rations ? " (rations replenished)" : "");
        return 0;
    }
    if (temple_heal_do > 0) {
        SaveGame sg;
        bool have_sg = false;
        if (!save.empty()) { sg = load_save(save); have_sg = sg.ok; }
        if (!have_sg) { fprintf(stderr, "lof: --temple-heal-do needs a valid --save\n"); return 1; }
        int h = temple_heal_do - 1;
        if (h < 0 || h >= (int)sg.chars.size() || sg.chars[h].present == 0) {
            fprintf(stderr, "lof: --temple-heal-do %d: no present character\n", temple_heal_do);
            return 1;
        }
        int p = temple_heal_payer > 0 ? temple_heal_payer - 1 : h;
        if (p < 0 || p >= (int)sg.chars.size() || sg.chars[p].present == 0) {
            fprintf(stderr, "lof: --temple-heal-payer %d: no present character\n", p + 1);
            return 1;
        }
        if (amulet_out.empty()) { fprintf(stderr, "lof: --temple-heal-do needs --amulet-out\n"); return 1; }
        int cost = temple_heal_cost(sg.chars[h]);
        if (cost == 0) {
            fprintf(stderr, "lof: character %d is already at full health\n", temple_heal_do);
            return 1;
        }
        if (!pay_gold(sg.chars[p], cost)) {
            fprintf(stderr, "lof: character %d cannot pay %d gold\n", p + 1, cost);
            return 1;
        }
        temple_heal_apply(sg.chars[h]);
        if (!write_save_file(amulet_out, build_save(sg))) {
            fprintf(stderr, "lof: cannot write %s\n", amulet_out.c_str());
            return 1;
        }
        fprintf(stderr, "lof: healed character %d for %d gold (payer %d)\n",
                temple_heal_do, cost, p + 1);
        return 0;
    }
    if (temple_revive_do > 0) {
        SaveGame sg;
        bool have_sg = false;
        if (!save.empty()) { sg = load_save(save); have_sg = sg.ok; }
        if (!have_sg) { fprintf(stderr, "lof: --temple-revive-do needs a valid --save\n"); return 1; }
        int h = temple_revive_do - 1;
        if (h < 0 || h >= (int)sg.chars.size() || sg.chars[h].present == 0) {
            fprintf(stderr, "lof: --temple-revive-do %d: no present character\n", temple_revive_do);
            return 1;
        }
        int p = temple_revive_payer > 0 ? temple_revive_payer - 1 : h;
        if (p < 0 || p >= (int)sg.chars.size() || sg.chars[p].present == 0) {
            fprintf(stderr, "lof: --temple-revive-payer %d: no present character\n", p + 1);
            return 1;
        }
        if (amulet_out.empty()) { fprintf(stderr, "lof: --temple-revive-do needs --amulet-out\n"); return 1; }
        int cost = temple_revive_cost(sg.chars[h]);
        if (!pay_gold(sg.chars[p], cost)) {
            fprintf(stderr, "lof: character %d cannot pay %d gold\n", p + 1, cost);
            return 1;
        }
        bool ok = temple_revive(sg.chars[h], temple_revive_roll);
        if (!write_save_file(amulet_out, build_save(sg))) {
            fprintf(stderr, "lof: cannot write %s\n", amulet_out.c_str());
            return 1;
        }
        fprintf(stderr, "lof: revive of character %d %s (paid %d gold)\n",
                temple_revive_do, ok ? "succeeded" : "failed", cost);
        return 0;
    }
    if (bank_deposit_do > 0 || bank_withdraw_do > 0) {
        bool is_dep = bank_deposit_do > 0;
        int who = is_dep ? bank_deposit_do : bank_withdraw_do;
        uint32_t amt = is_dep ? bank_deposit_amt : bank_withdraw_amt;
        SaveGame sg;
        bool have_sg = false;
        if (!save.empty()) { sg = load_save(save); have_sg = sg.ok; }
        if (!have_sg) { fprintf(stderr, "lof: bank needs a valid --save\n"); return 1; }
        int idx = who - 1;
        if (idx < 0 || idx >= (int)sg.chars.size() || sg.chars[idx].present == 0) {
            fprintf(stderr, "lof: bank %d: no present character\n", who);
            return 1;
        }
        if (amulet_out.empty()) { fprintf(stderr, "lof: bank needs --amulet-out\n"); return 1; }
        if (amt == 0) amt = is_dep ? char_gold(sg.chars[idx]) : char_bank(sg.chars[idx]);
        bool ok = is_dep ? bank_deposit(sg.chars[idx], amt)
                         : bank_withdraw(sg.chars[idx], amt);
        if (!ok) {
            fprintf(stderr, "lof: character %d has not enough %s for %u\n", who,
                    is_dep ? "gold" : "account", amt);
            return 1;
        }
        if (!write_save_file(amulet_out, build_save(sg))) {
            fprintf(stderr, "lof: cannot write %s\n", amulet_out.c_str());
            return 1;
        }
        fprintf(stderr, "lof: %s %u for character %d (gold %u, account %u)\n",
                is_dep ? "deposited" : "withdrew", amt, who,
                char_gold(sg.chars[idx]), char_bank(sg.chars[idx]));
        return 0;
    }
    if (joint_deposit_do > 0) {
        SaveGame sg;
        bool have_sg = false;
        if (!save.empty()) { sg = load_save(save); have_sg = sg.ok; }
        if (!have_sg) { fprintf(stderr, "lof: --joint-deposit needs a valid --save\n"); return 1; }
        if (amulet_out.empty()) { fprintf(stderr, "lof: --joint-deposit needs --amulet-out\n"); return 1; }
        if (!joint_deposit(sg, joint_deposit_do - 1)) {
            fprintf(stderr, "lof: joint deposit to character %d failed (no gold or too heavy)\n",
                    joint_deposit_do);
            return 1;
        }
        if (!write_save_file(amulet_out, build_save(sg))) {
            fprintf(stderr, "lof: cannot write %s\n", amulet_out.c_str());
            return 1;
        }
        fprintf(stderr, "lof: joint deposit -> character %d (gold %u)\n",
                joint_deposit_do, char_gold(sg.chars[joint_deposit_do - 1]));
        return 0;
    }
    if (buy_do > 0) {
        SaveGame sg;
        bool have_sg = false;
        if (!save.empty()) { sg = load_save(save); have_sg = sg.ok; }
        if (!have_sg) { fprintf(stderr, "lof: --buy needs a valid --save\n"); return 1; }
        if (amulet_out.empty()) { fprintf(stderr, "lof: --buy needs --amulet-out\n"); return 1; }
        const ItemRecord* it = item_by_index(buy_item_idx);
        if (!it) { fprintf(stderr, "lof: --buy %d: no such item\n", buy_item_idx); return 1; }
        int price = it->price;
        ShopData shop = load_shop(dir + "/ROST/SHOP");
        if (!buy_item(sg, buy_do - 1, buy_item_idx, &shop)) {
            fprintf(stderr, "lof: cannot buy item %d for character %d (price %d)\n",
                    buy_item_idx, buy_do, price);
            return 1;
        }
        if (!write_save_file(amulet_out, build_save(sg))) {
            fprintf(stderr, "lof: cannot write %s\n", amulet_out.c_str());
            return 1;
        }
        fprintf(stderr, "lof: character %d bought '%s' for %d gold (gold now %u)\n",
                buy_do, it->name.c_str(), price, char_gold(sg.chars[buy_do - 1]));
        return 0;
    }
    if (use_do > 0) {
        SaveGame sg;
        bool have_sg = false;
        if (!save.empty()) { sg = load_save(save); have_sg = sg.ok; }
        if (!have_sg) { fprintf(stderr, "lof: --use needs a valid --save\n"); return 1; }
        if (amulet_out.empty()) { fprintf(stderr, "lof: --use needs --amulet-out\n"); return 1; }
        if (use_do - 1 < 0 || use_do - 1 >= (int)sg.chars.size() ||
            sg.chars[use_do - 1].present == 0) {
            fprintf(stderr, "lof: --use %d: no such character\n", use_do);
            return 1;
        }
        const ItemRecord* it = item_by_index(use_item_idx);
        if (!it) { fprintf(stderr, "lof: --use: no such item %d\n", use_item_idx); return 1; }
        SaveChar& c = sg.chars[use_do - 1];
        auto hp_of = [](const SaveChar& sc) {
            return (uint16_t)(sc.raw[kCharHpCur22] | (sc.raw[kCharHpCur22 + 1] << 8));
        };
        uint16_t hp_before = hp_of(c);
        std::string verb = use_item_effect(c, use_item_idx);
        if (verb.empty()) {
            fprintf(stderr, "lof: item %d ('%s') has no derived use-effect\n",
                    use_item_idx, it->name.c_str());
            return 1;
        }
        if (!write_save_file(amulet_out, build_save(sg))) {
            fprintf(stderr, "lof: cannot write %s\n", amulet_out.c_str());
            return 1;
        }
        fprintf(stderr, "lof: character %d used '%s' (%s); hp %u -> %u\n",
                use_do, it->name.c_str(), verb.c_str(), hp_before, hp_of(c));
        return 0;
    }
    if (cast_do > 0) {
        SaveGame sg;
        bool have_sg = false;
        if (!save.empty()) { sg = load_save(save); have_sg = sg.ok; }
        if (!have_sg) { fprintf(stderr, "lof: --cast needs a valid --save\n"); return 1; }
        if (amulet_out.empty()) { fprintf(stderr, "lof: --cast needs --amulet-out\n"); return 1; }
        if (cast_do - 1 < 0 || cast_do - 1 >= (int)sg.chars.size() ||
            sg.chars[cast_do - 1].present == 0) {
            fprintf(stderr, "lof: --cast %d: no such character\n", cast_do);
            return 1;
        }
        const SpellRecord* sp = spell_by_index(cast_spell);
        if (!sp) { fprintf(stderr, "lof: --cast: no such spell %d\n", cast_spell); return 1; }
        SaveChar& c = sg.chars[cast_do - 1];
        auto hp_of = [](const SaveChar& sc) {
            return (uint16_t)(sc.raw[kCharHpCur22] | (sc.raw[kCharHpCur22 + 1] << 8));
        };
        uint16_t hp_before = hp_of(c);
        std::string verb = apply_spell_effect(c, *sp);
        if (verb.empty()) {
            fprintf(stderr, "lof: spell %d ('%s') has no derived character effect\n",
                    cast_spell, sp->name.c_str());
            return 1;
        }
        if (!write_save_file(amulet_out, build_save(sg))) {
            fprintf(stderr, "lof: cannot write %s\n", amulet_out.c_str());
            return 1;
        }
        fprintf(stderr, "lof: cast '%s' on character %d (%s); hp %u -> %u\n",
                sp->name.c_str(), cast_do, verb.c_str(), hp_before, hp_of(c));
        return 0;
    }
    if (spells_do > 0) {
        SaveGame sg;
        bool have_sg = false;
        if (!save.empty()) { sg = load_save(save); have_sg = sg.ok; }
        if (!have_sg) { fprintf(stderr, "lof: --spells needs a valid --save\n"); return 1; }
        if (spells_do - 1 < 0 || spells_do - 1 >= (int)sg.chars.size()) {
            fprintf(stderr, "lof: --spells %d: no such character\n", spells_do);
            return 1;
        }
        auto slots = spell_slots(sg.chars[spells_do - 1]);
        if (slots.empty()) {
            printf("lof: character %d has no memorised spells\n", spells_do);
            return 0;
        }
        printf("lof: character %d memorised spells:", spells_do);
        for (auto& sl : slots) {
            const SpellRecord* sp = spell_by_index(sl.index);
            printf(" [%d] %s (x%d)", sl.index, sp ? sp->name.c_str() : "?", sl.charges);
        }
        printf("\n");
        return 0;
    }
    if (train_do > 0) {
        SaveGame sg;
        bool have_sg = false;
        if (!save.empty()) { sg = load_save(save); have_sg = sg.ok; }
        if (!have_sg) { fprintf(stderr, "lof: --train needs a valid --save\n"); return 1; }
        if (train_do - 1 < 0 || train_do - 1 >= (int)sg.chars.size() ||
            sg.chars[train_do - 1].present == 0) {
            fprintf(stderr, "lof: --train %d: no such character\n", train_do);
            return 1;
        }
        LevelUpResult lr = level_up(sg.chars[train_do - 1], train_hp);
        if (!amulet_out.empty() &&
            !write_save_file(amulet_out, build_save(sg))) {
            fprintf(stderr, "lof: cannot write %s\n", amulet_out.c_str());
            return 1;
        }
        printf("lof: character %d -> level %d (+%d hp); improved:",
               train_do, lr.new_level, lr.hp_gain);
        for (auto& s : lr.improved) printf(" %s", s.c_str());
        printf("\n");
        return 0;
    }
    if (charsheet_flag) {
        std::string err;
        Screen s;
        SaveGame bsg;
        bool have_sg = false;
        if (!save.empty()) { bsg = load_save(save); have_sg = bsg.ok; }
        if (!compose_charsheet_screen(dir, bsg, have_sg, charsheet_member, s, err)) {
            fprintf(stderr, "lof: %s\n", err.c_str());
            return 1;
        }
        if (have_cursor) draw_cursor(s, cur_x, cur_y);
        if (have_shot && !save_png(shot, s)) { fprintf(stderr, "lof: cannot write %s\n", shot.c_str()); return 1; }
        if (!headless) present_sdl(s, "Legend of Faerghail");
        return 0;
    }
    if (town_flag || emporium_flag || bank_flag || house_flag || temple_flag ||
        inn_rest_flag || inn_rest_pay_flag || temple_heal_flag || temple_revive_flag) {
        std::string err;
        Screen s;
        SaveGame bsg;
        const SaveGame* sgptr = nullptr;
        if (!save.empty()) { bsg = load_save(save); if (bsg.ok) sgptr = &bsg; }
        bool ok = temple_heal_flag   ? compose_temple_prompt(dir, "Whose wounds should",
                                                             "we heal (1-6)?", s, err, sgptr)
                  : temple_revive_flag ? compose_temple_prompt(dir, "Who should be revived",
                                                             "(1-6)?", s, err, sgptr)
                  : inn_rest_flag      ? compose_inn_rest_screen(dir, s, err, sgptr)
                  : inn_rest_pay_flag  ? compose_inn_rest_pay_screen(dir, inn_rest_pay_cost, s, err, sgptr)
                  : temple_flag  ? compose_temple_screen(dir, s, err, sgptr)
                  : house_flag    ? compose_house_screen(dir, s, err, sgptr)
                  : bank_flag   ? compose_bank_screen(dir, s, err, sgptr)
                  : emporium_flag ? compose_emporium_screen(dir, s, err, sgptr)
                                : compose_town_screen(dir, town_name_from_save(save), s, err, sgptr);
        if (!ok) {
            fprintf(stderr, "lof: %s\n", err.c_str());
            return 1;
        }
        if (have_cursor) draw_cursor(s, cur_x, cur_y);
        if (have_shot && !save_png(shot, s)) { fprintf(stderr, "lof: cannot write %s\n", shot.c_str()); return 1; }
        if (!headless) present_sdl(s, "Legend of Faerghail");
        return 0;
    }
    if (encounter_flag) {
        std::string err;
        SaveGame sg;
        bool have_sg = false;
        if (!save.empty()) {
            sg = load_save(save);
            have_sg = sg.ok;
        }
        std::vector<SnsRecord> roster = monster_roster(dir, 0);
        int sprite_ch = dungeon_room_art_number(dir, 0, encounter_groups[0].rec);
        if (sprite_ch < 0) sprite_ch = 0;
        std::string intro = combat_intro_names().at(0);
        Screen s;
        if (!compose_encounter_screen(dir, intro.c_str(), encounter_groups, roster,
                                      sprite_ch, sg, have_sg, s, err)) {
            fprintf(stderr, "lof: %s\n", err.c_str());
            return 1;
        }
        if (have_cursor) draw_cursor(s, cur_x, cur_y);
        if (have_shot && !save_png(shot, s)) { fprintf(stderr, "lof: cannot write %s\n", shot.c_str()); return 1; }
        if (!headless) present_sdl(s, "Legend of Faerghail");
        return 0;
    }
    if (amulet_flag) {
        std::string err;
        SaveGame sg;
        bool have_sg = false;
        if (!save.empty()) {
            sg = load_save(save);
            have_sg = sg.ok;
        }
        if (amulet_give > 0) {
            if (!have_sg) { fprintf(stderr, "lof: --amulet-give needs --save\n"); return 1; }
            int idx = amulet_give - 1;
            if (idx < 0 || idx >= (int)sg.chars.size() || sg.chars[idx].present == 0) {
                fprintf(stderr, "lof: --amulet-give %d: no present character\n", amulet_give);
                return 1;
            }
            if (amulet_out.empty()) { fprintf(stderr, "lof: --amulet-give needs --amulet-out\n"); return 1; }
            int slot = give_item(sg.chars[idx], kItemAmulet);
            if (slot < 0) { fprintf(stderr, "lof: inventory full\n"); return 1; }
            if (!write_save_file(amulet_out, build_save(sg))) {
                fprintf(stderr, "lof: cannot write %s\n", amulet_out.c_str());
                return 1;
            }
            fprintf(stderr, "lof: gave %s to character %d (slot %d)\n",
                    item_by_index(kItemAmulet)->name.c_str(), amulet_give, slot);
        }
        Screen s;
        if (!compose_amulet_screen(dir, sg, have_sg, s, err)) {
            fprintf(stderr, "lof: %s\n", err.c_str());
            return 1;
        }
        if (have_cursor) draw_cursor(s, cur_x, cur_y);
        if (have_shot && !save_png(shot, s)) { fprintf(stderr, "lof: cannot write %s\n", shot.c_str()); return 1; }
        if (!headless) present_sdl(s, "Legend of Faerghail");
        return 0;
    }
    // --fight: resolve one melee round for the save's party against a synthetic
    // defender group, print the table and (with --shot) render it.  The round
    // model is the derived one (port/combat.h).
    if (fight_flag) {
        std::string err;
        if (save.empty()) { fprintf(stderr, "lof: --fight needs --save\n"); return 1; }
        SaveGame sg = load_save(save);
        if (!sg.ok) { fprintf(stderr, "lof: --fight needs a valid --save\n"); return 1; }
        // The defender row comes from the NSC record's own combat record
        // (defender_row(), builder 0x27A1E), so the hit score uses the
        // engine's data instead of a measured constant.
        PartyRow def_row = encounter_defender_row(dir, fight_monster_scen,
                                                 fight_monster_rec, fight_hp);
        std::vector<Combatant> party =
            build_party_combatants(sg, fight_hit_score, def_row);
        std::vector<Combatant> defenders = build_defenders(fight_count, fight_hp);
        for (Combatant& d : defenders) d.melee.xp_mult = 0;
        // The defenders' XP multiplier is their NSC record's +0x2c byte (the
        // builder at 0x1195A copies the 78-byte record into the combat record,
        // so +0x2c == stats[6]).  `--monster SCEN:REC` selects the record;
        // the default is the pinned oracle's Mercenary (NSC00 record 12).
        {
            char p[32];
            snprintf(p, sizeof p, "/SYST/NSC%02d", fight_monster_scen);
            auto recs = load_nsc(dir + p);
            if (fight_monster_rec >= 1 &&
                fight_monster_rec <= (int)recs.size()) {
                int xm = xp_per_damage(recs[fight_monster_rec - 1].stats[6]);
                for (Combatant& d : defenders) d.melee.xp_mult = xm;
            }
        }
        std::vector<uint32_t> exp_before;
        for (const Combatant& m : party) exp_before.push_back(m.exp);
        CombatRng rng;
        rng.seed = fight_seed;
        RoundResult rr = resolve_round(rng, party, defenders);
        printf("round seed=%u defenders=%d hp=%d\n", fight_seed, fight_count, fight_hp);
        for (size_t i = 0; i < party.size(); i++) {
            const CombatRoundRow& r = rr.rows[i];
            printf("  %d %-10s hp %3d/%3d  %+4d  %s  WPB=%d%d%d\n", (int)i + 1,
                   party[i].name.c_str(), party[i].hp, party[i].max_hp,
                   r.hp_delta,
                   r.attack == 2 ? "out of action"
                                 : (r.attack == 1 ? "Passed" : "Failed"),
                   r.w ? 1 : 0, r.p ? 1 : 0, r.b ? 1 : 0);
        }
        for (const Combatant& d : defenders)
            printf("  defender %-10s hp %3d/%3d %s\n", d.name.c_str(), d.hp,
                   d.max_hp, d.hp > 0 ? "" : "(down)");
        for (size_t i = 0; i < party.size(); i++)
            if (party[i].exp != exp_before[i])
                printf("  xp %-10s +%u\n", party[i].name.c_str(),
                       (unsigned)(party[i].exp - exp_before[i]));
        if (!shot.empty()) {
            std::vector<RoundRow> rows;
            for (const CombatRoundRow& cr : rr.rows) {
                RoundRow row;
                row.hp_delta = cr.hp_delta;
                row.w = cr.w;
                row.p = cr.p;
                row.b = cr.b;
                row.attack = cr.attack;
                rows.push_back(row);
            }
            int sprite_ch = dungeon_room_art_number(dir, 0, 12);
            if (sprite_ch < 0) sprite_ch = 24;
            Screen s;
            if (!compose_round_result_screen(dir, sprite_ch, sg, true, rows, s, err)) {
                fprintf(stderr, "lof: %s\n", err.c_str());
                return 1;
            }
            if (!save_png(shot, s)) { fprintf(stderr, "lof: cannot write %s\n", shot.c_str()); return 1; }
        }
        return 0;
    }
    if (decision_flag) {
        std::string err;
        SaveGame sg;
        bool have_sg = false;
        if (!save.empty()) {
            sg = load_save(save);
            have_sg = sg.ok;
        }
        if (!have_sg) { fprintf(stderr, "lof: --decision needs a valid --save\n"); return 1; }
        // The encounter that precedes the decision screen: the oracle capture
        // shows the same viewport as ref/oracle/combat.png (the Mercenary
        // encounter, SYST/NSC00 record 12 -> CHAR0/CH24).
        int sprite_ch = dungeon_room_art_number(dir, 0, 12);
        if (sprite_ch < 0) {
            fprintf(stderr, "lof: --decision needs SYST/NSC00 record 12\n");
            return 1;
        }
        Screen s;
        if (!compose_decision_screen(dir, sprite_ch, sg, have_sg, s, err)) {
            fprintf(stderr, "lof: %s\n", err.c_str());
            return 1;
        }
        if (have_cursor) draw_cursor(s, cur_x, cur_y);
        if (have_shot && !save_png(shot, s)) { fprintf(stderr, "lof: cannot write %s\n", shot.c_str()); return 1; }
        if (!headless) present_sdl(s, "Legend of Faerghail");
        return 0;
    }
    if (round_result_flag) {
        std::string err;
        SaveGame sg;
        bool have_sg = false;
        if (!save.empty()) {
            sg = load_save(save);
            have_sg = sg.ok;
        }
        if (!have_sg) { fprintf(stderr, "lof: --round-result needs a valid --save\n"); return 1; }
        // The table is drawn over the encounter that produced the round; the
        // oracle frame follows the same Mercenary encounter as --decision.
        int sprite_ch = dungeon_room_art_number(dir, 0, 12);
        if (sprite_ch < 0) {
            fprintf(stderr, "lof: --round-result needs SYST/NSC00 record 12\n");
            return 1;
        }
        Screen s;
        if (!compose_round_result_screen(dir, sprite_ch, sg, have_sg, round_rows,
                                         s, err)) {
            fprintf(stderr, "lof: %s\n", err.c_str());
            return 1;
        }
        if (have_cursor) draw_cursor(s, cur_x, cur_y);
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
        if (have_cursor) draw_cursor(s, cur_x, cur_y);
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
    if (have_cursor) draw_cursor(scr, cur_x, cur_y);

    if (have_shot && !save_png(shot, scr)) {
        fprintf(stderr, "lof: cannot write %s\n", shot.c_str());
        return 1;
    }
    if (!headless) present_sdl_frames(frames, "Legend of Faerghail");
    return 0;
}
