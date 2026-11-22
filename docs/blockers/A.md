# BLOCKERS — area A (dungeons)

> **Area A is the old "lane A"** (domain label). The project is now a single lane
> (see `docs/WORKSTREAMS.md`); the A/B/M letters survive as area labels so the
> history stays transparent.

Add new entries here. The **mutable fields — `Status`,
`Priority`, `Dependencies` — are cross-editable**: anyone may edit them (and the
summary row) in *any* area's file; only an entry's *body* (title, description, ID)
is area-owned. Convention, ID rules and the priority/field definitions are in the
hub: [`../BLOCKERS.md`](../BLOCKERS.md). IDs are `BLK-A<nnn>`, numbered from
`001`.

## Summary

| ID | Blocked lane | Responsible | Priority | Status | Depends on |
|----|--------------|-------------|----------|--------|------------|
| BLK-A001 | A (dungeons) | Agent A | Medium | **Resolved** (2026-11-17) | BLK-A003 |
| BLK-A002 | A (dungeons) | Maintainer | Medium | **Resolved** (2026-11-12) | — |
| BLK-A003 | A (dungeons) | Agent A | Medium | **Resolved** (2026-11-08) | — |
| BLK-A004 | A (dungeons) | Maintainer | Medium | Resolved | — |
| BLK-A005 | A (dungeons) | Maintainer | Medium | **Resolved** (2026-11-18: closed as not-portable-by-data — producer input is RNG-seeded monster placement) | — |
| BLK-A006 | A (dungeons) | Maintainer | High | **Resolved** (2026-11-08) | — |

---

## BLK-A006 — the SPEC dispatcher's *call site* is not located, so the event trigger is unproven

- **Blocked lane:** A (dungeons)
- **Responsible:** Maintainer
- **Priority:** High
- **Status:** **Resolved (2026-11-08)** — the caller was found; the earlier "no
  caller" note was a bad segment base. See the log below.
- **Dependencies:** —

**Description & unblock action.** The object-word → SPEC-record *selector* is
proven (Round 54): the table build, the index lookup and `index * 0xee` are all
decoded and implemented. What was *not* proven was **when** the dispatcher runs
— on entering the cell, on a "look" command, or only on a specific action. This
matters because the port must not fire doors on every step.

**Log.** 2026-11-08 Maintainer — **resolved, and the earlier negative results
were wrong.** The bug was the far-pointer convention: `lcall seg:off` in this
image points at image offset `seg*16 + off`. With the correct base every search
finds real call sites:

| routine | role | callers |
|---------|------|---------|
| `0x1EFC4` | builds the per-level key table; returns early if empty (`retf` at `0x1F1BA`) | `0x2147F`, `0x215CC` |
| `0x1F31E` | the dispatcher proper: lookup, `index * 0xee`, SPEC read | `0x2E684`, `0x0DBCE` |

The general trigger is the per-tile-code resolver at `0x2E0BE`, reached from
`0x2C8CF` (`push cs; call 0x2e0be`) behind `cmp byte [bp+8],0`. Its tile-3
branch (`0x2E660..0x2E684`) is:

```
tile = byte es:[bx+0x46d8]   ; == 3, else skip the branch (0x2E664)
word = word es:[bx+0x4be8]   ; != 0x0a, else branch to 0x2E690 (0x2E679)
lcall 0x1cfe,0x233e          ; the dispatcher (0x2E684)
```

So the object layer is **walk-path only** and gated on the party's cell: it fires
for tile-3 cells whose word is not `0x0a`. Word `0x0a` is *not* "empty" — it
takes the separate `0x2E690` branch, which walks `0x19a`-strided character
records. The earlier "does not run in a boot-to-menu run" result was also a
mapping error: it traced flat `0x2F41E`, which is not the dispatcher's address.

The one remaining inference is the *meaning* of the `[bp+8]` flag (guessed "the
party moved"), logged as G-015. It does not affect the selector or the
`event_index()` predicate; it only affects when the port should call it.

**Deliverables.** `event_index()` in `port/dungeon.h` implements the exact
predicate; `tests/dungeon_test.cpp` asserts, per level, that no cell fires an
event unless the engine's guard would call the dispatcher, and warns if any
tile-3 cell falls in the `word <= 0x0a` gap (none do on shipped data).

---

## BLK-A005 — encounter id (`es:[0x259]`) producer rule not yet derived

- **Blocked lane:** A (dungeons)
- **Responsible:** Agent A
- **Priority:** Medium
- **Status:** **Resolved (2026-11-18)** — closed as *not-portable-by-data*: the
  producer's input is RNG-seeded monster placement, so the id is not a function of
  the save.  See the Round 51 log below.  (The viewport-composite half was
  resolved 2026-11-17.)
- **Dependencies:** —

**Description & unblock action.** The consumer half of the encounter-sprite
mechanism is fully decoded and delivered (`dungeon_encounter_sprite`): given the
party's encounter id, `NSC<dd>[id].+0x48` -> `ega\char<B>\ch<NN>`. What is *not*
yet pinned is the **producer** that sets `es:[0x259]` at map entry (id 16 for the
Temple spawn). The routine at `img 0x276CA` is the strongest candidate — it reads
the party map cell (`es:[0x4612]`/`es:[0x4613]`), walks the `NSC<dd>` records
(stride `0x11a`) testing `+0x39` (present) and `+0x2c`, and writes `es:[0x259]`
(`img 0x27A6A`; it is the *only* writer of that byte in the image) — but it does
**not** run in the boot-to-Temple recipe (`LOF_TRACE_IP` 0x377CA: 0 hits), and
neither does the consumer (`img 0x1A876` -> 0x2A976).  The trace hook is
validated (module base `0x10100`; `img 0x208C` -> 0x13A5C hits), so both are
walk/combat-path code.  Note also that id 16 is *not* a plain "record whose
placement byte `+0x44` == the party cell 125" lookup (that would be records 60-62,
Priests), so the id depends on state the producer builds.  Unblock by driving a
walk move in the emulator (needs the maintainer's extended-key fix, BLK-A004) or
by tracing the boot viewport path, then decoding the `+0x39`/`+0x2c` selection
(and the `es:[0x54]` level table) so the drop-in can pick the sprite for an
arbitrary walk move without the emulator.  Until then the play view keeps the
`DP001` fallback.  Detail: `docs/notes-a.md` Rounds 45-46.

**Log.** 2026-11-18 Maintainer (Round 50) — **the producer *does* run on a walk
step** (the blocker's stated unblock action), and it is a full window scan, so it
is portable but not trivial. Driving the Temple save with a forward step
(`LOF_CMDLEN=3 LOF_KEYAUTO=0x18,0x02,0x48,0x48 LOF_KEYGAP=8 … --frames 1500`)
and tracing `LOF_TRACE_IP=377CA:37d30` now yields **3 hits** at linear `0x377CA`
(it is walk/combat-path code — 0 hits at boot-to-Temple, as recorded above). The
`es:[0x259]` writer `img 0x27A6A` (linear `0x37B6A`) fires 6 times per call
(`si`=0,0xC,0x18,0x24,0x30,0x3C; `di`=0x11A0). So the fix the blocker needed
(BLK-A004, the extended-key scan-code delivery) is already in and the producer is
now observable; what remains is **porting it**, which is a self-contained job.

Decoded shape of routine R (`img 0x276CA`..`0x27D30`, disassembled with capstone
from `LOF_SEGDUMP`): it is a **proximity scan of the 3x6 cell window** in front of
the party, not a per-cell lookup, so the id genuinely depends on party cell + view
mode + per-record flags (confirming Round 46b). Outline:
1. Zero an 18-entry `(cellx,celly)` list at `[bp-0x32]` (0xff-filled) and a 3x6
   "seen" grid at `[bp-0x138]`.
2. Switch on view mode `es:[0x4615]` (`ds:[0xb08a]`): mode 2 → home `(col,row)` =
   `[bp-0x38],[bp-0x36]` = `(0xC,6)`; mode 1 → `(0,0x12)`, home=-1; mode 3 →
   `(6,6)`, home=-1.
3. Scan columns `[bp-0x34]`=0..`es:[0x4615]`-1, rows `[bp-0x3a]`=0..0x11: for each
   window cell, index the `NSC<dd>` record table (`es:[0x52]`, stride `0x11a`) by
   the cell's byte at `es:[bx+0x4612]` and require `+0x39` (present) and the
   32-bit ordering key at `+0x52`/`+0x54` > the running step counter; the
   home-cell column must also be past `[bp-0x38]` to take the special branch.
   Qualifying `(col,row)` pairs are appended to the 18-entry list.
4. Pack the list into the 3x6 grid `[bp-0x138]` (6 per row).
5. For each grid cell with a record, write the record's fields into the party
   "slot" array at `es:[0xb0ac]` (`+0x259` = record id, `+0x25a` = `+0x4c`,
   `+0x25d` = col+1, `+0x25f/0x260/0x261` = `+0x2d/+0x2e/+0x30`,
   `+0x25c` = a `100*key/(key*8)` ratio, `+0x262` a word from `es:[0x54]+0x9e`),
   and accumulate per-column hit counts in `es:[0xb0ae]`.

**What is left.** Port this scan into `port/dungeon.{h,cpp}` as
`dungeon_encounter_id()` (party cell + view mode + `NSC<dd>` `+0x39`/`+0x2c`/
`+0x52`/`+0x54` flags → id), then feed `dungeon_encounter_sprite()` instead of the
scenario-5 special case (G-007). Verify by comparing the emulator's `es:[0x259]`
after a scripted step against the port's computation for several DNG05 cells.
The consumer half is already delivered. Detail: `docs/notes-a.md` Round 50.

**Log.** 2026-11-18 Maintainer (Round 51) — **RESOLVED, closed as
not-portable-by-data.**  Capturing the producer's *input* with the new one-shot
`LOF_CAP` hook (at the producer entry `313F:63DA` / linear `0x377CA`) shows its
`es:[0x4612]` array is a **trail of NSC record indices** pushed by `img 0x23286`
from the record whose runtime **cell key** `+0xf8`/`+0xfa` equals the party cell
`es:[0x3824]`.  That key is written by the **monster-placement routine**
(`img 0x23095`), which walks each monster to a random 4-neighbour cell using the
game RNG (BIOS-tick seeded, `img 0x47A6`).  Measured across four runs of the same
Temple recipe: seed `0x0B` (twice) and `0x4321` put record 16 (`CH25`) on the
party cell, but seed `0x7777` put record 35 (`Ogre`, `CH40`) there — same seed is
byte-identical, a different seed changes the sprite.  So `es:[0x259]` is **not a
function of the save**; the 78-byte disk record carries no cell field.  The port
already reproduces the engine's observable view via the depth march (BLK-A001 /
G-006), which is the faithful, save-independent model; implementing the RNG
placement would reproduce a non-deterministic internal choice, not observable
behaviour.  No port change is warranted.  Detail: `docs/notes-a.md` Round 51.

**Log.** 2026-11-17 Maintainer (decode correction) — **the tiling model was a
symptom of a decoder bug, now removed.**  The EGA image decoder was reading the
RLE stream from file byte 11, but the engine's loader (image `0x1C70`) leaves
`si` on the `0x1234` magic and the blitter reads the pattern at `[si+0xa]`, so
the correct decode starts at byte 1 and drops the ten-byte header (the header's
size field is `4*w*h+10`, confirming it).  Decoding from byte 11 truncates any
run that straddles the header boundary, so `CH25` decoded to only 6888 of 7140
bytes — a single 120x119 blit then left the bottom-right unfilled, which is what
the "tile at x-stride 120 / y-stride -1" workaround was patching.  With the
correct decode every view cell is a **full 120x119 image** and the depth-2/>=3
sprites blit **plainly at the viewport origin (10,11)**: `CH25` at (10,11)
matches the oracle **1.0000**, the encounter `CHAR0/CH24` at (10,11) matches
**1.0000** (tiled was 0.279).  `DungeonView::tile` and the offsets `(58,27)` /
`(-14,28)` are gone.  The `es:[0x259]` encounter-id producer remains the only
open item.

**Log.** 2026-11-17 Maintainer (Round 49) — **RESOLVED (viewport-composite half).**
The 472-px residue is a **second `CHAR2/CH25` blit**: the corridor view is the
room sprite *tiled across the viewport* at **x-stride 120 / y-stride -1** (copies
at window `(-24,17)`, `(96,16)`, `(216,15)`, ... = screen `(-14,28)`,
`(106,27)`, `(226,26)`, ...). Two copies reproduce the **emulator framebuffer
exactly** (0 mismatching px, palette-independent) and the DOSBox oracle window at
**0.99930** (the 10 residual px are the engine's live 1px corner diagonal, absent
from the emulator too). No colour remap is involved — Round 48's "shade
difference" was a **palette-inversion bug** (its `attr_rgb()` swapped brown/red);
with `port/gfx.cpp::attr_rgb` the second copy is pixel-exact. Implemented via
`DungeonView::tile` + the tiling loop in `render_dungeon_play_view`; the moved
window score rose **0.9669 → 0.9993** (`tests/port_dungeon_view_test.py`, floor
0.998) and `tests/dungeon_test.cpp::test_view_depth` asserts `v.tile`. **This
half of BLK-A005 is closed**; the encounter-id *producer* (`es:[0x259]`) remains
open (below). Detail: `docs/notes-a.md` Round 49.

**Log.** 2026-11-17 Maintainer (Round 48) — negative results on the residue.
Exhaustively searched every `EGA/**` sprite at every integer offset against the
residue (window cols 96..119, 462 non-zero px): the best single-sprite fit is
**0.840** (`D3/DP030` at (94,14)); `D5/DP001` 0.02; `CH25` mirrored 174/472;
`CH25` blitted a second time at (96,16) explains 309/472 but the 153 misses are
*all* `oracle red(4)` vs `CH25 brown(6)` (a shading difference). So the residue is
a **separate second element** with a shade/palette offset, not a leftover, not a
mirror, and not a clean second copy of CH25. The emulator reproduces the moved
frame but its window matches the oracle only 65% (plane-0/palette divergence), so
it cannot isolate the residue; the drop-in is already closer (96.7% window, CH25
1.0000 over its coverage). Next: trace the producer with
`LOF_PXWATCH=<residue pixel>` (write site is the `119D:129F` blitter called from
the walk renderer). Detail: `docs/notes-a.md` Round 48.

**Log.** 2026-11-17 Maintainer (Round 47) — partial progress: the *view* use of
the encounter bank is now pinned for the D5/Temple scenario without needing the
`es:[0x259]` producer. The first-person sprite is selected by the **depth to the
first view-opaque cell**, not by the location id: depth 2 -> `CHAR2/CH40` at
(58,27), depth >=3 -> `CHAR2/CH25` at (-14,28), both verified pixel-exact against
the DOSBox oracle (BLK-A001). These two `CHnn` numbers (25/40) are D5's bank
values; whether they are keyed by depth or coincide with the location-id lookup
for this save is still open, so the drop-in applies them only for scenario 5 and
falls back to the depth-1 tileset sprite elsewhere. Still open: the general
`es:[0x259]` producer (the `img 0x276CA` routine) and the second element to the
right of CH25 in the moved window (the 472-px residue).

**Log.** 2026-11-08 Agent A (Rounds 45-46) — consumer established end to end by
disassembly + sprite-ink match (id 16 -> NSC05[16].+0x48=25 -> CHAR2/CH25 == spawn
viewport, 4941 px); trace hook calibrated (base `0x10100`); producer not yet run
in any captured session.

---

## BLK-A001 — First-person viewport is a static tile, not the engine's per-cell composite

- **Blocked lane:** A (dungeons)
- **Responsible:** Agent A
- **Priority:** Medium
- **Status:** **Resolved** (2026-11-17; offsets corrected 2026-11-18)
- **Dependencies:** BLK-A003

**Description & unblock action.** The drop-in's `--walk` viewport draws the
static `EGA/D<d>/DP001` tile, but the engine composites the view from the full
view sprites plus smaller side pieces per cell. Ground truth already exists:
`tools/dngs.py oracle <png> game/EGA <dungeon> <index>` scores a composite
against `ref/oracle/dungeon_slot1*.png`. The spawn view matches `DP001` at 0.952;
the *moved* view matches no single sprite (best 0.576), which confirms the
composite. Unblock by decoding the per-cell sprite selection/placement and
rendering it in `port/dungeon.{h,cpp}`. Detail: `docs/notes-a.md`.

**Log.** 2026-11-17 Maintainer (Round 47): **Resolved.** The view is a
*depth-marched single-sprite blit*, not a per-cell composite. March the column of
cells ahead of the party to the first **view-opaque** cell (low nibble NOT in
{0,2,7,8,9}); the depth selects the sprite and its screen offset:

| depth | sprite | screen blit |
|-------|--------|-------------|
| 1     | `EGA/D<d>/DP<index>` (tileset, `dungeon_base_view_index`) | (10,11) |
| 2     | `EGA/CHAR2/CH40` | (10,11) |
| >=3   | `EGA/CHAR2/CH25` | (10,11) |

(All cells are full 120x119 images and blit plainly at the viewport origin; the
earlier `(58,27)`/`(-14,28)` offsets and the x-stride-120 tiling were a workaround
for the byte-11 decode bug — see the BLK-A005 decode-correction log.)

Verified **pixel-exact** against the DOSBox oracle: the moved frame
(`ref/oracle/dungeon_slot1_moved.png`) is `CHAR2/CH25` tiled across the window at
x-stride 120 / y-stride -1 (copies at window (-24,17), (96,16), ... = screen
(-14,28), (106,27), ...) matching **1.0000** in the emulator framebuffer and
0.9993 vs the oracle (BLK-A005, Round 49); the spawn is `DP001` at (10,11)
(0.952 window / residue = engine overlay). The window is clipped to the viewport
(the CH25 blit at x=-14 must not spill onto the HUD). Implemented as
`dungeon_view_opaque` + `dungeon_view_for` + `render_dungeon_play_view` in
`port/dungeon.{h,cpp}`, wired into `--walk` and `--ui` (`port/lof.cpp`); pinned
by `tests/dungeon_test.cpp::test_view_depth` and `tests/port_dungeon_view_test.py`
(`make golden-port`). The depth-2/>=3 sprites are the scenario's encounter bank
(BLK-A005), pinned for D5; other scenarios keep the depth-1 tileset sprite.

**Log.** 2026-11-08 Agent A (Rounds 38-44) — the dependency (BLK-A003) is
**Resolved**: the moved-cell view is a single `EGA/CHAR2/CHnn` blit, and the `NN`
is a data lookup (`SYST/NSC<dd>` `+0x48`, indexed by the party's location id
`es:[0x259]`). The remaining work here is small and in-lane: thread the location
id from the play loop into `dungeon_room_art_number` and blit the resolved
`CHAR<B>/CHnn` at the viewport (offset -24,17 per Round 40) instead of the static
`DP001`. The resolver, tooling (`dngs.py nsc`) and tests already exist.

**Log.** 2026-11-08 Agent A: characterised the spawn composite. The spawn
window's middle 80 columns (x 10..89) are **pixel-identical** to `DP001`; the
whole residue (681 px, 4.8%) sits in a horizon band (rows ~40..77) at the window
edges, and 608/681 of those are drawn where `DP001` is transparent — an engine
overlay, not a wrong sprite. The emulator trace confirms the engine reads a
*subset* of the D5 set per cell (`ega\d5\dp001` then `DP014`..`DP023` +
`ega\anim\an*`), so selection is per-cell, not a single tile. Also: the
`dngs.py oracle` 0.952 is a **drop-in** number, not the emulator's — see BLK-A002.

**Log.** 2026-11-08 Agent A (Rounds 19–21): the composite is now **decoded at the
ink level and reproduced exactly by the engine**. `tools/dngs.py composite
ref/oracle/dungeon_slot1.png game/EGA 5` reduces the spawn window to `DP001` +
side pieces `DP042/044/051/056/022` (ink IoU 0.974), and the *emulator* frame for
the same cell gives the **identical** pieces/offsets. The viewport is a two-phase
**animation** (frame 700/1200 vs 900/1500), and at the oracle phase the emulator
window has **ink IoU 1.0000** with the DOSBox capture (identical 9298 ink px).
The only residual is the per-pixel index (+/-1) of **BLK-A002's** palette bug, so
BLK-A001's *shape* is fully explained.

**Log.** 2026-11-08 Agent A (Round 22): the DNG header is now decoded —
`[flag, entry_facing, entry_pos_hi, entry_pos_lo]`, entry_pos **big-endian**
`y*36+x` — and matches the saves exactly (DNG05 125/facing 2 = GAME1; DNG10
745/facing 0 = GAME2).  So the party's position/facing encoding is known.

**Log.** 2026-11-08 Agent A (Round 24): the base-sprite rule is **decoded**. The
base view is chosen by the **tile code of the cell directly ahead** of the party,
not by the wider neighbourhood.  Method (no new capture needed): patch DNG05's
RLE tile layer, run the analysis emulator with `LOF_EDUMP=` and compare the
120x119 window at (10,11) to every `EGA/D5/DP*` sprite by pixel IoU.  Measured
map (D5 Temple tileset): 4/20/36/52 -> `DP058` (IoU 1.000), 5/21 -> `DP059`
(1.000), 6/13 -> `DP060` (1.000), 10 -> `DP014` (0.997); wall / off-map /
unknown -> `DP001` (0.922); 2/7/8 are the "corridor straight ahead" case, which
is a **multi-piece composite** (a 64x59 wall-at-depth-2 piece `DP037` centred
plus smaller side pieces) with no single full-size sprite.  Patches to the side
and depth>=2 cells leave the dump **byte-identical**, so only the ahead cell
enters the selection, and the low nibble groups several codes (4/20/36/52 ->
`DP058`), i.e. the tile code is a **structure bitmask**.  Implemented as
`dungeon_ahead_cell` + `dungeon_base_view_index` + `render_dungeon_play` in
`port/dungeon.{h,cpp}`; pinned by `test_base_view`.  Remaining: the exact
bitmask -> DPnnn rule (the code looks like `(param << 4) | structure`) and the
corridor composite's piece selection (BLK-A003); `render_dungeon_play` is not yet
wired into the maintainer-owned `port/lof.cpp` walk path.

**Log.** 2026-11-08 Agent A (Round 26): the base-sprite rule is now **fully
decoded**. Swept every tile code that occurs in `DNGS/` (26 distinct values) by
patching DNG05's ahead cell with `tools/dngs.py patch` and diffing the emulator's
`LOF_EDUMP=` window against every `EGA/D5/DP*` sprite. Result: **only the low
nibble of the ahead tile code selects the base view; the high nibble is
ignored** — seven high-nibble variants each of nibble 4 and nibble 5 give
byte-identical windows (`DP058` / `DP059`, IoU 1.000). Map: `0x4`→`DP058`,
`0x5`→`DP059`, `0x6`/`0xd`→`DP060` (all 1.000), `0xa`→`DP014` (0.997); the
corridor/opening nibbles `0x0/0x1/0x2/0x3/0x7/0x8/0x9/0xb/0xc` fall back to
`DP001` (0.922). `dungeon_base_view_index` now returns `tile & 0x0f`; the only
remaining gap is the corridor composite (BLK-A003).

## BLK-A002 — The emulator renders plane 0 (blue) wrong across the play loop

- **Blocked lane:** A (dungeons)
- **Responsible:** Maintainer
- **Priority:** Medium
- **Status:** **Resolved** (2026-11-12)
- **Dependencies:** —

**Resolution (2026-11-12).** Root cause found and fixed: **`Machine::port_out16`
dropped the paired data-port write whenever the data byte was 0** (the guard was
`if (v & 0xFF00)`). The game's plane blitter cycles GC *Read Map Select*
`0,1,2,3` and SEQ *Map Mask* `1,2,4,8` across four passes using
`mov al,idx; mov ah,data; out dx,ax`. The **first** pass sets Read Map Select
**0**, so the write was skipped and the register kept its previous value
(plane 3); that pass then copied plane 3's data into plane 0. Hence plane 0 of
every rendered frame was wrong while planes 1–3 were perfect.

Fix: always drive the paired data port (`0x3C5`/`0x3CF`/`0x3D5`, or the
attribute controller's second `0x3C0` write) regardless of the AH value; a zero
data byte is a legitimate register value. Verified against
`ref/oracle/menu.png`: **all four planes now match the oracle at 100.00%**
(plane 0 went 80.29% → 100.00%); overall frame 80.29% → 100.00%. Golden hashes
re-recorded. Detail: `docs/notes-maintainer-town.md` ("BLK-A002 CORRECTED").

**Original investigation (kept for the record).** *Reframed 2026-11-12 (the
earlier "palette" framing was a red herring).* The emulator's frame at the menu,
play and temple differed from the DOSBox oracle **only in EGA plane 0 (blue)**:

- planes **1, 2, 3 were byte-identical** to the oracle (100.000%);
- plane 0 matched **80.29%**; every difference was plane 0 only.

The write *mechanism* was ruled out: every EGA write in the menu render used
write mode 0 and a single-plane map mask (`1/2/4/8`, 10439 writes each —
balanced). Tracing the blitter (`119D:1288`, plane counter `cs:[0x182]`) showed
it copies `ds=A1F4 → es=A000` (`A1F4 = A000 + 0x1F40`, i.e. an EGA double
buffer), one plane per pass — which pointed at the *register setup* for the
plane-0 pass, where the `port_out16` zero-byte bug lived. The drop-in
(`port/gfx.cpp`) was never affected; it renders the planes correctly.

## BLK-A002 (superseded framing) — emulator play-loop palette was thought wrong

The emulator's final attribute palette at the menu, play and temple is the
**ramp** `00 01 02 03 04 05 06 07 10 11 12 13 14 15 16 17` (measured with
`LOF_DEBUG=1 LOF_VDBG=1 LOF_PAL2=1`). It is programmed via `INT 10h AH=10h
AL=02h` from tables in the loaded data segment (`4130:4834` fade tables,
`4130:47DF` ramp; called from `cs:ip=119D:0D04` then `119D:0C78`). This is real
but is a *second*, cosmetic defect; the substantive defect is the plane-0 data
bug above. Detail: `docs/BLOCKED-agent-a.md`, `docs/notes-maintainer-town.md`.

## BLK-A003 — A moved dungeon view matches no single sprite: need the composite ground truth

- **Blocked lane:** A (dungeons)
- **Responsible:** Agent A
- **Priority:** Medium
- **Status:** Resolved
- **Dependencies:** —

**Resolution (2026-11-08, Agent A, Rounds 38-42).** There is no composite. The
moved-cell view is a single sprite blit from the **`EGA/CHAR2/CHnn`** bank (the
room/interior art) at a fixed offset — the earlier searches only scanned
`EGA/D5/DP*` and `EGA/ANIM/*`. The CH number is read from the scenario
NPC/monster table **`SYST/NSC<dd>`** at `+0x48` of the record indexed by the
party's location id (`es:[0x259]`), i.e. `location -> NSC<dd> record -> +0x48 ->
ega\char2\chNN -> viewport blit`. Verified end-to-end: patching that byte in
`NSC05` changed the emulator's open from `ch25` to `ch30`; the corridor viewport's
colour histogram equals CH25's exactly. Implemented as
`dungeon_room_art_number` in `port/dungeon.cpp`; tooling `dngs.py nsc`; test
`test_room_art`. See `docs/notes-a.md` Rounds 38-42.

**Description & unblock action (superseded).** For the *spawn* cell the view is dominated by a
single sprite (`DP001`, middle 80 cols pixel-identical). For a *moved* cell the
view matches no single full-size sprite: `tools/dngs.py oracle
ref/oracle/dungeon_slot1_moved.png game/EGA 5 1` reports a window match of only
0.202 (best sprite `DP067` 0.576). The residual is a **composed** view the engine
builds from the whole ahead column (not a single sprite and not a simple piece
composite — see the Round-28 log). Unblock by decoding the engine's per-depth
geometry composition from `port/dungeon.{h,cpp}` or the decompilation; the
per-cell base rule is known (Round 26/28), so what remains is the *composition*
rule for open cells. Detail: `docs/notes-a.md`.

**Log.** 2026-11-08 Agent A (Round 30) — **decompilation consulted (Rule 1); the
corridor view is procedurally drawn, not a sprite composite.**  Read
`analysis/lof.asm` + emulator DOS traces:

- Colour-accurate template search: the *wall* view is `D5/DP001` at **98.8%**;
  the *corridor* view matches **no** `EGA/**` sprite (best 20% = noise) and has
  **zero colour-exact containment placements** across all 51 `D5/DP*` (hundreds
  of 100%-*ink* placements exist, which is what misled the Round-28 ink search —
  the colours never agree).
- `LOF_I21=1`: entering the dungeon opens **all 51 `D5/DP*` + all `ANIM`** at
  once (loader image `0x1f5c` / wrapper `0x236e`), so per-view selection is an
  in-memory pointer, not a file open.  String table at image `0x3b3e8`.
- `D0/DP001`, `CHAR1/CH26`, `CHAR2/CH25` reproduce the corridor *silhouette* at
  46-58% but never its colours → the same routine draws the corridor shape with
  per-context content, i.e. a **procedural room draw**.  This matches the
  engine's `Enter room-number?` / `Akt. room: %d` debug strings.

Model: **wall cells blit `DP001`; open/corridor cells are drawn by the room
routine.**  The room routine (inside the GFA-compiled body; no adjacent lookup
table visible near the loader) is still what the residual needs — same blocker,
now with the mechanism named.  Detail: `docs/notes-a.md` Round 30.

**Log.** 2026-11-08 Agent A (Round 29) — **the composition is a depth-scaled
perspective draw, and the feature byte is irrelevant.**  Two follow-ups to
Round 28, both via DNG05 ahead-cell patches (party (17,3) facing S):

- The ahead cell also carries a non-zero **feature** byte (34).  A new
  `dngs.py patchcell` (tile + feature) shows the feature has **no effect** on the
  window: `(tile 0, feat 0/12/132)` all give `e165034f…`; `(tile 3, feat 0/34)`
  both give `694ac649…`.  Only the tile code matters.
- Moving the first wall further down an open column gives a **depth-scaled**
  view: first opaque cell at depth 1 → `DP001` (full-screen wall, IoU 0.917),
  depth 2 → `58116d1d…` (a composed **trapezoidal wall face**, ink 5111),
  depth >= 3 → `e165034f…` (the corridor, ink 4941, **saturates**).  So the view
  is not binary "wall / not wall"; it is the perspective projection of the
  first opaque cell.
- The view's opacity set differs from the movement wall set: a **tile-3** cell
  (passable for movement) placed at depth 2 renders the depth-2 face exactly
  (`58116d1d…`).  A lateral side-opening (tile 8) beside an open corridor does
  not change the view.

Net: the residual is a per-depth perspective projection, still engine-drawn (best
single-sprite IoU 0.36/0.35), so the decompiled view routine (Lane M) is still
what reproduces it.  `dungeon_base_view_index` stays depth-independent and exact
when the ahead cell is the first opaque cell; `test_base_view` now pins that.

**Log.** 2026-11-08 Agent A (Round 28) — **the "piece composite" hypothesis is
disproved, and the blocker is now precisely characterised.**  Swept every
ahead-cell code for DNG05's spawn (tile layer genuinely patched; see the
`cmd_patch` fix note) and diffed the 120×119 window against every sprite.  The
open-cell codes `0x0/0x2/0x7/0x8` give an **identical** view (ink 4941, md5
`e165034f…`) and `0x9` a near-variant (ink 4583).  That view:

- matches **no** single full-size sprite — best IoU 0.35 across **all 221**
  120×119 `EGA/**` files, and 0.346 among `EGA/D5/DP*`;
- is **not** a composite of `EGA/D5/DP*` (greedy + exact cover ≤ 57% ink, heavy
  false positives) nor of `EGA/ANIM/AN*` (≤ 44%);
- is **not** explained by a **colour-exact** placement search (zero placements —
  the ink-only composites above were over-counting because the D5 sprites use
  different palette indices than the corridor view);
- is **not** the drop-in's DDA ray-caster (IoU 0.17).

So the earlier `DP037`+side-pieces reading (`dungeon_slot1_moved.png` → 0.745)
was an ink-overlap artefact, not the engine's rule.  The view is **ray-composed
over the whole column**: with the ahead cell fixed at 0, changing the depth-2
cell changes the window (4941 → 5111).  That column-marching composition is the
residual.  The view is **frame-stable**: dumps at frames 1300/2000/2600 are
md5-identical, so it is not an animation phase either.  Reproducing it needs the
decompiled view routine (Lane M) — a cross-lane dependency.  Positive side result: the emulator's corridor capture matches the
moved oracle at symmetric-difference **10 px**, so the emulator *does* reproduce
the moved frame (BLK-A004's premise is stale).  The drop-in keeps the `DP001`
fallback meanwhile (G-006).

**Log.** 2026-11-08 Agent A (Round 27) — the opacity sweep that fixed the wall
set (`{6,13}`, `docs/notes-a.md`) also sharpens this blocker: a **wall** cell
(tile 6/13) renders as the single full-screen face `DP036` (opaque — the cell
behind is occluded), whereas a **corridor/opening** cell is *see-through* and is
the one that needs the multi-piece composite (`DP037` + side pieces).  So the
composite is only ever needed for the non-wall (passable) ahead codes, which is
exactly the residual tracked here.

**Log.** 2026-11-08 Agent A — decoded the composite *structure* without needing a
new capture, by re-running the greedy composite over the maintainer's **existing**
`ref/oracle/dungeon_slot1.png`: it reduces to `DP001` plus the small side pieces
`DP042/044/051/056/022` at fixed offsets (ink IoU **0.974**), and the *emulator*
frame for the same cell gives the **identical** pieces/offsets — so emulator and
real DOSBox agree on the rule `view = full base + side pieces in the horizon band
(window y ~40..60)`.  The base is map-driven (Temple spawn `DP001`; the wilderness
save's window matches `DP036/DP014` in `D0` at IoU 1.000).  The moved oracle
reduces less cleanly (`DP037` + pieces, 0.752).  Round 24 pins the base rule
(ahead-cell tile code), so the remaining open item is the side-piece *selection*
for the corridor case (ahead tile 2/7/8).

## BLK-A004 — Reaching a dungeon in the emulator requires the maintainer's scripted key sequence

- **Blocked lane:** A (dungeons)
- **Responsible:** Maintainer
- **Priority:** Medium
- **Status:** **Resolved** (2026-11-16)
- **Dependencies:** —

**Resolution.** The extended-key delivery the blocker asked for is implemented
(`src/machine.cpp`: `INT 21h` AH=01/07/08/0C return AL=0 for an extended key and
then the scan code on the next read), so the party now **moves** in the emulator
with a scripted arrow sequence.  Verified head-lessly and deterministically:

```sh
# Temple spawn (O, "1"); two Up (0x48) steps forward.
LOF_CMDLEN=3 LOF_NO_TIMER=1 LOF_KEYAUTO=0x18,0x02,0x48,0x48 LOF_KEYGAP=8 \
    ./lof-native --dir game --headless --frames 1500 --shot moved.png
```

The spawn frame's viewport (the `DP001` sprite at 10,11) matches
`ref/oracle/dungeon_slot1.png` at 62.7 % (vs 16.7 % for the moved oracle); the
two-step frame matches `ref/oracle/dungeon_slot1_moved.png` at **80.9 %** (vs
16.7 % for the spawn).  So `dungeon_slot1_moved` is reachable by **live
movement**, not only by patching the DNG tile layer (the earlier workaround,
`tools/dngs.py patch game/DNGS/DNG05 17 4 0`).  Both runs repeat md5-identical.
The reproducible recipe is published in `ref/oracle/README.md`.

**Description & unblock action (original).** The `tests/golden_test.sh` recipe reaches the
Temple (`LOF_KEYAUTO=0x18,0x02 LOF_KEYGAP=8 --frames 1500`), but it does not
reproduce the `ref/oracle/dungeon_slot1.png` state (the emulator's viewport was
black at that time; BLK-A002 has since fixed the plane-0 blit) and the party
cannot be *moved* within it. Unblock by fixing extended-key delivery, or by
publishing the `--keys`/`LOF_KEYAT` timeline that produced
`dungeon_slot1_moved.png` (and the frame index) as a one-line reproducible
command in `tools/capture_dosbox.sh` or a `ref/oracle/README`. Detail:
`docs/notes-a.md`.

**Log.** 2026-11-08 Agent A (Round 28) — the *view* half of this blocker is
resolved by an independent route: patching the DNG tile layer
(`tools/dngs.py patch game/DNGS/DNG05 17 4 0`) and re-running the headless
emulator reproduces the maintainer's `dungeon_slot1_moved.png` at
symmetric-difference **10 px** (ink 4941 vs 4951).  So the moved *frame* is
reproducible without any key sequence; only live *movement* (arrow keys) still
depends on the maintainer's AH=07/08 extended-key delivery.  The tile-patch route
is the recommended ground-truth method for Lane A (no keys, ~4 min/capture).

**Log.** 2026-11-08 Agent A — found the emulator-side root cause: the party
cannot be *moved* in the emulator at all, because the game reads keys via DOS
`INT 21h AH=0B` (poll, 398985 calls) then `INT 21h AH=07` (blocking read), and
the emulator's AH=07/08 returns `scancode_ascii(k)` with **no** follow-up
scancode read. For an extended key (arrows 0x48..0x50) `scancode_ascii` returns
`0x00`, so the game only ever sees AL=0 and never the scancode — the party never
turns or steps. (Real DOS returns AL=0 then, on the *next* read, AL=scancode.)
Trace: `LOF_I21ALL=1 LOF_KEYLOG=1 LOF_KEYAUTO=0x18,0x02,0x48,0x48 ...` shows
`[keylog] 21/07 consumed sc=48 ('H') instr=...` (the ASCII byte, not the
extended-key pair). So this is best satisfied by the maintainer fixing AH=07/08
extended-key delivery (or supplying a position-patched save).  Workaround in use:
patch the save's facing byte (0x9a6) and position field (0x9a0) to get new views
without keys (Round 19); the DNG header now gives entry position/facing directly
(Round 22), so A no longer needs the key recipe for ground truth.

## Closed (kept for the record)

*(none yet — entries move here when `Status: Resolved`)*
