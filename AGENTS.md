# AGENTS.md — Legend of Faerghail native drop-in

> **New here? Read `docs/RULES.md` first (the owner's standing rules + their
> changelog), then `docs/GUESSWORK.md` (the guess register) and
> `docs/OWNER-TASKS.md` (owner-only work), then `docs/HANDOFF.md`,
> `docs/RISKS.md`, and `docs/WORKSTREAMS.md`.** The rules file
> is authoritative: **never guess**, and **do not edit a file another active
> agent is working on** — every underived value must be removed, filed as a
> blocker if another area is needed, or (if it cannot be removed) reported to the
> owner and re-reminded every session. They
> cover how to survive a workspace reset and the file map. This
> file is the deep technical reference. When you get blocked, record it in the
> area's blocker file under `docs/blockers/` (A = dungeons, B = characters, M =
> shared/engine); the hub `docs/BLOCKERS.md` defines the `BLK-<area><nnn>` IDs and
> fields. There is **one lane** now — see `docs/WORKSTREAMS.md`.

## Goal

Play the 1990 DOS RPG *Legend of Faerghail* (reLINE Software) natively on Ubuntu
**without executing the original executables**.

The deliverable is a pair of drop-in binaries — `lof` and `vor` — that sit in the
game folder in place of `LOF.EXE` / `VOR.EXE`. They **must not run the original
EXEs**; they read the game's *data* files (`EGA/`, `SYST/`, `DNGS/`, `ROST/`,
`DSKTAB.DFS`, `*.SAV`, ...) and reproduce the original behaviour as faithfully as
possible by re-implementing the engine in editable C/C++. The drop-in must work
even if `LOF.EXE` / `VOR.EXE` are absent, so that a player who owns the game data
can still play.

## IMPORTANT: what NOT to do

- **Do NOT execute the original `LOF.EXE` / `VOR.EXE`.** The emulator (below) is
  a *development aid* for decompilation and reference only; the shipped drop-in
  must not depend on it or on the original binaries at run time.

## Original-binary analysis (reference only)

The 8086/EGA emulator in `src/` exists solely to *run the originals once*, so we
can observe behaviour and produce a decompilation (`tools/decompile.py`,
`analysis/*.asm`). Findings from that analysis drive the re-implementation.

- `VOR.EXE` is **both** the intro/title screen (title graphic + a text page) **and**
  the copy-protection step: it plants the `lofx` signature in the INT 0xFA vector
  slot, and `LOF.EXE`'s startup guard aborts unless that signature is present.
  It prints "Please reboot your system without resident programs." only when a
  prior instance is already resident. So VOR *does* show content and must be
  reproduced (it is not copy-protection-only).
- `LOF.EXE` loads its graphics from `EGA\...` (e.g. `ega\in\titl`, `ega\char0\ch00`,
  `ega\up\up00`..), its disk table from `DSKTAB.DFS`, and system data from
  `SYST\SNSC`.
- `VOR.EXE` draws its title from `EGA\IN\DRAG` (320x200, 4-plane EGA image).

## Data formats

### Which release is used (and why)

`Legend of Faerghail` was developed by Electronic Design Hannover and published
by reLINE Software (both German) in 1990 for Amiga, Atari ST and MS-DOS. The
task asks for a German DOS build if one exists, otherwise the international one.
Research (Internet Archive software search, Wikipedia/OGDB, abandonware mirrors)
found **no separate German DOS release**: every archived MS-DOS dump is the
English build, and the game was shipped internationally with English text (the
manual even notes translation errors). We therefore use the international
MS-DOS release, which is what the drop-in targets.

Three archives were obtained and are kept under `downloads/` (git-ignored):

- `lof_msdos.zip` — "Legend of Faerghail (1990)", the 5.25" MS-DOS build
  (extracted to `downloads/ext_msdos/legfaer`). This is the copy the drop-in is
  developed against.
- `lof_35.zip` — the 3.5" MS-DOS build (`downloads/ext_35/3.5/DISK0{1,2,3}.IMG`).
- `lof_ark.zip` — the same 5.25" build from the DOS Abandonware Ark.

The extracted game folder (`game/`) holds `LOF.EXE`, `VOR.EXE` and the data
directories the drop-in reads. The `.EXE`s are only ever run by the analysis
emulator; the shipped `lof`/`vor` do not use them.

### EGA image files (`EGA/**`) — decoded, see `tools/ega.py`

All `EGA` files are RLE-compressed 4-plane EGA images:

    off 0  : escape byte (RLE marker; the stream starts right after the header)
    off 1  : 0x1234 magic (word)          <- the loader tests `cmp [si],0x1234`
    off 3  : width in bytes per row (word)  -> pixel width = width*8
    off 5  : height in rows (word)
    off 7  : decompressed size + 10 (word)
    off 9  : 2 reserved bytes
    off 11 : RLE stream

RLE: literal bytes copy through; the escape byte introduces a (count, value)
pair emitting `value` repeated `count` times. The decoded buffer is four EGA
bitplanes concatenated (plane0=blue, 1=green, 2=red, 3=intensity), each
`width*height` bytes; MSB is the leftmost pixel. Verified: the decoded
`EGA/IN/DRAG` reproduces the emulator's VOR title frame.

Loader code: `LOF.EXE` image offset `0x1A378` (`lcall 0x18d:0x16a8`); the
blitter that consumes the decoded buffer is at image offset `0x1C7B`.

### EGA palette (VOR intro)

`VOR.EXE` programs the EGA attribute palette at run time with
`INT 10h AH=10h AL=02h` (set all 16 palette registers from `ES:DX`), using five
17-byte tables in its data segment (`ES`=image segment, table at `0x4834`,
`0x4845`, `0x4856`, `0x4867`, `0x4878`; 16 registers + 1 overscan). The values
are the standard EGA 6-bit colour selectors; the emulator's `ega_rgb()` decodes
them (primary guns weight 2, secondary weight 1). The intro fades between these
palettes.

### VOR intro text page

After a key press, VOR shows the **credits page** (game credits, reLINE Software
Hannover; "IBM-PC Version & GFA-converting: Holger Heinrich", etc.). The page is
the pre-rendered full-screen bitmap `EGA/IN/CRED` (320x200, standard EGA image
format) — it is *not* a vector font and the text is *not* stored as plaintext.
`lof --screen CRED` reproduces it exactly. (VOR draws only `EGA/IN/DRAG` + this
still; the menu/game come from LOF.EXE.)

### Boot / menu screens

`START.BAT` runs `vor` then `lof`. LOF **refuses to start unless launched from
START.BAT** (it inspects the DOS command tail / parent and prints "Please start
LOF with START.BAT"). The emulator's `LOF_CMDLEN=n` injects a non-empty PSP
command tail to get past this.

Visible sequence (EGA 320x200): credits still (`CRED`) → the main menu, whose
static artwork is `EGA/UP/MSCR` (full 320x200 background) plus `EGA/UP/MENU`
(184x105 menu frame). Menu entries (OCR): pick-pocket / Rest / Look for new
character / Erase a character / Add a new character / Dismiss a character /
Open file, ...

### System data files (`SYST/`)

- `SYST/SNSC` — 14 records x 78 bytes. Each record begins with two 19-byte,
  NUL-padded names: the **singular** form then the **plural** form (e.g. "Dragon"
  / "Dragons"). A 40-byte statistics block follows. These are the 14 character /
  creature classes (Siegurd, Eljot, Ihl Kazar, Mask Bearer, Dragon, Earth, Fire,
  Water, Air, Angel, Woodkeeper, Phantomfighter, Warrior, Werewolf).
- `SYST/NSCnn` — a count byte followed by that many 78-byte records in the same
  layout (per-scenario NPC/monster tables).
- `SYST/TXTn` — a table of little-endian **(offset, length) word pairs** followed
  by the string bodies (the offsets point at the bodies; the table ends where the
  first body begins). `TXT0` holds the long dialogue texts (English).
- `DSKTAB.DFS` — 140 bytes, all zero in this release (disk/volume table; zeroed
  because the game was installed to a hard disk).

## Build

> Full build guide (prerequisites, every target, SDL2 vs head-less, running,
> troubleshooting): **[`docs/BUILD.md`](docs/BUILD.md)**.  The short version:

```sh
make            # SDL2 window build if SDL2 present, else falls back
make headless   # no SDL2 (used for CI / headless debugging)
make test       # CPU self-test
```

Output binary: `./lof-native`.

The verification tests (`make golden-port`, `make check`) need Python with
`numpy` and `Pillow` (the frame comparators), and the interactive front-end
builds an SDL2 window when `libsdl2-dev` + `pkg-config` are present (otherwise
`make headless` / the default fallback is head-less and `--ui` runs scripted
input only).  On a fresh Debian/Ubuntu box:

```sh
apt-get install -y libsdl2-dev pkg-config   # optional: live window
pip install numpy pillow                     # required by the frame tests
```

`tests/golden_test.sh` (the `golden` target, run by `check.sh`) is the emulator
regression sweep: 12 independent, deterministic scenarios.  They run
concurrently on `GOLDEN_JOBS` workers (default `nproc`), so a sweep takes a few
minutes instead of ~25; `GOLDEN_JOBS=1` forces the old serial order.  The
recorded hashes in `tests/golden.txt` are emitted in a fixed order either way,
so parallelism does not change what is asserted.  Re-record only after an
intended change: `./tests/golden_test.sh --record`.

## Run (analysis emulator — dev aid only)

```sh
./lof-native --dir game --headless --frames 300 --shot out.png   # render a frame
./lof-native --dir game                                          # interactive (SDL2)
```

`--dir` points at the game data directory (must contain `LOF.EXE`, `VOR.EXE`,
`EGA/`, `SYST/`, `DNGS/`, `DSKTAB.DFS`, ...).  When `--dir` is omitted the data
directory defaults to the folder holding the executable.

Useful flags: `--exe <path>` (load a specific program), `--vor` (run VOR first,
then LOF — the original START.BAT order), `--shot-frame N` (capture frame N).

This emulator is a **development aid** for producing the decompilation and
reference captures. It is not the deliverable.

### Drop-in binaries (the deliverable) — `port/`

`make port` builds `lof` and `vor` in the repo root; `make dropin` copies them into
`game/` under the names `START.BAT` invokes. These are **native re-implementations**
that read the game *data* files and never touch `LOF.EXE` / `VOR.EXE`.

- `port/gfx.{h,cpp}` — EGA decoder (same format as `tools/ega.py`), 4-plane
  blitter, 16-colour palette, minimal PNG writer, optional SDL2 window.
- `port/vor.cpp` — loads `EGA/IN/DRAG`, draws it full screen (the intro/title).
- `port/lof.cpp` — boot/title path; more screens are added as formats are decoded.

Both accept `--dir DIR`, `--headless`, `--shot FILE.png` (for verification).
`lof` additionally accepts `--screen NAME` (render one named screen, e.g.
`--screen CRED`, `--screen UP/MSCR`, `--screen EGA/UP/MENU`), `--play SUB/DPNNN`
(compose the in-game play screen, see below), `--encounter`, `--decision`,
`--round-result`, `--fight` (resolve one derived combat round, see below),
`--ui` (run the interactive front-end, see below) and
`--shot-frame N` (which frame of the intro sequence `--shot` captures;
default = last).

Progress:

- [x] VOR title screen (`EGA/IN/DRAG`) reproduced; verified against DOSBox-X.
- [x] VOR credits page (`EGA/IN/CRED`, shown after a key press) reproduced.
- [x] LOF boot sequence: title (`EGA/IN/TITL`) -> credits (`EGA/IN/CRED`) ->
      main menu.
- [x] LOF main menu artwork: `EGA/UP/MSCR` (full background) + `EGA/UP/MENU`
      (menu frame blitted at 7,4).
- [x] Font decoding: `EGA/UP/CHR1`/`CHR2` glyph mapping (see below).
- [x] LOF play screen (static composition): `--play D0/DP001` draws the HUD
      background `EGA/UP/MSCR` with the first-person viewport tile
      `EGA/D0/DP001` blitted into the 120x119 window at (10,11), then the full
      six-member party panel from the save (`--save GAMES/GAMEn`) in the compact
      small-caps face: the name (white), the profession (black), the `+0x1e`
      stat number (dark gray, `%d` + a `%` glyph) and the `GOOD` status word
      (white).  The whole panel now matches the oracle **pixel-exactly**.
      `--play auto --save GAMES/GAMEn` picks the view directory from the save's
      location (see the save-location note below).  The full screen matches the
      DOSBox slot-2 capture `ref/oracle/play.png` at ~94.2%
      (residue = the engine's dynamic viewport overlays only). Guarded by
      `tests/port_play_test.py` (`make golden-port`). In-game movement and
      re-rendering are not yet implemented; the front-end now enters this screen
      via `--ui` (see below).
- [x] LOF interactive front-end (`--ui`): a keyboard-driven state machine over
      the intro. Any key advances title -> credits -> menu; at the menu the
      recovered hot-keys apply: `O` loads a save (default `GAMES/GAME2`) and
      switches to the in-game play screen, `Q`/Esc quits. Input goes through
      `Window` (SDL2 when built with it, otherwise the loop is scripted). The
      `--ui-keys "32,32,79"` option feeds a fixed ASCII key sequence so the
      transitions can be regression-tested head-lessly (no display); `--save`
      selects the slot and `--shot` captures the final frame. Guarded by
      `tests/port_ui_test.py` (`make golden-port`), which checks that the
      scripted menu frame is byte-identical to the direct `--shot` menu and that
      the scripted `O` transition reproduces the play screen (~94.2% vs the
      DOSBox capture, the same residue as `--play`).
- [x] VOR preloader: `EGA/IN/DRAG` (dragon title) then `EGA/IN/CRED` (credits).
      Both are pure image blits, so the port reproduces each *pixel-exactly*
      from the game data. Guarded by `tests/port_vor_test.py` (`make
      golden-port`); no DOSBox capture needed.
- [x] LOF town + building screens (`--town`, `--emporium`, `--bank`, `--house`,
      `--temple`): all share one shape — the HUD background `EGA/UP/MSCR` with a
      town-scene tile blitted into the 120x119 viewport at (10,11) plus `CHR1`
      text at x=139 (rows 92..172).  The town menu uses the street tile
      `EGA/TWN/TWN00`; the buildings use their exterior tile and greeting/menu
      strings (all the engine's own):
        * Emporium = `TWN01`: "Well hi, wealthy heroes, what / would you like,
          huh?" + (B)uy/(S)ell/(R)epair wares, (G)ather gold, (C)ontinue;
        * Bank = `TWN02`: "Greetings, brave heroes, what can we do for you?" +
          (D)eposit/open, (W)ithdraw/close, (E)xamine accounts, (J)oint
          deposit, (C)ontinue;
        * House of trades = `TWN04`: "The Grand Board of Trades / greets you in
          Thyn." + "Welcome!" + (T)raining, (L)earn magic spells, (A)nother
          language, (J)oint deposit, (C)ontinue;
        * Temple = `TWN05`: "welcome to our / Great Temple!" + (H)eal character,
          (R)evive a dead character, (J)oint deposit, (C)ontinue.
      The viewport tile is drawn as the engine's **two-tile scrolled blit**
      (`blit_wrapped_viewport`): the tile's column 0 lands at viewport x=`x0`,
      and the columns that scroll off the right edge wrap back in on the left one
      scanline lower.  `x0`=0 is a plain blit (town + the building exteriors);
      the main menu uses `x0`=16 and the **Temple uses `x0`=40**.  The viewport
      and text are pixel-exact against the emulator frames; only the engine's
      16x16 top-left patch (the software mouse cursor, reproduced by
      `--cursor 0,0`) and the dynamic party panel differ.  Guarded by
      `tests/port_town_test.py` and `tests/port_building_test.py`
      (`make golden-port`).
- [x] Inn + Temple sub-prompts.  Neither the Inn nor the Temple is a `D5`
      first-person interior (that earlier note was a misconception — there is no
      such state).  The **Inn screen *is* the boot main menu**: the game starts
      inside the Inn and the town `(I)nn` key redisplays the same menu
      (`ref/oracle/inn.png` is byte-identical to `menu.png` except the dynamic
      party panel).  The Inn's `(R)est` option opens a sub-prompt over the *same*
      scrolled `TWN03` viewport the menu uses (`--inn-rest`): "Would you like a
      simple / nights rest, or do you also / wish to have all rations /
      replenished?" + (S)imple nights stay / (W)ith replenished rations /
      (J)oint deposit / (C)ontinue.  The Temple's `(H)eal` / `(R)evive` options
      likewise open a character-selection sub-prompt over the *same* `TWN05`
      viewport (`--temple-heal` / `--temple-revive`): "Whose wounds should / we
      heal (1-6)?" / "Who should be revived / (1-6)?" + (C)ontinue.  All strings
      are the engine's own.  Guarded by `tests/port_inn_test.py` and the
      `temple_heal`/`temple_revive` rows of `tests/port_building_test.py`.
- [x] Encounter screen (`--encounter`): the wandering-monster screen the engine
      draws when it spawns enemies (first met from the wilderness).  It reuses
      the play screen's chrome — `EGA/UP/MSCR`, the party/status panel (its
      oracle panel is *byte-identical* to the play oracle's, so it is drawn by
      the same `draw_party_and_status`) — and blits the encountered monster's
      creature sprite into the 120x119 viewport.  The sprite is the monster
      record's `+0x48` art: `SYST/NSC00` record 12 (Mercenary) has `+0x48`=24,
      i.e. **`EGA/CHAR0/CH24`** (the emulator's own open trace for the oracle
      encounter, `LOF_I21=1`, opens `ega\char0\ch24`).  It is a full 120x119
      cell blitted **plainly at the viewport origin (10,11)**, which reproduces
      the oracle viewport at **1.0000** (verified for two sprites, `CH24` and
      `CH26`; the origin is the unique best offset — GUESSWORK G-008).
      Text is `CHR1` at a 6-px pitch, x=139: the intro "You unknowingly meet"
      and "What will you do ?" in colour 8; each group line "4 Mercenaries
      (Rnk 2)" / "6 Mercenaries (Rnk 1)" with its count+name in colour 8 and
      its " (Rnk R)" tail black; the four choices "(A)ttack" …
      "(W)ithdraw at once" black.  Whole-screen match **0.96** vs
      `ref/oracle/combat.png` (residue = the dynamic party/status values, as on
      the play screen).  `--encounter [rec[:count[:rank]]]` is repeatable (one
      group per line); the bare flag renders the pinned oracle (two Mercenary
      groups).  Guarded by `tests/port_encounter_test.py` (`make golden-port`).
- [x] Combat decision screen (`--decision`): the per-round prompt "Please make
      your decision / for the next round!".  It is the **encounter composition
      with only the right-hand text block replaced** — the two oracles
      (`ref/oracle/combat.png` and `combat_decision.png`) share their viewport,
      roster and bottom strip pixel-exactly, which pins that derivation.
      The rows are the engine's own printf sequence (image `0x18734`):
      1-based index (`"%ld"`, colour 8, x=139), `"%-10s "` name (x=145), the
      rank code, the action word (x=235), then `"@ %ld"`.
      * The rank code is `combat_abbrev_names()[char[+0x24]]` (`KIL`/`ATK`/
        `DEF`/`RET`, strings at `ds:0x0EB0`, file `0x42BC0`) selected through
        the 4-entry far-pointer table at image `0x187F0`.  **`+0x24` is the
        combat rank**, not "body/health state" — the engine spells the four
        values out in its rank-entry menu: `(1)Enter Kill-Rank` ..
        `(4)Enter Retreat-Rank` (`ds:0x713C..0x717A`, compared at
        `0x1913C`/`0x1916C`/`0x1919C`/`0x191CC`), and `0x197F0` stores the
        pressed digit minus `'1'`.  The older annotation had misread those
        compares as a dying/dead test.
      * The action word is `action_names()[char[+0x0f]]` (table `ds:0x0EC0`,
        file `0x42BD0`: Defends/Attacks/Stalks/Use object/Cast Spell).
      * The trailing number is `+0x0d + 1` when `+0x16 == 0xff` (guarded at
        image `0x186F3` on signed `+0x0d` in `0..2` and `+0x0f != 0`), else
        `+0x16 + 1` (image `0x18880`).  It follows the 10-cell action field, so
        the cursor is at x=295 and the `@`/space advance 6 px each without
        inking, leaving the digit at x=307.
      * A non-zero `+0x49` replaces the action column with `ds:0x6FF2`
        "is out of action." and suppresses the number (image `0x18839`).
      * Row filter: slot present and `+0x1f < 5` (image `0x18798`/`0x1879E`).
      Whole screen matches `ref/oracle/combat_decision.png` at **0.96**
      (viewport 1.000, text 0.993; the residue is the dynamic party/status
      values plus the emulator capture's index-12 descender pixels).  Guarded
      by `tests/port_decision_test.py`; the rank mapping, the row guard and the
      trailing-field inputs are pinned by `tests/char_test.cpp`.
- [x] Combat **round-result** screen (`--round-result`): `The result of this
      round:` + the melee table `Name       HP   W P B Attack`.  It is the same
      encounter chrome as `--decision` with the table replacing the decision
      block.  Layout (image `0x1867D`..`0x189F6`): title y=92 (colour 8),
      header y=108 and rule y=116 (colour 0), one row per **present** member at
      y=124+8n (colour 8), `(C)ontinue` at y=180 (colour 0).  The columns fall
      out of the engine's printf formats: `"%-10s "` name (x=139, 11 cells),
      `"%4ld "` HP change right-aligned in four 6-px cells (x=205), `"* "/"- "`
      W/P/B at x=235/247/259, `Passed`/`Failed`/`-` at x=271 (the `-` is
      `char +0x49 != 0`).  The row values are the engine's **four per-round
      arrays**, not save fields (`--round-result delta:w:p:b:a` takes them per
      member; the bare flag renders the pinned oracle round).  The oracle
      `ref/oracle/round_result.png` is deterministic; the port's text **ink
      mask matches 1.0000** (the residual index-12 pixels are the emulator's
      text artefact — present in `combat*.png` too, absent from DOSBox).
      Guarded by `tests/port_round_result_test.py` (`make golden-port`).
      Deriving the layout also settled **what the text drawer's third argument
      is**: a *newline* flag, not a "style" — see the GUESSWORK G-011
      correction (face = `cs:0x1412` via `lcall 0x18d:0x1787`, colour =
      `cs:0x1414` via `lcall 0x18d:0x175a`).  Two font cells were missing and
      are now mapped: `:` = CHR1 band 1 cell 37, `*` = band 1 cell 30
      (`tests/font_test.cpp`).

- [x] Combat **round model** (`--fight`, and `C` in `--ui`): the engine's melee
      round, transcribed from the fallback path at image `0x14D80` (the branch
      every shipped `GAME2` attack actually takes: `record[+0x16] == 0xff` for
      all six, verified with `LOF_TRACE_IP=24840:24a60`).  Per attacker it rolls
      `min(10, (record[+0x5d]+record[+0x56])/record[+0x55])` attacks; each is
      `rand(weapon damage)+1` + `max(0, str/+0x55 + +0x5e - 15)`, plus
      `(+0x5f + level) * (dmg/10)` when `rand(100)+1 <= +0x27/+0x55`, then a hit
      test `rand(100)+1 < hit_score`; the hits are summed, scaled by
      `100/record32[+0x4e]` when that is below 100, divided by `+0x55`, **capped
      at 99**, and applied with `sub word victim[+0x0a],ax` (`0x1547E`) plus the
      XP award `+= dmg * [0xafc6]:0xf37+victim[+0x2c]`.  The generator is the
      MS-C LCG at `0x30ECC` (`seed*0x343FD+0x269EC3`, output `>>16 & 0x7FFF`).
      `port/combat.{h,cpp}` implements it (`CombatRng`, `melee_round`,
      `resolve_round`); `--fight [hp[:count]]` prints one round for a save's
      party and renders it through the *same* `--round-result` printer (pinned
      byte-identical by the test), and `--ui` wires play → decision → round
      result → play.  The weapon damage is the engine's own scan
      (`weapon_slots()`: class-1 items in either hand, `rand(2)` hand pick,
      class-5 staff fallback) and the hit score is derived from the combat rows
      (`hit_score()`).  Two inputs are still parameters (the XP multiplier's
      per-monster index and the W/P/B columns) — GUESSWORK G-012.
      Guarded by `tests/combat_test.cpp` (`make test`) and
      `tests/port_fight_test.py`.

### Font / text rendering (decoded)

`EGA/UP/CHR1` and `EGA/UP/CHR2` are 320x24 strips, each 40 8x8 glyph cells
stored as three 8-pixel bands (a plain 40-cell grid would only be 320 pixels
wide, so the bands are stacked vertically). Reading each cell MSB-first (as for
the other EGA images) gives recognisable glyphs, and the mapping is:

| band | cells 0..3 | cells 4..29 | cells 30..39 |
|------|-----------|-------------|--------------|
| 0    | symbols   | upper A..Z  | digits 0..9  |
| 1    | symbols   | lower a..z  | symbols      |
| 2    | symbols   | -           | -            |

`CHR2` additionally carries a **compact small-caps face**: for every ASCII
character the cell index is `toupper(ch) - 64` (so `A`..`Z` -> 1..26, `@` -> 0,
`[` -> 27), the glyph is the **top-left 5x5** of that band-0 cell, and glyphs
advance **5 px**. The engine uses this face for the party-panel names on the
play screen (`Decker`, `Siegurd`, …); it is *not* a separate asset and it is not
in `CHR1`. Decoded glyphs (`A C D H M S a c d e g i k l n r s u y`) are
self-consistent and render the oracle names at 100%.

The engine uses `CHR1` for the main-menu text, advancing 6 pixels per cell (the
cells overlap: the next glyph is 6px right, so the last two columns are shared).
The mapping, the two punctuation cells (`(` = band 1 cell 2, `)` = band 1
cell 3) and the 6px advance were confirmed by template-matching the strip
against a DOSBox capture of the main menu, whose entries are:

```
(P)ick-pocket
(R)est
                     <- blank row
(L)ook for new character
(E)rase a character
(A)dd a new character
(D)ismiss a character
(O)pen file
```

The menu is **keyboard-driven**. Confirmed by pressing each letter in the
emulator (golden frames `tests/golden.txt`) — only these five advance the screen,
and each renders a distinct one:

| Key | Scan | Screen |
|-----|------|--------|
| P `0x19` | pick-pocket | (no screen change from the start menu) |
| R `0x13` | Rest | (no screen change from the start menu) |
| L `0x26` | Look for new character | character sheet (portrait + stats panel) |
| E `0x12` | Erase a character | erase prompt |
| A `0x1e` | Add a new character | add prompt |
| D `0x20` | Dismiss a character | dismiss prompt |
| O `0x18` | Open file | load-file prompt |

(P and R act on the current party, so with no game started they are no-ops.) An
earlier note claimed the menu was mouse-only — that was wrong: only Enter had
been tried, which is not a menu hot-key.

### In-game (town) screen

Pressing `O` then Enter loads the save and enters the play loop. The engine then
reads `ROST/ROST` and `ROST/SHOP` (party + shop) and the town graphics
`EGA/TWN/TWN00..TWN05` (plus the shared `EGA/UP/*` UI assets), and draws the
town screen. This is the first reachable in-game state, so it is the natural
oracle for the drop-in's play loop and for area B's remaining stat/combat
semantics. Captured deterministically as the `town` scenario in
`tests/golden_test.sh` (`--keys 0x18,0x1c`).

Menu text is drawn at x=139, rows at y=124+8*n (colour 0). `port/font.{h,cpp}` loads the
strip at run time (no font data is embedded) and draws text; `tests/font_test.cpp`
covers the mapping, the advance and the draw path.

**Town name is save-selected, not hardcoded.** The town routine (image `0x53E0`)
tests the 32-bit cell `es:[0x460e]` (via `[ds:0xaf48]`) and draws `ds:0x5A39`
"Thyn. " when it is nonzero, else `ds:0x5A40` "Cyldane. ". The save loader
(image `0x1C952`) fills that cell from the **16-bit word at save offset
`0x9a4`** (sign-extended into AX:DX). So `--save` selects the name: nonzero →
Thyn, zero → Cyldane (the boot initialiser at image `0x179` leaves it at 1, i.e.
Thyn, with no save). The House-of-Trades greeting ("greets you in Thyn.") is a
**fixed immediate** in the engine (`mov ax,0x4B5F`, image `0x876D`) — the House
always says Thyn; do not make it vary. Guarded by `tests/port_town_test.py`.

**Font rendering (resolved — no transform).** The engine's menu/town/panel text
is the `EGA/UP/CHR1`/`CHR2` cell bitmaps blitted **directly**, left 6 columns at a
6-px advance; there is no bold/double-struck weighting step. This was confirmed
by pixel-matching the DOSBox oracle: every glyph of `Welcome, hardy` /
`adventurers!` / `(P)ick-pocket` … / `(C)ontinue` equals the CHR1 cell
`glyph_cell()` selects, and the *analysis emulator's* glyph shapes are
byte-identical to the oracle capture (only its palette differs). The port's menu
text is therefore pixel-exact (`tests/port_menu_test.py` pins the greeting /
option list / `(C)ontinue` regions). The earlier "bold, double-struck, no exact
match" note was an artefact of the wrong-palette era before `port/gfx.cpp` used
the standard EGA set — with the correct palette the CHR1 cells match exactly.
Resolved as `BLK-M001`.

**EGA palette.** The engine programs the attribute controller at start-up
(`INT 10h AH=10h AL=02h`) with the **standard EGA/BGI attribute set**
(`00 01 02 03 04 05 14 07 38 39 3A 3B 3C 3D 3E 3F`): index 6 is brown
`(170,85,0)` and index 8 dark gray `(85,85,85)`. A naive "index i>=8 -> i+8"
remap renders index 6 yellow and index 8 green and is **wrong**. Verified by
decoding the DOSBox menu capture: its 13 on-screen colours are exactly the stock
set. `port/gfx.cpp` carries the set in `kEgaAttr`; with it the port menu matches
the DOSBox oracle to 0.16% (`make golden-port`).

> **Emulator palette caveat.** The analysis emulator renders the menu with the
> palette the game *programs* via `INT 10h AH=10h AL=02h` (its last such call
> writes `00 01 02 03 04 05 06 07 10 11 12 13 14 15 16 17`, so index 6 becomes
> dark yellow `(170,170,0)`). DOSBox — the ground-truth oracle — shows the
> *stock* set instead (index 6 brown `(170,85,0)`). The drop-in follows DOSBox,
> not the emulator; do **not** "fix" the port to match the emulator here.



Roadmap:

1. Keep the analysis emulator working (below) to observe/annotate the originals.
2. Extract the data-file formats (EGA graphics, `DSKTAB.DFS`, `SYST\SNSC`, saves).
3. Re-implement, in order: VOR title screen -> LOF boot/menu -> the play loop.
4. `make dropin` installs the re-implementation into `game/` as `lof` and `vor`.

### Debug env hooks (all optional)

| var | effect |
|-----|--------|
| `LOF_PLANT_LOFX=1`   | plant the copy-protection `lofx` value at 0x3E8 (bypass guard) |
| `LOF_DEBUG=1`        | per-run summary (instr count, mode, plane nonzero, int counts) |
| `LOF_VDBG=1`         | framebuffer/palette/seq/gc dump at end |
| `LOF_PAL2=1`         | per-register attribute palette dump |
| `LOF_EDUMP=<file>`   | dump the 4 EGA planes (4 x 0x10000 bytes) |
| `LOF_DUMP=<file>`    | dump the full 1 MB linear memory |
| `LOF_SEGDUMP=<file>,<seg>:<off>:<len>` | dump `len` linear bytes at `seg:off` (e.g. read a runtime data table) |
| `LOF_VTRACE=1`       | plane write min/max/nonzero statistics |
| `LOF_WMODE=1`        | histogram of (map-mask, write-mode, set/reset, bitmask) |
| `LOF_PTRACE=1`       | EGA/VGA port OUT trace |
| `LOF_GC=1` `LOF_BM=1` `LOF_ATTR=1` `LOF_INT10P=1` | register-level traces |
| `LOF_I21=1` `LOF_I21ALL=1` | DOS INT 21h open/read trace / per-function counts |
| `LOF_MODE=1`         | trace INT 10h AH=00 video mode sets |
| `LOF_CLICK`/`LOF_KEYS` | scripted input for headless runs |
| `LOF_KEYONCE=1`      | inject `--keys` once instead of every 20 frames |
| `LOF_KEYSTEP=N`      | deliver one `--keys` entry every N frames |
| `LOF_KEYAT=f:sc,...` | queue each scancode at an exact frame (multi-screen scripts) |
| `LOF_HOUR`/`LOF_MIN`/`LOF_SEC` | override the DOS clock (INT 21h AH=2Ch) |
| `LOF_WATCH=lo:hi`   | log every byte/word write to `[lo,hi)` (hex) with the writing `cs:ip` |
| `LOF_PXWATCH=x,y`   | log the `cs:ip` that writes EGA viewport pixel `(x,y)` (+ `LOF_PXWATCH_N=n` to cap hits) — find "which code draws this pixel" directly. Also prints `si/di/bx/bp/ds` at the write. `LOF_PXDUMP=<file>` (+ `LOF_PXDUMP_AT=<hit>`, default 0) snapshots the DS segment (`ds:0000..01FF`) at that hit — recovers a scratch sprite the engine composed before blitting |
| `LOF_TRACE_IP=lo:hi` | trace instructions whose linear address (`cs*16+ip`) is in `[lo,hi)` (hex); a bare `addr` still matches exactly. `LOF_TRACE_BT=1` also dumps the raw `ss:sp` words and the frame chain, for routines with no valid `bp`. **Address trap:** the disassembly offsets in `docs/notes-*.md` are offsets into the flat image (`LOF.EXE[0x54F0:]`), and the image is loaded at **linear `image + 0x10100`** (load segment `0x1010`), so a routine at image `0x17982` runs at `0x27A82`. Segment immediates in the file (e.g. `mov dx,0x367e`) are *relocated* by the loader (`+0x1010` → `0x468E`), so do not assume `linear == image offset`; confirm with `LOF_DUMP` and a byte search |
| `LOF_CAP=<file>` (+ `LOF_CAP_N=<n>`, default 1) | one-shot: dump the full 1 MB linear memory at the **Nth** hit of the `LOF_TRACE_IP` point — reads a routine's *input* state (or, with `N` past the first hit, its output). Used to capture the BLK-A005 producer's monster-placement input |
| `LOF_TICK=n`         | value returned by `INT 1Ah` (BIOS time), i.e. the RNG seed (default `0x0B`); `LOF_I1A=1` traces it |
| `LOF_DISASM=seg:off:len` | hex-dump `len` bytes of the loaded image at `seg:off` |
| `LOF_ALLOC=1`        | log each INT 21h AH=48 allocation (want, free paragraph) |

**Gotcha — `LOF_DEBUG`'s `mode=` field is decimal.** A run at EGA mode `0Dh`
prints `mode=13`; that is **13 decimal = 0x0D**, i.e. EGA 320x200x16, *not* VGA
mode 13h. The game only ever selects EGA `0Dh` (see `src/machine.cpp`). Do not
read the debug field as hex.

## Town / play-loop flow (oracle for the drop-in)

The save filename the engine opens is `GAMES\GAME%d` (no padding; verified by
the `LOF_I21=1` open trace, which opens `GAMES\GAME1` for slot 1).  There are
**five** save slots: the load prompt string is `ds:0x7b8a` = `   M  1 2 3 4 5 C`,
so slots are `1`..`5` only (`M` = menu, `C` = cancel).  An older note claimed
`%3d` / `GAMES/GAME  1` with two padding spaces and six slots — both wrong.
`Machine::resolve()` still strips spaces and trailing dots (DOS ignores them),
so it is harmless either way.

The play loop is entered from the main menu by `O` (Open) then the **digit key**
of the slot (`1`..`5`, scancodes `0x02`..`0x06`); `Enter` on the dialog does
nothing.  The load is long and its timing varies, so headless scripts must feed
the keys adaptively (`LOF_KEYAUTO="0x18,0x02" LOF_KEYGAP=8`), not by frame.

Two shipped saves reach the play loop and are both guarded by
`tests/golden_test.sh`:

| slot | scenario | planes md5 (60/1500 frames) | screen |
|------|----------|------------------------------|--------|
| 1    | `temple` | `a3f719edd43bc692dc230539fdf34705` | the **Temple dungeon**, dark first-person viewport |
| 2    | `play`   | `585da631a63e2199db7425c5b422ee52` | the wilderness |

Owner-supplied saves live in **`saves/`** under content-addressed names
(`<map>-<party-lead>-<md5[0:8]>.sav`), never under slot names, so a new upload
can never overwrite an archived save.  `tools/ingest-save.sh` files any save
dropped in `saves/inbox/` or at the repo root (never overwriting) and rebuilds
`saves/manifest.tsv`; see `saves/README.md`.  `bootstrap.sh` installs the
well-known fixtures into slots 1, 4 and 5 for manual play, but tests read the
archived files directly.
Slots 1 and 2 now render **differently** (distinct hashes above), each matching
its own DOSBox oracle (`ref/oracle/dungeon_slot1*.png` for the dungeon,
`ref/oracle/play.png` for the wilderness).  This was blocked for a long time by
the `REP CMPS/SCAS` `CX` bug (see the FIXED BUG section): the corrupted
`strlen` broke the save-load path, so the engine never redrew the viewport and
both slots showed the menu scene.  With the CPU fix the viewport is redrawn per
slot.  Verify play-screen work against DOSBox per-slot captures.

The **town is menu-only** (no free movement): the main screen offers
`(E)mporium (B)ank (I)nn (H)ouse of trades (T)emple (C)ontinue`.  Each building
has its own sub-menu.  Leaving town is the `(C)ontinue` path; the sub-menu drawn
after the first `C` (string `ds=0x5AA2`, x=139) is the town-exit menu, and a
second `C` on it heads out.

The **wilderness and dungeons are reached by loading a save that starts there**
(slots 1 and 2 above); walking out of town is not required.

### Save location -> map and view directory (2026-11-10)

The party's location is stored as **two digit bytes** in the save: the ones digit
at offset **0x9a2** and the tens digit at **0x9a4**.  The engine builds the map
name `DNGS\DNG%01ld%01ld` (format string at LOF.EXE file offset 0x4bb35), so 5
-> `DNGS/DNG05`, 10 -> `DNGS/DNG10`.  The first-person view tiles come from
`EGA/D<ones-digit>` (5 -> `D5`, 10 -> `D0`); `D8`/`D9` are never used.

Verified by patching the bytes and reading the emulator's open trace
(`LOF_I21=1`): GAME2's ones byte 0 -> 1 opens `DNGS\DNG11` + `ega\d1`; its tens
byte 1 -> 0 opens `DNGS\DNG00` + `ega\d0`.  A value with no map file (e.g. 7 ->
`DNG17`) makes the load fail and the engine stays in town (no `DNGS` open).

`port/char.{h,cpp}` expose `SaveGame::loc_ones`/`loc_tens`, `save_location()`,
`dungeon_map_name()` and `dungeon_view_dir()`; `lof --play auto --save <slot>`
renders the play screen for a save's own location.  Guarded by
`tests/char_test.cpp`.

**The play screen reuses the menu chrome.**  `EGA/UP/MSCR` is still blitted
first (its bottom strip at y>=196 matches the oracle byte-for-byte); the engine
then repaints the **left panel** (x6..135) with the party roster + location
label and paints the 3D **viewport** on the right (x139..304, y92..196).

Verified against the DOSBox oracle (`ref/oracle/`):

* Loading slot 1 lands in **"The Temple"** with the party list on the left and a
  *flat grey wall* (index 7) filling the viewport until the party turns/moves —
  the viewport is a depth-marched sprite blit (see the viewport section below),
  not a full-screen asset.
* The status fields read `Direction`, `Time of day`, `Spirits`, `Rations`
  (e.g. `Spirits Good`, `Rations 19`).
* **Status box values are save-derived (2026-11-20).**  The time-of-day word is
  the save's dword at `0x9b6` (loaded into global `0x3b0c`), indexing the engine
  table `ds:0x0f04` ("Midnight".."Evening"); the morale word is the save's word
  at `0x9ba` (global `0x3b10`), indexing `ds:0x0f24`
  ("Good"/"Satisfactory"/"Mediocre"/"Alarming"/"Bad").  Rations is the sum of
  every present member's `+0x70` byte.  Measured from the oracle: the
  time-of-day value is drawn white (index 15, same as the labels), while morale
  and rations are light gray (index 7).  The port matches the oracle
  pixel-exactly when the save value agrees with the oracle's live moment
  (GAME2's save says Evening/Good while the oracle shows
  Midnight/Satisfactory because the clock and morale drift after a save).
  Fields: `SaveGame::time_of_day`/`morale`, `kSaveTimeOfDay`/`kSaveMorale`.

### First-person viewport — depth-marched single-sprite blit (2026-11-17)

The viewport is **not** a per-cell composite and **not** a fixed `DP001` blit:
it is a *single pre-rendered sprite* selected by marching the column of cells
ahead of the party to the first **view-opaque** cell.  The depth selects the
sprite; **every view cell is a full 120x119 image and blits plainly at the
viewport origin (10,11)**:

| depth | sprite | screen blit |
|-------|--------|-------------|
| 1     | `EGA/D<d>/DP<index>` (`dungeon_base_view_index`) | (10,11) |
| 2     | `EGA/CHAR2/CH40` | (10,11) |
| >=3   | `EGA/CHAR2/CH25` | (10,11) |

**View-opaque** is by the tile code's **low nibble**: `0/2/7/8/9` are open (the
march continues); everything else stops it.  This is **not** the movement wall
set — code `3` is walkable but view-opaque, so the march stops on it.

Ground truth: the emulator depth sweep (`LOF_EDUMP=` + a `DNGS/DNG05` tile patch)
and, decisively, the DOSBox oracle.  The moved frame
(`ref/oracle/dungeon_slot1_moved.png`) is `CHAR2/CH25` at (10,11) matching
**1.0000**; the spawn (`ref/oracle/dungeon_slot1.png`) is `DP001` at (10,11).
The earlier "tiled at x-stride 120 / y-stride -1" reading was a symptom of the
EGA decoder's byte-11 truncation (fixed — see the decoder note below and
`docs/blockers/A.md` BLK-A005).

Implemented as `dungeon_view_opaque` + `dungeon_view_for` + `render_dungeon_play_view`
in `port/dungeon.{h,cpp}`; `--walk` and `--ui` compose the HUD + panel with no
viewport tile and then blit the depth-selected sprite.  Guarded by `tests/dungeon_test.cpp::test_view_depth` and
`tests/port_dungeon_view_test.py` (`make golden-port`).

**Still open (BLK-A005).**  The depth-2/≥3 `CHAR2/CHnn` numbers (40, 25) are the
scenario's encounter-sprite bank (`NSC05[id].+0x48`); whether they are keyed by
*depth* or coincide with the location-id lookup for the Temple save is not
proven, so the drop-in applies them for **scenario 5 only** and keeps the
depth-1 tileset sprite elsewhere.  The encounter-id *producer* (`es:[0x259]`) is
also still undecoded.

### FIXED BUG — emulator crashed on the slot-1 (dungeon) load

The analysis emulator used to die with `cs:ip=0000:0000` ~10.67M instructions
after `O`+`1`, right after the save is read.  **Root cause: the REP CMPS/SCAS
handlers in `src/cpu.cpp` forced `CX=0` after the loop instead of leaving the
*remaining* count in `CX`.**  The MS-C run-time's `strlen` is `repne scasb; not
cx; dec cx`, so it returned `0xFFFE`.  That garbage width reached the printf
width field for `sprintf(dest, "%2d%%", 50)`, which then emitted `"    50"`
(four spurious pad spaces) into the caller's 4-byte stack local at `[bp-4]`
(`3A8D:46FE`), overwriting the saved `BP` and the far-return frame; the eventual
`RETF` jumped to `0000:0000`.

Fix (commit `cpu: fix REP CMPS/SCAS ...`): decrement `CX` per iteration and stop
on the REP condition, so an early ZF exit leaves `CX != 0`.  Regression test:
`tests/cpu_test.cpp::test_string_rep`.  After the fix the slot-1 load completes
and the boot menu renders measurably closer to the DOSBox oracle (mean abs diff
29.0 -> 20.9).  Slot 2 (wilderness) was unaffected because its formatted values
happened not to overflow a buffer.

Repro (now clean):

```sh
LOF_RING=1 LOF_CRASHDBG=1 LOF_CMDLEN=3 LOF_KEYAUTO=0x18,0x02 LOF_KEYGAP=8 \
  ./lof-native --dir game --headless --frames 1500 --shot /dev/null
```

### Internal clock (game behaviour, not a bug)

The engine has an **internal clock** that advances on its own; the phase of day
(`Midnight`, `Dawn`, `Early morning`, `Morning`, `Midday`, `Afternoon`,
`Sunset`, `Evening` — strings `ds=0x2348..0x23E2`) is shown in the lower-left
box.  The town gates are closed outside "early morning", and waiting without
resting makes the party "tired and won't go on".  This is *not* the wall clock;
do not expect `LOF_HOUR` to gate the exit.

## Game data (not committed)

`downloads/` and `game/` are git-ignored (proprietary).  **Save games are the
exception:** a save the *user created* with the game is not proprietary — it is a
project asset (Rule 12, `docs/RULES.md`) and lives, committed, in `saves/` (see
`saves/README.md`).  To restore the proprietary game data:

- `downloads/lof_msdos.zip` — the **DOS** release (use this one).  It is
  byte-identical to archive.org item `msdos_Legend_of_Faerghail_1990`
  (`Legend_of_Faerghail_1990.zip`, md5 `52eedcec208a1ae4677c6abf5ce57eb0`).
- Extract to `downloads/ext_msdos/legfaer`, then copy to `game/`.
- `game/disk1.id`..`disk3.id` are dummy files created to satisfy the disk check.

Only the DOS version is used.  A German-language DOS build does not exist
(reLINE shipped the DOS release in English/international; the German release was
Amiga/Atari ST only), so the international DOS release is the correct choice.

## Reference captures

`tools/capture_dosbox.sh` runs the **original** game under DOSBox (EGA, 320x200)
in Xvfb and saves an exact 320x200 frame via DOSBox's own screenshot key, so we
can diff the port against real hardware behaviour.  Requires `dosbox`, `Xvfb`,
`xdotool` (`apt install dosbox xvfb xdotool`).

```sh
tools/capture_dosbox.sh ref/oracle/menu --wait 30                 # boot/menu
tools/capture_dosbox.sh /tmp/dung --wait 30 --keys "o 1" --post 20  # load slot 1
```

It mounts `game/` and runs **`START.BAT`**, not `LOF.EXE` directly: LOF refuses
to start on its own (it prints `Please start LOF with START.BAT` and exits)
because `VOR` must run first for the disk copy-protection check.  Reference
frames used during development live in `/tmp/native/` (ephemeral) and `ref/`;
committed oracle frames are in `ref/oracle/`.

**The oracle is the ground truth, not the emulator.**  When the port and the
emulator disagree with DOSBox, DOSBox wins.

`tools/capture_dosbox_pair.sh` captures two frames in **one** DOSBox session, so
both share a single EGA palette and can be diffed against each other directly.
Use it to compare the menu with an in-game screen:

```sh
tools/capture_dosbox_pair.sh ref/oracle   # -> ref/oracle/menu2.png + town2.png
```

### Emulator play-screen rendering (updated 2026-11-10)

The earlier claim that "the emulator does not render the real play screen" is
**obsolete** (it predated the REP CMPS/SCAS `CX` fix).  After loading a save the
emulator now reads the view tiles (`ega\d5\dp001..dp060` for slot 1,
`ega\d0\dp001..` for slot 2) and its viewport region `(x139..305, y92..196)`
matches the DOSBox oracle **pixel-exactly** for slot 1
(`ref/oracle/dungeon_slot1.png`).  Both show the same dithered flat grey wall
(colour 7) that the real game draws until the party turns or moves.

What still differs is the **left roster panel** (`x < 139`): the oracle paints
the party roster + status rows there, while the emulator's panel renders
similar structure with different colours/layout (≈48% pixel match for slot 1).
Use `ref/oracle/` as the target for the roster, not the emulator.

The two shipped saves render **distinct** frames (distinct EGA-plane hashes in
`tests/golden.txt`: `temple` slot 1 vs `play` slot 2); the difference is
confined to the left panel — both show the same flat grey viewport.

The emulator's slot→map/view mapping is correct and is the ground truth for the
save-location field: slot 1 opens `DNGS\DNG05` + `ega\d5`, slot 2 opens
`DNGS\DNG10` + `ega\d0`.

## Architecture

- `src/cpu.cpp`   — 8086 interpreter (`cpu_step`), decoder, REP prefix handling.
- `src/ega.cpp`   — EGA registers, plane memory, write modes 0/1/2, palette.
- `src/machine.cpp` — memory, MZ loader (LZEXE unpack), INT 10h/16h/21h, ports.
- `src/main.cpp`  — CLI, frame loop, PNG writer, headless/SDL2 frontends.
- `src/emu.h`     — shared types.

## Fixed bugs (do not regress)

- CMP must not write back its destination; 0x83 is 16-bit; SUB/SBB/CMP flag order.
- VOR.EXE LZEXE unpacking (was printing "Packed file is corrupt").
- Copy-protection guard (`lofx` + disk ids).
- INT 10h AH=12h EGA detection (game must select EGA mode 0Dh, not VGA).
  Covered by `test_ega_detect_int10_12()` in `make test`.
- 16-bit `out dx, ax` must drive the paired EGA data port with AH:
  - 0x3C4/0x3C5, 0x3CE/0x3CF, 0x3D4/0x3D5 → write AH to port+1.
  - 0x3C0 attribute controller: **index and data both go to 0x3C0**, so a 16-bit
    OUT writes AL (index) then AH (data) to 0x3C0.
  - The data byte must be written **even when AH is 0**: a zero data byte is a
    legitimate register value (Read Map Select 0, Map Mask 0).  Guarding the
    write with `if (AH != 0)` silently drops it and the register keeps its old
    value.  This was BLK-A002: the game's plane blitter sets Read Map Select to
    0 on its first pass, the write was dropped, so plane 0 was blitted from the
    still-selected plane 3 and **every frame's plane 0 was wrong** (planes 1–3
    perfect, plane 0 80.29% vs the oracle).  Guarded by the golden frames.
- Graphics-controller register numbers: 7 = Read Map Select, 8 = Bit Mask
  (bit mask was wrongly mapped to 7, which forced a 0x3F mask → dropped pixels).
- EGA colour: primary guns weight 2, secondary weight 1 (index 7 = light gray).
- DOS INT 21h AH=3D must return the handle in AX (not preserve AH), else reads fail.
- BIOS INT 16h must return the scan code in AH and the ASCII value in AL; games
  key off either byte.  DOS INT 21h AH=01/07/08 return the ASCII value in AL
  (extended keys return AL=0, with the scan code on the following read).
- The **scan code after an extended key** must actually be delivered: an
  extended key (arrow / PgUp / function key, i.e. any scancode whose ASCII is 0)
  returns `AL=0` on the first read and the **scan code in AL on the next read**
  (`ext_scan_pending` in `src/machine.cpp`).  Without it the arrow keys are
  invisible to LOF (it reads with `INT 21h AH=07` and keys off the scan code),
  which is why the party could not turn/step on the wilderness map.  Applied to
  AH=01/07/08 and the 0Ch dispatch; AH=0Bh reports it as pending.  The town
  entry (`O,2,Down,Up` -> town menu) is the end-to-end regression check.
- `scancode_ascii` table: 0x2C is `z` (the row is `zxcvbnm,./`, no leading `\`),
  0x29 is `` ` ``, 0x2B is `\`.  The **full** unshifted Set-1 row is now pinned by
  `test_keyboard_full_row()` in `make test`.
- `reset_cpu_state` must clear transient CPU state (`halted`, prefix/repeat
  flags), not just the registers — otherwise a second program (LOF after VOR)
  inherits the first one's halt flag.  Covered by `make test`.
- EGA image header is **11 bytes**, not 7: escape byte, `0x1234`, width (word),
  height (word), decompressed-size+10 (word), two reserved bytes; the RLE
  stream starts at offset 11.  Decoding from offset 7 shifts every image 32px
  right and garbles the font strip.  The size field equals `4*w*h + 10` for
  every file in `EGA/`.  Font cells are indexed from 0 (band 0 = `A-Z` then
  `0-9`; band 1 = `a-z`).  Some images (`IN/DRAG`, `IN/CRED`) end early and the
  rest of the planes are implicitly zero — pad before blitting.
- DOS INT 21h **AH=4Ah must actually resize** the block at ES:BX.  LOF's .EXE
  prologue shrinks its own program block to 0x4C92 paragraphs, then calls
  AH=48h; if AH=4Ah is a no-op the bump allocator hands back memory *inside*
  LOF's block (below its stack), and LOF's `rep stosw` zeroes the return address
  on the stack, so a `RETF` jumps to 0000:0000 and the game dies right after the
  SNSC load.  AH=4Ah sets `alloc_free` to the shrunk block end; AH=48h then
  returns memory above the stack, as real DOS does.  Covered by the emulator
  booting LOF to the main menu.
- **The drop-in must run with the original EXEs absent.** `lof`/`vor` read only
  the game *data* files (`EGA/`, `SYST/`, `DNGS/`, `ROST/`, `DSKTAB.DFS`,
  `*.SAV`); they must never open or execute `LOF.EXE`/`VOR.EXE`.  Verified by
  `make golden-noexe` (`tests/port_noexe_test.py`), which renders the menu from a
  game tree that contains no `.EXE` and checks it against the oracle.
- DOS 8.3 name resolution must ignore padding spaces: the engine builds the save
  name with printf `%3d` ("GAME  2"), so `Machine::resolve` drops spaces when
  matching components (`"GAME  2"` -> `GAME2`).  Covered by the `temple`/`play`
  scenarios in `tests/golden_test.sh` (both enter the play loop only if the load
  succeeds).

## Dungeon files (`DNGS/`) — decoded

`DNGS/DNG00`..`DNGxx` hold the scenario maps (a dungeon / outdoor level).  Each
file is RLE-compressed and **every shipped map decodes to the same size**, which
pins the format exactly:

```
byte 0      RLE escape byte (0xff in all shipped maps)
byte 1..    RLE stream: a literal byte is copied as-is; the escape byte
            introduces a (count, value) pair emitting `value` `count` times
```

Decoded payload = 4-byte header + three 36x36 byte layers
(`4 + 3*1296 = 3892` bytes):

| offset | field |
|---|---|
| header[0] | dungeon level / plane id (0 except DNG13 = 4) |
| header[1] | style / tileset selector (0..3) |
| header[2] | music / ambient id (0..4) |
| header[3] | spawn / start cell index (0..255) |
| layer 0 (36x36) | **wall / feature plane** — the floor plan the first-person renderer reads (0 = open) |
| layer 1 (36x36) | object plane (doors, items, stairs, decorations) |
| layer 2 (36x36) | entity / event plane |

Layer 0 is the coherent floor plan.  `DNG05` (the Temple, save slot 1) is the
clearest: a solid `0x06` border with a symmetrical room grid; `0x06` = wall,
`0x00` = open, `0x03`/`0x08` = door frames, `0x07` (`O`) = a marker.  The files
whose name ends in `0` (`DNG00`, `DNG10`) are the outdoor/wilderness levels; they
use a sparser layout with many `?` (values >9) codes.

The header-field **labels above are inferred** (the byte positions and the fixed
3892-byte size are certain; the meaning of each header byte is not yet proven by
disassembly).

**Verified:** all 29 shipped maps decode to exactly 3892 bytes and share the
header structure; `DNG13` alone has `level=4` (the deep dungeon).  The native
reader is `port/dungeon.{h,cpp}` (`load_dungeon`); `tests/dungeon_test.cpp`
checks the RLE + layer slicing on a synthetic map and the real `DNG05`/`DNG10`/
`DNG13` headers and wall counts.  `./lof --dump-dungeon` prints the floor plans.

Still open: the exact tile-id vocabulary (which wall graphic each code selects)
and the semantics of layers 1/2.
