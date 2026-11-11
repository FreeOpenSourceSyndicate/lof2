# BLOCKERS — lane M (maintainer)

Owned by **Maintainer**. Add and edit entries here; do **not** edit another
lane's file — the only exception is adding/changing a `Dependencies` field on an
entry (and its summary-table row). Convention, ID rules and the priority/field
definitions are in the hub: [`../BLOCKERS.md`](../BLOCKERS.md). IDs are
`BLK-M<nnn>`, numbered from `001`.

Lane M covers the shared, serial code (`src/*` emulator, `port/lof.cpp`,
`port/gfx.*`, `port/font.*`, `port/data.*`, `port/screens.*`) and any cross-lane
work that no single lane owns.

## Summary

| ID | Blocked lane | Responsible | Priority | Status | Depends on |
|----|--------------|-------------|----------|--------|------------|
| BLK-M001 | M (maintainer) | Maintainer | Medium | Open | — |

---

## BLK-M001 — Engine font glyphs are an unidentified bold/double-struck transform

- **Blocked lane:** M (maintainer)
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
