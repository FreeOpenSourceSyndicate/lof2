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

## BLK-M002 — Character-sheet detail residue (status-block dashes, gender profession table)

- **Blocked lane:** M (maintainer)
- **Responsible:** Maintainer
- **Priority:** Low
- **Status:** Open
- **Dependencies:** —

The `lof --charsheet` screen (`port/lof.cpp`, `compose_charsheet_screen`) now
matches `ref/oracle/charsheet.png` at **0.9960** whole-screen with the class
portrait and the **entire 3x2 party grid pixel-exact**
(`tests/port_charsheet_test.py`). Two small residues remain, neither affecting
gameplay:

1. **Spirits/Rations value dashes (char-sheet-specific, ~135 px).** The
   character sheet's location/status box (green, index 8) draws the
   `Spirits  `/`Rations  ` labels but the *values* are not reproduced. The
   oracle shows a handful of index-4 (dark-red) 2-px specks at y≈182
   (x≈74,84–87,108–109,124–129) that do **not** form a readable string and are
   in a palette index (4) the sheet uses nowhere else for text — most likely
   DOSBox-capture noise (compare the index-6 specks in the stat panel below),
   not an engine draw. The status-block value drawer for the char-sheet's
   "placeholder" branch has not been decoded; the specks are omitted rather than
   guessed.

2. **Gender-variant profession table.** The char-sheet disassembly
   (`0x2676e`) selects the profession string from a table indexed by
   `record[+0x18]` (a sex/class selector); a gender-variant table
   (`gender_profession_names()`, e.g. Priest→Priestess) is visible in the
   disassembly. The port uses the plain `profession_names()` table for the
   race+profession line so the sheet agrees with the party grid (which also uses
   it). Which sex value selects which table is not proven; the only oracle
   (Decker, male) is satisfied either way.

**Resolved (2026-11-24) — party-grid proportional gauges.** The earlier report
that the grid carried a ~480 px mismatch is fixed. `draw_party_grid` now draws
the engine's **two** gauges under the "HT"/"HP" labels, both colour 15 (pale
green under the sheet palette):
  * the **upper** bar (rows `y+18..20`) fills `hp_cur*50/hp_max` px — a full-HP
    slot is 50 px, a 9/11-HP slot 40 px (= `9*50/11`, integer division);
  * the **lower** bar (rows `y+22..24`) fills `mp_cur*50/mp_max` px and is drawn
    **only when the character has magic** (`+0x68` max > 0).
Both gauges are derived from `ref/oracle/charsheet.png`: the six slots match
**0 diff**, and the shared `ref/oracle/house.png` grid stays pixel-exact
(`tests/port_building_test.py`). This also corrects the town/building grids.

The remaining stat-panel residue (~74 px) is sparse index-6 (brown) specks in
the DOSBox capture on a few label glyphs — the port's green label colour is
consistent with the rest of each glyph, so these are capture noise, not an
engine draw.

The dynamic time-of-day line also differs (the GAME2 save says "Evening", the
oracle was captured at "Midnight!") — that is correct save-derived behaviour, not
a blocker (same as the play screen).
