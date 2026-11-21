# PLAN — prioritized task list, estimates, and project completion

Status report for the owner. Refreshed at the start of each session (T-3).
The authoritative *technical* queue is `docs/HANDOFF.md` §4; open guesses are in
`docs/GUESSWORK.md`; rules are in `docs/RULES.md`. This file answers three
questions: **what is left, in what order, how long, and how done are we?**

Last refreshed: 2026-11-08 (session of agent lof2/M-5, sole active agent).

---

## 1. Overall project completion — ~75 %

Weighted by player-visible subsystem, not by line count. The drop-in already
boots, shows the intro, reaches the menu, walks a town, visits every building,
walks the Temple dungeon in first person, meets wandering monsters and resolves
a combat round. What is missing is concentrated in three places: the *outdoor
(dungeon-tileset) first-person viewport*, *level transitions / object
interactions*, and *saving*.

| Subsystem | Done | Weight | Notes |
|---|---|---|---|
| Analysis emulator (the oracle) | 100 % | 12 | stable; reproduces every oracle frame used below |
| VOR intro + credits | 100 % | 4 | `EGA/IN/DRAG`, `EGA/IN/CRED` |
| Boot → menu | 100 % | 6 | `EGA/UP/MSCR` + `MENU`, font |
| Town (Thyn, Cyldane) + buildings | 100 % | 10 | inn, shop/emporium, bank, house, temple (+ heal/revive/rest prompts) |
| DNGS decode (all 29 levels) | 100 % | 5 | tiles + object layer + header |
| **Dungeon first-person view** | **85 %** | 14 | Temple (`D5`) depth march is pixel-exact; the **outdoor (`D0`) composite is now pixel-exact** (A-1 done); remaining: the outdoor map-object layer (a pole/tree sprite outside the `D0` bank) and non-`DP001` outdoor bases |
| **Object/feature semantics** | **25 %** | 8 | door-like signature proven; door/stairs/trap/item dispatch not decoded |
| **Level transitions (stairs)** | **0 %** | 4 | no stairs/exit handling anywhere |
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
| **A-1** | ✅ **DONE** — **Outdoor viewport rendering (`D0` bank)**. The outdoor view is a **composite**, not a sprite ladder: base `D0/DP001` at (0,0) plus three horizon-band pieces `DP012`@(0,49), `DP004`@(15,46), `DP005`@(90,50), each blitted with an index-0 transparent mask. Reproduces the 120×119 window of `ref/oracle/play.png` **pixel-exactly (14280/14280)** and the same set is measured at facing E, so the pieces are the fixed horizon band of the outdoor base, not facing-selected scenery. The rule is keyed on the base resolving to `DP001` in the `D0` bank (a tile-geometry predicate was tested and contradicts the measured topology, so it is not used). Shipped through one helper, `dungeon_view_overlays()`, used by the play loop, `--walk` and `--play D0/DPnnn`; `tests/port_play_test.py` now requires the viewport to be pixel-exact. Open measurement: a full outdoor cell sweep to confirm the pieces are cell-independent (notes-a.md Round 25). | **done** | `ref/oracle/play.png`; emulator facing sweep at DNG10 cell 745 | `port/dungeon.{h,cpp}`, `port/gfx.{h,cpp}`, `port/screens.{h,cpp}`, `tests/port_play_test.py` |
| **A-2** | **Stairs / level transition** — nothing moves the party between `DNGS` levels. Find the tile/object code that descends, decode the destination rule, implement. | **6–10 h** | emulator: walk onto a stair cell, trace which `DNGnn` it opens (`LOF_I21=1`); the `DNG` header `entry_pos` supplies the landing cell | `port/dungeon.{h,cpp}`, `port/lof.cpp` |
| **A-3** | **Object-code semantics** (doors, stairs, traps, items) — the object layer's high byte is only proven "door-like" by geometry. Decode the dispatcher's per-code behaviour. | **10–16 h** | the event dispatcher at image `0x1F31E` (already partly disassembled in `docs/notes-a.md` Rounds 16); SPEC record `+0xe2` behaviour type | `port/dungeon.{h,cpp}`, `tools/dngs.py` |
| **A-4** | **G-007 residue** — depth-2/≥3 bank sprite numbers are scenario-5 values. Capture a second scenario's depth-2/≥3 view and read its `CHnn`; table per scenario if it differs. | **3–5 h** | emulator depth sweep on a non-Temple save | `port/dungeon.cpp`, `docs/GUESSWORK.md` G-007 |
| **A-5** | **G-005 mover half** — the wall *set* `{6,13}` is decompiled at the renderer but the step-rejection test is still oracle-derived. Isolate the collision routine. | **4–6 h** | `analysis/lof.asm` move path; the four trail-shift sites are excluded already | `port/dungeon.cpp`, `docs/GUESSWORK.md` G-005 |

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
2. ~~**A-1** (outdoor viewport)~~ — **done**, viewport pixel-exact (notes-a.md
   Round 25).
3. **A-2** (stairs) — without it the dungeons are single-room demos.
4. **M-1** (save writer) — without it nothing a player does persists.
5. Then A-3, B-1, A-4/A-5, M-2, M-3, B-2, B-3.

Total remaining effort: **~77–125 agent-hours** (A-1's 8–14 h removed).

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
- Two items are **not** removable by derivation and are tracked as such:
  the RNG-seeded monster placement (GUESSWORK G-007, closed as
  not-portable-by-data) and the descriptive item-effect labels (G-004).
- No guess is introduced by this plan. Every task above names a ground truth.
