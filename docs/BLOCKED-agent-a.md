# BLOCKED — workstream A (dungeons): first-person render needs an oracle frame

Owner: Agent A.  Branch: `work/agent-a`.  Status: **decode + walkable model done
and green; the first-person render is blocked on an oracle frame.**

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
