# BLOCKERS — area B (characters / items)

> **Area B is the old "lane B"** (domain label). The project is now a single lane
> (see `docs/WORKSTREAMS.md`); the A/B/M letters survive as area labels so the
> history stays transparent.

Add new entries here. The **mutable fields — `Status`,
`Priority`, `Dependencies` — are cross-editable**: anyone may edit them (and the
summary row) in *any* area's file; only an entry's *body* (title, description, ID)
is area-owned. Convention, ID rules and the priority/field definitions are in the
hub: [`../BLOCKERS.md`](../BLOCKERS.md). IDs are `BLK-B<nnn>`, numbered from
`001`.

## Summary

| ID | Blocked lane | Responsible | Priority | Status | Depends on |
|----|--------------|-------------|----------|--------|------------|
| BLK-B001 | B (characters/items) | Maintainer | High | Resolved | — |
| BLK-B002 | B (characters/items) | Maintainer | Medium | Resolved | BLK-B001 |
| BLK-B003 | B (characters/items) | Agent B | Medium | Resolved | — |
| BLK-B004 | B (characters/items) | Maintainer | Medium | Partly resolved (dispatcher+state+actions grounded; `W P B` columns open) | — |
| BLK-B005 | B (characters/items) | Agent B | Medium | Resolved | — |

---

## BLK-B001 — Workstream B needs a scripted oracle path into combat / shop / item-use

- **Blocked lane:** B (characters/items)
- **Responsible:** Maintainer
- **Priority:** High
- **Status:** Resolved (2026-11-11)
- **Dependencies:** —

**Description & unblock action.** The oracle previously stopped at the `town`
screen (`--keys 0x18,0x1c`). Everything B can ground statically is done; the last
runtime tables (combat resolution, shop stock/prices, item-use) can only be
confirmed against real in-game state.

**Resolution.** The `town` scenario was wrong: the intro ends in the town's
**Inn with no party members**, and `O` opens the load menu (digits 1-5 or `C`;
**Enter is ignored**). The real town screen is reached from the Inn with
`A` (add a character), `2` (the 2nd character), `C` (leave the add menu), `C`
(leave the Inn) — scancodes `0x1e,0x03,0x2e,0x2e`. From the town menu the
building hot-keys then work: `E` = Emporium (shop), `T` = Temple, etc. These are
now golden scenarios `town`, `shop` and `temple_bldg` in
`tests/golden_test.sh` (`LOF_KEYGAP=250`, since each screen dwells ~a million
instructions). See `docs/notes-maintainer-town.md`
("How to actually reach the town screen"). B can now ground the shop/item-use
tables against these reads. Note (repo owner): enemies are generated on load and
move randomly, so combat is not reproducible from a save alone.

## BLK-B002 — Expose the runtime item-use table from a live engine

- **Blocked lane:** B (characters/items)
- **Responsible:** Maintainer
- **Priority:** Medium
- **Status:** Resolved (2026-11-12)
- **Dependencies:** BLK-B001

**Description & unblock action.** The play loop reads a 36-byte item-use table at
`es:[item*0x24 + 0x2712]` (bit 2 = "usable"; read at image `0x13A82`, `0x1CFD2`,
`0x1D156`). It was believed to be a **runtime** table in a segment no data file
carries. Unblock by adding a debug dump (like `LOF_EDUMP` but for that segment)
and capturing it once BLK-B001 lets the run reach the code that populates it.
Detail: `docs/notes-b.md`.

**Resolution (2026-11-12).** The premise was wrong: the table at
`es:[idx*0x24 + 0x2712]` is the **spell table**, and it is **static EXE data**,
not runtime. A live 1 MB dump of the `town` path (new `LOF_SEGDUMP` hook in
`src/main.cpp`) is **byte-identical** to the raw `LOF.EXE` image at `0x3EF32`
(file `0x44422`); 141 records of `[22-byte name][14 data bytes]`, already
transcribed correctly in `port/char_spells.inc`. No live-engine dump is needed.
The cited read sites (`0x13A82` = image `0x0E592`, `0x1CFD2` = `0x17AE2`,
`0x1D156` = `0x17C66`) are the spell-cast handlers. Full evidence:
`docs/notes-maintainer-itemuse.md`. This also spawned **BLK-B003**.

## BLK-B003 — `port/char_itemuse.inc` is sourced from the wrong EXE offset

- **Blocked lane:** B (characters/items)
- **Responsible:** Agent B
- **Priority:** Medium
- **Status:** Resolved (2026-11-13)
- **Dependencies:** —

**Description & unblock action.** `tools/spec.py` extracts the "item-use table"
from *file* offset `0x42712` (image `0x3D222` = `ds:0xa02`). No instruction in
the image references a `disp16` base of `0xa02` (only two incidental literal
`0x0a02` bytes exist), so `item_use_flags()` / `item_is_usable()` are built on
bytes the engine never reads. The handlers that BLK-B002 pointed at read the
**spell** table at `ds:0x2712` (image `0x3EF32`); an item's own state comes from
its 32-byte item record (`es:[bx + 0xfda]`, `shl bx,5`), not a 36-byte table.
Unblock by re-deriving the API from the real item record (the use/class byte the
handlers test is the item record's own byte) or removing the mis-sourced API.
Evidence: `docs/notes-maintainer-itemuse.md`. Detail also in `docs/notes-b.md`.

**Resolution (2026-11-13).** Re-derived from the use routine at image `0x0E562`.
The read `es:[bx*0x24 + 0x2712]` indexes the **spell** table (record base
`DS:0x26FC` = file `0x4440C`; `0x2712 = 0x26FC + 0x16`, the byte just past a
record's 14-byte parameter block). The index is the item record's `+0x18` byte,
which is a 1-based **spell index** — not an "armour sub-type". Changes:

- `port/char_itemuse.inc` (and its `--emit-itemuse` path in `tools/spec.py`)
  **deleted**; the mis-sourced table is gone.
- `ItemRecord::spell` added (+0x18 low byte); `item_use_index()`,
  `item_use_flags()` and `item_spell()` now resolve through the spell table.
- `item_is_usable()` is `(spells()[spell-1].data[0] & 0x07) != 0`, with the
  engine's potion carve-out (`item_class == 9`). 24 of 185 items qualify.
- `tests/char_test.cpp` pins the corrected mapping (Torch→spell 2 "Light",
  Healing potion→spell 80 "Healing I", 24 usable items).

Spot checks confirm `+0x18` is the spell index: Torch→"Light", Healing potion→
"Healing I", Healing II→"Healing II", Ring of power→"Regeneration I", Healers'
staff→"Restoration", Staff of light→"Fear undead".

**Follow-up (2026-11-13).** Two residual choices surfaced while proving the gate
and were filed on the guess watchlist rather than presented as derived:
**G-003** (the `spell == 0` non-potion case reads an uninitialised flag) and
**G-004** (`item_effect_name()` labels are grounded only for the effect values
the engine actually tests — dispatch `0x0E89D`, unequip `0x29984`). Evidence:
`docs/notes-b.md`.

**Follow-up 2 (2026-11-14).** Owner live play **resolved G-003**: using an
ordinary item prints "is not useful in this situation.", so `item_is_usable()`'s
`false` default matches the engine. The same report grounded item effect 34
(lantern: consumes oil, "make light"). G-004 narrowed to the effect values the
engine never tests.

## BLK-B004 — Ground the combat action dispatcher (round resolution)

- **Blocked lane:** B (characters/items)
- **Responsible:** Maintainer
- **Priority:** Medium
- **Status:** Partly resolved (2026-11-10) — dispatcher + round state grounded;
  action enum confirmed by live play (2026-11-17); `W P B` column semantics
  still open (needs decompile of the print routine or a populated-row capture)
- **Dependencies:** —

**Description & unblock action.** The encounter *screen* is now grounded (see
`docs/notes-b.md`, "Encounter / combat screen"): the intro variants, the four
`(A)ttack`/`(G)reet`/`(T)alk`/`(W)ithdraw` choices, the group count/rank line
(`combat_group_line`) and the melee/withdraw round-table headers all match the
first wilderness encounter (`ref/oracle/combat.png`, pixel-identical). The
enemy source is `SYST/NSC00` (`monster_roster`).

What remains is the action **dispatcher** and round resolution: which handler
each menu choice runs, how a round resolves, and how the round-result table is
filled. Those routines live in the image's code segment and reach their strings
through segment-relative pointers, so a plain immediate scan of
`analysis/lof.asm` finds the strings but not the branching. Unblock by exposing
a combat-round debug dump from the emulator — e.g. dump the resolved
per-character action/target bytes (char struct `+0x0e`/`+0x0f`) and the round
table rows after a scripted round — the same way `LOF_I21`/`LOF_EDUMP` grounded
SPEC/ROST. Then B can pin the dispatcher against real state.

Note (repo owner): combat is not reproducible from a save alone — enemies are
generated on load and move randomly — so the dump must pin the RNG seed (see
`docs/GUESSWORK.md` "RNG seeded from the BIOS tick counter"). The deterministic
`LOF_NO_TIMER=1` + `LOF_KEYAUTO=...` path reaches the encounter but does not yet
drive a round.

**Progress (2026-11-10) — the dispatcher and round state are grounded; the
emulator does drive a round.**  Full write-up in `docs/notes-b.md` ("Combat
action dispatcher — grounded").  In short:

- The **live party struct** is at linear `0x600B4` (= `0x5C92:0x3794`, reached
  via `es=[ds:0xafa6]=0x468E`, `les bx, es:[0x30]`), 410 bytes/record, name at
  `+1`.  A second copy sits at `0x471E8`.  (The save-file buffer is *not* the one
  combat writes.)
- The per-character **action byte is `+0x0f`**, default `1` (Attacks); `(D)efend`
  sets it to `0`.  `+0x0d` is the selected flag (`0x00`→`0xff`), `+0x0e` the
  target (filled by the `Who has … aimed at (1-6) ?` prompt).  The enum is the
  already-derived `kActionNames` (`0=Defends,1=Attacks,2=Stalks,3=Use object,
  4=Cast Spell`).
- Driving it needs `LOF_KEYAT` (the walk eats earlier keys): encounter `A` →
  config `A` → digit `1`..`6` → action letter → `C`ontinue, e.g.
  `LOF_KEYAT="50:0x18,100:0x03,150:0x50,200:0x48,250:0x2e,650:0x1e,780:0x02,900:0x20,1020:0x2e"`.
  Deterministic (`LOF_NO_TIMER=1`), md5-stable.
- The **round-result table** is `The result of this round:` + `Name       HP   W
  P B Attack` (melee, image `0x48D2D`) / `Gld. Rt Exp. HP` (withdraw,
  `0x49048`), one row per member.

**Still open.**  (a) the round table's per-column *semantics* (`W P B`,
`Attack`) are read off the header text only, not confirmed against a live
withdraw/melee table; (c) the meaning of the `+0x70` byte (round setup fills it
from `rand(5)+1`; independent of `+0x0f`).

**Owner live-play confirmation (2026-11-17) — (b) is closed, (a) narrowed.**
The owner played a round (full quote in `docs/OWNER-TASKS.md`, OT-002):

- **`Attack` column** = a Passed/Failed flag for whether the character performed
  their action; matches the only two engine literals, `"Failed"` (image
  `0x438B1`) and `"Passed"` (`0x438B8`).
- **`(S)neak`** = a check; on success the character "disappears in the shadows"
  (icon removed from the positioning panel), is not attacked that round, then
  next round auto-selects Attack and gets a sneak attack ("creeps up on … from
  behind"), and may target any rank.  Matches the engine messages
  `" disappears in the shadows..."` (`0x4309A`) / `" creeps up on "` (`0x43195`)
  / `" and now "` (`0x431A4`) / `"from behind!"` (`0x431AE`).
- **`(U)se object`** = opens the inventory to use an item (e.g. a crossbow for
  ranged combat) → enum index 3.  **`(P)erform magic`** = opens the spell list →
  enum index 4.  So the `+0x0f` enum values 2/3/4 are now confirmed by live play.
- **`W P B`** — the owner believes `W`=weapon, `P`=armour (German *Panzerung*)
  but could **not** identify `B`.  This remains the only open sub-item of (a):
  settle it by decompiling the round-table print routine, or with a follow-up
  owner observation of a populated `W P B` row.

## BLK-B005 — Implement the ROST per-record checksum in `port/char.cpp`

- **Blocked lane:** B (characters/items)
- **Responsible:** Agent B
- **Priority:** Medium
- **Status:** Resolved (2026-11-16, eighteenth session)
- **Dependencies:** —

**Resolution.** `port/char.{h,cpp}` now exposes
`uint32_t rost_record_checksum(const uint8_t* rec410)` (the alternating
ADD/XOR fold from asm `0x1BF18`) and `load_rost()` sets a `bool intact` flag on
every `RostRecord` (`tail == checksum(raw[0..409])`). `tests/char_test.cpp` adds
`synthetic_rost_checksum()` (hand-computed cases 411/415/413), a
`synthetic_rost_load()` round-trip (valid trailer → `intact`; a flipped payload
byte → not `intact`), and asserts every record of `game/ROST/ROST`,
`ref/rost/ROST1.cheat` and `ref/rost/ROST2.banked` is `intact`. `make test`
green.

**Original request (for the record).** The previous maintainer's open item — the
"integrity/checksum word" at record offset `0x19A` (410) — is now **decoded and
verified** by lane M (the algorithm is *not* a guess; it is traceable to the
decompilation and reproduces the stored trailer on 23/23 records). See
`docs/notes-maintainer-rost.md` for the full derivation and disassembly.

Summary: the 4-byte little-endian trailer after each 410-byte record is
`checksum(rec[0..409])`, an alternating ADD/XOR fold with a carry:

```
acc = 1
for i, c in enumerate(rec410):
    if c == 0: c = 1
    acc = (acc + c) if i % 2 == 0 else (acc ^ c)
trailer = uint32(acc)     # routine asm 0x1BF18, far-called 0x1BF1:0x0008
```

The engine verifies it on load (asm `0x2A1E1`) and, on mismatch, writes the
tamper marker `0xFC` into record byte `0x0C` (asm `0x2A1F6`); a later routine
(asm `0xB0CB`) prints the "caught ... trying to fraud" message when it sees
`0x0C == 0xFC`.

`port/char.cpp::load_rost()` already parses `rec.tail` but never checks it.
This is a lane-B task (the file is owned by Agent B), so it is filed here rather
than implemented by lane M. Requested:

1. Add `uint32_t rost_record_checksum(const uint8_t* rec410)` (algorithm above).
2. In `load_rost()`, set an `intact` flag (`tail == checksum(raw)`) on the
   record (and expose the `0x0C` byte if the tamper marker should be surfaced).
3. Add a `tests/char_test.cpp` case against the committed fixtures
   `ref/rost/ROST1.cheat`, `ref/rost/ROST2.banked` and `game/ROST/ROST`.
