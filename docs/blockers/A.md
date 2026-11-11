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

## BLK-A002 — The emulator's whole play-loop palette is wrong (ramp, not the standard EGA set)

- **Blocked lane:** A (dungeons)
- **Responsible:** Maintainer
- **Priority:** Medium
- **Status:** In progress
- **Dependencies:** —

**Description & unblock action.** The emulator renders the *entire* play loop in
the wrong palette — not just the dungeon. At the menu, the play screen and the
temple, the emulator's final attribute palette is the **ramp**
`00 01 02 03 04 05 06 07 10 11 12 13 14 15 16 17` (measured with
`LOF_DEBUG=1 LOF_VDBG=1 LOF_PAL2=1`), which turns brown into yellow, blue into
cyan, dark-grey into dark-green, etc. Re-rendering the emulator's own plane dump
with the **standard EGA palette** (`00 01 02 03 04 05 06 07 38 39 3A 3B 3C 3D 3E
3F`) lifts the match against the oracle on all three screens:

| screen | ramp (emulator) | standard |
|--------|-----------------|----------|
| menu   | 59.2% | **80.3%** |
| play   | 57.7% | **77.9%** |
| temple | 58.3% | **75.1%** |

The **drop-in** (`port/`) already uses the standard palette and matches
`ref/oracle/menu.png` at **99.8%** (`tests/port_menu_test.py`), so the standard
set is the correct one and only the emulator is wrong. Cause: the game programs
the palette via `INT 10h AH=10h AL=02h` from tables in the loaded data segment
(`4130:4834` fade tables, `4130:47DF` ramp; `cs:ip=119D:0D04` then `119D:0C78`).
The emulator stores the table bytes verbatim as attribute selectors, so the ramp
table yields `attr_pal[n] = 0x10+n` (dark-green…white) instead of the standard
`0x38..0x3F` high half. Unblock by determining the palette the engine actually
intends (the source tables do not contain the standard set, so the engine
translates them or writes `0x3C0` directly) and reproducing it in `src/ega.cpp`
/ the `INT 10h` handler. **Ground truth (repo owner, in-game):** the Temple is
greyscale right after loading (matches the standard palette's grey indices
0/7/8/15); colour appears only when moving (green specs on near walls). Detail:
`docs/BLOCKED-agent-a.md`, `docs/notes-maintainer-town.md`.
