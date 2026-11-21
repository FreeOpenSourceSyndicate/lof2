# PLAN — prioritized task list, estimates, and project completion

Status report for the owner. Refreshed at the start of each session (T-3).
The authoritative *technical* queue is `docs/HANDOFF.md` §4; open guesses are in
`docs/GUESSWORK.md`; rules are in `docs/RULES.md`. This file answers three
questions: **what is left, in what order, how long, and how done are we?**

Last refreshed: 2026-11-08 (session of agent lof2/M-5, sole active agent).

---

## 1. Overall project completion — ~82 %

Weighted by player-visible subsystem, not by line count.  The table below sums
to **82.4 %** (weights total 100); it is rounded to ~82 % because the two largest
open items — outdoor scenery and the save writer — are the ones most likely to
hide extra work.  The previous revision said ~75 % while its own table summed to
78.5 %; the arithmetic is now stated so the figure can be checked.

The drop-in already boots, shows the intro, reaches the menu, walks a town, visits every building,
walks the Temple dungeon in first person, meets wandering monsters and resolves
a combat round.  What is missing is concentrated in three places: the *outdoor
scenery rule* (the view is exact for one cell and approximate elsewhere), the
*object interactions* beyond stairs (doors, traps, pickups), and *saving* --
there is still no save writer at all.

| Subsystem | Done | Weight | Notes |
|---|---|---|---|
| Analysis emulator (the oracle) | 100 % | 12 | stable; reproduces every oracle frame used below |
| VOR intro + credits | 100 % | 4 | `EGA/IN/DRAG`, `EGA/IN/CRED` |
| Boot → menu | 100 % | 6 | `EGA/UP/MSCR` + `MENU`, font |
| Town (Thyn, Cyldane) + buildings | 100 % | 10 | inn, shop/emporium, bank, house, temple (+ heal/revive/rest prompts) |
| DNGS decode (all 29 levels) | 100 % | 5 | tiles + object layer + header |
| **Dungeon first-person view** | **80 %** | 14 | Temple (`D5`) depth march is pixel-exact. The outdoor (`D0`) composite is pixel-exact **for one cell only**; a sealed-patch sweep shows the scenery piece set is overfit (**G-013**), so *scored against the game* this is not 85 % — it is exact on the oracle and approximate elsewhere. The outdoor **wall** case is derived and tested (`DP036` at the map edge, `DP001` otherwise). The oracle cell is now **0 px wrong** — the last 106 px were a missing object draw (`EGA/UP/UP22`), not a wrong base. Remaining: whether the horizon band is per-cell, the doorway/opaque-fill view, and the `CHAR2` room cases (A-6) |
| **Object/feature semantics** | **40 %** | 8 | stairs/portals decoded (A-2); door-like signature proven; door/trap/item dispatch not decoded |
| **Level transitions (stairs)** | **85 %** | 4 | stairs/portals/level changes work and are decompiled (A-2 done, image `0x2e7fa`); open: the non-stair object codes (doors/traps) |
| Encounter screen | 100 % | 5 | `SYST/NSCnn` record → `CHAR0/CHnn` |
| Combat (decision → round → result) | 75 % | 12 | melee round derived; spells, quick-combat loop, monster attack path open |
| Character data (SPEC/ROST/SNSC) | 90 % | 6 | parse + checksum done; char-sheet *screen* not pixel-matched |
| Items | 70 % | 6 | table parsed, equip deltas derived; use-effects partial |
| Save **read** | 100 % | 4 | archive fixtures, checksum, content addressing |
| **Save write** | **0 %** | 4 | no writer exists at all |

---

## 2. Prioritized task list (Area A first — this agent's workstream)

Estimates are **agent-hours** for one competent agent who must re-derive the
ground truth first. "Ground truth" says how correctness will be proven, because
Rule 1 forbids guessing.

### Area A — dungeons (my workstream; do these first)

| # | Task | Est. | Ground truth / next step | Files |
|---|------|------|--------------------------|-------|
| **A-1** | ✅ **DONE, with a caveat** — **Outdoor viewport rendering (`D0` bank)**. The outdoor view is a **composite**: base `D0/DP001` plus horizon-band pieces `DP012`@(0,49), `DP004`@(15,46), `DP005`@(90,50), blitted with an index-0 transparent mask (the mask is load-bearing: an opaque paste scores 0.9225 instead of 0.9926). Reproduces the 120×119 window of `ref/oracle/play.png` **pixel-exactly**, shipped through `dungeon_view_overlays()` for the play loop, `--walk` and `--play D0/DPnnn`; `tests/port_play_test.py` requires the viewport to be pixel-exact. **The earlier 'facing E confirms it' claim is retracted** — facing does not change the frame at cell 745 at all. A re-sealed save sweep (notes-a Round 50) shows the piece set is **overfit to the one oracle cell**: cells 636/673 are closer to bare DP001, and cell 492 draws a different mid-band. Filed as **G-013**; the measured set is kept so the oracle stays exact. **UPDATE:** the "overfit" residual was diagnosed — the composite was 106 px wrong on the oracle cell, and those 106 px are the **map-object sprite** `EGA/UP/UP22` at (70,53), which the port was not drawing. With it the window is **pixel-exact, 0 px wrong** (`tests/port_play_test.py` asserts the viewport matches). Ablation confirms all three band pieces are necessary. **Remaining work: whether the band is per-cell** (A-6). | **done (one cell)** | `ref/oracle/play.png`; sealed-patch cell sweep | `port/dungeon.{h,cpp}`, `tests/port_play_test.py` |
| **A-2** | ✅ **DONE** — **Stairs / level transition.** `walk_key()` moves the party between `DNGS` levels; the destination rule, the portal-table index (`location%10`, not the dungeon group) and the stairs/hole wording (`[0x37fe] == 7`) are all decompiled (image `0x2e7fa`). `tests/dungeon_test.cpp` asserts both portals land on a **walkable** cell. | **done** | stair handler `0x2e7fa`; notes-a Round 52 | `port/dungeon.{h,cpp}`, `port/lof.cpp` |
| **A-3** | **Object-code semantics** (doors, traps, items) — the object layer's high byte is only proven "door-like" by geometry. Stairs/portals are now decoded (A-2); the remaining codes (doors, traps, item pickups) are not. | **8–12 h** | event dispatcher at image `0x1F31E`; SPEC record `+0xe2` behaviour type | `port/dungeon.{h,cpp}`, `tools/dngs.py` |
| **A-4** | **G-013 residue** — depth-2/≥3 bank sprite numbers are scenario-5 values. Capture a second scenario's depth-2/≥3 view and read its `CHnn`; table per scenario if it differs. | **3–5 h** | emulator depth sweep on a non-Temple save | `port/dungeon.cpp`, `docs/GUESSWORK.md` G-013 |
| **A-5** | **G-005 mover half** — the wall *set* `{6,13}` is decompiled at the renderer but the step-rejection test is still oracle-derived. Isolate the collision routine. | **4–6 h** | `analysis/lof.asm` move path; the four trail-shift sites are excluded already | `port/dungeon.cpp`, `docs/GUESSWORK.md` G-005 |
| **A-6** | **Outdoor scenery-selection rule** (the residue of A-1 / **G-013**) — decode what picks the horizon-band scenery for an outdoor cell, so the view stops being exact for one cell only. The ahead-cell tile code does **not** separate the known cells (745/636/673 all have an open ahead cell), so it is the object/feature record or a scenario table. | **6–10 h** | **Partly answered in Round 50b**: the wall nibble now splits into `D0/DP036` (wall ends the map, 0.9648 on 3 cells) vs `D0/DP001` (wall with depth, 0.9499) and is implemented + tested. Remaining: the doorway (`0x0a`) composite (cell 95 matches `D0/DP002` only 0.5569) and the open-cell `CHAR2` room cases. The 70-entry `ega\d0\dpnnn` path table at image `0x361E1` is the next thing to read | `port/dungeon.cpp`, `docs/GUESSWORK.md` G-013 |

### Area M — engine/front-end (shared; mine too as sole agent)

| # | Task | Est. | Ground truth / next step | Files |
|---|------|------|--------------------------|-------|
| M-1 | **Save writer** — no writer exists. Serialise the party + world state back to `GAMES/GAMEn` so play persists. | 8–12 h | round-trip a shipped save: read → write → byte-identical (and ROST checksum recomputed) | `port/char.{h,cpp}`, `port/lof.cpp`, `tests/saves_test.py` |
| M-2 | **Character-sheet screen** — oracle mismatch (17 837/64 000 px; palette lacks the oracle's red/yellow). | 6–10 h | `ref/oracle/charsheet.png` (recipe in `docs/notes-maintainer-town.md`) | `port/lof.cpp`, `port/char.*` |
| M-3 | **All-out-of-action message** (HANDOFF item 11) — trigger decoded (`[bp-0x36]`, image `0x18449`), blocked on the message colour. | 2–4 h | needs a mid-encounter oracle frame | `port/lof.cpp` |
| M-4 | ✅ **DONE** — **Party position comes from the save, not the `DNG` header.** `load_save()` now reads `0x9a0` (cell) and `0x9a6` (facing); `dungeon_place_cell()` places the party there and the DNG header is only the fallback when the word is absent. Four of the five archived saves store a cell that differs from their level header, so the header was the wrong source. | done | **Emulator-verified:** patching `0x9a0` 125→455 in `GAMES/GAME  1` moves the emulator viewport 1.0000→0.0380 vs `dungeon_slot1.png`. Port vs emulator on the Gonk save: viewport 0.2415 → 0.5088, whole screen 0.6230 → 0.6826; all five saves now spawn at their stored cell/facing. | `port/char.{h,cpp}`, `port/dungeon.{h,cpp}`, `port/lof.cpp`, `tests/port_walk_test.py` |

### Area B — characters, items, combat

| # | Task | Est. | Ground truth / next step | Files |
|---|------|------|--------------------------|-------|
| B-1 | **Item use effects** — the use-routine dispatch (`0x0E89D`/`0x0EDB0`) is located; wire the proven effect values into the port. | 8–14 h | those sites + the effect values already proven in G-004 | `port/char.{h,cpp}` |
| B-2 | **Spells in combat** — cast-spell action is unimplemented in `resolve_round()`. | 12–20 h | spell table + the cast path in the combat dispatcher | `port/combat.{h,cpp}` |
| B-3 | **Quick-combat loop** (`0x17BAA`) + **monster attack path** (`0x159F8`) | 8–14 h | those routines (partly disassembled) | `port/combat.*` |

---

## 3. Recommended order

1. ~~**M-4** (save position)~~ — **done**, viewport comparisons are now honest.
2. ~~**A-2** (stairs)~~ — **done** (notes-a Round 52).
3. **A-6** (outdoor scenery rule) — the only thing standing between "exact on one
   cell" and "exact outdoors".  Highest value per hour in Area A now.  **Round 51
   already removed half of it** (the object draw); what is left is whether the
   horizon band varies per cell.
4. **M-1** (save writer) — without it nothing a player does persists.
5. Then A-3, B-1, A-4/A-5, M-2, M-3, B-2, B-3.

### Remaining-effort total (checkable)

| task | low h | high h |
|------|-------|--------|
| A-3 object codes | 8 | 12 |
| A-4 G-013 second scenario | 3 | 5 |
| A-5 mover/collision | 4 | 6 |
| A-6 outdoor scenery rule | 6 | 10 |
| M-1 save writer | 8 | 12 |
| M-2 character sheet | 6 | 10 |
| M-3 all-out message | 2 | 4 |
| B-1 item use effects | 8 | 14 |
| B-2 spells in combat | 12 | 20 |
| B-3 quick-combat + monster attack | 8 | 14 |
| **total** | **65** | **107** |

So **~65–107 agent-hours** remain (previously stated as 75–123; that figure was
not the sum of the rows, this one is).  Add ~30 % if the work must be re-derived
by an agent with no prior context on this codebase.  A-1's main body is done, but
its residue (A-6) is real work that did not exist before the sweep — the sweep
converted an assumed-done item into a measured partial one, and Round 51 then
split it again (the object draw is now done; the per-cell band question is not).

---

## 4. Traps to avoid (learned the hard way)

- **The loaded save file is `GAMES/GAME  1` (8.3-padded with two spaces), not
  `GAMES/GAME1`.** Both exist. `bootstrap.sh` installs the archive saves to the
  *unpadded* names, but the engine opens the padded ones, so patching `GAME1`
  silently changes nothing and three "different" runs come out byte-identical.
  When testing a save, patch the padded file and confirm the md5 changed.
- **A null result may be a wrong-file result.** Before concluding "this field
  does not drive the view", check the output md5s actually differ between runs.
- **`DNGS` files are RLE-compressed**: patching a byte at a fixed offset corrupts
  the stream. Decode, edit, re-encode (or write an all-literal stream).
- Compare the *viewport* window (11:130, 10:130) as well as the whole screen;
  the party/status panel dominates a whole-screen score and hides view bugs.

---

## 5. Honest caveats

- The completion percentages are estimates of *player-visible* coverage, not a
  proof of correctness; only the subsystems marked 100 % are oracle-verified.
- Two items are **not** removable by derivation as things stand and are
  tracked as such: the RNG-seeded monster placement (GUESSWORK G-007, closed as
  not-portable-by-data) and the descriptive item-effect labels (G-004).
  The outdoor scenery set (G-013) was on this list and has been **removed from
  it**: Round 51 showed it was not a fitted constant at all but a missing object
  draw, and the oracle window is now exact.
- **A correction to this plan's own history:** A-1 was recorded as done on the
  strength of two captures that turned out to be the *same* frame (facing does
  not change the view at that cell).  The lesson is in §4: a second capture is
  only evidence if its bytes actually differ.
- No guess is introduced by this plan. Every task above names a ground truth.
