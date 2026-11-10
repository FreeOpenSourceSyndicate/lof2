# BLOCKED — workstream A (dungeons): first-person render needs an oracle frame

Owner: Agent A.  Branch: `work/agent-a`.  Status: **UNBLOCKED — the save-load
crash is fixed on the merged branch and the emulator now enters the dungeon.**

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

## What is done

- `DNGS/DNGnn` decode: complete and verified across all 29 files
  (`port/dungeon.{h,cpp}`, `tools/dngs.py`).  36x36 grid; RLE (escape byte +
  `(count,value)` runs); header `h0 h1 field_lo field_hi`; tile layer then 1296
  two-byte object records.
- Walkable model: `dungeon_passable` / `dungeon_reachable` / `dungeon_can_step`
  / `dungeon_step` / `dungeon_turn` / `dungeon_place`, and a top-down
  `render_dungeon_map`.  Wall set `{3,12}` keeps 23/29 levels fully connected
  (mean largest component 0.993).
- Object layer identified as **door/feature placement** (most non-zero features
  sit on wall cells open straight through): DNG01 46/62, DNG17 216/327.
- Tests: `tests/dungeon_test.cpp` (7 real cells, 29-level load, connectivity
  guard, door guard).  `make test` green; `make check` HEALTHY.

## What is blocked

The workstream's DoD also asks for a **walkable dungeon map screen that matches
the oracle frame**.  Producing that frame requires the game to actually enter a
dungeon under the emulator, which it never does here:

- The maintainer's `play` golden crashes — jump to `0000:0000` at
  instr≈1.07e7, right after save load — for every game dir / `LOF_HOUR` / key
  timing and across all maintainer commits (`ab42e4a`, `75dc673`, `5a6c7ea`).
  The other 6/7 goldens match exactly, so it is data-independent.
- The user's slot-1 save ("The Temple") and slot 2 never read a `DNGS/` dungeon;
  the read list stays `ROST/ROST ROST/SHOP ega\twn\twn00..05 ega\up\*`.
- A long brute-force key feed (`O`,`1` then `C`/arrows/Enter/letters, 4000
  frames, `LOF_I21=1`) and the fresh-game path (`A`,`A`,`C`,`C`,`1`) never reach
  a dungeon either.
- `LOF_PLANT_LOFX=1`, `--vor`, and hour/key-gap sweeps do not change the crash.
- Static: the `UCFRDLMPO` legend (ds 0xBB2C) is referenced once (`mov ax,0xbb2b`
  at file 0x76D1); the debug cluster (`Debugmode activ!`, `Enter
  dungeon-number?`) has no key/tail trigger found.  The GFA-compiled body needs
  real disassembly work (in `analysis/lof.asm`) to locate the code→sprite table.

## What would unblock it

1. A captured dungeon frame from real hardware / DOSBox (the `tools/capture_dosbox.sh`
   path), with the save/menu sequence that reaches a dungeon, **or**
2. A working emulator save-load (fix the `0000:0000` jump), **or**
3. The code→sprite lookup table extracted from `analysis/lof.asm` **plus** one
   reference frame to confirm the depth geometry.

## What the maintainer should do

- Fix or route around the save-load `0000:0000` jump so `--dir game` can enter a
  dungeon (the crash is in the shared `src/` emulator; A must not edit it).
- Or capture a dungeon frame via `tools/capture_dosbox.sh` and drop it in `ref/`.
- Or point A at the routine in `analysis/lof.asm` that reads `DNGS\DNG%01ld%01ld`
  and indexes `ega\Dn\DPnnn`, so the code→sprite table can be lifted.
