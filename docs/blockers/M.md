# BLOCKERS — area M (shared / engine)

> **Area M is the old "lane M"** (domain label). The project is now a single lane
> (see `docs/WORKSTREAMS.md`); the A/B/M letters survive as area labels so the
> history stays transparent. Area M also owns the shared/serial code.

Add new entries here. The **mutable fields — `Status`,
`Priority`, `Dependencies` — are cross-editable**: anyone may edit them (and the
summary row) in *any* area's file; only an entry's *body* (title, description, ID)
is area-owned. Convention, ID rules and the priority/field definitions are in the
hub: [`../BLOCKERS.md`](../BLOCKERS.md). IDs are `BLK-M<nnn>`, numbered from
`001`.

Area M covers the shared, serial code (`src/*` emulator, `port/lof.cpp`,
`port/gfx.*`, `port/font.*`, `port/data.*`, `port/screens.*`) and any cross-area
work.

## Summary

| ID | Blocked lane | Responsible | Priority | Status | Depends on |
|----|--------------|-------------|----------|--------|------------|
| BLK-M001 | M (maintainer) | Maintainer | Medium | Resolved | — |
| BLK-M002 | M (maintainer) | Maintainer | Low | Open | — |

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

---

## BLK-M002 — Character-sheet / party-grid detail residue (status-block dashes, HP gauge position, gender profession table)

- **Blocked lane:** M (maintainer)
- **Responsible:** Maintainer
- **Priority:** Low
- **Status:** Open
- **Dependencies:** —

The `lof --charsheet` screen (`port/lof.cpp`, `compose_charsheet_screen`) now
matches `ref/oracle/charsheet.png` at **0.9885** whole-screen with the class
portrait **pixel-exact** (`tests/port_charsheet_test.py`). Three small residues
remain, none of which affect gameplay:

1. **Spirits/Rations value dashes (char-sheet-specific, ~32 px).** The
   character sheet's location/status box (green, index 8) draws the
   `Spirits  `/`Rations  ` labels but the *values* are not reproduced: the
   oracle shows a few index-4 (dark-red) dashes at y≈182 (x≈74,84–87,110,125),
   not a regular 6-px-pitch string. The status-block value drawer for the
   char-sheet's "placeholder" branch has not been decoded; the dashes are
   omitted rather than guessed.

2. **Party-grid HP gauge position (shared with the play screen, ~480 px).** The
   shared `draw_party_grid` draws ONE 50×3 white bar at `(x, y+18)` under the
   "HT" label. The oracle shows, for some slots (Handy; all of the lower row), a
   SECOND bar 4 px lower (`y+22`) — i.e. an "HP" gauge the port does not draw —
   and the third column's upper bar is 40 px wide, not 50. The rule that decides
   when the second bar appears (and the per-slot width) is undecoded. The
   char-sheet grid is *index-identical* to the play grid (`ref/oracle/play.png`,
   0 diff between the two oracles), so this is the same pre-existing
   `draw_party_grid` inaccuracy the play screen already carries; fixing it fixes
   both.

3. **Gender-variant profession table.** The char-sheet disassembly
   (`0x2676e`) selects the profession string from a table indexed by
   `record[+0x18]` (a sex/class selector); a gender-variant table
   (`gender_profession_names()`, e.g. Priest→Priestess) is visible in the
   disassembly. The port uses the plain `profession_names()` table for the
   race+profession line so the sheet agrees with the party grid (which also uses
   it). Which sex value selects which table is not proven; the only oracle
   (Decker, male) is satisfied either way.

The dynamic time-of-day line also differs (the GAME2 save says "Evening", the
oracle was captured at "Midnight!") — that is correct save-derived behaviour, not
a blocker (same as the play screen).
