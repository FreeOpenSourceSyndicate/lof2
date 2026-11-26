# P-8 character sheet — analysis notes

Oracle: `ref/oracle/charsheet.png` (Decker the Paladin, page 1 of the sheet).
Reach: **press the member's number key 1–6** (owner-confirmed 2026-11-26), or
left-click the member in the character list; exit with the right mouse button.
So it IS reachable by keyboard (the earlier "unknown reach key" is solved: it's
the party-slot digit, not the combat "1"/encounter "1").

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

## Portrait — RESOLVED: a single class sprite + a custom palette

NOT a composite. Driving the emulator (press the member's number key on the play
screen) opens the sheet, and it loads exactly **one** sprite,
`CHAR3/CH<profession>`, blitted at (10,11): Grischnak the Warrior -> `CH00`,
Decker the Paladin -> `CH09`. No face/armour overlays load.

The earlier 0.63 mismatch was a **palette** error: the character sheet programs
its OWN palette (greens), NOT the standard EGA set. Derived from the CH09<->oracle
correspondence (most-common RGB per 4-bit index):

| idx | RGB | idx | RGB |
|----|----------|----|-----------|
| 0  | (0,0,0)  | 8  | (0,85,0)  |
| 4  | (170,0,0)| 12 | (170,85,0)|
| 6  | (170,170,0)| 14 | (170,255,0) |
| 7  | (170,170,170)| 15 | (170,255,170) |

With it, `CHAR3/CH09` reproduces the oracle portrait **1.0000** (14280 px).

So P-8 = `CHAR3/CH<profession>` (portrait) + the sheet's custom palette + the
grid/text/location (all measured). Class->CH so far: Warrior=CH00, Paladin=CH09
(derive the rest by pressing 2..6, or from the class table). Palette indices not
used by the portrait (1,2,3,5,9,10,11,13) come from the text — dump via the
emulator palette or the oracle's other regions.

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
