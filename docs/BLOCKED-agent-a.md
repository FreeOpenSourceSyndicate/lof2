# BLOCKED — workstream A (dungeons): first-person viewport needs an oracle frame

Owner: Agent A.  Branch: `work/agent-a`.  Status: **decode + walkable model done
and green; the first-person viewport composition is the one remaining DoD item
and is blocked on an oracle that cannot be produced headlessly.**

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
- Sprite library characterised: `LOF.EXE` ships a contiguous 70-entry path table
  `ega\d0\dp000`..`ega\d0\dp069`; `DP000` is absent on disk everywhere, so the
  library is `DP001..DP069`, and the engine **preloads the whole table** then
  composes the viewport in memory (no per-view file open to key the rule off).
- Tests: `tests/dungeon_test.cpp` — 7 real cells, 29-level load, connectivity
  guard, door guard, and an end-to-end walk test on DNG05.  `make test` green.

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
