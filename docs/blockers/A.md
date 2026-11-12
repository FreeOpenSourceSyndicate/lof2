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
| BLK-A002 | A (dungeons) | Maintainer | Medium | In progress | — |

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
- **Status:** In progress
- **Dependencies:** —

**Description & unblock action.** *Reframed 2026-11-12 (the earlier "palette"
framing was a red herring).* The emulator's frame at the menu, play and temple
differs from the DOSBox oracle **only in EGA plane 0 (blue)**. Comparing the
emulator's own index image (`LOF_EDUMP`, planes decoded to attribute indices)
against the oracle's index image (oracle RGB inverted through the standard
palette; all 64000 oracle pixels invert cleanly):

- planes **1, 2, 3 are byte-identical** to the oracle (100.000%);
- plane 0 matches **80.29%**; 12616 pixels differ and **every** difference is
  plane 0 only (`idx_emu ^ idx_oracle == 0x1` for all of them).

The emulator *also* ends up with the wrong attribute palette (ramp
`00..07 10..17` rather than the standard EGA set), but that is a **separate**
cosmetic issue: re-palletting the dump cannot exceed ~80% because the index
image itself is only 80.3% right, and that 80.3% is exactly the figure the
"standard palette" comparison reported. So the earlier conclusion ("standard
palette correct, only the emulator palette wrong") was wrong — the standard
palette was hiding a plane-0 *data* bug.

**What the engine does.** The game blits the framebuffer one plane at a time:
`119D:0184` cycles Graphics-Controller *Read Map Select* (`0x3CE <- 4`,
`ah=cs:[0x17a+idx]`) and Sequencer *Map Mask* (`0x3C4 <- 2`,
`ah=cs:[0x17e+idx]`), runtime table `cs:[0x17a] = 00 01 02 03 | 01 02 04 08`,
plane counter `cs:[0x182]`. The copy loops (`119D:0F90`, `119D:1250` via
`119D:1295`) `rep movsb` plane 0 under map-mask `0x01`, then advance.

**Unblock.** The write *mechanism* is not the cause: every EGA write in the menu
render uses **write mode 0** and a single-plane map mask (`1/2/4/8`, 10439 writes
each — balanced), so plane 0 is written just like planes 1–3 yet only plane 0 is
wrong. The defect is in the plane-0 **source data** (what the engine feeds the
plane-0 pass). Trace the `rep movsb` at `119D:129D` back to its `es:[si]` source
and compare to the planes 1–3 source; verify against `ref/oracle/menu.png`
(expect plane-0, and thus overall, match to approach 100%). **Emulator-only:**
the drop-in (`port/gfx.cpp`) renders the planes correctly (matches the oracle at
99.8%), so do not change it. Detail: `docs/notes-maintainer-town.md`
("BLK-A002 CORRECTED").

## BLK-A002 (superseded framing) — emulator play-loop palette was thought wrong

The emulator's final attribute palette at the menu, play and temple is the
**ramp** `00 01 02 03 04 05 06 07 10 11 12 13 14 15 16 17` (measured with
`LOF_DEBUG=1 LOF_VDBG=1 LOF_PAL2=1`). It is programmed via `INT 10h AH=10h
AL=02h` from tables in the loaded data segment (`4130:4834` fade tables,
`4130:47DF` ramp; called from `cs:ip=119D:0D04` then `119D:0C78`). This is real
but is a *second*, cosmetic defect; the substantive defect is the plane-0 data
bug above. Detail: `docs/BLOCKED-agent-a.md`, `docs/notes-maintainer-town.md`.
