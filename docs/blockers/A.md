# BLOCKERS — lane A (dungeons)

Owned by **Agent A**. Add new entries here. The **mutable fields — `Status`,
`Priority`, `Dependencies` — are cross-editable**: anyone may edit them (and the
summary row) in *any* lane's file; only an entry's *body* (title, description, ID)
is lane-owned. Convention, ID rules and the priority/field definitions are in the
hub: [`../BLOCKERS.md`](../BLOCKERS.md). IDs are `BLK-A<nnn>`, numbered from
`001`.

## Summary

| ID | Blocked lane | Responsible | Priority | Status | Depends on |
|----|--------------|-------------|----------|--------|------------|
| BLK-A001 | A (dungeons) | Agent A | Medium | In progress | BLK-A003 |
| BLK-A002 | A (dungeons) | Maintainer | Medium | **Resolved** (2026-11-12) | — |
| BLK-A003 | A (dungeons) | Agent A | Medium | Open | — |
| BLK-A004 | A (dungeons) | Maintainer | Medium | Open | — |

---

## BLK-A001 — First-person viewport is a static tile, not the engine's per-cell composite

- **Blocked lane:** A (dungeons)
- **Responsible:** Agent A
- **Priority:** Medium
- **Status:** In progress
- **Dependencies:** BLK-A003

**Description & unblock action.** The drop-in's `--walk` viewport draws the
static `EGA/D<d>/DP001` tile, but the engine composites the view from the full
view sprites plus smaller side pieces per cell. Ground truth already exists:
`tools/dngs.py oracle <png> game/EGA <dungeon> <index>` scores a composite
against `ref/oracle/dungeon_slot1*.png`. The spawn view matches `DP001` at 0.952;
the *moved* view matches no single sprite (best 0.576), which confirms the
composite. Unblock by decoding the per-cell sprite selection/placement and
rendering it in `port/dungeon.{h,cpp}`. Detail: `docs/notes-a.md`.

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
- **Status:** Open
- **Dependencies:** —

**Description & unblock action.** For the *spawn* cell the view is dominated by a
single sprite (`DP001`, middle 80 cols pixel-identical). For a *moved* cell the
view matches no single full-size sprite: `tools/dngs.py oracle
ref/oracle/dungeon_slot1_moved.png game/EGA 5 1` reports a window match of only
0.202 (best sprite `DP067` 0.576). That residual is either a genuine **composite**
(full views + the smaller `DPnnn` side pieces, e.g. 64x59 / 32x59 / 16x29) or an
**animation phase** mismatch (the viewport animates; the engine loads
`ega\anim\an*`). Unblock by decoding the piece selection from
`port/dungeon.{h,cpp}`; the per-cell base rule is now known (BLK-A001 Round 24),
so what remains is the *side-piece* selection for the horizon band. Detail:
`docs/notes-a.md`.

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
- **Status:** Open
- **Dependencies:** —

**Description & unblock action.** The `tests/golden_test.sh` recipe reaches the
Temple (`LOF_KEYAUTO=0x18,0x02 LOF_KEYGAP=8 --frames 1500`), but it does not
reproduce the `ref/oracle/dungeon_slot1.png` state (the emulator's viewport was
black at that time; BLK-A002 has since fixed the plane-0 blit) and the party
cannot be *moved* within it. Unblock by fixing extended-key delivery, or by
publishing the `--keys`/`LOF_KEYAT` timeline that produced
`dungeon_slot1_moved.png` (and the frame index) as a one-line reproducible
command in `tools/capture_dosbox.sh` or a `ref/oracle/README`. Detail:
`docs/notes-a.md`.

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
