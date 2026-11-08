# notes-a.md — Agent A (dungeons)

CLAIM: Workstream A — decode `DNGS/DNGnn` (29 files) and render a walkable map.
Files owned: `port/dungeon.{h,cpp}`, `tools/dngs.py`, `tests/dungeon_test.cpp`,
`docs/notes-a.md`.

## Format (decoded, verified across all 29 files)

```
off 0    : escape byte (RLE marker; 0xFF for most files, 0xFE for DNG01, ...)
off 1..  : RLE stream — literal bytes copy through; escape introduces a
           (count, value) pair emitting `value` repeated `count` times.
```

Decoding from offset 1 yields **exactly 3892 bytes for every file**:

```
0    .. 3    : header (4 bytes)
4    .. 1299 : tile map, 36x36 = 1296 bytes, row-major
1300 .. 3891 : object/feature layer, 1296 records x 2 bytes, row-major
```

* Tile map: values 0,2,3,6,7,8,12 (6 = floor/empty for most levels). Border
  consistency (top/bottom rows + left/right cols constant) holds at width 36
  for 27/29 files, confirming the 36-wide grid.
* Object layer: little-endian words. Low byte is 0 for almost every cell; the
  high byte carries a small code that overlaps the tile value set (0x06,0x07,
  0x35,0x57,...), so it reads as a per-cell feature/decoration code.  Note the
  code is stored in the *high* byte (i.e. raw byte order is (0, code)); this is
  NOT a little-endian little-value — keep the split `obj_code`/`obj_low`.
* Header: h0 = 0 for all but DNG13 (4). h1 ∈ {0,1,2,3}. h2|h3 is a 16-bit
  little-endian word; it is **not** a decompressed size (that is always 3892)
  and its meaning is still open. Stored verbatim as `field`.

Known reference cells (asserted in tests/dungeon_test.cpp):
  DNG00 h1=3 field=52738  tile(0,0)=12 tile(35,35)=12 tile(17,17)=0
  DNG01 h1=1 field=65280  tile(0,0)=6  tile(17,17)=0
  DNG26 h1=3 field=17156  tile(0,0)=6  tile(5,6)=6

## Progress

- [x] RLE scheme + fixed 3892-byte decode confirmed for all 29 files.
- [x] 36x36 tile grid + 2-byte object records implemented (`port/dungeon.cpp`).
- [x] `tools/dngs.py` explorer, `tests/dungeon_test.cpp`, wired into `make test`.
- [ ] Identify tile code -> EGA tile (`EGA/D0..D7/DPnnn`, sizes 16x19, 16x29,
      32x29, ...; DP001 is a 120x119 full-screen background).  These look like a
      first-person view tileset, not a top-down map; needs oracle ground truth.
- [ ] Object-code semantics (doors/stairs/items/traps?).
- [ ] Draw one dungeon screen matching the oracle frame.

## Oracle notes

Driving the menu to a playable state is still open. A baseline run with
`LOF_CMDLEN=3` reads, in order: DSKTAB.DFS, disk0.id, SYST/SNSC, EGA/UP/* (menu
art + font), EGA/TWN/TWN00..05 (town), ROST/ROST, ROST/SHOP. No `DNGS/` read yet;
reaching a dungeon needs a loaded game. `GAMES/GAME1..GAME5` (13134 bytes) are
save files; the tail (>=11838) is a `06`-filled block, possibly an in-save map.
