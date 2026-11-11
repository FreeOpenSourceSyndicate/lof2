# BLOCKERS — the shared blocker registry

This is the **one place** every agent lists what is stopping it and what would
unblock it. It replaces ad-hoc "I'm stuck" messages: a blocker written here is
visible to every other agent, the maintainer, and any replacement agent, with no
context loss. It is a shared file — **anyone may append**, but do not rewrite
another agent's entry except for the `Dependencies` field or to mark it resolved.

> Per-incident deep dives still live in `docs/BLOCKED-<name>.md` (a long
> diagnosis, a reproducer, evidence). The registry entry is the *index*: it says
> what is blocked, by whom, at what priority, and links to the detail file.

## Priority values (use exactly these three words)

| Priority | Meaning |
|----------|---------|
| **High** | Needs immediate attention. Work is stopped or actively losing time until this is removed. |
| **Medium** | Relevant tasks depend on this, but other work is still possible, so some delay is tolerable. |
| **Low** | Will have to be done at some point, but it isn't important yet. |

A blocker's priority is **not** fixed: if you are the responsible agent and you
mark a `Dependencies` entry, the dependency's priority may have to rise (a
`Medium` dependency of a `High` blocker is effectively urgent).

## Fields (every entry has all of these)

| Field | Meaning |
|-------|---------|
| **ID** | Stable reference, `BLK-NNN` (next free number). Use it in PRs/notes as `see BLK-004`. |
| **Blocked lane** | Which workstream cannot proceed: `A (dungeons)`, `B (characters/items)`, `Maintainer`, or `All`. |
| **Responsible** | Who must unblock it: `Maintainer`, `Agent A`, `Agent B`, `External (repo owner)`, or `Anyone`. |
| **Priority** | `High` / `Medium` / `Low` (definitions above). |
| **Status** | `Open` / `In progress` / `Resolved`. Set to `Resolved` when fixed; keep the entry for the record. |
| **Description & unblock action** | Precise: what is blocked, the evidence/reproducer, and exactly what must be done to remove it. |
| **Dependencies** | Other `BLK-NNN` that must be handled first. Written by the responsible agent. Empty = none. |

## How to use it

- **Blocked?** Add an entry at the bottom (next free `BLK-NNN`), add a row to the
  summary table, and link any `docs/BLOCKED-<name>.md` detail. Keep it short and
  precise — a replacement agent must be able to act on it cold.
- **Responsible agent?** Set `Status: In progress`, then `Resolved` with a
  one-line resolution (commit/PR ref). Mark `Dependencies` as you discover them.
- **Dependency changed?** Update the `Dependencies` field and, if needed, the
  priority of the blocker you depend on (add a dated `Log` line explaining why).
- Commit and push the registry the same way as any other repo change.

---

## Summary

| ID | Blocked lane | Responsible | Priority | Status | Depends on |
|----|--------------|-------------|----------|--------|------------|
| BLK-001 | A (dungeons) | Agent A | Medium | Open | — |
| BLK-002 | A (dungeons) | Maintainer | Medium | In progress | — |
| BLK-003 | Maintainer | Maintainer | Medium | Open | — |
| BLK-004 | B (characters/items) | Maintainer | High | Open | — |
| BLK-005 | B (characters/items) | Maintainer | Medium | Open | BLK-004 |

---

## BLK-001 — First-person viewport is a static tile, not the engine's per-cell composite

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

## BLK-002 — The emulator's whole play-loop palette is wrong (ramp, not the standard EGA set)

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

## BLK-003 — Engine font glyphs are an unidentified bold/double-struck transform

- **Blocked lane:** Maintainer
- **Responsible:** Maintainer
- **Priority:** Medium
- **Status:** Open
- **Dependencies:** —

**Description & unblock action.** The menu/town/panel glyphs the engine blits do
not byte-match the `CHR1`/`CHR2` strip cells: sliding a rendered `(P)ick-pocket`
against the oracle leaves a residual, and the greeting's first letter is a bold,
double-struck form with no exact match in `CHR1`, `CHR2`, the decoded planes,
`MENU`, `MSCR`, `LOF.EXE` or `VOR.EXE`. The strings/layout are confirmed from
`LOF.EXE`, so only the glyph *bitmaps* are unknown — the engine appears to build
them from the strip with an extra step (weighting/duplication). Unblock by
disassembling the glyph-blit routine that reads the `4130:` buffer `CHR1` loads
into, then reproducing the transform in `port/font.cpp`. Detail: `AGENTS.md`
("Known font-rendering gap").

## BLK-004 — Workstream B needs a scripted oracle path into combat / shop / item-use

- **Blocked lane:** B (characters/items)
- **Responsible:** Maintainer
- **Priority:** High
- **Status:** Open
- **Dependencies:** —

**Description & unblock action.** The oracle currently stops at the `town` screen
(`--keys 0x18,0x1c`). Everything B can ground statically is done; the last
runtime tables (combat resolution, shop stock/prices, item-use) can only be
confirmed against real in-game state. Unblock by scripting a key path (via the
`--keys` / `LOF_KEYAT` input hooks in `src/main.cpp`) that reaches a combat
encounter and the shop/item-use screens, and recording golden frames for them.
Detail: `docs/notes-b.md` ("Blockers → Needs the maintainer").

## BLK-005 — Expose the runtime item-use table from a live engine

- **Blocked lane:** B (characters/items)
- **Responsible:** Maintainer
- **Priority:** Medium
- **Status:** Open
- **Dependencies:** BLK-004

**Description & unblock action.** The play loop reads a 36-byte item-use table at
`es:[item*0x24 + 0x2712]` (bit 2 = "usable"; read at image `0x13A82`, `0x1CFD2`,
`0x1D156`). It is a **runtime** table in a segment no data file carries, so its
contents are only reachable from a running engine. Unblock by adding a debug dump
(like `LOF_EDUMP` but for that segment) and capturing it once BLK-004 lets the
run reach the code that populates it. Detail: `docs/notes-b.md`.

---

## Closed (kept for the record)

*(none yet — entries move here when `Status: Resolved`)*
