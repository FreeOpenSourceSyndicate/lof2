# BLOCKED — workstream A (dungeons): first-person viewport needs an oracle frame

Owner: Agent A.  Branch: `work/agent-a`.  Status: **LARGELY UNBLOCKED — the
base-sprite rule is decoded (Round 24); the remaining gap is the exact
tile-bitmask → DPnnn rule and the procedural overlay.**

## Round 24 update (2026-11-08) — the base-sprite rule is a tile-code table

The earlier report below said the base-sprite rule was unknowable without more
oracles.  It is not: the rule is a **tile-code → DPnnn table** keyed on the tile
code of the cell **directly ahead** of the party.  Method (no new oracle needed):
patch DNG05's RLE tile layer, run the analysis emulator with `LOF_EDUMP=`, and
compare the 120x119 window to every `EGA/D5/DP*` sprite by pixel IoU.  Full
table in `docs/notes-a.md` Round 24.  Highlights: 4/20/36/52 → DP058 (IoU
1.000), 5/21 → DP059 (1.000), 6/13 → DP060 (1.000), 10 → DP014 (0.997), wall /
off-map / unknown → DP001 (0.922).  Side and depth-≥2 cells are provably
irrelevant (patched dumps are byte-identical).  Implemented as
`dungeon_base_view_index` + `dungeon_ahead_cell` + `render_dungeon_play` in
`port/dungeon.{h,cpp}` and pinned by `test_base_view`.

Still open (tracked as BLK-001/BLK-006): the exact bitmask → DPnnn rule (the
tile code looks like `(param << 4) | structure`; codes 2/7/8 are the
"corridor straight ahead, no side wall" case whose view the engine *composites*
from a 64x59 piece `DP037` centred plus smaller side pieces) and the residual
procedural overlay.  The emulator now renders the viewport (maintainer fix), so
further sweeps are possible; the sandbox copies fail the disk check, so sweeps
must run against `game/` sequentially.

The original report is kept below for the record.

## RESOLVED (merge of `origin/main` into `work/maintainer-town`)

The blocker below was the emulator's save-load path never reaching the play
loop.  Root cause was **not** the allocator (the earlier hypothesis) but the
DOS 8.3 name resolver: the engine opens the save as `GAMES\GAME%3d`
(printf `%3d`), i.e. `GAMES/GAME  2` with two padding spaces, and
`Machine::resolve()` compared components with only case folding, so
`game  2` never matched the shipped `GAME2`.  The open failed silently and the
load no-op'd.  Fix `9cde8ea` ("resolve space-padded 8.3 names like DOS") lives
on the maintainer branch and was **not** on `main`; the merge brings it in.

After the merge, loading a save enters the dungeon directly:

```
slot 1 (Temple):  GAMES/GAME1 -> EGA/D5/DP001..DP066 -> DNGS/DNG05
slot 2 (wild):    GAMES/GAME2 -> EGA/D0/DP001..DP066 -> DNGS/DNG10
```

`make test` and `./tests/golden_test.sh` are both green, including the `temple`
and `play` frames.  Oracle frames for the dungeon render already exist in
`ref/oracle/` (`dungeon_slot1.png`, `dungeon_slot1_moved.png`); the emulator's
slot-1 frame now reproduces them.  Workstream A can proceed with the
first-person dungeon render against these.

### Measured status of the slot-1 frame (maintainer, 2026-11-11)

The load is unblocked, and the first-person composition is **essentially
correct** — the remaining gap is palette, not geometry:

- **Shape match 99.6%.**  Thresholding both frames to non-black, the emulator's
  slot-1 frame and `ref/oracle/dungeon_slot1.png` agree on **99.6%** of pixels
  (99.3% inside the 120x119 viewport); luminance correlation is **0.906**.
- **Per-colour counts match within ~2%** in the viewport (black 4876 vs 4982;
  the near-white highlight 5987 vs 6700), and the emulator's light-green
  `#AAFFAA` lands on the oracle's white `#FFFFFF` with **98.6%** overlap.
- So the depth geometry / code→sprite selection is right.  The oracle draws the
  temple in **greys** (EGA indices 7/8/15: `#AAAAAA`/`#555555`/`#FFFFFF`) while
  the emulator draws the same scene in **colour** (olive `#AAAA00`, blue
  `#0055AA`, light-green `#AAFFAA`).  The game programs its palette via INT 10h
  AH=10h/AL=02 (the emulator applies those 16-entry tables), so the divergence is
  in how the EGA plane bits map to attribute indices at render time (a write-mode
  / plane-read question), **not** a missing sprite table.
- The frame also changes with the frame count (58% raw match at 1500 → 66% at
  2500), so the oracle PNG is one moment of a live animation; a flat RGB
  template match is the wrong test.  Compare on shape, not raw RGB.

Bottom line: workstream A's feared "index not yet decoded" gap is smaller than
documented — the composition reproduces to 99.6% on shape.  The tractable next
step is the EGA attribute/palette path, not the sprite table.  Re-record the
oracle with the exact capture sequence before treating 100% RGB as the bar.

The original report is kept below for the record.

## Done

- `DNGS/DNGnn` decode: complete and verified across all 29 files
  (`port/dungeon.{h,cpp}`, `tools/dngs.py`).  36x36 grid; RLE (escape byte +
  `(count,value)` runs); 4-byte header; 1296-byte tile layer then 1296 two-byte
  object records.  Header length re-confirmed empirically (offset 4 maximises
  layer coherence).
- Walkable model: `dungeon_passable` / `dungeon_reachable` / `dungeon_can_step`
  / `dungeon_step` / `dungeon_turn` / `dungeon_place`, plus a top-down
  `render_dungeon_map`.  Wall set `{3,12}` keeps 23/29 levels fully connected
  (mean largest component 0.993).
- Object layer identified as **door/feature placement** (most non-zero features
  sit on wall cells open straight through): DNG01 46/62, DNG17 216/327.
- View directory resolved: it is the **ones digit** of the map id
  (`EGA/D<ones-digit>`).  Double-verified from the emulator's own open trace:
  slot 1 loads `DNGS\DNG05` + `ega\d5\dp001..dp069`; slot 2 loads `DNGS\DNG10` +
  `ega\d0\dp001..dp069`.  Fixed `dungeon_view_path` (was using the whole id, so
  every id >= 10 pointed at a non-existent `EGA/D10/...`).
- Canonical entry view `DP001` pixel-verified at 95.2% against the Temple spawn
  oracle (`ref/oracle/dungeon_slot1.png`); `render_dungeon_default_view()`.
- Full play screen composed: `render_dungeon_screen()` blits the static frame
  `EGA/UP/MSCR` then the view sprite at (10,11) — verified at 0.921 (frame),
  0.952 (viewport) and 0.928 (whole screen) against the same oracle, so the
  screen *structure* is reproduced end to end.  Covered by `test_dungeon_screen`.
  (Not yet reachable from `lof`: `port/lof.cpp` has no dungeon wiring — a
  maintainer-owned file.)
- View-sprite themes pinned: `EGA/D1..D7` are seven themes over one identical
  51-slot set; `EGA/D0` is a distinct 63-slot wilderness set.  Covered by
  `test_view_themes`.
- Sprite library characterised: `LOF.EXE` ships a contiguous 70-entry path table
  `ega\d0\dp000`..`ega\d0\dp069`; `DP000` is absent on disk everywhere, so the
  library is `DP001..DP069`, and the engine **preloads the whole table** then
  composes the viewport in memory (no per-view file open to key the rule off).
- Tests: `tests/dungeon_test.cpp` — 7 real cells, 29-level load, connectivity
  guard, door guard, an end-to-end walk test on DNG05, the theme guard and the
  full-screen composite check.  `make test` green.

## What is blocked

The DoD also asks for a **walkable dungeon map screen that matches the oracle
frame**.  The decode, the walk model and the entry view all work.  The viewport
turns out to have **two** undocumented halves, and reproducing either needs an
oracle that is not available here:

1. **The base-sprite rule.**  Which `DPnnn` slot the engine picks for a given
   (position, facing, local structure).  The library is now well understood —
   `EGA/D1..D7` are seven texture themes over one identical 51-slot set, so the
   rule maps to a *slot index* independent of theme (the contiguous
   `ega\d0\dp000..dp069` table in `LOF.EXE`) — but the mapping itself is
   GFA-compiled code with no relocation entry to key off.
2. **A procedural overlay.**  The Temple spawn is `DP001` at 95.2 %; the other
   4.8 % is a compact grey block that **no** `EGA` image reproduces (searched
   every sprite, every placement, conflict-free: none).  So the engine *draws*
   part of the viewport from the map + party state — a "pick the right `DPnnn`"
   rule alone cannot reproduce a frame.

Producing/validating either half needs an oracle, and none is available here:

- **The analysis emulator cannot be the oracle.**  Its slot-1 viewport is
  entirely black (0% non-black pixels; 90% unlike the menu viewport), i.e. it
  never blits the view sprites.  This is the same class of gap the maintainer
  documented in `docs/notes-maintainer-town.md` ("the emulator does NOT render
  the real play screen").
- **The DOSBox oracle frames are human-captured and not reproducible by script.**
  The maintainer's own recipe (`tools/capture_dosbox.sh ... --keys "o 1"`) and my
  key-driven runs (`space…,o,1`, then arrows) all land on the **main menu**
  (0.0 diff vs `ref/oracle/menu.png`); keys reach the guest (the right-hand panel
  changes) but the scripted path does not reach a dungeon.  I installed a WM
  (openbox) so DOSBox gets focus under Xvfb; still no dungeon.  The existing
  `ref/oracle/dungeon_slot1*.png` were captured interactively.
- **Static derivation is a dead end for this rule.**  It is GFA-compiled code
  whose sprite/path table is addressed via a computed base with no relocation
  entry (Round 9), so there is no table to lift without real disassembly of
  `analysis/lof.asm`.
- The save format (party map id + x/y/facing) is not decoded and is outside
  workstream A's files, so I cannot even confirm a specific (pos, facing) from
  the saves to fit against.

## What would unblock it

1. **One human-captured frame from inside a dungeon** (any map, any
   position/facing) committed to `ref/`, ideally a small set that varies
   (pos, facing, structure).  That alone lets me fit the composition rule and
   verify a drop-in render.
2. The code→sprite lookup/composition routine lifted from `analysis/lof.asm`
   (maintainer lane).
3. Fix the shared emulator's EGA blit path so `src/` renders the viewport
   sprites (A must not edit `src/`), which would make the emulator a usable
   oracle.

Note that because of the procedural half, a *single* oracle frame is not enough
even to fit the base rule — a small set that varies (position, facing,
structure) is needed.

## What the maintainer should do

- Capture 3–5 dungeon frames via DOSBox and drop them in `ref/` (or tell A the
  exact key sequence that enters a dungeon under `capture_dosbox.sh`), **or**
- point A at the routine in `analysis/lof.asm` that reads `DNGS\DNG%01ld%01ld`
  and indexes `ega\Dn\DPnnn`, **or**
- fix the emulator's viewport rendering.
