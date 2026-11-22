# PLAN — prioritized task list, estimates, and project completion

Status report for the owner, and the **single work queue** — one agent works
this top to bottom. Refreshed at the start of each session (T-3). Open guesses
are in `docs/GUESSWORK.md`; rules are in `docs/RULES.md`; the deeper technical
history is in `docs/HANDOFF.md` §4. This file answers three questions: **what is
left, in what order, how long, and how done are we?**

Last refreshed: 2026-11-21 (session of agent lof2/M-4, sole active agent).
This revision **integrates** the queue written by lof2/M-5 (its P-1..P-10 are
kept, renumbered by priority, and the mapping is stated in §2) and adds the
items that list was missing: town exit / wilderness travel, the town state
effects (inn rest, temple heal/revive, shop and bank transactions) and the
`P`/`B` combat flags.  The combat model also gained the derived defender rows
and the `W` column since that revision (see §1, combat row).

---

## 1. Overall project completion — ~87 %

Weighted by player-visible subsystem, not by line count.  The table below sums
to **86.8 %** (weights total 100); it is rounded to ~87 %.  The previous
revision said ~86 % (86.4 %), and ~82 % before the save writer landed.  The
arithmetic is stated so the figure can be checked: the only change since 86.4 is
the combat row, 75 % → 78 %, because the defender rows and the `W` column are now
**derived** rather than measured (`0x27A1E`, `0x15852`), which is
0.03 * 12 = 0.36 points.

The drop-in already boots, shows the intro, reaches the menu, walks a town, visits every building,
walks the Temple dungeon in first person, meets wandering monsters, resolves
a combat round (with monsters' own defence values and a real weapon-break rule)
and writes saves back.  What is missing is concentrated in three places: the
*outdoor scenery rule* (the view is exact for one cell and approximate
elsewhere), *object interactions* beyond stairs (doors, traps, pickups), and
*the monster's turn* (the attack path and the quick-combat loop).

| Subsystem | Done | Weight | Notes |
|---|---|---|---|
| Analysis emulator (the oracle) | 100 % | 12 | stable; reproduces every oracle frame used below |
| VOR intro + credits | 100 % | 4 | `EGA/IN/DRAG`, `EGA/IN/CRED` |
| Boot → menu | 100 % | 6 | `EGA/UP/MSCR` + `MENU`, font |
| Town (Thyn, Cyldane) + buildings | 100 % | 10 | inn, shop/emporium, bank, house, temple (+ heal/revive/rest prompts). **Menus only** — the transactions and state effects are P-7 |
| DNGS decode (all 29 levels) | 100 % | 5 | tiles + object layer + header |
| Dungeon first-person view | 80 % | 14 | Temple (`D5`) depth march pixel-exact. The outdoor (`D0`) composite is exact **for one cell only**; the scenery piece set is overfit (**G-013**) — P-9. The outdoor **wall** case is derived and tested (`DP036` at the map edge, `DP001` otherwise); the oracle cell is 0 px wrong |
| Object/feature semantics | 40 % | 8 | selector, trigger, type dispatch, text path, once-only gate and the `+0x0e` answer line are live; the answer-input path, the recruit stat block and the per-type *effects* are P-2 |
| Level transitions (stairs) | 85 % | 4 | stairs/portals/level changes work and are decompiled (`0x2e7fa`); open: non-stair object codes |
| Encounter screen | 100 % | 5 | `SYST/NSCnn` record → `CHAR0/CHnn` |
| **Combat (decision → round → result)** | **78 %** | 12 | melee round derived (`0x14D80` fallback), hit score, hand-slot weapon pick, `rand(damage)+1`, XP multiplier, defender rows (`0x27A1E`) and the `W` break check (`0x15852`) all derived. Open: the monster attack path `0x159F8`, the quick-combat loop `0x17BAA`, spells, `P`/`B` |
| Character data (SPEC/ROST/SNSC) | 90 % | 6 | parse + checksum + save write done; char-sheet *screen* not pixel-matched (P-8) |
| Items | 70 % | 6 | table parsed, equip deltas derived; use-effects partial (P-6) |
| Save **read** | 100 % | 4 | archive fixtures, checksum, content addressing |
| Save **write** | 100 % | 4 | `build_save`/`write_save_file`; read→write byte-identical on every intact archive save; `--save-out` persists the party; tampered saves detected and flagged |

---

## 2. Prioritized task queue (single agent — no lanes)

**There is one agent and one queue.** The old `A-*` / `B-*` / `M-*` lane prefixes
are retired. Task IDs are `P-n`, assigned purely by priority; the mapping from
lof2/M-5's numbering is given per row so its history stays readable.

Priority is **player-visible impact**: can the player move through the world
(doors, the town exit), can they fight (monsters hitting back, spells), can they
keep a session (state effects), then fidelity and polish.  Estimates are
**agent-hours** for one competent agent who must re-derive the ground truth
first.

| # | Task | Est. | Why this rank | Ground truth / next step |
|---|------|------|---------------|--------------------------|
| ~~**P-1**~~ | ~~**Save writer.**~~ **DONE** | — | Persistence — the largest single capability of the last revision. | `build_save()`/`write_save_file()`; read→write **byte-identical** on every intact archive save; `lof --walk --save-out` persists. `port/char.{h,cpp}`, `tests/char_test.cpp`, `tests/port_walk_test.py` |
| **P-2** | **Object-code semantics** — the recruit stat block, `[0x7d1c]`, the per-type effects (old P-2) | 1–4 | **High.** Doors and pickups are how a dungeon is explored. **Answer-input path DONE 2026-11-21:** `fire_cell_event()` now classifies the reply (`classify_answer`), prints the outcome line `+0x0e` per the `+0x4b` rule, and fires the effect **only** on a match; `lof --walk --answer TEXT` drives it, pinned by `tests/answer_path_test.py` (no answer → prompt only; `N` → line, no effect; `Y` → line + effect). | The type-7 door writes `0x37fa`/`0x460e`/`0x3824`/`0x3b15` (`0x20AD7`..`0x20B13`); the recruit stat block is the SNSC copy at `0x20F02`/`0x1D99A`. `port/dungeon.{h,cpp}`, `port/lof.cpp` |
| **P-3** | **Monster attack path + quick-combat loop** (`0x159F8`, `0x17BAA`) (old P-3) | 8–14 | **High.** Combat where the enemy never strikes is not combat. | **Entry mapped** (`docs/notes-b.md`): marker-1 gate, `+0x37 == 0x0c` HP cap, `+0x106`/`+0x108` → `0x137DE`, `+0x10e` → `0x15DF8`, `6*attacker[+0x05]` into `[0xafca]:0x3d48`, `[0xafc2]:0x3de9`; all combat segment globals alias arena `0x468E` and are static data. **The per-round row rebuild is DECODED (2026-11-21)** — image `0x27B3A`..`0x27B65`, found with `LOF_WATCH` on a live round: `+0x06 = -(10*(4 - rec[+0x29]))` (Mercenary `0xEC`), `+0x04 = 100*HP/(8*rec[+0x2c])`, `+0x0a` = the HP word from `[0xb082]:0x52 -> +0x9e`.  `defender_row()` implements both.  So the rows are done.  The path itself is now partially transcribed (`docs/notes-b.md` §3a): the dispatcher `0x137E6` sends every non-party actor to `0x159F8`; the *live* route is `0x15DF8` -> `0x15E1F` (the message variant at `0x15B1E` is not taken, because the monster instance's `+0x10e` is 0); gates `char[+0x1f] != 7`, `row[+0x16] == 0xff`, `inst[+0x106]|+0x108 == 0`; then the `+0x2b` range code and the `0x212f:0x9342` target call.  **Next:** resolve `row[+0x16]` (it lies outside the 12-byte row as captured — settle whether the row is longer or `[bp+6]` differs), transcribe `0x15E1F`..`0x15Fxx` and the damage application, then the quick loop `0x17BAA`. `port/combat.{h,cpp}` |
| **P-4** | **Town exit / wilderness travel** — *new in this revision* | 12–24 | **High.** The wilderness is currently reachable only by loading a save that starts there. The town `(C)ontinue` path is the normal way out. | The town-exit sub-menu (`ds:0x5AA2`, x=139) after the first `C`, then the second `C` heads out; the overworld map, its movement and its encounter rolls. `port/lof.cpp`, `port/screens.*` |
| **P-5** | **Spells in combat** (old P-4) | 12–20 | **High**, but only once P-3's loop exists to cast in. | The spell table + the cast path in the combat dispatcher; `Cast Spell` is `+0x0f == 4`. `port/combat.{h,cpp}` |
| **P-6** | **Item use effects** (old P-5) | 8–14 | **Medium-high.** Potions/scrolls are how a party survives. | Dispatch `0x0E89D`/`0x0EDB0` located; wire the proven effect values (G-004). `port/char.{h,cpp}` |
| **P-7** | **Town state effects** — inn rest, temple heal/revive, shop and bank transactions — *new in this revision* | 10–18 | **Medium-high.** The screens exist but do nothing, so gold and HP cannot change; that makes the town a museum rather than a place to prepare. | The heal/rest/transaction routines behind `--inn-rest`, `--temple-heal`, `--temple-revive`, `--emporium`, `--bank`; `write_save_file()` already exists for the result. `port/lof.cpp`, `port/char.*` |
| **P-8** | **Character-sheet screen** (old P-8) | 6–10 | **Medium-low.** Seen often, no mechanics depend on it. | `ref/oracle/charsheet.png` (recipe in `docs/notes-maintainer-town.md`); palette lacks the oracle's red/yellow. `port/lof.cpp` |
| **P-9** | **Outdoor scenery-selection rule** (old P-6, G-013 residue) | 6–10 | **Medium.** Fidelity, not progression. | Whether the horizon band is per-cell; cells 636/673 sit at ~0.94 with no object; the 70-entry `ega\d0\dpnnn` table at image `0x361E1`. `port/dungeon.cpp` |
| **P-10** | **Mover / collision test** (old P-7, G-005 residue) | 4–6 | **Medium.** Movement works; edge cases only. | The `analysis/lof.asm` move path (the four trail-shift sites are already excluded). `port/dungeon.cpp` |
| **P-11** | **`P` / `B` round flags** — *new in this revision* | 2–4 | **Low, and blocked on P-3.** Two of the three round-result columns are always blank. **`P` guard derived 2026-11-21** (`0x1756D`..`0x1759D`): defender dexterity `+0x46 > 11`, a shield in slot `[bp-0x30]` (armour `[bp-0x3e]` at `0x1777A`), then `sub slot[+0x85], 2` and `P = 1`; message gate `[0xafc2]:0x3de9 == 0`. Remaining: **it is blocked on P-3, not on the slot scan.**  The `P` event runs in the *defender* path, and the only defender that has inventory is a party member — i.e. it needs a monster attack to land.  Until P-3 exists nothing in the port can reach it, so the slot scan (`weapon_slots()` covers only the two weapon hands; the shield/armour slots would mirror `defense_from_items`' equipped-class scan, item class 3 = shield, 2 = armour) is the second half of the work, not the first. `B` is untouched (`0x12846`/`0x13D7D`/`0x1471C`). Flag array `[0xafdc]+0x5608`: `W` done, `P` `+0x5609`, `B` likely `+0x560A`. `port/combat.*`, `port/char.*` |
| **P-12** | **Depth-2/≥3 bank sprite numbers** (old P-9) | 3–5 | **Low.** Non-Temple deep-dungeon views only. | Emulator depth sweep on a non-Temple save. `port/dungeon.cpp` |
| **P-13** | **All-out-of-action message** (old P-10) | 2–4 | **Low.** Cosmetic. | Trigger decoded (`[bp-0x36]`, `0x18449`); blocked on the message colour — needs a mid-encounter oracle frame. `port/lof.cpp` |

### Done (kept for context; no action)

| Was | Task | Result |
|-----|------|--------|
| A-1 | Outdoor viewport composite (`D0`) | ✅ Exact on the oracle cell (0 px wrong); residue is P-9 |
| A-2 | Stairs / level transitions | ✅ Destination rule, portal index (`location%10`), stairs wording (`0x2e7fa`) |
| M-4 | Party position from the save, not the `DNG` header | ✅ `0x9a0` (cell) / `0x9a6` (facing) |
| M-4 | Combat: hit score, hand slots, damage roll, XP multiplier | ✅ `0x27F14` rows, `0x13A43` pick, `rand(damage)+1`, `xp_per_damage()` |
| M-4 | Combat: defender rows + the `W` column | ✅ `0x27A1E` and `0x15852`; `--fight` prints `WPB=ddd` |
| M-4 | `AH=4Ah` resize, LZEXE unpack, EGA 11-byte header, INT 16h scan/ascii | ✅ See `AGENTS.md` "Fixed bugs (do not regress)" |

---

## 3. Order of work

The queue in §2 **is** the order; work it top to bottom.  Two ordering facts
worth stating:

* **P-4 (town exit) is ranked above P-5 (spells)** even though spells are
  bigger: leaving town is progression, casting is a tactic inside a fight the
  player can already have.
* **P-9 (outdoor scenery) is ranked low** despite being the oldest open item —
  it is fidelity, and the view is already recognisable.

### Remaining-effort total (checkable)

| task | low h | high h |
|------|-------|--------|
| P-2 object codes (stat block, per-type effects) | 1 | 4 |
| P-3 monster attack + combat loop | 8 | 14 |
| P-4 town exit / wilderness travel | 12 | 24 |
| P-5 spells in combat | 12 | 20 |
| P-6 item use effects | 8 | 14 |
| P-7 town state effects (rest/heal/shop/bank) | 10 | 18 |
| P-8 character sheet | 6 | 10 |
| P-9 outdoor scenery rule | 6 | 10 |
| P-10 mover/collision | 4 | 6 |
| P-11 `P`/`B` flags | 2 | 4 |
| P-12 depth-2+ sprites | 3 | 5 |
| P-13 all-out message | 2 | 4 |
| **total** | **74** | **133** |

So **~74–133 agent-hours** remain.  The upper bound is 40 h above lof2/M-5's
estimate because P-4 (town exit) and P-7 (town state effects) were missing from
that list, not because any finished work regressed.

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
- **A correction to this plan's own history:** the outdoor composite was recorded
  as "pixel-exact" on the strength of two captures that turned out to be the
  *same* frame (facing does not change the view at that cell), and the score was
  0.9926, not 1.0.  Two lessons, both in §4: a second capture is only evidence if
  its bytes actually differ, and a score below 1.0 must be *explained pixel by
  pixel* before it is called a residual.
- No guess is introduced by this plan. Every task above names a ground truth.
