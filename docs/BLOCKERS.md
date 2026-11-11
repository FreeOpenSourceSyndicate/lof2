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
| BLK-002 | A (dungeons) | Maintainer | Medium | Open | — |
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

## BLK-002 — Dungeon viewport palette: emulator draws colour, oracle is grey

- **Blocked lane:** A (dungeons)
- **Responsible:** Maintainer
- **Priority:** Medium
- **Status:** Open
- **Dependencies:** —

**Description & unblock action.** The emulator's slot-1 frame and
`ref/oracle/dungeon_slot1.png` agree on ~99.6% of *shape* but differ in colour:
the oracle draws the temple in greys (EGA indices 7/8/15) while the emulator
draws olive/blue/light-green. The game programs its palette via `INT 10h
AH=10h/AL=02`, so the divergence is in how plane bits map to attribute indices at
render time (write-mode / plane-read), **not** a missing sprite table. Unblock by
tracing the EGA attribute/write-mode path in `src/ega.cpp`. Before treating 100%
RGB as the bar, re-record the oracle with the exact capture sequence — the frame
is a live animation (58% raw match at frame 1500, 66% at 2500). Detail:
`docs/BLOCKED-agent-a.md`.

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
