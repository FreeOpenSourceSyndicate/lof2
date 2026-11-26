# P-8 character sheet — analysis notes

Oracle: `ref/oracle/charsheet.png` (Decker the Paladin, page 1 of the sheet).
Reach (from the manual, `docs/notes-manual.md`): LEFT-CLICK a party member in
the character list; exit with the right mouse button. It is a mouse action, not
a keyboard hot-key.

## Layout (matches the oracle; cross-checked with the manual §VII)

- **Left panel** = the character **portrait** (the class "graphic") over the
  location block ("Valley of Faerghail / Facing North / Midnight! / Spirits … /
  Rations …").
- **Right panel** = the shared 3×2 party grid (top), the decorative dragon-eye
  divider, then the **stat text** block:
  ```
  Decker : a chaotic
  Human-Paladin
  Rnk 01    Hit point 0010 / 0010
  ..        Magic .... 0000 / 0000
  Str 19    State ... Good
  Con 14    Experience 0000000379
  Dex 12    Weight .... 0029 / 0530
  Int 12
  Wis 15
  (E)xchange gold/rations
  (N)ext page
  (C)ontinue
  ```
  This is exactly manual sheet page 1 (attributes). Pages 2–5 (objects,
  spells, special abilities, languages) are reached with `(N)ext`.

## Portrait — the unresolved piece (composite, like BLK-A005's view)

Searched every `CHAR0..CHAR3` ≥100×100 sprite against the oracle portrait at a
range of offsets. Best is **`CHAR3/CH09` at (10,11)** — the Paladin class body:

- whole 120×119 match **0.63**,
- head rows (0–35) match **0.81** (so the head IS CH09),
- chest 0.64, but **legs (y80–119) only 0.48** — the lower body differs.

So the portrait is `CHAR3/CH09` with something overlaid/replaced on the lower
body (and possibly a face/helmet part), exactly the same class of problem as the
deep-view compositor. The individual `CHAR1` parts the earlier note mentioned
(bare face CH67, breastplate CH68) are all 120×119 full frames, i.e. overlays
composited onto the body — the blit order/positions are not yet derived.

## Next steps (do not guess)

1. Enumerate all `CHAR0..CHAR3` full-frame sprites that, overlaid on `CH09` at
   measured offsets, raise the portrait match; pin the exact order/positions from
   the oracle (per-region, like BLK-A005).
2. Implement `lof --charsheet [--save GAMEn]` composing background + portrait +
   `draw_party_grid` + the stat text (CHR1) + the location block; measure every
   text row/column from the oracle before hard-coding (Rule 1).
3. `tests/port_charsheet_test.py`: assert the grid + stat-text + location
   regions match pixel/ink-exact, and the portrait separately, against
   `ref/oracle/charsheet.png`.
