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
