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
| BLK-B004 | B (characters/items) | Maintainer | Medium | **Resolved (2026-11-18)**: dispatcher+state+printer grounded; **column meanings derived from the decompilation** — W = weapon damaged, P = shield/armour damaged, B = ability improved, Attack = Passed/Failed. Owner play corroborated `P`; OT-003 (optional W/B corroboration) closed as not-needed | — |
| BLK-B005 | B (characters/items) | Agent B | Medium | Resolved | — |
| BLK-B006 | B (characters/items) | Maintainer | Low | **Partial (2026-11-24)**: **(R)epair DONE** — `repair_cost()`/`repair_item()`, cost = price − condition·price/100, magic/pristine refused, condition→100, pinned by `char_test::test_repair` + `port_building_test`. **Math helpers decoded** — the runtime library `0x2f0e` is now fully mapped: `0x1efe`=32×32 unsigned mul, `0x204a`=32÷32 unsigned div, `0x20ac`=32-bit unsigned modulo (verified by disassembly). **(S)ell / buy-haggling open — maintainer research, NOT an external block**: the price modifier reads a runtime arena window `[0xaf52]:0x4616` (a byte + a run of small signed modifiers `0,-10,-2,-4,-1,-1,-1,-4,…`, dumped with the Emporium open; `0x4616` itself reads 0) **spine now decoded 2026-11-24**: sell/repair share handler `0x5938` (sell=`S`->arg 0, repair=`R`->arg 1); it is an item-selection menu (char pick `0x212f:0x789c`, item list `0x212f:0x547e`) -> confirm `0x5818` -> remove item + add gold. Sell price (image `0x5C24..0x5D4A`): item price = item[+0xfce] (32-byte records); **condition term** = `(price*condition)/100 - price/25` (condition = inventory byte `+0x85`; helpers `0x1efe`=mul, `0x1e62`=signed div by 100, then `- price/25` via `div 0x19`); **haggle base** = `(price/100)*(haggle+1)` where `haggle = rand15 / charisma[+0x25]` (signed div `0x1f32`) and `rand15` is the LCG `0x1dec` (the *same* MS-C LCG as `CombatRng`, global seed `[0xaf06]`); final = base + condition term, decremented until `< price-10`. So the sell price is **random**, driven by the charisma-seeded LCG. The buy-side `[0xaf52]:0x4616==1` "fixed price" flag is buy-only (not in the sell path). **Still open:** pin the exact cap floor/signedness and confirm the rand->price end-to-end — needs a *scripted sell* with a known LCG seed (OT-006), since a random price has no single oracle. Not guessed. | — |

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
- **Status:** **Resolved (2026-11-18)** — dispatcher + round state grounded;
  action enum confirmed by live play (2026-11-17); `W P B` column meanings
  derived from the decompilation (W = weapon damaged, P = shield/armour damaged,
  B = ability improved, Attack = Passed/Failed).  Owner play corroborated `P`;
  OT-003 (optional W/B corroboration) closed as not-needed.
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

**Progress (2026-11-17, twenty-second session) — the round-table printer is
found; only the `W P B` *meaning* remains.**  The melee round table is printed
by image **`0x186A3`**, inside the combat round function **`0x1810E`**.  It
reads four per-character bytes from the party segment (`0x468E`, via
`[ds:0xafdc]`/`[ds:0xafd4]`/`[ds:0xafd6]`):

| column | address | printed |
|--------|---------|---------|
| **W** | `es:[i*2 + 0x5608]` | `* ` if ≠0 else `- ` |
| **P** | `es:[i*2 + 0x5609]` | `* ` if ≠0 else `- ` |
| **B** | `es:[i + 0x224]` | `* ` if ≠0 else `- ` |
| **Attack** | `es:[i + 0x3d48]` | `-` if out of action (`+0x49≠0`); else `Failed` if ≠0; else `Passed` |

The four arrays are **zeroed at round setup** (image `0x18198`–`0x181B8`) and
set to `1` by the **combat** setters below.  A `LOF_WATCH=4bee8:4bef4` run of
the scripted combat drive confirms a **W bit is set** mid-round (`1DAF:7CDA` =
image `0x156CA`), and that run's `LOF_TRACE_IP` shows **`ds = 0x4C92`** at the
setter — which pins the string base to **`0x41D10`** (see the correction below).
Full write-up + reproducible method: `docs/notes-b.md`
("BLK-B004 — the round-result printer FOUND").

> **CORRECTION (2026-11-18).** The earlier text here read the setters with the
> **wrong DS base `0x40000`** and therefore called them "shared town/combat
> setters" (House of Trades / Temple / pick-pocketing). That was wrong: the base
> is **`0x41D10`**, and the setters are **combat** messages. The correct meanings
> are in the next section. (The wrong base arose because `0x40000` is *also* a
> plausible base — DS `0x4AC1` — but the engine's DS at these setters is
> `0x4C92`.)

**Column meaning (resolved 2026-11-18, from the decompilation + owner play):**

- **`W`** = the character's **weapon was damaged** this round. Setters `0x156CA`
  (`sub byte es:[bx+si+0x85],2` on the **weapon** slot, `[bp-0x38]`) and
  `0x15955` (slot set to `0` — the weapon breaks); messages `"The weapon of … "`
  / `"Unfortunately, the weapon … breaks with a loud bang!"`.
- **`P`** = a **protective item (shield or armour) was damaged** this round.
  Setters `0x17658` (**shield** slot `[bp-0x30]`, −2), `0x1777A` (**armour**
  slot `[bp-0x3e]`, −2); the `0x17290` rust variant zeroes the weapon/shield/
  armour slot and prints `"The weapon/shield/armour rusts!"`.
- **`B`** = an **ability improved** this round. Setters `0x12846`/`0x13D7D`/
  `0x1471C` print `"(Defensive ability grows!)"` / `"(Sneaking ability
  improves!)"` / `"(Concentration improves!)"`.
- **`Attack`** = the action **Passed/Failed** (owner-confirmed).

`char[slot*4 + 0x85]` is the slot's condition byte (0..100; `port/char.h`).

**Owner live-play corroboration (2026-11-17, expanded report).**  The owner
described the table in full and confirmed several columns:

- the table has **one row per living party member**, of the form
  `[name]   -7 - * - Passed` / `[name]   -5 - - - Failed`;
- the **second column is the change in that member's HP** for the round (matches
  the printer's HP delta `char[+0x22] − snapshot`);
- the **`Attack` column** is `Passed`/`Failed` — whether the member's chosen
  action (attack, cast, …) succeeded;
- **`P = *` ⟺ armour −2%** — exactly the derived `P` rule (armour slot −2);
- **`B` is *not* used ammunition** (the owner tested and ruled that out) —
  consistent with the derived meaning (an ability/skill improvement).

The owner could not reproduce a **`W`** `*` (never damaged a weapon) and never
saw a **`B`** `*`, so those two remain derived-from-decompilation rather than
live-observed.  **OT-003** was narrowed to exactly those two cases and is
**optional corroboration** (Rule 1: the decompilation is the primary source, and
the `W`/`B` meanings already stand); the owner has since **closed it as
not-needed**.  The derived `W`/`B` strings were re-checked against the raw image
(present at file `0x48752`/`0x487fd` for `W`, `0x4841a`/`0x485a8`/`0x4866b` for
`B`).

**Quick vs regular rounds (owner, 2026-11-17).**  The round-result **table** is
shown only in **quick combat**; the per-action **messages** (weapon-damage /
ability lines) are shown only in a **regular** round.  The two never appear
together, which is why the owner could not cross-check a `*` against its message.
This is why the `W`/`B` cells cannot easily be confirmed by eye — the meanings
stay decompilation-derived.

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

---

## BLK-B006 — Emporium (S)ell / (R)epair price formula (blocked on 3 runtime helpers)

- **Blocked lane:** B (characters/items)
- **Responsible:** Maintainer
- **Priority:** Low
- **Status:** Open — partial decode (2026-11-24); blocked on runtime math helpers
- **Dependencies:** —

**What.** The Emporium `(S)ell` / `(R)epair` handler is image `0x5938` (strings
`0x3f13` "Who wants to sell wares (1-6)?" / `0x3f34` "Whose wares should be
repaired (1-6)?").  The `[bp+6]` argument selects sell (`== 0`, branch `0x5C24`)
vs repair (`!= 0`, branch `0x6172`).  The **buy** path is already implemented
(full price, `buy_item()`); only sell/repair prices are missing.

**Sell-price structure decoded (image `0x5C24`..`0x5D30`).** For the chosen
character's item slot (`[bp-0x2a]`, `char_index*0x19a` base in `[0xaf4a]`):

```
price   = item[+0x12]                         ; record+0x12, the price field
cond    = char[si+0x85]                        ; the item slot's condition byte
; call A = 0x2f0e:0x1efe(price, cond)  then  0x2f0e:0x1e62(...)
base    = A - price/25 ; if base < 1: base = 1 ; 0x5C86 `div si`(si=0x19)
bonus   = (price/100) * (char[+0x25] + 1)       ; 0x2f0e:0x2f6a + `div 0x64` + `mul cx`
final   = base + bonus                          ; 0x5D16 add [bp-0x16]
```

The condition byte `+0x85` and `char[+0x25]` (a Charisma-like stat) feed the
price, matching the "condition/Charisma scaling" note in `docs/notes-b.md`.

**Blocker (updated 2026-11-24).**  The far `segment:offset` → flat-image mapping is
**solved for code**: `flat = stored_seg*16 + off` (see AGENTS.md "CODE-segment
mapping").  With that, the helpers decode to the MS-C runtime math library:
`0x2f0e:0x1efe` = 32×32 multiply (low 32), `0x204a` = unsigned 32÷32, `0x1e62` =
signed 32÷32.  So the sell-price arithmetic is now readable.  What is **not**
yet pinned: (a) the **haggling randomisation** — `0x2a7d:0x2f6a` (the
`char[+0x25]` transform) calls the RNG `0x1dec` and then `0x1f32`, i.e. the
bonus has a random component (Charisma haggling); (b) the identities of
`char[+0x85]` (item condition) and `char[+0x25]` (a Charisma-like stat) and the
`0x1f32(rand, stat)` semantics.  Do **not** guess the sell price; sell/repair
stay unimplemented until those are pinned (needs a live `LOF_SEGDUMP` of the
party segment plus a DOSBox sell capture to validate the gold delta).
