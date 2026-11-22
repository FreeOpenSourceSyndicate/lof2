# PLAN — prioritized task list, estimates, and project completion

Status report for the owner, and the **single work queue** — one agent works
this top to bottom. Refreshed at the start of each session (T-3). Open guesses
are in `docs/GUESSWORK.md`; rules are in `docs/RULES.md`; the deeper technical
history is in `docs/HANDOFF.md` §4. This file answers three questions: **what is
left, in what order, how long, and how done are we?**

Last refreshed: 2026-11-08 (session of agent lof2/M-5, sole active agent).

---

## 1. Overall project completion — ~86 %

Weighted by player-visible subsystem, not by line count.  The table below sums
to **86.4 %** (weights total 100); it is rounded to ~86 %.  The previous revision
said ~75 % while its own table summed to 78.5 %, then ~82 % at 82.4 %; the
arithmetic is stated so the figure can be checked (and was re-checked: 86.4 is
82.4 + the 4-point save writer, which went 0 → 100 %).  That is the largest
single capability added since the project began: the drop-in can now hold a
session.

The drop-in already boots, shows the intro, reaches the menu, walks a town, visits every building,
walks the Temple dungeon in first person, meets wandering monsters and resolves
a combat round.  What is missing is concentrated in three places: the *outdoor
scenery rule* (the view is exact for one cell and approximate elsewhere), the
*object interactions* beyond stairs (doors, traps, pickups).  Saving is no
longer one of them: saves are written, sealed and reloaded.

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
| **Save write** | **100 %** | 4 | `build_save`/`write_save_file`; read→write byte-identical on every intact archive save; `--save-out` persists the party; tampered saves detected and flagged |

---

## 2. Prioritized task queue (single agent — no lanes)

**There is one agent and one queue.** The old `A-*` / `B-*` / `M-*` lane prefixes
are retired: they only recorded which parallel agent happened to pick an item up,
and that history is in git. Everything below is owned by the maintainer. Task IDs
are now `P-n`, assigned purely by priority.

Priority is **player-visible impact**, in this order: can the player finish a
session and keep it (persistence), can they move through the world (doors,
collisions), can they fight (monsters hitting back), then fidelity and polish.

Estimates are **agent-hours** for one competent agent who must re-derive the
ground truth first. "Ground truth" names how correctness will be proven, because
`docs/RULES.md` forbids guessing.

| # | Task | Est. | Why this rank | Ground truth / next step |
|---|------|------|---------------|--------------------------|
| ~~**P-1**~~ | ~~**Save writer.**~~ **DONE.** | — | `build_save()`/`write_save_file()` emit a sealed 13134-byte save; `lof --walk --save-out` persists the party. Also fixed the reader's 32-slot bug (world state was being parsed as a 7th character, doubling the rations readout 19→38). | Proven: read→write is **byte-identical** on every intact archived save; turn persists and resumes; tests mutation-checked. `port/char.{h,cpp}`, `port/lof.cpp`, `tests/char_test.cpp`, `tests/port_walk_test.py` |
| **P-2** | **Object-code semantics** (doors, traps, item pickups). **Selector solved** (2026-11-08): the object layer *does* choose SPEC events — a per-level table of distinct object words from **tile-3 cells only**, in row-major order (capped 70), and the word's *index* in that table is the SPEC record number (dispatcher `0x1F31E`: build `0x1F0C0`, lookup `0x1F46E`, `index*0xee` at `0x1F520`). Implemented as `Dungeon::event_keys()`/`event_index()`, pinned by `test_event_keys` (mutation-checked). **Remaining:** wire the dispatch into the play loop (trigger on entering an event cell) and map each SPEC type to its effect. | 4–6 h left | **High.** Dungeons cannot be explored without doors; this blocks progression, not just fidelity. | Rule proven; remaining work is the trigger + per-type effects. `port/dungeon.{h,cpp}`, `port/char.*`, `port/lof.cpp` |
| **P-3** | **Monster attack path + quick-combat loop** (`0x159F8`, `0x17BAA`). Melee rounds resolve, but monsters do not act and there is no combat loop. | 8–14 h | **High.** Combat where the enemy never strikes is not combat; this is the core of the game. | Those routines (partly disassembled). `port/combat.{h,cpp}` |
| **P-4** | **Spells in combat.** The cast-spell action is unimplemented in `resolve_round()`. | 12–20 h | **High**, but depends on P-3's loop existing. | Spell table + the cast path in the combat dispatcher. `port/combat.{h,cpp}` |
| **P-5** | **Item use effects.** Dispatch (`0x0E89D`/`0x0EDB0`) located; wire the proven effect values in. | 8–14 h | **Medium-high.** Potions/scrolls are how a party survives; effects are partially derived already. | Those sites + the effect values already proven in G-004. `port/char.{h,cpp}` |
| **P-6** | **Outdoor scenery-selection rule** (G-013 residue). Outdoor view is exact on the oracle cell, approximate elsewhere. | 6–10 h | **Medium.** Fidelity, not progression — the view is already recognisable. | Whether the horizon band is per-cell; cells 636/673 sit at ~0.94 with no object. 70-entry `ega\d0\dpnnn` table at image `0x361E1`. `port/dungeon.cpp` |
| **P-7** | **Mover / collision test** (G-005 residue). Wall set `{6,13}` is decompiled at the renderer, but step-rejection is still oracle-derived. | 4–6 h | **Medium.** Movement works; this is about edge-case correctness. | `analysis/lof.asm` move path (the four trail-shift sites are already excluded). `port/dungeon.cpp` |
| **P-8** | **Character-sheet screen.** Oracle mismatch 17 837/64 000 px; palette lacks the oracle's red/yellow. | 6–10 h | **Medium-low.** A screen the player sees often, but no mechanics depend on it. | `ref/oracle/charsheet.png` (recipe in `docs/notes-maintainer-town.md`). `port/lof.cpp`, `port/char.*` |
| **P-9** | **Depth-2/≥3 bank sprite numbers** (scenario-5 values only). | 3–5 h | **Low.** Affects deep-dungeon views in non-Temple scenarios. | Emulator depth sweep on a non-Temple save. `port/dungeon.cpp` |
| **P-10** | **All-out-of-action message.** Trigger decoded (`[bp-0x36]`, image `0x18449`), blocked on message colour. | 2–4 h | **Low.** Cosmetic message. | Needs a mid-encounter oracle frame. `port/lof.cpp` |

### Done (kept for context; no action)

| Was | Task | Result |
|-----|------|--------|
| A-1 | Outdoor viewport composite (`D0`) | ✅ Exact on the oracle cell (**0 px wrong**). The last 106 px were a missing map-object draw (`EGA/UP/UP22` @ 70,53), not a fitted constant — see notes-a Round 51. Residue is P-6. |
| A-2 | Stairs / level transitions | ✅ Destination rule, portal index (`location%10`) and stairs wording decompiled (image `0x2e7fa`). |
| M-4 | Party position from the save, not the `DNG` header | ✅ `0x9a0` (cell) / `0x9a6` (facing); emulator-verified. |
| M-4a | `AH=4Ah` resize, LZEXE unpack, EGA 11-byte header, INT 16h scan/ascii | ✅ See `AGENTS.md` "Fixed bugs (do not regress)". |

---

## 3. Order of work

The queue in §2 **is** the order; work it top to bottom. The ranking rationale
is the "Why this rank" column, so there is no second list to keep in sync.

Two ordering facts worth stating because they are not obvious from the table:

* **P-4 (spells) is ranked below P-3** even though it is bigger, because a spell
  can only be cast once a combat loop exists to cast it in.
* **P-6 (outdoor scenery) is ranked below P-3/P-5** despite being the oldest open
  item. It is fidelity, not progression: the view is already recognisable, while
  P-2/P-3 block actual play.

### Remaining-effort total (checkable)

| task | low h | high h |
|------|-------|--------|
| P-2 object codes (selector done; trigger+effects left) | 4 | 6 |
| P-3 monster attack + combat loop | 8 | 14 |
| P-4 spells in combat | 12 | 20 |
| P-5 item use effects | 8 | 14 |
| P-6 outdoor scenery rule | 6 | 10 |
| P-7 mover/collision | 4 | 6 |
| P-8 character sheet | 6 | 10 |
| P-9 depth-2+ sprites | 3 | 5 |
| P-10 all-out message | 2 | 4 |
| **total** | **57** | **95** |

So **~57–95 agent-hours** remain (P-1 is done). Add ~30 % if the work must be re-derived by an
agent with no prior context on this codebase.

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
