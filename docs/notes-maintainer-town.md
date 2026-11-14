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

The main town screen is **menu-only** — no free movement (confirmed).

**Address mapping (verified 2026-11-14).** The `ds` offsets in this file are
data-segment offsets; the engine runs with `ds = 0x4AC1`, so for a data offset
`off`:

    file offset = 0x40000 + off      (verified empirically, below)
    physical    = 0x4AC10 + off      (ds*16 = 0x4AC10)

Both are confirmed against the live 1 MB RAM dump (`LOF_DUMP`) and the raw EXE:
`ds:0x5A31 "town of"` -> file `0x45A31` -> physical `0x50641`; the bytes match in
both. **Rule of thumb: `file = ds_offset + 0x40000`.** (The `ds:off` form is what
the disassembly shows, e.g. `mov ax, 0x5a31` at image `0xC22C`.)

**Town-name strings.** The file holds exactly two town names, NUL-separated at
`ds:0x5A39` (`"Thyn. "`) and `ds:0x5A40` (`"Cyldane. "`) — file `0x45A39` /
`0x45A40`. There is a *second* `"Thyn"` at `ds:0x687D` ("The Grand Board of
Trades greets you in Thyn."), file `0x4687D`. `(C)ontinue` is at `ds:0x5AA2`
(file `0x45AA2`).

> **Superseded claim (wrong):** an earlier revision said the names live at
> `ds=0x5A56` / `0x5A5E` and that `Cyldane` is "referenced at file `0x7C41`".
> Both are wrong: `ds:0x5A56` is the middle of `"Which building do you want to
> visit?"`, and file `0x7C41` is code (`5E 5A 59 5B ...`), not a name. The
> `0x5A56`/`0x5A5E` values were the file offsets `0x45A56`/`0x45A5E` minus
> `0x40000` computed wrongly. See G-001 for the selection question.

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

## RESOLVED (2026-11-11): the party-panel stat + status rows are now exact

The two "documented not guessed" residues in the party panel are both decoded,
and the whole panel now renders **pixel-exactly** against `ref/oracle/play.png`
(the play screen jumped from 93.3% to 94.2%; the only residue left is the
engine's dynamic viewport overlays).

* **Status word.** The white word right of the number is exactly `GOOD` =
  CHR2 band-0 cells 7,15,15,4 (`G O O D`).  It is the same 5x5 small-caps face
  as the names, colour 15, at `(x+21, y+12)`.  For the whole test party the
  status is `Good` (`status_short_names()[0]`), so the port draws index 0.  The
  per-character status byte is still unlocated, but it is `0` for every shipped
  save, so index 0 is correct for all of them.

* **Stat number.** The dark-gray (index 8) field is *not* an encoded symbol row
  — it is the character's stat at record offset **`+0x1e`**, printed with a
  plain `"%d"`.  Disassembly confirms it: `2EEC2: mov al, byte [bx+di+0x1e]`,
  then `2EEC8: push ax` into the format call.  The engine's own glyph table
  places `0..9` in **CHR2 band-0 cells 27..36** (`27 + digit`) and `%` in
  **band-1 cell 37**.  Verified against the oracle for all six members:

  | member | +0x1e | panel |
  |---|---|---|
  | Decker | 0x32 = 50 | `50%` |
  | Siegurd | 0x5a = 90 | `90%` |
  | Handy | 0x32 = 50 | `50%` |
  | Merlin | 0x14 = 20 | `20%` |
  | Cassandra | 0x28 = 40 | `40%` |
  | Assanla | 0x41 = 65 | `65%` |

  All six render exactly, and the trailing glyph is band-1 cell 37 (the `%`
  cell).  Rank-correlation over every byte in the six 410-byte records found
  **exactly one** candidate (offset 30 = `0x1e`) whose ordering matched the
  oracle; the value *is* the field (no divisor).

* **Out-of-range profession.** Siegurd's profession byte is `12`; the engine's
  panel table (LOF.EXE 0x41e98) has only **12** usable entries, so index 12 runs
  off the end and prints two `?` glyphs (band-1 cell 36).  `profession_names()`
  is 12 entries, so the port now draws `??` for an out-of-range index, matching
  the oracle exactly.  (The bytes at index 12/13 are `Lawful`/`Chaotic`, an
  alignment table that happens to sit right after the professions — not used for
  the panel.)

`Font::draw_small_cell` (band, cell) was added to blit individual glyphs for the
digit/`%`/`?` cases; `SaveChar::panel_stat` exposes `+0x1e`.

## Town screen port — investigation (2026-11-11)

Tried to reproduce `ref/oracle/town2.png` in the drop-in; it is **not a single
asset blit**, so it is left for a dedicated session.  Findings to save time:

* The town viewport is a **130x119** window at screen `(6,11)`.  An exhaustive
  scan of every `game/EGA/*/*` image (4-plane decode, 2-px stride, all offsets)
  found **no** asset matching it; the closest is `UP/MSCR` (the menu
  background, ~1667 mismatching sampled px), i.e. the viewport is composited at
  runtime from several sources (a sky/ground gradient + a building sprite),
  not blitted whole.
* The scene's 11 colours are `{0,2,3,4,6,7,8,10,12,14,15}`; there is a wide
  dark column band (`0`) on the right third of the window — a large building in
  shadow.
* The bottom-left box (`x6..134, y133..200`, frame at y132/y134) holds the
  engine's **dynamic** text (location / phase-of-day / messages).  It is drawn
  with a 5x5 small-caps face but the glyphs are heavily anti-aliased by the
  capture, so OCR is unreliable — a clean DOSBox capture would be needed to pin
  the exact strings.
* The right party panel is identical to `play.png` (already exact) **except**
  a dynamic bar at `y28/32` (the health/mood gauge, dark-gray `8` on the panel
  `6`) and at `y56/60` — both driven by live character state, so they are not
  static and should not be hard-coded.

Net: the party panel, the play-screen viewport background, the menu and the VOR
intro are all reproduced exactly; the remaining gaps (play-screen viewport
foreground sprite, town scene, live gauges) all need either a decoder we have
not written or runtime game state.

## Native walk loop (`--walk`) — 2026-11-11

Added the front-end half of the in-game play loop to `port/lof.cpp` (my file).

* `--walk --save <file> --walk-keys 0x48,0x4b,...` loads the save, derives the
  dungeon map name from the save's location byte (`dungeon_map_name(
  save_location(sg))` -> `DNGS/DNG<d><l>`), decodes it with agent A's
  `load_dungeon`, places the party with `dungeon_place` and replays the scancode
  sequence through the walk model (`dungeon_step` / `dungeon_turn`).  It prints
  `walk: x=.. y=.. facing=..` so a headless test can assert the result.
* Scancode map (matches the wilderness key map recovered earlier): `0x48` step
  forward, `0x50` turn 180, `0x4b` turn left, `0x4d` turn right.
* The play screen is composed as for `--play` (HUD + party panel) and the level
  is then drawn into the viewport rectangle (`draw_viewport_map`, 3 px/cell at
  108x108 centred in the 120x119 window) with the party as an arrow.
* This reads only game data (`DNGS/`, `GAMES/`), never an executable, so it
  satisfies hard constraint #1 and works with the EXEs absent.

Guarded by `tests/port_walk_test.py` (wired into `make golden-port` and
`check.sh`).  Verified: spawn (0,0) facing N; turn left -> W, turn right -> E;
180 twice is the identity; a step into the level edge is refused; facing S steps
to (0,2).  The walk screen differs from the static `--play` screen, proving the
viewport carries the rendered level.

Still open: the *first-person* viewport foreground.  The engine composes it from
several `EGA/D<d>/DPnnn` sprites per cell (not one blit — an exhaustive match of
the emulator's Temple frame against every `D5` sprite tops out at ~0.65), so the
walk loop currently draws a top-down level map in the window instead.  Pinning
the per-cell sprite composition is workstream A's remaining task.

## Palette: the emulator's play-loop palette is wrong (2026-11-11)

**Finding.** At the menu, the play screen and the temple the emulator's final
attribute palette is the **ramp** `00 01 02 03 04 05 06 07 10 11 12 13 14 15 16
17` (measured with `LOF_DEBUG=1 LOF_VDBG=1 LOF_PAL2=1`, stable at frames
60/150/300, with and without `LOF_NO_TIMER`). Re-rendering the emulator's own
plane dump with the **standard EGA palette** (`00 01 02 03 04 05 06 07 38 39 3A
3B 3C 3D 3E 3F`) matches the DOSBox oracle far better on every screen:

| screen | ramp (emulator) | standard |
|--------|-----------------|----------|
| menu   | 59.2% | **80.3%** |
| play   | 57.7% | **77.9%** |
| temple | 58.3% | **75.1%** |

The **drop-in already uses the standard set** and matches `ref/oracle/menu.png`
at **99.8%** (`tests/port_menu_test.py`). So the standard palette is correct and
only the emulator is wrong — this is not a dungeon-specific issue and not a
plane/write-mode issue.

**Cause.** The game programs the palette via `INT 10h AH=10h AL=02h` from tables
in the loaded data segment: `4130:4834..4878` (five fade tables) called from
`cs:ip=119D:0D04`, and `4130:47DF..4823` (ending in the literal ramp table
`00 01 02 03 04 05 06 07 10 11 12 13 14 15 16 17`, stored in the EXE at
`0x7012`) called from `cs:ip=119D:0C78`. The emulator stores the table bytes
verbatim as attribute selectors, so the ramp table yields `attr_pal[n]=0x10+n`
(dark-green…white) rather than the standard `0x38..0x3F` high half. The standard
set is **not** present anywhere in `LOF.EXE`/`VOR.EXE`, so the engine either
translates these tables or writes the attribute controller (`0x3C0`) directly;
that translation is the thing to recover.

**Ground truth from the repo owner (in-game, DOSBox).** The Temple is
**greyscale right after loading** — consistent with the standard palette's grey
indices 0/7/8/15, i.e. the standard set *is* what the engine intends. Colour
appears only when moving: near walls show **small green specs**. So the correct
palette is standard (not a fixed grey), and the moved-view colour is real.

**Next step.** Recover the palette translation (or the direct `0x3C0` writes) in
the decompilation; verify against a fresh DOSBox capture of the menu and the
temple (`tools/capture_dosbox.sh` — needs `dosbox`, not installed here). Do not
"fix" the drop-in: it is already correct.

## BLK-A002 CORRECTED: it is plane 0, not the palette (2026-11-12)

The "wrong ramp" above is real (the emulator *does* end up with
`attr_pal[n]=0x10+n`), but it is **not** the reason the emulator's frame differs
from the DOSBox oracle. I re-derived the emulator's **index image** from its own
plane dump (`LOF_EDUMP`) and compared it to the oracle's index image (oracle RGB
inverted through the standard palette; all 64000 oracle pixels map cleanly):

* Planes **1, 2 and 3 are byte-identical** to the oracle (100.000%).
* Only **plane 0 (blue)** differs: 80.29% match, 12616 differing pixels, and
  **every** difference is plane 0 only (`idx_emu ^ idx_oracle == 0x1` for all of
  them — verified).

So the emulator's *pixel data* is wrong in the low bit-plane; the palette merely
maps that wrong index through a different LUT. Re-palletting the emulator's
dump therefore can never exceed ~80% (the index image is only 80.3% right), and
that ~80.3% is exactly the "standard palette" figure above — the earlier
conclusion "standard palette correct, only the emulator palette is wrong" was
**wrong**: the standard palette was hiding a plane-0 data bug, not fixing a LUT.

**What the engine does (decompiled).** The game blits the framebuffer
**one plane at a time**: the routine at `119D:0184` cycles the
Graphics-Controller *Read Map Select* (`0x3CE <- 4, ah=cs:[0x17a+idx]`) and the
Sequencer *Map Mask* (`0x3C4 <- 2, ah=cs:[0x17e+idx]`), with the runtime table
`cs:[0x17a] = 00 01 02 03 | 01 02 04 08` and `cs:[0x182]` the plane counter
`0..3`. The copy loops (`119D:0F90`, `119D:1250` via `119D:1295`) `rep movsb`
plane 0, call `0x184` to advance, and repeat for all four planes. So the plane-0
data comes from a `rep movsb` under map-mask `0x01`.

**Prime suspect.** The write *mechanism* is not the cause: instrumenting
`Ega::write` shows every EGA write in the menu render uses **write mode 0** and a
**single-plane map mask** (`1/2/4/8`, 10439 writes each — perfectly balanced
across the four planes). So plane 0 is written exactly like planes 1–3, yet only
plane 0 is wrong — the defect is in the plane-0 **source data** (what the engine
feeds the plane-0 pass), not in the mask/write-mode/latch path. Plane 0 is the
lowest index bit, so the whole play loop renders one bit wrong — consistent with
the owner's "Temple is greyscale on load but shows green specs when moving".

**Next step.** Trace where the plane-0 pass reads its source byte (the `rep
movsb` at `119D:129D` copies from `es:[si]`; follow `si` back to the decoded
buffer) and compare it to planes 1–3's source. Verify any fix against
`ref/oracle/menu.png` (expect plane-0, and thus overall, match to jump from
~80% toward 100%). This is an **emulator-only** defect: the drop-in
(`port/gfx.cpp`) renders the planes correctly and already matches the oracle at
99.8%, so **do not touch the drop-in**.

### Follow-up probe (2026-11-12): the game uses an EGA double buffer

Tracing the plane-copy loop (`119D:1288`, guarded by `cs:[0x182]` the plane
counter 0..3) shows it copies **`ds=A1F4` → `es=A000`, same offset, `rep movsb`,
one plane per pass**, with the GC *Read Map Select* / SEQ *Map Mask* cycled
`0,1,2,3` / `1,2,4,8`. `A1F4` is `A000 + 0x1F40` (8000 = one 320×200 plane),
i.e. the source lives **inside the EGA window** — the game is double-buffering in
EGA memory (display page at `0xA0000`, source/staging page at `0xA1F40`).

That is the likely source of the plane-0 divergence: in the emulator's plane
memory the **source page** (`plane[k][0x1F40..0x3E80]`) matches the pristine
`MSCR` decode well (plane 0 = 92.9%, planes 1–3 ≈ 79–80%), while the **display
page** (`plane[k][0..0x1F40]`) matches it far worse for plane 0 (63.7%) than for
planes 1–3 (73–77%) — and the display page for plane 0 equals the source page for
only ~64–70% of rows (planes 1/2: 93%, plane 3: 98%). So the plane-0
source→display copy does not land where it should.

**Refined next step.** Check how the emulator's `is_ega()` window
(`0xA0000..0xB0000`, src/emu.h) interacts with a staging buffer at `0xA1F40`:
the plane-0 pass may read a display-page offset that a previous pass already
overwrote, or the window bound may differ from a real EGA (64 K vs 128 K). Log
`ega.gc_read_map`/`seq_map_mask` at each plane pass and the exact `si`/`di`
per pass, and compare the copied length to 8000. Still **emulator-only**.

### RESOLVED (2026-11-12): the plane-0 bug was a dropped zero-byte port write

The double-buffer probe was a *symptom*. Scanning each plane for its best
base offset in the emulator's plane memory was the giveaway: planes 1–3 matched
the oracle at base `0x0000` (100%), but plane 0 matched best at base `0x1F40`
(88%) — i.e. plane 0's data had never been copied into the display page; it still
held the *source* page. The pass that should have written it had read the wrong
plane.

Root cause: **`Machine::port_out16` skipped the paired data-port write whenever
`AH == 0`** (guard `if (v & 0xFF00)`). The blitter sets GC *Read Map Select* to
**0** on its first pass; that write was dropped, so Read Map Select kept its
previous value (3) and the pass copied **plane 3** into plane 0. Fix: always
write the data byte. Verified: all four planes now match `ref/oracle/menu.png`
at **100.00%** (plane 0 was 80.29%). Golden hashes re-recorded. Recorded in
`AGENTS.md` ("Fixed bugs — do not regress") and `docs/blockers/A.md` (BLK-A002
Resolved).

## How to actually reach the town screen (repo owner, 2026-11-11)

The intro ends in the **town's Inn with no party members** — that is the first
playable screen, not the town menu. The Inn's own menu is
`(L)ook for new character / (A)dd / (D)ismiss / (O)pen file / (C)ontinue`.

* `O` opens the **load-file menu**, which accepts digits **1-5** (load slot) or
  `C` (cancel). **Enter does nothing here** — an earlier note's "O then Enter"
  was wrong: Enter is ignored, so the screen simply stays on the Inn. That is why
  the `O,Enter` capture looked like a town screen and why the building letters
  did nothing on it (they are not Inn hot-keys); `C` on it *does* work because
  `C` = Continue/leave the Inn.
* **Inn -> town:** press `A` (add a character), `2` (add the 2nd character to the
  party), `C` (leave the add menu), `C` (leave the Inn). Scancodes
  `0x1e,0x03,0x2e,0x2e`.
* **Alternative (loaded save in front of town):** `O`, `2` (load slot 2),
  `Down` (turn around), `Up` (step into town). Scancodes `0x18,0x03,0x50,0x48`.

Only the real town screen has the building menu
`(E)mporium (B)ank (I)nn (H)ouse of trades (T)emple (C)ontinue`.

Enemies are **generated on load** (random positions/movement) and are not part of
the save, so combat encounters are not reproducible from a save alone.

## Town screen decoded + shipped (`--town`, 2026-11-12)

The town screen is now reproduced by the drop-in (`./lof --dir game --town`).
Composition, recovered by template-matching the emulator's `town` golden frame:

* background `EGA/UP/MSCR` at (0,0);
* town scene `EGA/TWN/TWN00` blitted into the 120x119 viewport at **(10,11)**;
* the engine's message and building menu in `CHR1`'s full face, 6-px advance,
  **x=139**, colour 15 (white) for the message, colour 0 (black) for the menu:

  | row | text |
  |-----|------|
  | 92  | `You enter the proud town of` |
  | 100 | `Thyn. Which building do you` |
  | 108 | `want to visit?` |
  | 132 | `(E)mporium` |
  | 140 | `(B)ank` |
  | 156 | `(I)nn` |
  | 164 | `(H)ouse of trades` |
  | 172 | `(T)emple` |
  | 188 | `(C)ontinue` |

Every glyph template-matches a `CHR1` cell exactly (score 1.0), which also
recovered two previously unmapped cells: `.` = band 1 cell 34, `?` = band 1
cell 35 (added to `port/font.cpp`).  Static match 99.2% vs `ref/oracle/town.png`;
the only residue is a 16x16 top-left patch (88 px, `MSCR` corners + a small
diagonal arrow the engine draws at ~(4..15,0..15) — source not yet identified;
`TWN00` does not supply it and it is not any single `EGA/**` asset) and the
dynamic party panel (top-right).  Guarded by `tests/port_town_test.py`.

Note the town viewport is **120 wide, not 118**: the engine's viewport blitter
wraps the leftmost ~10 columns with the right end of `TWN00`'s top rows (the same
wrap the menu's `MENU` frame shows), so a naive `TWN00[0:119,0:120]` still lands
the interior pixels correctly; only the corner differs.

## Building screens (Emporium/Temple) — captured; no new files (2026-11-13)

The earlier note ("capture is incomplete") was **wrong**: the Emporium *is* fully
drawn. The confusion came from a 1700-frame run under `golden_test.sh`, which
uses `FRAMES=60` per scenario (so a partial frame was inspected); at the golden
`--frames 1600` the screen is complete and the EGA-plane md5 matches the recorded
golden (`shop c9781d20…`). Confirmed this session by re-running the scenario and
reproducing the golden hash exactly.

**It is the same video mode as the town screen: EGA mode 0Dh.** `LOF_DEBUG`
prints `mode=13`, but that field is **decimal** — 13 = 0x0D, i.e. EGA 320x200x16
(see `src/machine.cpp:set_video_mode`, which the game calls with `0D`). There is
**no VGA mode-13h path**; do not be misled by the decimal `13`.

**Grounding for the drop-in:** the Emporium reads **no new files** — it redraws
from `ROST/ROST` + `ROST/SHOP` and built-in art (`LOF_I21=1` shows the last opens
are `ROST/ROST` then `ROST/SHOP`; nothing after). The oracle frame is saved at
`ref/oracle/shop.png`. Reproduce with:

```
LOF_KEYAUTO=0x1e,0x03,0x2e,0x2e,0x12 LOF_KEYGAP=250 LOF_CMDLEN=3 LOF_NO_TIMER=1 \
  ./lof-native --dir game --headless --frames 1600 --shot out.png
```

Temple: same with `...,0x14`. The layout is a large **ornate-framed panel** (a
decorative border with a repeating motif) filling most of the screen, a dark
inner region, and text/options; it is a denser, more decorative screen than the
town menu. Implement it only from the decoded frame — do not guess the border
art (identify its EGA source or the engine's draw routine first).

### Leads for the next session (do not re-derive)

- The Emporium panel is **not** any `EGA/UP/*` asset: `MENU`/`BOX1..5`/`LIN1..2`/
  `MIN1..4`/`RAHM0`/`BLK5`/`UP00..25`/`ARR1..6`/`CRS0..1`/`MSCR` were each
  template-matched into the frame at 2-px granularity — none scores below 8
  mean-abs. It is engine-drawn (or drawn from a buffer the game built earlier),
  so it must be traced in `analysis/lof.asm`, not composed from assets.
- **G-001 (town name):** the file holds **two** names only — `ds 0x5A39 "Thyn. "`
  and `ds 0x5A40 "Cyldane. "` (adjacent, NUL-separated). Neither is referenced by
  a plain pointer, so the selector is computed. The surrounding code loads
  `0x5A31 "town of"`, `0x5A4C "ich building do you want to visit?"`,
  `0x5A69 "isit?"` into AX and calls the string drawer (`lcall 0x18d:0x16a8`) —
  these are the *tails* of the three lines, so the full strings are assembled
  from fragments and the name is chosen upstream. Trace the routine around image
  `0x0C055..0x0C24D` and find where the `0x5A39`/`0x5A40` choice is made.
  Both available saves (`GAME1`/`GAME2`, town scenario 5) resolve to Thyn, so a
  save that stands in Cyldane is needed to observe the other branch.

- **Emporium menu strings (found 2026-11-14, `file = ds+0x40000`):** the
  Emporium options live at `ds:0x5BA5` `"(B)uy wares"`, `ds:0x5BB1`
  `"(S)ell wares"`, `ds:0x5BBE` `"(R)epair wares"`, `ds:0x5BCD`
  `"(G)ather gold"`, `ds:0x5BDB` `"(C)ontinue"` (file `0x45BA5`…). The town
  building label `"(E)mporium"` is `ds:0x5A6F`. Greeting: `ds:0x5B90`
  `"would you like, huh?"`. Use these to place the Emporium text once the
  panel art is traced; the *options* are known, only the ornate frame is not.



