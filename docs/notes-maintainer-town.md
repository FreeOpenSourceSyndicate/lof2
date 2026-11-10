# notes-maintainer-town.md — town / play-loop oracle

CLAIM: reach the first playable in-game state (town → Continue → out of town /
dungeon) to use as the oracle for the native port's play loop.  Files touched:
`src/main.cpp`, `src/machine.cpp`, `AGENTS.md`, this file.

## What works now

* **Save file name.**  The engine builds the load name with `GAMES\GAME%3d`
  (printf `%3d`), so the file it opens is `GAMES/GAME  1` (two spaces).  The
  repo shipped `GAME1`; copy it to `GAME  1` and the load succeeds.  Confirmed:
  `--keys 0x18,0x1c` with `LOF_CMDLEN=3` reads `GAMES/GAME  1` and draws town.
  (Credit: workstream A's notes-a.md.)
* **New input hooks** in `src/main.cpp`:
  * `LOF_KEYONCE=1` — inject `--keys` once (not every 20 frames).
  * `LOF_KEYSTEP=N` — one key every N frames.
  * `LOF_KEYAT="frame:sc,frame:sc,..."` — pin each scancode to an exact frame.
    Multi-screen flows have wildly different dwell times (loading a save runs
    ~35M instructions ≈ frame 270), so fixed-step delivery overshoots.
* **Clock hook** in `src/machine.cpp`: `LOF_HOUR`/`LOF_MIN`/`LOF_SEC` override
  INT 21h AH=2Ch.

## Town layout (verified in DOSBox, DOS EN release)

The starting location is the **Inn**, with an empty party.  Menu:

```
(P)ick-pocket   (R)est   (L)ook for a new character (create)
(E)rase character (delete from pool)
(A)dd a new character (pool -> group)
(D)ismiss a character (group -> pool)
(O)pen file (load a saved game)   (C)ontinue
```

An empty group cannot leave the Inn.  The first pool character is dead; adding a
*second* (live) one lets `(C)` work.  So the inn is where `(O)pen file` lives —
the scripted path `O,1` loads the save and lands in the **town main menu**.

Town main menu (string at `ds=0x5A4A`):

```
Which building do you want to visit?
(E)mporium (B)ank (I)nn (H)ouse of trades (T)emple (C)ontinue
```

`(C)ontinue` here leaves town.  The **first** time you leave town a text offers
an **amulet**: press `1`-`6` to give it to a hero (with one hero you must press
`1`), or `(L)eave it` to decline.  This amulet prompt is why a plain "press C
twice" script never reached the map — it needs a third key.

Building sub-menus:

| building | options |
|----------|---------|
| (E)mporium | (B)uy (S)ell (R)epair (G)ather Gold (C)ontinue |
| (B)ank | (D)eposit/open (W)ithdraw/close (E)xamine accounts (J)oint deposit (C)ontinue |
| (H)ouse of Trades | (T)raining (L)earn magic spells (A)nother Language (J)oint deposit (C)ontinue |
| (T)emple | (H)eal (R)evive a dead character (J)oint deposit (C)ontinue |

## Wilderness map (after leaving town)

Arrow keys move a first-person party:

* up   = step forward
* left = turn left (no move)
* right= turn right (no move)
* down = turn 180° (no move)
* `(C)` = asks which spell to cast

Time: **stands still in town** (except when `(R)esting` at the inn), but on the
map it advances on its own even while idle.  When the party's mood reaches
"bad" you cannot move ("The companions are tired and won't go on"); resting
helps.

## Town menu strings (from LOF.EXE, ds base 0x40000)

Strings at `ds=0x5A4A`:

```
Which building do you want to visit?
(E)mporium (B)ank (I)nn (H)ouse of trades (T)emple (C)ontinue
```

The main town screen is **menu-only** — no free movement (confirmed).  The town
name is `Thyn` / `Cyldane` (`ds=0x5A56` / `0x5A5E`; `Cyldane` is referenced at
file `0x7C41`).  `(C)ontinue` at `ds=0x5AA2` is referenced at file `0x9A37`.

## Internal clock (NOT the wall clock)

The engine keeps its own time; it advances as you play.  The phase of day is
drawn in the lower-left box:

| phase | ds offset |
|-------|-----------|
| Midnight | 0x2348 |
| Dawn | 0x235E |
| Early morning | 0x2374 |
| Morning | 0x238A |
| Midday | 0x23A0 |
| Afternoon | 0x23B6 |
| Sunset | 0x23CC |
| Evening | 0x23E2 |

Related strings: "The town gates are closed and locked 'till early morning"
(file `0x4B92E`), "The companions are tired and won't go on" (file `0x4B8EC`),
"Dawn / Early morning / Morning / Midday" (`0x4237E`).  So leaving town is
gated on the in-game phase; the party also tires if you wait without resting.
`LOF_HOUR` therefore does **not** open the gates.

## Exact frame timing (measured)

* frame ~10: main menu ready (press O = 0x18).
* frame ~60: load prompt (press 1 = 0x02).  The save load runs to ~frame 270.
* frame ≥ 300: town menu is drawn (pressing C = 0x2e here draws the
  "Which building…" sub-menu; the sub-menu's own `(C)ontinue` is what leaves).

Frame hashes seen: town = `1ad8f17e`, first-C = `c3ff0fd8`, town-exit C-C =
`291922e1`.  After the first C the read list still contains only
`ROST/ROST ROST/SHOP ega\twn\twn00..05 ega\up\*` — no `DNGS`, no `EGA/Dn`,
no `SYST/SPEC`.

## Still open (the actual blocker)

Pressing `C` twice does **not** yet read a dungeon/wilderness file.  Best guesses
for the next session, in order:

1. The second `C` is being swallowed (the sub-menu may need its key later, or the
   `AH=0Ch` flush in `machine.cpp` eats it).  Try `LOF_KEYAT` with the second key
   a few frames *after* the sub-menu frame and screenshot at 20-frame intervals
   to watch for the transition.
2. The sub-menu's `(C)ontinue` may prompt (Y/N or a target), so a second key is
   not enough.
3. Rest first (the `(R)est` path) to pass time to "early morning", then Continue.
4. Debug escape hatch: the `Debugmode activ` string (`ds=0x2511`, file `0x8017`)
   and `Enter dungeon-number?` / `Enter level-number?` (`ds=0x25D3` / `0x25E9`)
   suggest a hidden debug flow; the `ds=0x2511` ref at file `0x8017` is *inside*
   a `rep movsb` loop (image `0x2B58`), i.e. it is copy data, not a direct
   print — so debug mode is toggled some other way (TBD).

## Note

`game/` and `downloads/` are git-ignored (proprietary); the `GAME  1` copy lives
only in the local `game/GAMES/` tree, not in git.

## BREAKTHROUGH (2025 session): reach the outdoor map via save slot 2

The user reported in DOSBox: **do not load slot 1** ("The file GAMESGAME1 is
not a valid game"); **loading slot 2 places the player outside, in front of the
town**.  That is the wilderness/map screen the workstream needed.

Reproduced headlessly with the new `LOF_KEYAUTO` adaptive key feed:

```sh
LOF_CMDLEN=3 LOF_KEYAUTO="0x18,0x03" LOF_KEYGAP=10 \
  ./lof-native --dir game --headless --frames 1000 --shot /tmp/o2.png
```

* `0x18` = `O` (Open file) at the inn, `0x03` = `2` (slot 2).
* The engine opens `GAMES\GAME  2` (printf `"%3d"`, two spaces), same as the
  `GAME  1` fix in `bootstrap.sh`.
* Result screen hash `eb564253` — the outdoor view (left "sign post" panel
  filled, right panel is the map viewport).  **This is NOT the town menu**
  (`1ad8f17e`) and NOT the frozen `15e4e723`.

Movement works: after `eb564253`, feeding three arrow scancodes
(`0x48` up / `0x4b` left / `0x4d` right / `0x50` down) is *consumed* by the
game (keylog shows 5 consumed reads) and changes the screen to `625cb439`
with a 5032-pixel diff in x[139..303] y[92..195] — the map viewport redraws.

### Why earlier attempts stalled

* The new-game exit sequence `a - 2 - c - c - c - l` reaches a state
  (`15e4e723`) that tight-polls `INT 21h AH=0Bh` forever after the 3rd key
  and never consumes another key via DOS or INT 16h.  Do not use that path.
* Frame-scheduled key injection (`LOF_KEYAT`) drops keys on screens whose
  load time varies (the town load runs ~6M instructions).  Use `LOF_KEYAUTO`,
  which feeds the next scancode only after the game drains the previous one.

### Emulator additions this session

* `LOF_KEYAUTO="sc,sc,..."` + `LOF_KEYGAP=N` — queue-drain-gated key feed
  (`src/main.cpp`).
* `LOF_KEYLOG=1` — logs every key the game consumes (who / scancode /
  instruction count) and every push (`src/machine.cpp`).

## Residual menu diff: 100 px, all a 1-px seam in the top-left corner

`make golden-port` reports `diff=100/64000` (left box=33, top chrome=37,
right menu=0, bottom strip=0).  Every remaining pixel is in the rectangle
`x[0..25] y[0..15]`, i.e. the top-left corner where the town image meets the
`MSCR` frame bevel.

Reading the colour indices side by side, the oracle paints the frame bevel
(`F`/`3`/`8`) along a 1-px diagonal there while the port paints the town
image's colours (`2`/`A`/`E`) — the port's town blit overruns the frame bevel by
one pixel along that seam.  It is cosmetic (0.16%), not a palette or text bug.
Not chased further: the town screen itself (a whole missing drop-in screen) is
worth more than 100 corner pixels.

## The emulator does NOT render the real play screen (2026-11-09)

The analysis emulator (`lof-native`) renders the **same viewport** for the main
menu, the dungeon (save slot 1) and the town/wilderness (save slot 2).  This was
verified byte-for-byte, not eyeballed:

* `emu_menu.left == emu_slot1.left == emu_slot2.left` — **0 differing pixels** in
  the region `x < 139` (the viewport plus its frame).
* The EGA plane dump of the play screen differs from the menu dump in only
  **3154 bytes**, every one of them in the right-hand text rows (y 92..195).
  The viewport rows (11..130) are never redrawn.  So the game updates the party
  panel text but leaves the viewport showing the menu scene.

The real game does not do this.  A DOSBox capture of the town screen
(`ref/oracle/town2.png`), taken **in the same DOSBox session** as
`ref/oracle/menu2.png` so both share one palette, has a viewport that differs
from the menu's over the whole region — a genuine outdoor scene.

### Why this matters

* `tests/golden.txt`'s `temple` and `play` hashes pin the emulator's **wrong**
  play screen.  They remain valid regression guards *for the emulator*, but they
  must **not** be used as the target for the drop-in's play screen.
* Play-screen work must be verified against a DOSBox capture
  (`ref/oracle/town2.png`), not against the emulator.

### What is still true

The game does load the save (`GAMES\GAME  2` is opened three times) and does
blit the viewport (EGA write site `119D:129F`, ~7616 writes ≈ one 120x119
viewport blit).  Why the blitted scene equals the menu's is unresolved; the
likely causes are a save-decode/file-read difference or an EGA graphics-state
difference between the emulator and DOSBox.  This is now the top
emulator-fidelity task — until it is fixed, the emulator cannot validate any
in-game screen.

## RESOLVED (2026-11-09): the "identical viewport" was a PATH-RESOLVER BUG

The finding above was an artifact of the emulator's own path resolver, not a
rendering bug.  The engine opens the save as `GAMES\GAME%3d` (printf `"%3d"`),
i.e. the name has **two padding spaces**: `GAMES\GAME  2`.  The archive ships
`GAMES/GAME2` (no spaces).  `Machine::resolve()` matched components with an
exact, case-folded comparison, so `"game  2"` never matched the host file
`GAME2`; the slot-2 `3D open` **failed silently** (no following `3F read`), the
load was a no-op, and the engine stayed on the main menu — which is exactly why
the "play" viewport equalled the menu's.

DOS and DOSBox ignore the padding spaces (a DOSBox run against a game tree with
**no** `GAME  2` alias still loads slot 2), so the fix is to normalise the name
the same way: drop spaces (and trailing dots) before comparing, in `resolve()`.

After the fix the slot-2 open is followed by
`3F read want=11834 got=11834 file=game/GAMES/GAME2`, the EGA write count at
site `119D:129F` rises from 7616 to 8316 (a second viewport blit *does* run), and
the play screen no longer equals the menu.  **This changes the `play` golden
hash**, so `tests/golden_test.sh --record` was re-run.

### Residual caveat (still a real rendering bug)

With the resolver fixed, slots 1 and 2 both enter the play loop but render
**byte-identically** (`slot1.bin == slot2.bin`), so `temple` and `play` share a
golden hash.  DOSBox renders them differently — slot 1 is the dark Temple
dungeon, slot 2 the bright wilderness (`ref/oracle/town2.png`).  So the
viewport *content* is still wrong; verify play-screen work against DOSBox
per-slot captures, not against the emulator.

### Reproduce

```
LOF_NO_TIMER=1 LOF_CMDLEN=3 LOF_KEYAUTO=0x18,0x03 LOF_KEYGAP=8 \
  ./lof-native --dir game --headless --frames 1000 --shot /tmp/play.png
# left region is identical to: ./lof-native ... --frames 60 --shot /tmp/menu.png
```

## Play screen: the drop-in renders it from assets (2026-11-09, later)

The emulator's residual (slots 1/2 byte-identical) is an engine control-flow
divergence that I did not chase further: the real deliverable is the native
**drop-in**, and it does not need the emulator to reproduce the play screen.
The drop-in composes it directly from decoded assets:

    lof --play D0/DP001     # MSCR background + viewport tile

Recovered layout (matched against a DOSBox capture of save slot 2,
`ref/oracle/play.png`):

* background = `EGA/UP/MSCR` (the same static HUD as the menu), blitted at (0,0);
* first-person viewport = `EGA/<Dn>/<DPnnn>` (120x119) blitted into the window
  at **(10,11)**.  `D0..D7` are **view directories selected by the save's
  location**, not compass facings: the party's location is a two-digit dungeon
  id (see below) and the engine reads `EGA/D<ones-digit>`.  So dungeon 05 (the
  Temple) draws from `EGA/D5` and dungeon 10 (the wilderness) from `EGA/D0`;
  `D8`/`D9` are never used.  `DP001`/`DP014`/`DP036` (+ `DP058..DP060`,
  `DP067`) are the full 120x119 views, and the smaller `DPnnn` (16x29, 32x59, …)
  are the partial views used near map edges.

### The party location field (2026-11-10)

The party's current location is stored in the save as **two separate digit
bytes**: the ones digit at file offset **0x9a2** and the tens digit at
**0x9a4**.  The engine composes the map name as `DNGS\DNG%01ld%01ld` (the
format string sits at LOF.EXE file offset 0x4bb35), i.e. 5 -> `DNGS\DNG05`,
10 -> `DNGS\DNG10`.  The view tiles come from `EGA\D<ones-digit>`.

Verified by patching the bytes and watching the emulator's open trace
(`LOF_I21=1`): setting GAME2's ones byte (0x9a2) 0 -> 1 makes it open
`DNGS\DNG11` + `ega\d1`; setting its tens byte (0x9a4) 1 -> 0 makes it open
`DNGS\DNG00` + `ega\d0`.  Patching to a value whose map is absent (e.g. 0x9a2
= 7 -> DNG17, no such file) makes the load fail and the engine stays in the
town, which is why those probes show no `DNGS` open at all.

`port/char.cpp` exposes `save_location()` / `dungeon_map_name()` /
`dungeon_view_dir()`; `lof --play auto --save <slot>` renders the play screen
for a save's own location (GAME1 -> `D5/DP001`, GAME2 -> `D0/DP001`).

`MSCR + DP001 at (10,11)` matches the DOSBox capture at **91.7%**.  The residue:

* **viewport** (~1240 px): a band near the horizon the engine overlays with the
  map's own tile data — the engine does not simply blit `DP001`;
* **party panel** (~2910 px): the six character names (`Decker`, `Siegurd`,
  `Handy`, `Merlin`, `Cassandra`, `Assanla`) in a 3x2 grid.  They are drawn in a
  compact **5x5** font (6-px advance) that is **not** in `EGA/UP/CHR1`/`CHR2`
  and is **not** an obvious byte table in `LOF.EXE`, so the drop-in omits it
  rather than render it in the wrong font (which would lower the match).

`tests/port_play_test.py` pins this (>=90% match); `make golden-port` runs it.

### Emulator parity (updated 2026-11-10)
`lof-native` **does** open the view tiles: a slot-1 (Temple) load issues
`open ega\d5\dp001`, then `dp014..dp060`, and slot 2 (wilderness) issues
`ega\d0\dp001..`.  (The earlier note that it "never issues an open for any
`ega\d0\dpnnn`" was a symptom of the REP CMPS/SCAS bug — the load aborted before
reaching the view code.  Fixed; see the "identical viewport" entry above.)  The
view directory follows the save's location: slot 1 -> `D5`, slot 2 -> `D0`.

### Correction: the party-panel font IS in CHR2 (2026-11-09)

The earlier note above ("a compact 5x5 font ... **not** in `EGA/UP/CHR1`/`CHR2`")
was **wrong**.  The font is `CHR2`'s band-0 glyphs, rendered as the **top-left
5x5** of cell `toupper(ch) - 64` with a **5-px advance**.  Recovered by
template-matching every 5x5 subwindow of every CHR1/CHR2 cell against the
oracle's name glyphs (all 18 letters matched exactly, 0 inconsistent), then
verified: rendering the six names this way reproduces the oracle panel at
**100%**.

The port now draws them: `lof --play D0/DP001 --save game/GAMES/GAME2` renders
the six names in a 3x2 grid at x = {148,208,268}, y = {9,37}, colour 15 (white),
and the play screen matches the DOSBox capture at **92.6%** (was 91.7%).
`tests/port_play_test.py` passes `--save` and pins the higher figure.

**Still open in the panel:** below each name the engine draws the character's
*profession* ("PALADIN", "MAGICIAN", "SMITH", "HEALER", …) and a stat line of
symbol glyphs.  The profession table is in `LOF.EXE` at image offset 0x41e98:
12-byte NUL-padded records `Warrior Barbarian Rogue Smith Scout Priest Druid
Magician Illusionist Paladin Healer Monk` (values 0..11); the per-character
profession byte is the save record's offset **0x1b** (Decker=9/Paladin,
Handy=3/Smith, Merlin=7/Magician, Cassandra=10/Healer, Assanla=2/Rogue;
Siegurd=12, one past the end — likely `Scout` with a 0-based-vs-1-based skew).
The stat line uses CHR2 band-1 symbol cells and is not yet decoded.

### Party-panel professions (2026-11-09, later)

The line under each name is the character's **profession**, drawn in the *same*
CHR2 5x5 face but in **black (index 0)**; the names are white (index 15).  The
profession byte is the save record's **+0x1b**, indexing the engine's own
12-entry table (`Warrior Barbarian Rogue Smith Scout Priest Druid Magician
Illusionist Paladin Healer Monk`) — already present in the port as
`profession_names()` (`port/char_names.inc`, recovered from LOF.EXE 0x41e98).
Verified: Decker=9/Paladin, Handy=3/Smith, Merlin=7/Magician,
Cassandra=10/Healer, Assanla=2/Rogue all render exactly.  Play screen now
**93.3%** (was 92.6%).

Two remaining panel residues, both documented not guessed:
- **Siegurd's profession** (byte 12) is one past the table, so the engine prints
  two CHR2 band-1 symbol glyphs there; leaving it blank is the closest match.
  Likely the byte is really *Scout* with a 0-vs-1 skew in the save format.
- The **stat-number row** (y21-25 / y49-53, colour 8 dark gray) is 3 glyphs per
  character; the first decodes to a digit (`2`,`6`,`2`,`5`,`1`,`3`) but the
  glyphs are *not* plain CHR2 digit cells — a different (probably arithmetic)
  encoding.  Not yet decoded; drawing it wrong lowers the match.

### Party-panel stat row (2026-11-09)

The third panel row per character is the **status**: a dark-gray (index 8)
number on the left and a **white (index 15) status word** on the right.  For all
six party members the word is exactly `GOOD` — `status_short_names()[0]`,
rendered in the same CHR2 5x5 face and verified 100% pixel-for-pixel against the
oracle at (169,21),(229,21),(289,21),(169,49),(229,49),(289,49).

The leading dark number is *not* a plain decimal: the glyphs are CHR2 band-0/1
cells (`0x32`=`` ` ``, `0x0f`=`O`/`[`, `0x25`=band-1 `e`), so the row is an
encoded stat rather than `itoa()`.  Neither `level46` nor `stat1d` maps to it,
and no single save byte is constant across the six while the words are.  Left
unrendered (drawing it wrong lowers the match); the status word is the
documented next step if the engine's stat formatter is recovered.

## RESOLVED (2026-11-09): slots 1/2 now render differently

The residual caveat above ("slots 1 and 2 render byte-identically") is **fixed**.
Root cause was *not* a viewport-rendering bug but the emulator's `REP CMPS/SCAS`
handlers forcing `CX=0`; the run-time's `strlen` (`repne scasb; not cx; dec cx`)
then returned `0xFFFE`, which broke the save-load formatting path (and crashed
slot 1 outright).  With `CX` left as the remaining count:

* slot 1 (`temple`) -> `a3f719edd43bc692dc230539fdf34705`
* slot 2 (`play`)   -> `585da631a63e2199db7425c5b422ee52`

They are now distinct, and each matches its own DOSBox oracle best:

| emulator frame | closest oracle | mean abs diff |
|----------------|----------------|---------------|
| slot 1 | `dungeon_slot1_moved.png` | 24.6 |
| slot 2 | `play.png` | 26.5 |

The viewport region (x139..304, y92..196) is still a flat wall for both
(oracle dungeon_slot1 is also flat at 170, but dungeon_slot1_moved shows 3
colours after the party turns) — that is the separate, still-open "viewport is
drawn from tiles, not a full-screen asset" task.
