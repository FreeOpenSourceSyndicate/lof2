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

## Oracle ground truth (important)

The `GAMES/GAMEn` filename is built as `GAMES\GAME%3d` (printf `%3d`), so the
file is `GAMES/GAME  1` (two spaces). The repo only shipped `GAME1`; copying
`GAME1` -> `GAME  1` lets the game actually load a save.

With `LOF_CMDLEN=3` and the menu keys `0x18` ("(O)pen file") then `0x02` ("1"),
the emulator opens `GAMES/GAME  1` and issues, in order:

```
read 11834 bytes   (character / game state)
read     4 bytes
read  1296 bytes   (the map!)
```

11834 + 4 + 1296 = 13134 = the file size, and **1296 = 36*36** — the same size
as the DNGS tile layer.  This independently confirms the 36x36 map geometry.
The save's map region (`data[11838:11838+1296]`) is the *town* map for this
save: mostly `6` with a small `0/2/4/5` structure, so the town map shares the
tile-code space with the dungeons.

After the save loads, the emulator does not read `DNGS/`; the loaded save starts
in town, so reaching a dungeon needs more navigation than the current scripted
input.  The tile-set files `EGA/D0..D7/DPnnn` (63/51 tiles each) are almost
certainly the walls the dungeons are drawn from (D0 is the town set), but that
mapping is unverified.

## Object layer is 1296 *words*, not three 1296-byte planes (proved)

The 2592-byte tail could be read two ways. Test: spatial coherence, i.e. how
often a non-zero cell has a non-zero 4-neighbour. Reading the tail as fixed
1296-byte planes gives `adj/nz ~ 0.12` (near-random scatter); reading it as
1296 little-endian words gives `adj/nz ~ 0.58-0.69` (well-structured). Summed
over all 29 files:

```
word lo-byte  nz= 309  adj= 212  adj/nz=0.686
word hi-byte  nz=3366  adj=1959  adj/nz=0.582
plane A       nz=1905  adj= 227  adj/nz=0.119
plane B       nz=1770  adj= 225  adj/nz=0.127
```

So the word reading is correct. `tests/dungeon_test.cpp::test_object_layout`
pins this down (asserts the hi-byte word view is >4x more coherent than the
first plane view). The hi byte is the feature code, the lo byte a sparse
(1..4) sub-index.

Object placement: every object cell sits on a wall tile `3` (1888 cells), a
border tile `1`/`2` (1122), or floor `6` (192); ~1/3 are on a wall/open
boundary. Consistent with doors / stair-mouths / decorations but not proven.

## Tile-code vocabulary

Full set across the 29 files:
`0,1,2,3,4,5,6,7,8,9,10,11,12,13,20,21,36,37,52,53,68,69,84,85,101,244`
with `0` (10226) and `6` (21740) dominating and `3` (1888) / `12` (1069) the
structural walls. The stride-16 groups `20/21 36/37 52/53 68/69 84/85` look
like wall + orientation variants.

## Progress

- [x] RLE scheme + fixed 3892-byte decode confirmed for all 29 files.
- [x] 36x36 tile grid + 1296 word records implemented (`port/dungeon.cpp`);
      geometry cross-validated by the save-file read size (1296) and the
      word-vs-planes coherence proof.
- [x] `tools/dngs.py` explorer, `tests/dungeon_test.cpp`, wired into `make test`.
- [x] `render_dungeon_map()` top-down renderer; `dungeon_passable()`,
      `dungeon_is_solid_tile()`, `dungeon_reachable()` flood-fill walk model.
- [ ] Tile code -> `EGA/Dn/DPnnn` mapping.  A first-person viewport render is
      almost certainly built from these tiles, but template-matching the `D0`
      tiles against the captured town frame failed (best match ~50% of pixels
      differ = no match), so the viewport origin/geometry or the per-tile
      masking is not what I assumed.
- [ ] Object-code semantics (doors/stairs/items/traps?).
- [ ] Draw one dungeon screen matching the oracle frame (blocked; see below).

## Connectivity sanity check on the wall set

Treating only `{3,12}` as solid, the passable region is **fully connected
(frac = 1.000)** in 24 of 29 levels — exactly what a hand-built maze should look
like. `DNG00` is only 9.6% connected, which fits it being the *town* (separate
buildings/areas); `DNG10`/`DNG17` are 0.97-0.99 (a few sealed pockets). So the
`{3,12}` wall classification is at least self-consistent and does not shred the
levels; a wrong wall set would fragment them far more. Still PROVISIONAL
pending an oracle frame, but this raises confidence.

## Level file naming and dungeon/debug string cluster

The game builds dungeon filenames with its own format string at `LOF.EXE`
offset `0x4bb2c`: `DNGS\DNG%01ld%01ld` — i.e. `DNG<dungeon><level>`, one digit
each. The 29 shipped files confirm it: the first digit runs 0..6 (7 dungeons)
and the second 0..7, e.g. `DNG00` (dungeon 0 level 0) .. `DNG66`. Dungeon 6 has
only one level. `port/dungeon.{h,cpp}` now exposes
`dungeon_file_name(dungeon, level)` and `tests/dungeon_test.cpp` round-trips
all 29 real files through it.

Adjacent to that format string is a small debug cluster (all still
unreachable, kept for the record):

- `Enter GFX-number?` (0x424f0) — likely the "dump any EGA image" debugger.
- `Enter dungeon-number?` / `Enter level-number?` (0x425d3 / 0x425e9) — would
  set the level directly; the pair maps onto the two `%01ld` fields above.
- `Enter room-number?` / `Akt. room: %d` (0x42548 / 0x4253a) — room selection
  once inside a dungeon.
- `Debugmode activ!` / `Debugmode inactiv!` (0x42511 / 0x42522) — the toggle.

No key or command-tail switch reaches this cluster (see the sweep above), so it
does not unblock the oracle frame, but it confirms the intended internal model:
a dungeon is identified by (dungeon, level) and rendered room by room.

## Dungeon graphics are layered, first-person, pre-scaled tiles

`EGA/D0/DP001..DP066` (63 files) are the dungeon wall/floor/ceiling images and
they are **not** flat 16x16 tiles. Decoded sizes come in discrete "depth" steps:

| codes (D0) | size    | role                                  |
|------------|---------|---------------------------------------|
| 001, 002   | 120x119 | full-viewport wall (1 step ahead)     |
| 003        | 64x59   | wall two steps ahead                  |
| 004        | 32x29   | wall far ahead                        |
| 006, 007   | 32x59   | left / right wall                     |
| 008..011   | 16x29   | side wall, mid depth                  |
| 012, 013   | 120x11  | ceiling / floor strip                 |
| 024, 025   | 32x119  | tall side wall                        |
| 062..066   | 120x59  | near wall, upper/lower                |

The viewport is 120x119 (matching the town viewport), and `DP001` at 120x119
fills it. This is the classic **first-person, per-depth sprite** layout: the
engine picks a differently pre-scaled bitmap for each depth band instead of
scaling at run time. That is exactly why template-matching the tiles against a
flat viewport failed earlier — there is no single tile size to match.

The asset-name table (near file offset `0x3b3e8`) lists them as
`ega\d0\dp000` .. `ega\d0\dp069`, followed by `ega\anim\anNN` frames. `D1`..`D7`
are **8 different dungeon tilesets** (each 51 files, same sizes starting at
`DP001`/`DP014`/`DP015`), selected per level — consistent with the German
`dungeon` level table. Which set a level uses, and the map-tile-code -> `DPnnn`
index, are still unverified (needs the oracle frame).

## Save file also carries a 36x36 map region

The oracle reads 11834 + 4 + 1296 bytes from `GAMES/GAME  1` (13134 bytes
total). The trailing 1296 bytes look exactly like a map tile layer (town is
mostly code 6 = floor, with a few 0/2/4/5/8) and the 4 bytes before it are
`f5 5c 00 00`. BUT: rewriting that 1296-byte region to all-`3` (walls) and
re-running the oracle produces a **pixel-identical** frame, so this cached
region is not the town's displayed map. It may be a scratch/automap buffer or a
different level. Either way it does not give me a dungeon frame.

## Debug mode (a possible unblocking hook)

`LOF.EXE` contains debug strings `"Debugmode activ!"` / `"Debugmode inactiv!"`
followed by `"Enter dungeon-number?"`, `"Enter level-number?"`,
`"Enter room-number?"`, `"Akt. room: %d"`. If the maintainer can trigger debug
mode (a key sequence, or a `LOF_*` hook in the emulator) the game can be sent
straight to a dungeon, which would produce the oracle frame this workstream
needs. This is the single most useful thing to enable.

## What is blocking, precisely

To finish the "matches the oracle frame" definition of done I need **a real
dungeon frame from the oracle**, which needs the emulator to be steered from
town into a dungeon. The scripted input does not achieve it: after loading a
save the game sits in town and reads no `DNGS/` file. Options, in order of
value:

1. **Maintainer:** a scripted-input recipe (or a debug key/flag) that walks the
   loaded save into a dungeon, so `DNGS/DNGnn` is read and a frame can be
   dumped. That unblocks the tile mapping directly.
2. **Maintainer:** point me at (or annotate) the draw routine for the
   first-person view in `analysis/lof.asm`, i.e. where the tile index into
   `EGA/Dn/DPnnn` is computed from the map — then I can derive the mapping
   statically without needing a frame.
3. Otherwise the static decode + walk model is as far as A can go.

### Exhaustive key sweep (done — no dungeon reachable)

With the save loaded (town), I injected every plausible key after `O`: all
letter/number hot-keys, both Enter scancodes (0x1C/0x28), arrows (0x48/0x4B/
0x4D/0x50), numpad (0x47..0x53), Ctrl+F1..F10, Esc (0x01), Tab (0x0F), Space
(0x39), backspace, `+`/`-`. None changed the viewport or opened any `DNGS/DNG*`
or `EGA/Dn/DP*` file. Paging the Count-of-Cyldane intro with Continue (0x2E)
does not start a walkable state either. So the town->dungeon transition needs
state I cannot reach by keypress alone from this save — confirming the ask
above. (`O` = Open file is confirmed working: it loads `GAMES/GAME  1`.)

### One concrete maintainer hook that would work

The dungeon entry point is a function that loads the level, loads
`EGA/Dn/DPnnn` for its tileset, and calls the first-person draw. A single
`LOF_DUNGEON=<n>` hook in `src/main.cpp` that, after the LOF loader has run,
writes the dungeon number/level to the fields the debug path reads and calls
that function would produce the frame directly — no key sequence needed. I
cannot add it because `src/` is maintainer-owned.
