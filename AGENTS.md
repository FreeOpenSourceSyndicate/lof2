# AGENTS.md — Legend of Faerghail native drop-in

> **New here? Read `docs/HANDOFF.md` first, then `docs/RISKS.md`, then
> `docs/WORKSTREAMS.md`.** They cover how to survive a workspace reset, how to
> avoid the poisoned sibling repo, and which files you own. This file is the
> deep technical reference. When you get blocked, record it in the shared
> blocker registry `docs/BLOCKERS.md`.

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
- **Do NOT look at the sibling repo `FreeOpenSourceSyndicate/lof`.** It is
  unreliable and its approach is a known dead end; treat it as poisoned.

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
(compose the in-game play screen, see below), `--ui` (run the interactive
front-end, see below) and `--shot-frame N` (which frame of the intro sequence
`--shot` captures; default = last).

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
oracle for the drop-in's play loop and for workstream B's remaining stat/combat
semantics. Captured deterministically as the `town` scenario in
`tests/golden_test.sh` (`--keys 0x18,0x1c`).

Menu text is drawn at x=139, rows at y=124+8*n (colour 0). `port/font.{h,cpp}` loads the
strip at run time (no font data is embedded) and draws text; `tests/font_test.cpp`
covers the mapping, the advance and the draw path.

**Known font-rendering gap (open).** The menu/town/panel text in the emulator
oracle does *not* byte-match the `CHR1`/`CHR2` strip cells: sliding a rendered
`(P)ick-pocket` / `Which building do you want to visit?` / `Welcome, hardy`
against the oracle frame leaves a residual of a few hundred pixels, and the
oracle glyph for the greeting's first letter is a *bold, double-struck* form
(each stem two pixels wide) with no exact match anywhere in `CHR1`, `CHR2`, the
decoded EGA planes, `MENU`, `MSCR`, `LOF.EXE` or `VOR.EXE`. The oracle's panel
background is the correct colour (the custom-palette fix below) and the *strings
and layout* are confirmed byte-for-byte from `LOF.EXE` (`Welcome, hardy` /
`adventurers!` / `(P)ick-pocket` … / `(C)ontinue` at x=139, rows y=92,100,116,
124,140,148,156,164,172,188), but the glyph *bitmaps* the engine blits are still
unidentified — the engine appears to build them from `EGA\UP\CHR1`/`CHR2` at run
time with an extra step (weighting/duplication) the drop-in does not yet model.
`port/lof.cpp` draws them with the `CHR1` cells (colour 0) as the current best
approximation. Next step: disassemble the engine's glyph-blit routine (the code
that reads the `4130:` buffer `CHR1` is loaded into) to recover the transform.

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
| `LOF_DISASM=seg:off:len` | hex-dump `len` bytes of the loaded image at `seg:off` |
| `LOF_ALLOC=1`        | log each INT 21h AH=48 allocation (want, free paragraph) |

## Town / play-loop flow (oracle for the drop-in)

The save filename the engine opens is `GAMES\GAME%3d` (printf `%3d`), i.e.
`GAMES/GAME  1` with two padding spaces.  `Machine::resolve()` strips spaces
(and trailing dots) before matching a host file, because DOS ignores them, so
the shipped `GAME1`..`GAME5` are found directly — no renamed copies are needed.
(This was a real bug: an exact-match resolver left `GAME  2` unresolved, the
load silently no-op'd, and the play screen stayed on the menu.)

The play loop is entered from the main menu by `O` (Open) then the **digit key**
of the slot (`1`..`6`, scancodes `0x02`..`0x07`); `Enter` on the dialog does
nothing.  The load is long and its timing varies, so headless scripts must feed
the keys adaptively (`LOF_KEYAUTO="0x18,0x02" LOF_KEYGAP=8`), not by frame.

Two shipped saves reach the play loop and are both guarded by
`tests/golden_test.sh`:

| slot | scenario | planes md5 (60/1500 frames) | screen |
|------|----------|------------------------------|--------|
| 1    | `temple` | `a3f719edd43bc692dc230539fdf34705` | the **Temple dungeon**, dark first-person viewport |
| 2    | `play`   | `585da631a63e2199db7425c5b422ee52` | the wilderness |

A `GAME1` dropped at the repo root is a *working* save (not committed; it is
proprietary game data).  `bootstrap.sh` installs it as slot 1 when present.
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
  the viewport is drawn from tiles, not a full-screen asset.
* The status fields read `Direction`, `Time of day`, `Spirits`, `Rations`
  (e.g. `Spirits Good`, `Rations 19`).

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

`downloads/` and `game/` are git-ignored (proprietary).  To restore:

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
- 16-bit `out dx, ax` must drive the paired EGA data port with AH:
  - 0x3C4/0x3C5, 0x3CE/0x3CF, 0x3D4/0x3D5 → write AH to port+1.
  - 0x3C0 attribute controller: **index and data both go to 0x3C0**, so a 16-bit
    OUT writes AL (index) then AH (data) to 0x3C0.
- Graphics-controller register numbers: 7 = Read Map Select, 8 = Bit Mask
  (bit mask was wrongly mapped to 7, which forced a 0x3F mask → dropped pixels).
- EGA colour: primary guns weight 2, secondary weight 1 (index 7 = light gray).
- DOS INT 21h AH=3D must return the handle in AX (not preserve AH), else reads fail.
- BIOS INT 16h must return the scan code in AH and the ASCII value in AL; games
  key off either byte.  DOS INT 21h AH=01/07/08 return the ASCII value in AL
  (extended keys return AL=0, with the scan code on the following read).
- `scancode_ascii` table: 0x2C is `z` (the row is `zxcvbnm,./`, no leading `\`),
  0x29 is `` ` ``, 0x2B is `\`.  Covered by `make test`.
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
