# PLAN — prioritized task list, estimates, and project completion

Status report for the owner, and the **single work queue** — one agent works
this top to bottom. Refreshed at the start of each session (T-3). Open guesses
are in `docs/GUESSWORK.md`; rules are in `docs/RULES.md`; the deeper technical
history is in `docs/HANDOFF.md` §4. This file answers three questions: **what is
left, in what order, how long, and how done are we?**

Last refreshed: 2026-11-23 (session of agent lof2/M-6, sole active agent).
This revision **reconciles the completion table with the queue**. The previous
revision's ~87 % weighted only the *screens* that are built and silently left
out the gameplay subsystems the queue already lists but that are still at
0–25 %: overworld travel (P-4), town transactions (P-7), spells (P-5) and the
monster's turn (P-3). Those rows are now in the table, so the honest whole-game
figure is **~70 %**; the old screens-and-model figure (~87 %) is kept below for
continuity. The P-1..P-13 queue is unchanged in order and scope except that
P-4 records its 2026-11-21 reconnaissance, P-3's estimate now matches the
reduced scope stated in its own row, and the two small queued guesses (P-14,
P-15) are added so nothing open is off the list.

---

## 1. Overall project completion — ~70 %

Weighted by player-visible subsystem, not by line count. Two figures are given
because the earlier table measured only the finished screens/models and did not
weight the gameplay still missing:

- **Whole-game table (this revision): ~70 %.** Weights total 100 and the
  weighted sum is **70.5 %**, rounded to ~70 %. This is the number to quote.
- **Screens-and-model figure (previous revision): ~87 %** (86.8 %). The same
  rows as then, without the gameplay rows added below; it overstates completion
  because P-3/P-4/P-5/P-7 were not weighted.

The drop-in already boots, shows the intro, reaches the menu, walks a town,
visits every building, walks the Temple dungeon in first person, meets wandering
monsters, resolves a combat round (with monsters' own defence values and a real
weapon-break rule) and writes saves back. What is missing is concentrated in
four places: *overworld / wilderness travel* (leaving town, walking the map and
its encounter rolls), *town transactions* (rest/heal/shop/bank change nothing
yet), *spells* in combat, and *the monster's turn*; then the fidelity residue
(outdoor scenery, the `P`/`B` flags, the character sheet).

| Subsystem | Done | Weight | Notes |
|---|---|---|---|
| Analysis emulator (the oracle) | 100 % | 8 | stable; reproduces every oracle frame used below. Dev-only tooling, not shipped, but part of the project |
| VOR intro + credits | 100 % | 3 | `EGA/IN/DRAG`, `EGA/IN/CRED` byte-exact; the credits palette vs DOSBox is P-14 |
| Boot → menu | 100 % | 4 | `EGA/UP/MSCR` + `MENU`, font |
| Town screens + buildings | 100 % | 7 | inn, shop/emporium, bank, house, temple (+ heal/revive/rest **prompts**). **Menus only** — the transactions and state effects are P-7 |
| Town state effects (rest/heal/shop/bank) | 10 % | 6 | the screens exist but do nothing; gold/HP cannot change — P-7 |
| DNGS decode (all 29 levels) | 100 % | 4 | tiles + object layer + header |
| Dungeon first-person view | 80 % | 11 | Temple (`D5`) depth march pixel-exact. The outdoor (`D0`) composite is exact **for one cell only**; the scenery rule is overfit (**G-013**) — P-9. The outdoor **wall** case is derived and tested (`DP036` at the map edge, `DP001` otherwise) |
| Object/feature semantics | 55 % | 7 | selector, trigger, type dispatch, text path, once-only gate, the `+0x0e` answer line, the type-16 coffin latch, the type-7 door **move**, the **type-11 teleport** and the **type-18 trap** are live; the map-mutation buttons (12/19/20/21/22) are cut content (unreachable in the shipped maps), so the recruit stat block (**G-018**) is the only reachable P-2 item left |
| Level transitions (stairs) | 85 % | 3 | stairs/portals/level changes work and are decompiled (`0x2e7fa`); open: non-stair object codes |
| Encounter screen | 100 % | 4 | `SYST/NSCnn` record → `CHAR0/CHnn` |
| Combat: decision / round / result model | 78 % | 9 | melee round derived (`0x14D80` fallback), hit score, hand-slot weapon pick, `rand(damage)+1`, XP multiplier, defender rows (`0x27A1E`) and the `W` break check (`0x15852`) all derived; `--fight`/`--ui` wired. Open: `P`/`B` (P-11), the no-live-target state |
| Combat: monster turn + spells | 25 % | 9 | monster path `0x159F8` is mapped but the shipped gate aborts (monsters do not act — the port matches), and spells (`+0x0f == 4`) and the quick-combat loop are unported — P-3, P-5 |
| Character data (SPEC/ROST/SNSC) | 90 % | 5 | parse + checksum + save write done; the char-sheet *screen* is not pixel-matched (P-8) |
| Items + use effects | 65 % | 5 | table parsed, equip deltas derived; use-effects partial (P-6) |
| Save **read** | 100 % | 3 | archive fixtures, checksum, content addressing |
| Save **write** | 100 % | 3 | `build_save`/`write_save_file`; read→write byte-identical on every intact archive save; `--save-out` persists; tampered saves detected and flagged |
| Overworld / wilderness travel | 28 % | 6 | the wilderness play screen renders and the walk model exists; the **town-exit give-artefact (amulet) page** renders ink-exact (`--amulet`) and its **give-item effect** is implemented (`--amulet-give`); leaving town (the `(C)ontinue` path + guard-sprite viewport + `--ui` wiring), overworld movement and encounter rolls are unported — P-4 |
| Misc polish (char sheet, all-out msg, depth-2+ sprites, VOR palette) | 15 % | 3 | P-8, P-13, P-12, P-14 |
| **Weighted total** | **71.0 %** | **100** | |

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

**Priority order (2026-11-23, re-ordered):** P-4 -> P-7 -> P-6 -> P-5 -> P-3 ->
P-2 -> P-8 -> P-9 -> P-10 -> P-11 -> P-12 -> P-13 -> P-14 -> P-15.  The rows
below are in this order.  Rationale: **progression** first (P-4, leaving town),
then **survival** (P-7 town, P-6 items), then **combat** (P-5 spells; P-3 demoted
because the shipped monster gate *aborts* — monsters do not act in the shipped
data, so P-3 has near-zero player-visible impact), then the **blocked** object
stat block (P-2/G-018), then **fidelity/polish**.  (P-1, the save writer, is done.)

| # | Task | Est. | Why this rank | Ground truth / next step |
|---|------|------|---------------|--------------------------|
| ~~**P-1**~~ | ~~**Save writer.**~~ **DONE** | — | Persistence — the largest single capability of the last revision. | `build_save()`/`write_save_file()`; read→write **byte-identical** on every intact archive save; `lof --walk --save-out` persists. `port/char.{h,cpp}`, `tests/char_test.cpp`, `tests/port_walk_test.py` |
| **P-4** | **Town exit / wilderness travel** | 10–20 | **High.** The wilderness is currently reachable only by loading a save that starts there. The town `(C)ontinue` path is the normal way out. | **Reconnaissance DONE 2026-11-21 (session 38):** the owner-supplied key script was validated against the emulator — new game + leave town = `[any key] - a - 2 - c - c - c - 1`; load = `[any key] - o - [slot]`. The third `c` opens the **amulet page**, which is the engine's generic *give-an-artefact* flow (engine strings `0x3CF49` "On starting your journey…", `0x43A00` "artefact:" + kind list, `0x43ADD` "(L)eave it", `0x3E6DC` "Amulet") — implement one `give_artefact()` and reuse it, not an amulet special case. The `1` leaves town into the wilderness, matching the slot-2 play capture at 0.9272. Captures archived: `ref/emu/amulet_newgame.png`, `ref/emu/town_exit_wilderness.png`. **Open, in order:** (1) the amulet page's font/layout (sweeps ruled out CHR1/CHR2 at 5/6/8 px and every EGA page image — it is code-drawn); (2) disassemble the give-item routine, first verifying the LZEXE image→`ds` mapping with `tools/mz.py load()+flat()` against the known pair file `0x42BD0` ↔ `ds:0x0EC0` ("Defends"); (3) the 24-frame `EGA/ANIM/AN00..AN53` transition + `EGA/PAGE/PG00`; (4) outdoor movement + encounter rolls. **Amulet/give-artefact page DONE 2026-11-23:** `lof --amulet [--save GAMEn]` composes the page (MSCR + TWN00 viewport + party panel + CHR1 text block); the text block (narration + "The Amulet" + "Who should receive that (1-6)?" + "(L)eave it", rows 92/100/108/116/132/148/164) is **ink-mask exact** vs `ref/emu/amulet_newgame.png` (`tests/port_amulet_test.py`, `make golden-port`). The viewport is the plain street tile TWN00 with no separate guard sprite — re-measured 2026-11-23 at 0.9993 vs TWN00 with the emulator palette; the earlier "78.8% distinct scene" was a stock-vs-emulator palette error, corrected after the owner confirmed in DOSBox that the page is the town page with only the text changed. Remaining P-4: the `--ui` town->wilderness transition wiring. **Give-item effect DONE 2026-11-23:** `give_item()` (port/char) reproduces the engine's item-add routine (image `0x69e2..0x6c5f`): first empty slot, `item`, `charges` from item +0x13, `condition`=100, `inv_used`++ (+0x6b, `0x6c2a`), carried weight refreshed; `lof --amulet-give N --save S --amulet-out O` applies it and writes a sealed save. Pinned by `char_test::test_give_item` and `port_amulet_test.py`. `port/lof.cpp`, `port/screens.*` |
| **P-7** | **Town state effects** — inn rest, temple heal/revive, shop and bank transactions — *new in this revision* | 10–18 | **Medium-high.** The screens exist but do nothing, so gold and HP cannot change; that makes the town a museum rather than a place to prepare. | The heal/rest/transaction routines behind `--inn-rest`, `--temple-heal`, `--temple-revive`, `--emporium`, `--bank`; `write_save_file()` already exists for the result. **Decode plan (2026-11-23):** the building greetings/options are data-driven and string-ref navigation is unreliable — code and data live in separate segments, so the ds<->image offset mapping is *not* constant (verified: the 'finds no place' string maps at image-0x3C821, but 'Thyn.'/'welcome to our' do not). The reliable method is runtime: drive the emulator (`lof-native`; golden scenarios `shop`/`temple_bldg` already reach the buildings) and use `LOF_WATCH` on the party-record segment's gold (+0x72 dword) and HP (+0x22 word) while pressing the heal/bank/rest keys; first locate the party-record segment via the `LOF_I21` ROST-load trace, then capture a **wounded** party (the add-char golden flow is undamaged, so a save with lost HP is needed). **Inn-rest who-pays prompt DONE 2026-11-23:** `lof --inn-rest-pay [COST]` renders the *second* Inn-rest step (after `--inn-rest`'s "simple/W rations" choice) — "A nights stay and meal costs <n>g," / "who pays (1-6)?" / "(C)ontinue" over the same MSCR+TWN03 chrome; ink-matched to `ref/emu/inn_rest_pay.png` (lines 1/3/4 pixel-exact, cost line 25px residual from the overlapping 6px-advance font). The price is the engine's computed value (`"A nights stay and meal " + "costs " + n + "g,"`, strings 0x412bf/0x412d7); its formula is not yet derived, so it is a parameter (oracle = 2g for a 1-member party). **Pay effect DONE 2026-11-23:** derived the handler (image `0x820a..0x8680`). Cost = `2*members` [+ `3*sum(min(14,(weight_max(+0x64)-weight_cur(+0x66))/10))` for the W option]; `pay_gold()` deducts it from the payer's gold dword (+0x72) and refuses if short (`0xbdec`); `rest_party()` clears the out-of-action countdown (+0x49 → clears +0x1f/+0x4b), sets rations to 14 (W), heals HP by `2*level + max(0,Con-14)` clamped to +0x20, and restores magic (`+0x69 += max(1,+0x68>>3)`, clamped). `lof --inn-rest-do N [--inn-rest-rations] --save S --amulet-out O` applies it and writes a sealed save. Pinned by `char_test::test_rest_effect` and the rest-effect check in `port_inn_rest_pay_test.py` (GAME2: payer 4 pays 12g, wounded char 9→11). Still open: the spell-memorisation pass (`0x841e..0x8487`), the post-rest 32-slot reset, and the clock advance ("Good night, I'll wake you in the morning", string 0x4133a). `port/lof.cpp`, `port/char.*` |
| **P-6** | **Item use effects** (old P-5) | 8–14 | **Medium-high.** Potions/scrolls are how a party survives. | Dispatch `0x0E89D`/`0x0EDB0` located; wire the proven effect values (G-004). `port/char.{h,cpp}` |
| **P-5** | **Spells in combat** (old P-4) | 12–20 | **High**, but only once P-3's loop exists to cast in. | The spell table + the cast path in the combat dispatcher; `Cast Spell` is `+0x0f == 4`. `port/combat.{h,cpp}` |
| **P-3** | **Monster attack path + quick-combat loop** (`0x159F8`, `0x17BAA`) (old P-3) | 4–7 | **High.** Combat where the enemy never strikes is not combat. | **Entry mapped** (`docs/notes-b.md`): marker-1 gate, `+0x37 == 0x0c` HP cap, `+0x106`/`+0x108` → `0x137DE`, `+0x10e` → `0x15DF8`, `6*attacker[+0x05]` into `[0xafca]:0x3d48`, `[0xafc2]:0x3de9`; all combat segment globals alias arena `0x468E` and are static data. **The per-round row rebuild is DECODED (2026-11-21)** — image `0x27B3A`..`0x27B65`, found with `LOF_WATCH` on a live round: `+0x06 = -(10*(4 - rec[+0x29]))` (Mercenary `0xEC`), `+0x04 = 100*HP/(8*rec[+0x2c])`, `+0x0a` = the HP word from `[0xb082]:0x52 -> +0x9e`.  `defender_row()` implements both.  So the rows are done.  The path itself is now partially transcribed (`docs/notes-b.md` §3a): the dispatcher `0x137E6` sends every non-party actor to `0x159F8`; the *live* route is `0x15DF8` -> `0x15E1F` (the message variant at `0x15B1E` is not taken, because the monster instance's `+0x10e` is 0); gates `char[+0x1f] != 7`, `row[+0x16] == 0xff`, `inst[+0x106]|+0x108 == 0`; then the `+0x2b` range code and the `0x212f:0x9342` target call.  **`row[+0x16]` is now resolved by capture**: `[bp+6]` *is* the row (`468E:0258`), the rows are 12 bytes, so `+0x16` aliases the **next row's HP low byte**; here it is `0x10`, so the gate **aborts and the Mercenary never acts** — which is exactly what the emulator does, and what the port already does (its round loop only iterates party members).  **Scope reduced (8–14 h -> 4–7 h).**  Remaining: (a) ~~sweep the encounters~~ — settled by arithmetic: the gate needs the next row's HP low byte to be `0xff`, rows are built with HP a multiple of 8, so only a *damaged* row at exactly 255 can open it (the shipped Mercenary 8/16 never does); a positive test would need a big monster damaged by 1; (b) transcribe `0x15E1F`..`0x15Fxx` and the damage application only for the reachable case; (c) `0x17BAA`, now identified as the **party action dispatcher** (six slots, action 3/4 -> the object/spell branch at `0x17C04`, else the attack path at `0x17C8F`), so its two arms belong to P-5/P-6 rather than to P-3. `port/combat.{h,cpp}` |
| **P-2** | **Object-code semantics** — the remaining per-type effects (old P-2) | 1–2 | **High.** Doors and pickups are how a dungeon is explored. **Answer-input path DONE 2026-11-21:** `fire_cell_event()` now classifies the reply (`classify_answer`), prints the outcome line `+0x0e` per the `+0x4b` rule, and fires the effect **only** on a match; `lof --walk --answer TEXT` drives it, pinned by `tests/answer_path_test.py` (no answer → prompt only; `N` → line, no effect; `Y` → line + effect). | **Recruit line DONE 2026-11-21:** the accept path prints the engine's own " joins the companionship." (image `0x44E05`); `0x1D99A` copies only the class *name* into the slot (its `strlen` stops at the record's NUL), so the roster is not mutated and the stat origin is filed as **G-018**. **Type-16 latch DONE 2026-11-21:** `[0x7d1c]` is a *two-visit latch* for type 16 (lava / sarcophagus): the entry path (`0x1FA8F`) sets it, and the next visit (`0x1F639`) clears it, prints `ds:0x80d2` "The companions arise out of the coffin." (read from the live data segment) and jumps to `0x21276`, skipping the record's own text. `CoffinLatch` implements it; char_test pins the cycle. **Type-7 door DONE 2026-11-21:** `door_target()` stores `+0xe4`/`+0xe6`/`+0xe8` as the destination (location, map, cell) plus the pending flag, exactly as `0x20AD7..0x20B13` writes `0x37fa`/`0x460e`/`0x3824`/`0x3b15`. **Type-7 door MOVE DONE 2026-11-23:** the `[0x3b15]` reader is `0x2C1F6` (in an asm gap, found by a raw image byte-search); it only *skips* the normal walk-buffer commit, so the handler's writes are the move. The save loader proves `+0xe4`/`+0xe6`/`+0xe8` = location ones/tens/cell, so `DoorTarget::destination_location()` = `tens*10+ones` and `dungeon_follow_door()` loads the map and places the party (G-017 resolved; `dungeon_test::test_door_follow`). **Type-11 teleport DONE 2026-11-23:** the same `fire_cell_event` switch now handles type 11 (handler `0x20CD7..0x20CE4` writes `+0xe4` into the party cell, same map, facing unchanged); `dungeon_teleport_cell()` + `test_teleport` + `teleport_event_test.py` (DNG17 Air Elemental -> cell 629). The remaining per-type handlers (12/19/20/21/22 map mutations, 18 trap damage) are decoded in notes-a. **Type-18 trap DONE 2026-11-23:** same cell write + `apply_trap_damage()` (`rand(+0xe6)+1` per present member, clamp; RNG image `0x30ECC`, modulo `0x31012`); unit-tested by `combat_test::test_trap_damage`. **Reachability:** the type-12/18/19/20/21/22 records are SPEC indices 81..125, but no shipped map carries an event index above 48 (all 29 scanned), so they are cut content — only type 11 (SPEC7[12],[13]) and the recruit (SPEC0[1]) are placed. The only reachable remaining P-2 item is therefore the recruit stat block (G-018). **G-018 sharpened 2026-11-23:** the slot is the first empty party slot (`0x1F38C`), but the copy (`0x1D99A`) leaves `present=0`, and the display (`0x2a7d:0x449a`) reads a different segment (`[0xb0d2]` vs the copy's `[0xb036]`) and skips the detail draw when `present==0` — so the join path alone does **not** create a functional member; the stat/present origin is genuinely elsewhere (still open; do not synthesise). `port/dungeon.{h,cpp}`, `port/lof.cpp`, `port/combat.{h,cpp}` |
| **P-8** | **Character-sheet screen** (old P-8) | 6–10 | **Medium-low.** Seen often, no mechanics depend on it. | `ref/oracle/charsheet.png` (recipe in `docs/notes-maintainer-town.md`); palette lacks the oracle's red/yellow. `port/lof.cpp` |
| **P-9** | **Outdoor scenery-selection rule** (old P-6, G-013 residue) | 6–10 | **Medium.** Fidelity, not progression. | Whether the horizon band is per-cell; cells 636/673 sit at ~0.94 with no object; the 70-entry `ega\d0\dpnnn` table at image `0x361E1`. `port/dungeon.cpp` |
| **P-10** | **Mover / collision test** (old P-7, G-005 residue) | 4–6 | **Medium.** Movement works; edge cases only. | The `analysis/lof.asm` move path (the four trail-shift sites are already excluded). `port/dungeon.cpp` |
| **P-11** | **`P` / `B` round flags** — *new in this revision* | 2–4 | **Low, and blocked on P-3.** Two of the three round-result columns are always blank. **`P` guard derived 2026-11-21** (`0x1756D`..`0x1759D`): defender dexterity `+0x46 > 11`, a shield in slot `[bp-0x30]` (armour `[bp-0x3e]` at `0x1777A`), then `sub slot[+0x85], 2` and `P = 1`; message gate `[0xafc2]:0x3de9 == 0`. Remaining: **it is blocked on P-3, not on the slot scan.**  The `P` event runs in the *defender* path, and the only defender that has inventory is a party member — i.e. it needs a monster attack to land.  Until P-3 exists nothing in the port can reach it, so the slot scan (`weapon_slots()` covers only the two weapon hands; the shield/armour slots would mirror `defense_from_items`' equipped-class scan, item class 3 = shield, 2 = armour) is the second half of the work, not the first. `B` is untouched (`0x12846`/`0x13D7D`/`0x1471C`). Flag array `[0xafdc]+0x5608`: `W` done, `P` `+0x5609`, `B` likely `+0x560A`. `port/combat.*`, `port/char.*` |
| **P-12** | **Depth-2/≥3 bank sprite numbers** (old P-9) | 3–5 | **Low.** Non-Temple deep-dungeon views only. | Emulator depth sweep on a non-Temple save. `port/dungeon.cpp` |
| **P-13** | **All-out-of-action message** (old P-10) | 2–4 | **Low.** Cosmetic. | Trigger decoded (`[bp-0x36]`, `0x18449`); blocked on the message colour — needs a mid-encounter oracle frame. `port/lof.cpp` |
| **P-14** | **VOR credits palette** — *new in this revision* | 2–4 | **Low.** Cosmetic; the VOR stills are byte-exact to the decoded data but a DOSBox capture of the credits matches only 81.3 % because of a consistent index translation (VOR likely programs a custom attribute palette for the stills). | Trace `INT 10h AH=10h` (set palette) in `VOR.EXE` and reproduce the custom set, or confirm DOSBox's default is the cause. `docs/notes-maintainer-vor.md`. `port/vor.cpp`, `port/gfx.cpp` |
| **P-15** | **Event-key table 70-entry overflow** (G-014) — *new in this revision* | 1–2 | **Low.** No shipped level reaches it; the port caps at `kEventKeyMax` and never overflows. | Feed the emulator a synthetic DNGS with >70 distinct tile-3 object words and watch `[0x7d00]` and the resulting SPEC index. `port/dungeon.cpp` |

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
| P-3 monster attack + combat loop (reduced scope) | 4 | 7 |
| P-4 town exit / wilderness travel | 10 | 20 |
| P-5 spells in combat | 12 | 20 |
| P-6 item use effects | 8 | 14 |
| P-7 town state effects (rest/heal/shop/bank) | 10 | 18 |
| P-8 character sheet | 6 | 10 |
| P-9 outdoor scenery rule | 6 | 10 |
| P-10 mover/collision | 4 | 6 |
| P-11 `P`/`B` flags | 2 | 4 |
| P-12 depth-2+ sprites | 3 | 5 |
| P-13 all-out message | 2 | 4 |
| P-14 VOR credits palette | 2 | 4 |
| P-15 event-key overflow (G-014) | 1 | 2 |
| **total** | **71** | **128** |

So **~71–128 agent-hours** remain.  P-3 dropped to its reduced scope (4–7 h, not
8–14 h) and P-4 to 10–20 h after its 2026-11-21 reconnaissance; P-14/P-15 add
3–6 h.  The range is still dominated by the four big gameplay items (P-4, P-5,
P-7 and the P-3/P-6 pair), not by polish.

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
- **P-4 reconnaissance** (2026-11-21, session 38) is recorded in the P-4 row: the
  leave-town key script is validated, the amulet page is the generic
  give-an-artefact flow, and the wilderness destination matches the slot-2
  capture at 0.9272.  The amulet page's font/layout, the give-item routine, the
  animation and the outdoor movement remain open.

