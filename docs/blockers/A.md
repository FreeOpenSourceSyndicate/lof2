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
fallback meanwhile (G-004).

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
