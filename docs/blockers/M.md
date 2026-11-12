# BLOCKERS — lane M (maintainer)

Owned by **Maintainer**. Add new entries here. The **mutable fields — `Status`,
`Priority`, `Dependencies` — are cross-editable**: anyone may edit them (and the
summary row) in *any* lane's file; only an entry's *body* (title, description, ID)
is lane-owned. Convention, ID rules and the priority/field definitions are in the
hub: [`../BLOCKERS.md`](../BLOCKERS.md). IDs are `BLK-M<nnn>`, numbered from
`001`.

Lane M covers the shared, serial code (`src/*` emulator, `port/lof.cpp`,
`port/gfx.*`, `port/font.*`, `port/data.*`, `port/screens.*`) and any cross-lane
work that no single lane owns.

## Summary

| ID | Blocked lane | Responsible | Priority | Status | Depends on |
|----|--------------|-------------|----------|--------|------------|
| BLK-M001 | M (maintainer) | Maintainer | Medium | Resolved | — |

---

## BLK-M001 — Engine font glyphs are an unidentified bold/double-struck transform

- **Blocked lane:** M (maintainer)
- **Responsible:** Maintainer
- **Priority:** Medium
- **Status:** Resolved
- **Dependencies:** —

**Description (original premise).** The menu/town/panel glyphs the engine blits
were believed not to byte-match the `CHR1`/`CHR2` strip cells — a residual when
sliding a rendered `(P)ick-pocket` against the oracle, and a "bold,
double-struck" greeting glyph with no exact match in `CHR1`, `CHR2`, `MENU`,
`MSCR`, `LOF.EXE` or `VOR.EXE` — implying the engine applied an extra
weighting/duplication step.

**Resolution — the premise was wrong; there is no transform.** The engine blits
the `EGA/UP/CHR1`/`CHR2` cells **directly** (left 6 columns, 6-px advance), and
the drop-in already does exactly that. Verified by pixel-matching the DOSBox
oracle: every glyph of `Welcome, hardy` / `adventurers!` / `(P)ick-pocket` … /
`(C)ontinue` equals the `CHR1` cell `glyph_cell()` selects (all 10 menu strings
exact), and the *analysis emulator's* glyph **shapes** are byte-identical to the
oracle capture (only its palette differs — the separate emulator-palette issue).
The "double-struck" appearance is simply CHR1's real 2-px stems (e.g. band-0
cell 22 `W` = `##.##` rows), not a transform.

The original residue came from the **wrong-palette era** (before `port/gfx.cpp`
used the standard EGA set, commit `7ca187c`): with the wrong palette the
anti-aliased-looking diff read as a font mismatch. With the correct palette the
port's menu text is pixel-exact and the play-screen status/panel text is exact.

**Guard.** `tests/port_menu_test.py` now pins the greeting / option-list /
`(C)ontinue` regions to zero diff, so a real font regression (mapping or advance)
fails the test. `AGENTS.md`'s "Known font-rendering gap" section is rewritten as
resolved.
