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
| BLK-A001 | A (dungeons) | Agent A | Medium | Open | — |
| BLK-A002 | A (dungeons) | Maintainer | Medium | **Resolved** (2026-11-12) | — |

---

## BLK-A001 — First-person viewport is a static tile, not the engine's per-cell composite

- **Blocked lane:** A (dungeons)
- **Responsible:** Agent A
- **Priority:** Medium
- **Status:** Open
- **Dependencies:** —

**Description & unblock action.** The drop-in's `--walk` viewport draws the
static `EGA/D<d>/DP001` tile, but the engine composites the view from the full
view sprites plus smaller side pieces per cell. Ground truth already exists:
`tools/dngs.py oracle <png> game/EGA <dungeon> <index>` scores a composite
against `ref/oracle/dungeon_slot1*.png`. The spawn view matches `DP001` at 0.952;
the *moved* view matches no single sprite (best 0.576), which confirms the
composite. Unblock by decoding the per-cell sprite selection/placement and
rendering it in `port/dungeon.{h,cpp}`. Detail: `docs/notes-a.md`.

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
