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
| BLK-001 | A (dungeons) | Agent A | Medium | In progress | BLK-006 |
| BLK-002 | A (dungeons) | Maintainer | Medium | Open | — |
| BLK-003 | Maintainer | Maintainer | Medium | Open | — |
| BLK-004 | B (characters/items) | Maintainer | High | Open | BLK-007 |
| BLK-005 | B (characters/items) | Maintainer | Medium | Open | BLK-004 |
| BLK-006 | A (dungeons) | Agent A | Medium | Open | BLK-007 |
| BLK-007 | A (dungeons) | Maintainer | Medium | Open | — |

---

## BLK-001 — First-person viewport is a static tile, not the engine's per-cell composite

- **Blocked lane:** A (dungeons)
- **Responsible:** Agent A
- **Priority:** Medium
- **Status:** In progress
- **Dependencies:** BLK-006

**Description & unblock action.** The drop-in's `--walk` viewport draws the
static `EGA/D<d>/DP001` tile, but the engine composites the view from the full
view sprites plus smaller side pieces per cell. Ground truth exists:
`tools/dngs.py oracle <png> game/EGA <dungeon> <index>` scores a composite
against `ref/oracle/dungeon_slot1*.png`. The spawn view matches `DP001` at 0.952;
the *moved* view matches no single sprite (best 0.576). Unblock by decoding the
per-cell sprite selection/placement and rendering it in `port/dungeon.{h,cpp}`.
Detail: `docs/notes-a.md`.

**Log.** 2026-11-08 Agent A: characterised the spawn composite. The spawn
window's middle 80 columns (x 10..89) are **pixel-identical** to `DP001`; the
whole residue (681 px, 4.8%) sits in a horizon band (rows ~40..77) at the window
edges, and 608/681 of those are drawn where `DP001` is transparent — an engine
overlay, not a wrong sprite. The emulator trace confirms the engine reads a
*subset* of the D5 set per cell (`ega\d5\dp001` then `DP014`..`DP023` +
`ega\anim\an*`), so selection is per-cell, not a single tile. Moved to
`In progress`; finishing it needs the moved-view ground truth (BLK-006/007).
Also: the `dngs.py oracle` 0.952 is a **drop-in** number, not the emulator's —
see BLK-002.

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

**Log.** 2026-11-08 Agent A (evidence for the maintainer, not a fix): reproduced
the 0.996 shape match **from the EGA planes** via `LOF_EDUMP` (the `--shot`
framebuffer is black in the viewport, so `--shot` is the wrong test). The
viewport confusion is exactly emu 6↔orc 7 and emu 9↔orc 8, i.e. **one attribute
bit** (bit 0). At the dungeon the emulator's `attr_pal` is the **identity**
(`00..07 10..17`), so the game's grey palette table is not being applied;
`seq_mask=01 gc_mode=00 bpp=4` shows planes are read correctly. This narrows the
fix to the palette path — likely `INT 10h AH=10h/AL=02` (or an AL=00/AL=03)
whose effect is lost, or a palette table the game builds *before* the EGA mode
set and never re-applies. Repro in `docs/notes-a.md` Round 18.

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

## BLK-006 — A moved dungeon view matches no single sprite: need the composite ground truth

- **Blocked lane:** A (dungeons)
- **Responsible:** Agent A
- **Priority:** Medium
- **Status:** Open
- **Dependencies:** BLK-007

**Description & unblock action.** For the *spawn* cell the view is dominated by a
single sprite (`DP001`, middle 80 cols pixel-identical). For a *moved* cell the
view matches no single full-size sprite: `tools/dngs.py oracle
ref/oracle/dungeon_slot1_moved.png game/EGA 5 1` reports a window match of only
0.202 (best sprite `DP067` 0.576). That residual is either a genuine **composite**
(full views + the smaller `DPnnn` side pieces, e.g. 64x59 / 32x59 / 16x29) or an
**animation phase** mismatch (the viewport animates; the engine loads
`ega\anim\an*`). To decide, I need the matching DOSBox ground truth: a capture of
the *moved* cell taken with the **exact key sequence** that produced it (see
BLK-007), ideally several consecutive frames so the animation phase can be
matched. Unblock by recording that with `tools/capture_dosbox.sh` and dropping it
in `ref/oracle/`; then the composite rule can be decoded from
`port/dungeon.{h,cpp}`. Detail: `docs/notes-a.md`.

## BLK-007 — Reaching a dungeon in the emulator requires the maintainer's scripted key sequence

- **Blocked lane:** A (dungeons) — also blocks B via BLK-004
- **Responsible:** Maintainer
- **Priority:** Medium
- **Status:** Open
- **Dependencies:** —

**Description & unblock action.** The `tests/golden_test.sh` recipe reaches the
Temple (`LOF_KEYAUTO=0x18,0x02 LOF_KEYGAP=8 --frames 1500`), but it does not
reproduce the `ref/oracle/dungeon_slot1.png` state (the emulator's viewport is
black; the oracle's is grey) and I could not move the party within it. The
maintainer produced `ref/oracle/dungeon_slot1*.png` (Temple slot-1 spawn *and* a
moved frame), so a working recipe exists. Unblock by publishing it — the
`--keys`/`LOF_KEYAT` timeline that produced `dungeon_slot1_moved.png`, and the
frame index — as a one-line reproducible command in `tools/capture_dosbox.sh` or
a `ref/oracle/README`. That single artefact unblocks BLK-006 (moved view),
BLK-004/BLK-005 (combat/shop/item-use) and BLK-001. Detail: `docs/notes-a.md`.

---

## Closed (kept for the record)

*(none yet — entries move here when `Status: Resolved`)*
