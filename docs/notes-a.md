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

Counted across all 29 files, `tile & 15` (the low nibble) and `tile >> 4` split
cleanly, confirming the `variant*16 + shape` bitfield:

- shapes (low nibble): `0` x10226, `1` x398, `2` x727, `3` x1888, `4` x29,
  `5` x28, `6` x21740, `7` x632, `8` x707, `9` x9, `10` x11, `11` x2,
  `12` x1069, `13` x118.
- variants (high nibble): `0` x37537, `1` x14, `2` x9, `3` x8, `4` x4, `5` x6,
  `6` x4, `15` x2. So only ~47 cells in the whole game carry a *variant*
  (non-zero high nibble); the run-of-the-mill cells are variant 0 with shape
  `0/6` (floor) or `3/12` (wall).

Object layer: read as 1296 words both bytes are meaningful *per cell*, not a
single word: the low byte is non-zero in only ~309 cells (a sparse "sub-code"
that is `1..4`) while the high byte is the object/feature code (`1..245`, 206
distinct values) and is spatially coherent on its own. `port/dungeon.h` exposes
`obj_low` (low byte) and `obj_code` (high byte).

Per dungeon, the *distinct* tile codes are few (9..16) and share a common core
`{0,1,2,3,6,7,8,13}` plus a couple of "special" codes that are always
`variant*16 + 4` or `variant*16 + 5` and differ from dungeon to dungeon:

| dungeon | extra/special codes |
|---------|---------------------|
| 0 | 20 21 85 244 |
| 1 | 4 5 36 37 |
| 2 | 20 21 52 53 |
| 3 | 36 37 68 69 |
| 4 | 52 53 85 244 |
| 5 | 4 68 101 |
| 6 | 84 |

So the special code is an authored per-dungeon value (a feature type such as a
stair/portal), not derivable from the tileset index — another reason the
code -> sprite mapping needs the oracle frame rather than pure static analysis.

The 8 tilesets `EGA/D0..D7` are the sprite pools (D0 has 63 sprites, D1..D7 have
51); the number of sprites per set far exceeds the ~9..16 distinct codes a level
uses, consistent with one code expanding to several pre-scaled depth sprites.

## Tileset atlas tool

`python3 tools/dngs.py atlas game/EGA D0 out.png [cols]` writes a grayscale
contact sheet of one tileset (`EGA/Dn/DPnnn`), each sprite padded into a grid
whose cell is the largest sprite in the set, and prints the code list in cell
order. This is the artifact for the *next* step: once the oracle dungeon frame
exists, read a tile code off the map and find its sprite by eye in the sheet
(the sprites are pre-scaled first-person views, so sizes vary per cell).

## The `ega\dN\dpNNN` string table (code -> sprite is a lookup, not identity)

Static scan of `LOF.EXE` (all printable runs >= 4) yields the game's own path
string table.  The relevant entries:

- `DNGS\DNG%01ld%01ld` at file `0x4bb33` (`ds`-relative `0xbb33`), glued to a
  cryptic legend `UCFRDLMPO` (file `0x4bb2c`).  Both are referenced from image
  `0x76d1`/`0x76d3` via `mov ax,0xbb2b`-style loads — a single load covers the
  legend byte *and* the first path character, so `UCFRDLMPO` is very likely a
  per-letter orientation/shape legend used to select the sprite for a code
  (e.g. `U/C/F/R/D/L/M/P/O` = the sprite to draw when that code is seen from
  that facing).  Not yet decoded.
- `ega\d0\dp000` .. `ega\d0\dp069` — **70 strings, one tileset (`d0`), indices
  `000..069`**, all present in `game/EGA/D0/` except `DP000` (the file starts at
  `DP001`, so index 0 has no art and is presumably "empty/floor").
- Other `ega\...` dirs referenced: `in`, `up`, `twn`, `anim`, `char0`, `page`.

The decoded tile codes (`0..13`, plus `variant*16+4/5` up to `244`) do **not**
match the sprite indices `0..69`: code 6 (floor) and code 3/12 (wall) have no
`DP006`/`DP003` counterpart by identity.  So the mapping from a map tile code to
a `dpNNN` sprite is a **lookup table inside `LOF.EXE`**, not the code itself.
(The `D0` file list has gaps — `058..060`, `067..069` missing, so it ships
`001..057` + `061..066`.  `D1`..`D7` instead ship `001`, `014..060`,
`067..069` — missing `002..013` and `061..066` — so the tilesets do not even
share one index set; `D0` is the odd one out with a `dp000` string too.)

Each dungeon has its own tileset (`D1`..`D7` are byte-different from `D0` and
from each other for the same index), so a code resolves to art *per dungeon*.

### Sprite structure (the perspective ladder)

The sprites are a **pre-scaled perspective ladder**: for a wall/feature at
distance `k` the game blits a sprite of a fixed size, and the set for one logical
tile is the sizes across `k`:

| size (w x h) | meaning |
|--------------|---------|
| 120x119 | k=1, full-face (closest) |
| 64x59   | k=2 |
| 32x29   | k=3 |
| 16x29 / 16x59 | side faces at k=3..4 |
| 8x29 / 8x28 | far side faces |
| 120x11  | floor/ceiling band |
| 32x119  | tall door frame |
| 120x59  | wide wall band |

`D0`'s indices group cleanly: `1..13` = a first family, `14..25` a second,
`26..35` a third, `36..47` a fourth, `48..57` a fifth, `61..66` wide bands.
So the viewport render is: for each of the up-to-4 map rows in front of the
party, pick the sprite for `(code, distance)` and blit it.  This is exactly why
template-matching one `D0` tile against a full frame failed — the frame is a
*composition* of several such sprites, not one tile.

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
      differ = no match).  **Caveat:** that test was uninformative — the captured
      frame was the *town*, which is drawn from `EGA/twn/twn00..05`, not from
      `D0`, so a mismatch was expected and does not falsify the viewport idea.
      **See "The `ega\dN\dpNNN` string table"
      below**: the map tile codes are *not* sprite indices, so a logical-tile ->
      sprite lookup table exists in `LOF.EXE` and must be found before the
      viewport can be drawn.
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

### Additional sweeps (all negative) — round 2

After the first sweep I tried more inputs, none of which reaches a dungeon:

- **Arrow-key walk test.** From the town, 25x `Up` (0x48) with 30-tick spacing:
  the town viewport never re-reads `EGA/TWN/TWN00..05` and never touches
  `DNGS/`. So the town panel is not walked with the arrows on this build/save.
- **Modifier combos** on the town: Alt+F1, Ctrl+F1, Alt+X, Alt+D,
  Ctrl+Alt+Del, Alt+Tab, Shift+F10, Ctrl+C, Alt+Enter, Ctrl+Shift+F12 — all
  pixel-identical to the plain town frame, i.e. no debug toggle.
- **Boot-time single keys** (letters, digits, F1..F10, Esc, Tab, Space, Enter):
  the only frames that change are the ones whose key is a documented menu
  hot-key or advances the credits; none opens debug mode.
- **Command tail**: `START.BAT` forwards `%1..%6` to `lof`, but the emulator's
  `LOF_CMDLEN` only writes a blank tail; no arg string is parsed as a switch by
  the game (its only tail check is the "started from START.BAT" gate).

### Static GFA check (negative)

`LOF.EXE` is a GFA-BASIC-compiled program, so a *tokenised* BASIC body would be
the way to read the map->tile-index computation statically. It is not present
unpacked: no `GFA`/`BASIC` signature and no plaintext of the debug strings'
*code* (only the string pool), so there is nothing tokenised to walk offline.
Static tracing remains a dead end for the tile mapping.

### Round 3 — town oracle reproduced, exit still blocked (using the maintainer's hooks)

The maintainer's `origin/work/maintainer-town` branch (not yet on `main`) adds
`LOF_KEYAT`/`LOF_KEYSTEP`/`LOF_KEYONCE`/`LOF_HOUR` to the emulator and documents
the reachable in-game town.  I built that branch in a throwaway worktree
(read-only; no repo files touched) and reproduced its town oracle exactly:

* `LOF_CMDLEN=3 LOF_KEYAT="10:0x18,60:0x02"` (O then 1) reaches the town; the
  frame matches `ref/town_oracle.png` to 32/256200 bytes (the differing pixels
  are the blinking cursor), FNV `0e5abc1d`.
* The golden-test recipe `--keys 0x18,0x1c` (O then Enter) reproduces the
  recorded `town` hash `8dcc44ac` exactly.

Leaving town is still blocked from my side, same as the maintainer's notes: with
`C` (0x2e) presses at every plausible frame (single, repeated, `LOF_KEYAT`-pinned
after the sub-menu), plus `1`/Enter follow-ups, the emulator **never reads
`DNGS/DNGnn` nor any `EGA/Dn` file** — the read list stays `ROST/ROST ROST/SHOP
ega\twn\twn00..05 ega\up\*`.  So the town->wilderness transition is not reachable
with the current keys, and there is still no oracle dungeon frame.  This is
purely a capture blocker; the decode and the code->sprite *structure* (below) are
independent of it.

### Round 4 — the maintainer's "play" golden does not reproduce (blocker report)

Per the "if loading the file doesn't work, check whether the maintainer made
progress on unblocking" instruction I re-checked the maintainer's branch
(`origin/work/maintainer-town`, tip `5a6c7ea`).  Their new golden-test `play`
scenario (the only oracle path they document) **does not reproduce** in this
workspace, from a clean worktree at their exact tip:

* Recipe: `LOF_EDUMP=<f> LOF_CMDLEN=3 LOF_NO_TIMER=1 LOF_KEYAUTO=0x18,0x03
  LOF_KEYGAP=8 ./lof-native --dir game --headless --frames 1000 --shot /dev/null`.
  Expected (their `tests/golden.txt`): `c46f4fe9be490a1b77560a1e8136441e`.
  Observed: `b43512c10b9b22396b6b0dcdecf7a148` — for **every** game dir tried
  (workspace, pristine extraction, slot-2-only) and every commit
  (`ab42e4a`, `75dc673`, `5a6c7ea`).  The other six goldens (boot/look/open/add/
  dism/erase) all match exactly, so my build and game data are sound.
* Cause: after the save load the CPU jumps to `0000:0000` (the known
  "zeroed return address" class) at a fixed `instr=10670017`, `frames=36`,
  before the play loop starts.  `LOF_CRASHDBG=1` shows `ss:sp=4C92:B56A` with the
  stack full of zeros.  The last INT 21h activity is the save read
  (`GAMES\GAME  2`: 11834 + 4 + 1296 bytes); no `DNGS`/`EGA/Dn` file is opened.
* The user's slot-1 save (`GAME1`, "The Temple") takes the same path: it is read
  in full (`GAMES\GAME  1`: 11834 + 4 + 1296) then crashes identically.  It does
  **not** read a DNGS dungeon, and its own 1296-byte region @11838 does not match
  any `DNGS/DNGnn` tile layer (best 1201/1296 = floor-dominated, so it is not the
  dungeon map either).  So neither save yields a dungeon oracle frame.

### Data-hygiene bug found and fixed (do not re-break)

`game/ROST/ROST` had drifted to 6625 bytes (shipped/pristine = 4555).  With the
corrupted file, workstream B's `char_test` fails (`roster.size() == 11`); with
the pristine file `make test` passes.  I restored `game/ROST/ROST` from
`downloads/ext_msdos/legfaer/ROST/ROST`.  If a run ever mutates it, restore from
the extraction — the engine only ever reads it.

### Net state of workstream A

Decode + walkable model are done and green (`dungeon_test`: 7 real cells, 29
levels).  The DoD's remaining half — a first-person dungeon *render* — is blocked
on an oracle frame, and the only two candidate oracles (maintainer's outdoor map,
user's slot-1 save) are both unreachable here.  No further A work is possible
without an unblock; stopping per the instruction.

### Round 5 — object layer = doorways (oracle-free evidence), sprite ladder

Still no oracle, so I hardened the parts of the format that can be settled
without one.

**The object/feature layer is door/feature placement, not a second map.**  For
each non-zero feature cell, ask whether it sits on a solid (`{3,12}`) wall cell
that is *open straight through* (both neighbours on the horizontal OR vertical
axis are passable) — the classic doorway signature:

| level | feature cells | door-like | wall-stub | on-floor |
|-------|---------------|-----------|-----------|----------|
| DNG01 | 62            | 46        | 0         | 16       |
| DNG17 | 327           | 216       | 60        | 51       |
| DNG00 | 100           | 9         | 69        | 22       |

DNG01 (corridor level) is 46/62 doorways; DNG17 (room level) 216/327.  DNG00 is
the outlier (9 doors, 69 stubs) — consistent with it being the *town*/special
map rather than a plain dungeon.  `tests/dungeon_test.cpp::test_object_doors`
asserts `door*2 > feat` for DNG01/DNG17.  `tools/dngs.py doors` prints the split.

**Connectivity guard across all 29 levels.**  With the `{3,12}` wall set, 23/29
levels are one connected region and the mean largest-component fraction is
0.993.  `test_connectivity_stats` asserts `>= levels-6` fully connected and mean
largest `>= 0.98`; a wrong `dungeon_is_solid_tile()` would shear the levels and
fail it.  (I swept alternative solid sets — adding 1/2/13 changes nothing
meaningful; `{3,12}` stays the right minimal set.  Treating 6 as solid instead,
i.e. reading the grid inverted, fragments 8/29, so 6 = floor is confirmed.)

**Sprite ladder tabulated.**  `EGA/D0` (63 sprites) sizes are a clean
perspective ladder: `120x119` (k=1 full face), `64x59` (k=2), `32x29` (k=3),
`24x19`/`16x29`/`16x19` (side faces), `8x29` (far), plus `120x11` floor/ceiling
bands, `32x119` door frames and `120x59` wide bands.  The indices group into five
families (`1..13, 14..25, 36..47, 48..57`, wide bands `61..66`), so one *logical*
tile resolves to several depth-scaled sprites.  This is the geometry a viewport
render will need; it still needs the code→family table (inside `LOF.EXE`, not yet
located) plus an oracle frame to confirm.

### Round 6 — deeper oracle attempts (all negative)

Trying to unblock the frame without `src/` edits:

- Long brute-force key feed on the maintainer build (O,1 then C/arrows/Enter/
  letters, 4000 frames, `LOF_I21=1`): **no `DNGS/` or `EGA/Dn` read ever
  occurs.**  The crash is before any dungeon code runs, so more keys cannot help.
- `LOF_PLANT_LOFX=1`, `--vor`, `LOF_HOUR` sweep, key-gap sweep: all identical
  crash (`b43512c1…`).  Data-independent.
- Static: the `UCFRDLMPO` legend (ds 0xBB2C) is referenced exactly once, at file
  `0x76D1` (`mov ax,0xbb2b`), inside a loader wrapper — no adjacent code→sprite
  table is visible without disassembling the (GFA-compiled) body, and the
  debug cluster (`Debugmode activ!` etc.) has no key/tail trigger.
- The maintainer's branch `work/oracle-input-golden` (54cd84e) only *fixed the
  input finding* (menu is keyboard-driven; L/E/A/D/O open screens) and added the
  golden test.  No save-load fix.  `work/maintainer-town` (5a6c7ea) just guards
  the wilderness `play` screen.  So directive (1) resolves to: the maintainer has
  **not** fixed save loading → stop the oracle hunt.

### Round 7 — render layer guard

`render_dungeon_map` already distinguishes floor / wall / door (colours 8/7/4/1
for floor/wall, 14 for a feature cell).  Added `test_render_layers`: render one
floor, one wall (`3`) and one door feature (`0x0100`) cell and assert the three
sampled centres carry three distinct colours, wall = 4 and door = 14.  This pins
the walkable-map rendering the DoD asks for (the first-person *viewport* still
needs the oracle).  Filed `docs/BLOCKED-agent-a.md`.

### Round 8 — save-load fix confirmed + first-person viewport

The maintainer's fix landed as PR #50 (`work/maintainer-town`): **REP CMPS/SCAS
must leave the remaining count in `CX`**, not force it to 0.  The MS-C run-time
`strlen` (`repne scasb; not cx; dec cx`) then returns the real length, so the
`%2d%%` width no longer overruns a 4-byte stack local and the slot-1 `RETF` no
longer jumps to `0000:0000`.  Built the PR head and ran both goldens — `temple`
(`a3f719ed…`) and `play` (`585da631…`) match exactly, no crash.  **Save loading
works.**

That PR also ships the oracle: `ref/oracle/dungeon_slot1.png` and
`dungeon_slot1_moved.png`, real DOSBox captures of the slot-1 Temple dungeon
(loaded with `O`,`1`).  Slot 1 is **dungeon 5** (the engine reads `DNGS/DNG05`
and the whole `EGA/D5/DPnnn` set).

**The 3D window is on the LEFT at (10,11) size 120x119** (correcting an earlier
wrong guess of x139..319 — that grey rectangle is the party panel).  The engine
draws this window by **blitting pre-rendered sprites** `EGA/D5/DPnnn`; it does
not ray-cast.  Evidence: `EGA/D5/DP001` blitted at (10,11) reproduces the spawn
window at **95.2%** pixel match (residue is a thin horizon band).  The oracle's
viewport histograms are {0:0.35, 7:0.29, 8:0.28, 15:0.08} at spawn and
{0:0.65, 2:0.11, 10:0.08, …} after a move.

The sprite set is a per-dungeon **view library**, not one image per cell.  For
D5 (51 files) the sizes are 120x119 (7 full views: DP001/014/036/058/059/060/067),
64x59, 32x119, 32x59, 32x29, 24x19, 16x59, 16x48, 16x47, 16x29, 16x26, 16x19,
8x29 — i.e. full views plus left/right wall pieces at several distances, which
the engine composites for corridor and junction views.  D0 (63 files) has the
same block structure plus `120x11` horizon strips (DP012/013).

Implemented:

* `render_dungeon_view()` (`port/dungeon.h/.cpp`) — the engine's real model:
  `load_ega_image` + `blit_image` of a named `DPnnn` at (10,11).  Pinned by
  `tests/dungeon_test.cpp::test_view_sprite`, which blits `EGA/D5/DP001` and
  asserts every pixel equals the decoded sprite (and that a missing file is
  reported, not drawn).
* `dungeon_view_path(dir, id, i)` / `dungeon_view_dir(id)` /
  `render_dungeon_default_view(dir, id)` — `<dir>/EGA/D<ones-digit>/DP%03d` and
  the canonical entry view (DP001).  `test_default_views` confirms the canonical
  DP001 blits for a spread of map ids (see Round 10).
* `render_dungeon_viewport()` — a synthetic ray-cast **fallback** (60-degree FOV
  DDA over the 36x36 grid, grey wall band on black) for levels with no view
  sprite / as an automap aid.  `test_viewport` pins: wall-ahead → whole window
  grey; open → mostly black with some wall; nearer wall covers more.
* `tools/dngs.py viewport <file> [x y facing]` prints the fallback view offline.

**Still open:** the rule that maps (dungeon, party position, facing, tile
structure) → `DPnnn` index.  A single sprite explains the spawn view but not the
moved view (best single-sprite match there is only 0.64), so the engine
composites several pieces; pinning the index needs the selection code or more
oracle frames.

### Round 9 — more oracle frames (DOSBox is available)

`dosbox 0.74-3` + `xvfb` + `xdotool` install cleanly (`apt-get install dosbox
xvfb xdotool imagemagick`), so we can generate our own oracle frames instead of
relying on PR #50's two stills.  Recipe: `tools/capture_dosbox.sh`'s approach,
but take **DOSBox's own screenshots** (`Ctrl+F5` -> exact 320x200 PNG in the
configured `captures/` dir) rather than an X11 window grab (which is scaled and
unreliable).  The dungeon-entry key sequence is `space` (title) ×2, then at the
inn `o` (Open file), `1` (slot 1) — matches PR #50's captures.

**The viewport is composited from several sprites, confirmed:** neither oracle
frame is a single `DPnnn` — spawn matches `DP001` 95.2% (a right-side wall block
differs, no single overlay sprite explains it) and the moved frame's best single
sprite is only 57.6%.  So the engine blits a base view then overlays wall pieces
by the local map structure.  Fully reproducing arbitrary views needs either the
(GFA-compiled) selection code or a systematic oracle sweep; both are out of scope
for this pass.  The drop-in draws the verified `DP001` entry view.

### Round 10 — the view directory is the map id's ones digit (verified)

The maintainer's `docs/notes-maintainer-town.md` (branch `work/maintainer-town`)
pins the model, and I reproduced it against the shipped data:

* The engine composes the map name as `DNGS\DNG%01ld%01ld` (two digit bytes: the
  ones digit at save offset 0x9a2, the tens digit at 0x9a4) and reads the view
  tiles from **`EGA\D<ones-digit>`**.  Verified by patching the bytes and
  watching the emulator's open trace: GAME2's 0x9a2 `0 -> 1` opens `DNGS\DNG11`
  + `ega\d1`; its 0x9a4 `1 -> 0` opens `DNGS\DNG00` + `ega\d0`.
* So **dungeon 05 (Temple) -> `EGA/D5`** and **dungeon 10 (wilderness) ->
  `EGA/D0`**.  `D8`/`D9` are never used; all 29 maps map onto `D0..D7`.
* View dirs `D1..D7` each ship the same 51-sprite set (full views
  `DP001/014/036/058/059/060/067` at 120x119, plus the 16x29 / 32x59 / … partial
  tiles); `D0` is the odd one out with 63 sprites and only four full views
  (`DP001/002/014/036`) — the wilderness.
* The engine opens `DP001` first on a play-loop load (it is the base the viewport
  is composited over), then overlays map tiles near the horizon.  A single
  sprite therefore never reproduces an arbitrary view (spawn = `DP001` 95.2%;
  the moved frame's best single sprite is 57.6%).

`dungeon_view_path()` / `dungeon_view_dir()` now take the **map id** and use its
ones digit, so `dungeon_view_path(root, 10)` -> `EGA/D0/DP001` and
`dungeon_view_path(root, 11, 36)` -> `EGA/D1/DP036` (pinned by `test_view_sprite`;
`test_default_views` blits the canonical `DP001` for a spread of ids including
`10`, `11`, `17`, `23`, `66`).  This corrects the earlier bug where the path used
the whole number (`EGA/D10/...`), which would fail for every map id >= 10.

**Still open:** which `DPnnn` index/offsets the engine picks for a given
(position, facing, local tile structure) — it is a runtime composition of the
base view with partial tiles, and the exact rule needs either the (GFA-compiled)
code or a systematic DOSBox oracle sweep.

### Round 11 — the sprite library and why the rule needs the code

Pulled the engine's own load trace from the analysis emulator on
`work/maintainer-town` (the branch with the `REP CMPS/SCAS` fix), which reaches
the play loop on **save slot 2** (`LOF_KEYAUTO="0x18,0x03"`):

* Slot 2 loads `DNGS\DNG10` and opens **`ega\d0\dp001..dp069`** — the map id's
  ones digit again (10 -> `D0`), independently confirming the Round-10 fix.
* Slot 1 loads `DNGS\DNG05` and opens `ega\d5\dp001..dp069`.
* So the engine **preloads the whole `DPnnn` library** (69 slots per map) *before*
  it reads the map, then composes the viewport in memory.  It never re-opens a
  sprite per move, which is why there is no per-view open to key the rule off.

Library structure (all `D1..D7` share it; `D0` has fewer full views):

* **Full views (120x119):** `DP001/014/036/058/059/060/067` for `D1..D7`;
  `D0` has only `DP001/002/014/036`.
* **Partial tiles:** distance-banded wall segments in size pairs
  (8x29, 16x19/26/29/47/48/59, 24x19, 32x29/59/119, 64x59) — e.g. `DP015/037`
  are the 64x59 pair, `DP018/019` the 32x59 pair.  `DP068/069` (32x119) are
  fully transparent (all index 0): empty side-wall slots.

What the two slot-1 DOSBox oracles show:

* **spawn** viewport matches the single full view `DP001` at **95.2%** — the
  entrance is pre-rendered;
* the **moved** viewport's best single sprite (full view or partial) is only
  **57.6%** (`DP067`), and no single overlay on any base reaches the spawn's
  95% — so an arbitrary view is a **multi-tile composition**, not one blit.

**Why this cannot be derived without guessing:** the composition is chosen at
run time from (position, facing, local map structure) by GFA-compiled code that
static disassembly cannot resolve (the path table is addressed via a computed
base and carries no relocation entry; see Round 9).  The emulator cannot serve
as an oracle for it — its EGA blit path leaves the viewport showing the menu
scene (documented in `notes-maintainer-town.md`), so it renders the *same*
window for menu, dungeon and town.  The only clean oracle is DOSBox, and each
frame is one (position, facing) data point that requires reliably driving the
game there first.  I therefore stop here rather than invent a heuristic.

**What the drop-in does today:** draws the canonical entry view (`DP001`) for
the save's map id via `render_dungeon_default_view()`, which is pixel-verified
for the Temple spawn; the composition rule is the documented open item.

Static confirmation in `LOF.EXE`: it ships a **contiguous 70-entry table**
`ega\d0\dp000`, `ega\d0\dp001`, … `ega\d0\dp069` (file offsets ~0x3b6d1+, right
after `ega\twn\twn00..05` and before `ega\anim\an00..`).  Every entry is `d0`;
the digit is substituted at run time from the map id's ones digit.  `DP000` is
absent on disk in every view dir (`D5` has 51 files `DP001..DP069`, `D0` 63),
and the load trace opens `dp001..dp069` in order — so slot 0 is unused and the
library is the 69 sprites `DP001..DP069`.  The selection rule is therefore a
*table index* chosen in memory (never a filename opened per view).

Negative evidence on the full views: for `D5` the other full views are **not**
mirrors/rotations of `DP001` (best transform match 0.27–0.41), so they are not
the four facings of one scene — they are distinct pre-rendered views selected
per (position, facing, structure).  For `D0` the handful of full views are
near-degenerate (`DP014` ≈ flipud of `DP001` at 0.01), i.e. the wilderness set
is much smaller and flatter.  This is consistent with the engine pre-rendering
a small set of canonical views and compositing the partial tiles around them.





### Round 12 — composition experiment + oracle reproducibility (2026-11-08)

**Greedy multi-tile composition.**  I ran an exhaustive/FFT-correlation greedy
search that builds a viewport by repeatedly pasting the non-transparent `DPnnn`
tile that removes the most mismatching pixels, against the Temple-spawn oracle
(`ref/oracle/dungeon_slot1.png`).  Starting from the real entry view `DP001` the
best it reaches is **0.209**; starting from `DP067` 0.581; starting from black
**0.659**.  So no small paste set reproduces the spawn — the real frame is not
`DP001` plus a couple of overlay tiles.  Combined with the *moved* frame: the
spawn and the one-step-forward frames differ over **82.9 %** of the viewport
(every 10px row/col band changes), i.e. the engine **recomposes the whole
viewport per (position, facing)**, it is not a fixed background with local
patches.  This is the strongest negative result yet and it rules out the
"canonical view + overlay" model.

**The emulator cannot be the oracle (confirmed).**  `./lof-native --dir game`
with slot 1 loaded leaves the viewport region (rows 11..129, cols 10..129)
**entirely black** — 0 % non-black pixels, and 90 % unlike the menu viewport.
It reads `DNGS\DNG05` and opens `ega\d5\dp001..dp069` (per `LOF_I21=1`) but never
blits the sprites.  So it is *not* a case of "renders the menu in the viewport";
it renders nothing there.  Either way it cannot supply viewport ground truth.

**DOSBox oracle is not reproducible by script.**  The maintainer's own recipe
(`tools/capture_dosbox.sh <dir> --wait 30 --keys "o 1"`) and my key-driven runs
(`space…,o,1`, then arrows) both land on the **main menu** — byte-identical to
`ref/oracle/menu.png` (0.0 diff).  Keys do reach the guest (the right-hand panel
redraws), so it is not a focus problem; the scripted path simply does not enter a
dungeon.  `openbox` installed as a WM did not change this.  The committed
`ref/oracle/dungeon_slot1*.png` were captured interactively and cannot be
regenerated here.

**Save format.**  Slot 1 is map 05 (`DNGS\DNG05` + `EGA/D5`) and slot 2 is map 10
(`DNGS\DNG10` + `EGA/D0`) per the open trace.  The party (map, x, y, facing)
offset inside `GAMES/GAME  n` is not decoded; that is workstream B/maintainer
territory, so I cannot even pin a specific (pos, facing) to fit the rule against.

### Round 13 — DP sprite-library geometry (data-only, no oracle needed)

`EGA/D5/DPnnn` sizes cluster into a clean grid (width x height, indices):

| size | n | indices |
|------|---|---------|
| 120x119 | 7 | 1, 14, 36, 58, 59, 60, 67 |
| 64x59   | 2 | 15, 37 |
| 32x59   | 4 | 18, 19, 40, 41 |
| 32x119  | 6 | 24, 25, 46, 47, 68, 69 |
| 32x29   | 2 | 16, 38 |
| 24x19   | 2 | 17, 39 |
| 16x59   | 4 | 26, 27, 48, 49 |
| 16x48   | 2 | 52, 53 |
| 16x47   | 2 | 30, 31 |
| 16x29   | 10 | 20, 21, 32, 33, 42, 43, 54, 55, 56, 57 |
| 16x26   | 2 | 34, 35 |
| 16x19   | 4 | 22, 23, 44, 45 |
| 8x29    | 4 | 28, 29, 50, 51 |

Key structure: **indices 14..35 and 36..57 are the same size-pattern duplicated
at offset 22** (14→36, 15→37, …; `[28,29]`→`[50,51]`, `[30,31]`→`[52,53]`,
`[34,35]`→`[56,57]`).  The only mismatch is the last pair: the first block ends
`[34,35]=16x26` and the second `[56,57]=16x29`, i.e. the copy is near-exact but
not pixel-identical (`DP(14+k)` vs `DP(36+k)` best-case identity 0.89 at k=10,
never a clean flip).  This looks like a **left/right pair of the same distant
wall/door tile set** (a near-doubled block) rather than one shared block, which
fits the "two off-centre columns of distant tiles" reading of the viewport.

Some views are left-right symmetric and some are not: `DP067` is 99.7 %
symmetric under `fliplr` (a straight corridor / symmetric room seen head-on),
`DP001` only 56.8 % (an off-centre feature, e.g. a door on one side).  The
Temple spawn oracle is 51.0 % symmetric, i.e. asymmetric — so the spawn is not a
straight-down-corridor view, which is why `DP001` (asymmetric) matched it and
`DP067` (symmetric) did not.

Take-away for the rule: the library is a small set of *distant wall/door tiles*
in two mirrored columns plus a full-view background, consistent with a
ray-cast-like compositor, but the exact index→(distance, side, feature)
assignment is still not derivable without more oracle frames.

### Round 14 — theme structure, save location byte, procedural residue

**D1..D7 are seven themes over one identical slot set.**  `EGA/D1..D7` each hold
the *same 51 sprite indices* (`DP001 DP014..DP060 DP067 DP068 DP069`), with
different pixel content per theme (all 51 md5s differ between D1 and D5).  So the
structural slot set is fixed and the digit only selects the texture theme —
strong evidence that the engine's selection rule maps (position, facing,
structure) to a *slot index* independent of the theme, which is exactly the
contiguous `ega\d0\dp000..dp069` table it ships.  `D0` is different: 63 slots
(the full `DP002..DP069`), i.e. the wilderness uses a distinct, larger set.

**Save location byte = the map id's ones digit.**  `GAMES/GAME  1` (the Temple)
has byte `0x9a2 = 5`, and the engine reads `DNGS\DNG05` + `ega\d5\...`; slots
2/3/5 have `0x9a2 = 0`.  So `0x9a2` is the **ones digit** of the dungeon number
(the tens digit lives elsewhere): slot 1 is map 05 (`0x9a2=5`), slot 2 is map 10
(`0x9a2=0`, and the engine reads `DNGS\DNG10`), and the view dir is `D<0x9a2>`.
This is the origin of the ones-digit rule (the maintainer found it by patching
this byte).

**The spawn residue is procedural, not a sprite.**  The Temple spawn is `DP001`
blitted at (10,11) to 95.2 %; the remaining 4.8 % is a compact darker-grey (8)
block at x≈88..100, y≈49..71.  I searched **every** `EGA/*/*` image for a
conflict-free match to that block (masked, all placements) and found **none**.
So the engine does not composite it from a pre-rendered sprite — it *draws* that
part of the view procedurally (a distance/feature overlay computed from the map
and party state).  That is why a pure "pick the right DPnnn" rule cannot explain
a frame: the viewport = a DPnnn base **plus** procedural geometry.

This reframes the DoD: reproducing a dungeon screen needs (a) the DPnnn base
index rule **and** (b) the procedural overlay rule — neither derivable from the
two available oracle frames alone.

### Round 15 — the full play screen is MSCR + the view sprite (verified)

Composed the whole screen and diffed it against the oracle
(`ref/oracle/dungeon_slot1.png`):

- frame region (outside the window) vs `EGA/UP/MSCR`: **0.921**
- viewport (10..129, 11..129) vs `EGA/D5/DP001`: **0.952**
- full 320x200 composite (`MSCR` then `DP001` at (10,11)): **0.928**

So the engine's model is exactly "blit the static frame `EGA/UP/MSCR`, then the
view sprite at (10,11)" — the same `MSCR` artwork the main menu uses.  The
remaining ~7 % is split between the viewport's procedural overlay (4.8 %) and the
frame region, so the *structure* of a dungeon screen is now reproduced
end-to-end; only the per-(position,facing) sprite-index and overlay rules are
open.  Implemented as `render_dungeon_screen()` in `port/dungeon.cpp` and
covered by `test_dungeon_screen` in `tests/dungeon_test.cpp`.

## Round 15 — event link + recovered oracle (2026-11-08)

Two independent results this round; both are **in-lane** (no `src/` edits).

### The save-load crash is a *merge* problem, not a code problem

`main`'s `src/cpu.cpp` still forces `CX=0` in REP CMPS/SCAS
(`case 0xA6/0xA7/0xAE/0xAF: if (c.rep) c.cx = 0;`).  The fix that lets the
emulator reach the play loop — *REP CMPS/SCAS must leave the remaining count in
CX* — exists only on `remotes/pr/50` (commit `69d6b54`), and that branch was
**never merged into `main`**.  Consequence: the emulator built from `main`
crashes to `0000:0000` immediately after loading a save (the MS-C run-time
`strlen` returns `0xFFFE`, the `%2d%%` width overruns a 4-byte stack local and
the slot-1 `RETF` jumps to 0:0).  Rebuilding `remotes/pr/50` reaches the play
loop and reads `DNGS/DNG05` + the whole `EGA/D5/DPnnn` set + `SYST/NSC05`.
So the "emulator viewport is wrong" wall (documented on pr/50) may itself be a
downstream symptom; regardless, **the emulator built from `main` cannot reach
the dungeon at all.**

### Recovered oracle frames (were "missing", actually on `remotes/pr/50`)

`ref/oracle/dungeon_slot1.png` (Temple spawn) and `dungeon_slot1_moved.png`
(one step on) live on `remotes/pr/50`, not on `main`.  Re-analysed with raw EGA
planes:

- Spawn viewport `[11:130,10:130]` == `EGA/D5/DP001` at **0.952**; blit offset
  is exactly (10,11) (an offset sweep −4..4 confirms (0,0) is the optimum).
- The 4.8% residue is **not a single overlay rectangle**: it is spread over the
  whole window (`x 0..119, y 0..77`) and its top row (y=0) contains colours the
  sprite lacks (oracle colour 3 at x2..4).  So `DP001` is the base but the
  engine draws something extra on top / around it.
- The moved frame matches **no** full-size sprite well (best `DP067` at 0.576),
  i.e. the moved view is a *composite* of the 7 full views + the 25 smaller
  side-wall pieces (`(119,32)`, `(59,32)`, `(29,16)`, …), not one blit.

### The cell → event (SPEC) link — located in the decompilation

The scenario event dispatcher is a single big function at image `1F31E`
(`push bp; mov bp,sp; mov ax,0x76; lcall 0x2f0e,0x26a` = 0x76-byte frame).  The
SPEC record for an event is addressed

```
1F713: mov ax, 0xee            ; record stride 238
1F716: imul word ptr [bp-0x44] ; record index  <-- the cell's event index
1F719: add ax, word ptr [0x7d04]   ; base segment:offset of the loaded SPECn
1F721: add ax, 0x50            ; text_b field
```

and the behaviour type is read as `cmp word ptr es:[bx+si+0xe2], TYPE`.  Across
the whole binary the dispatcher keys off exactly **{1, 7, 9, 0x0c, 0x0e, 0x11,
0x19}** — matching `port/char.h`'s `SPEC_TYPE_RECRUIT(1)`, `DOOR(7)`, `SIGN(9)`,
`BUTTON(12)`, `CHALLENGE(14)`, `PUZZLE(17)`, `ELEMENTAL(25)`.  This is an
*independent* confirmation of B's dispatch handler set (7 type constants), which
is the part of B's workstream that does **not** need a live screen.

The dungeon's per-cell **object code** (high byte of the 2-byte object layer) is
the natural source of that `[bp-0x44]` index: for DNG10/DNG00 (scenario 0) the
codes `1..33` select SPEC0 records whose type histogram matches the whole file
(incl. `14 -> type 24`), while DNG05/DNG11 (scenarios 5/1) select only `TEXT`
records.  A full byte-identical mapping is **not** yet proven (codes reach 132
vs 66 records), so it stays a hypothesis — see the blocker list.

### Superseded by Round 16

The object-code hypothesis above (and the "exact `[bp-0x44]` derivation" item)
is **wrong**: Round 16 shows `[bp-0x44]` is found by a location-key search, not
from the cell's object code.  Still open in-lane: the viewport compositing rule
(base sprite + side pieces + overlay) — see BLK-001.

## Round 16 — B's blocker resolved: there is NO cell-object -> SPEC index (2026-11-08)

Disassembling the event dispatcher (`image 1F31E`) linearly with capstone (the
checked-in `analysis/lof.asm` has gaps in this region) settles the question the
blocker asked.  The SPEC record index `[bp-0x44]` is **not** read from the
dungeon cell's object code; it is found by a **location-key search**:

```
1F1BC: mov ax, 0xee                 ; stride
1F1BF: imul word ptr [0x7d00]       ; * record-count  -> bytes to load
1F1C4: lcall ... (malloc)
1F1CC: mov [0x7d04], ax             ; SPEC file base (seg:off)
1F1CF: mov [0x7d06], dx
1F1E4..1F1FE: read the SPEC file    ; record-count at [0x7d00]
...
1F452: mov word ptr [bp-0x44], 0xffff   ; index = -1
1F457: mov word ptr [bp-0x42], 0xffff
1F45C: loop cell-index [bp-0x3c] = 0 .. [0x7d00]-1 :
1F481:   es = [0xb04e]; bx = es:[0x3824]*2
1F490:   ax = [0xb046]:es:[bx+0x4be8]        ; "current" location key (dword)
1F497:   bx = [bp-0x3c]*4
1F49E:   es = [0xb048]
1F4A2:   cmp es:[bx+0x56], ax ; cmp es:[bx+0x58], dx   ; == SPEC record's key?
1F4B6:   mov word ptr [bp-0x44], ax          ; <-- index = the MATCHED record
```

So the engine walks the loaded `SYST/SPECn` table and picks the record whose
**location key at `+0x56`** (dword) equals the party's current location key
(the per-cell table at `[0xb048]` with 4-byte entries, mirrored from
`[0xb046]:es:[0x4be8]` for the party's slot `[0xb04e]:es:[0x3824]`).  The
matched index then addresses the record at `[0x7d04] + index*0xee` (`+0x50`
passed to a predicate, `+0xe2` the behaviour type).

Consequences for the blocker:

* The dungeon's per-cell **object code is not** the SPEC selector.  (DNG05's
  codes reach 132 vs SPEC5's 66 records, so a direct index was already
  impossible.)  The earlier `code == index` correlation was a coincidence of
  DNG00/DNG10 using low codes.
* Events are **location-triggered**: a screen appears when the party stands on a
  cell whose location key matches a SPEC record's `+0x56` key.  To make an event
  screen reachable, the party must be at such a location — not merely standing on
  an object cell.
* Types `{1, 8}` are handled *separately* at `1F6DE`/`1F6E6`
  (`cmp word [bx+0xe2], 1` / `, 8`) and skip the text blit; the dispatcher's
  text path is driven by the seven type constants already recorded
  (`{1,7,9,0xc,0xe,0x11,0x19}`), which independently confirms B's handler set.

### Practical unblock for B

The emulator built from `main` cannot reach a dungeon (REP CMPS/SCAS bug, see
Round 15); the pr/50 build can, and it reads `DNGS/DNG05` + all `EGA/D5/DPnnn` +
`SYST/NSC05`.  The ground-truth oracle for the dungeon screen is
`ref/oracle/dungeon_slot1.png` (now copied into this repo, in-lane): the frame
is `EGA/UP/MSCR` (92.1% of the non-window pixels) with `EGA/D5/DP001` blitted at
(10,11) (95.2% of the window); the whole composite matches at 92.8%.  The
residue is the procedural horizon/floor band (the engine's tile overlay), which
is the remaining decode task — not a missing sprite.

### `tools/dngs.py oracle` — reproducible DoD check

Added `oracle <png> <ega_dir> <dungeon> [index]`, which composites
`EGA/UP/MSCR` + `EGA/D<d>/DP<index>` at (10,11) and scores it against a DOSBox
frame, ranking every full-size sprite in the tileset.  It needs no PIL (a small
in-file PNG reader handles DOSBox's palette PNGs).  Results:

```
oracle ref/oracle/dungeon_slot1.png  game/EGA 5 1
  frame region : 0.921   view window : 0.952   whole screen : 0.928
  best full-size sprites: DP001 0.952, DP059 0.397, DP067 0.349, ...
oracle ref/oracle/dungeon_slot1_moved.png game/EGA 5 1
  frame region : 0.894   view window : 0.202   whole screen : 0.739
  best full-size sprites: DP067 0.576, DP001 0.202, ...
```

The spawn window is `DP001` (0.952); the *moved* window matches no single sprite
(best 0.576), confirming the view is a **composite** of the full views + the
smaller side pieces.  The frame region is `MSCR` (0.89-0.92) in both.

## Round 17 — registry + emulator-viewport correction (2026-11-08)

Bookkeeping round (no engine change):

- **Registry.** `docs/BLOCKERS.md` now carries my lane's state: BLK-001
  `In progress` (with a log line), and two new entries — **BLK-006** (a moved
  view matches no single sprite; needs the moved-view ground truth) and
  **BLK-007** (maintainer: publish the key sequence that reaches a dungeon),
  which BLK-006/BLK-001/BLK-004 depend on.
- **Correction (important).** I briefly marked BLK-002 `Resolved` (as if the
  divergence were only the oracle capture). That was **wrong and I reverted it**:
  with current `main`, `tests/golden_test.sh`'s Temple recipe
  (`LOF_KEYAUTO=0x18,0x02 LOF_KEYGAP=8 LOF_CMDLEN=3 --frames 1500`) leaves the
  emulator's viewport **black** (EGA index 0) at frames 1500/2000/2500/3000,
  while the oracle viewport holds indices {0,3,7,8,15}. Decoding the emulator's
  dumped planes (`LOF_EDUMP`) shows the emulator *does* write colour there
  (indices 6,7,8,9,15) — so the maintainer's original report stands: the
  emulator's colour differs from the oracle. BLK-002 is **not** my lane, so I
  left the maintainer's text in place and only noted this.
- **Scope note.** The `dngs.py oracle` figures (0.921 / 0.952 / 0.928) score the
  **drop-in's** static `DP001` composite, not the emulator. The drop-in's
  viewport is a deliberate stand-in for the real per-cell composition (BLK-001).
- **Emulator trace.** The engine selects the view per cell from a *subset* of the
  D5 set: it opens `ega\d5\dp001` (facing), then `DP014`..`DP023`, then
  `ega\anim\an*`. That is the selection I need to decode once BLK-006/007 give me
  a moved-view ground truth.

## Round 18 — emulator geometry vs oracle, and BLK-002 root cause (2026-11-08)

I reproduced the maintainer's BLK-002 measurement **from the EGA planes** (not the
`--shot` framebuffer, which is black in the viewport — see below).  Recipe:

```
LOF_EDUMP=planes.bin LOF_KEYAUTO=0x18,0x02 LOF_KEYGAP=8 LOF_CMDLEN=3 \
  LOF_NO_TIMER=1 ./lof-native --dir game --headless --frames 1500 --shot /dev/null
```

Decoding `planes.bin` (4 x 0x10000, row stride 40 bytes) to attribute indices and
comparing to `ref/oracle/dungeon_slot1.png`:

- **Shape match 0.996** whole screen, **0.993** inside the 120x119 viewport —
  exactly the maintainer's number.  So the emulator's per-cell geometry is
  correct and **is** a usable geometry oracle.
- **Viewport histogram:** emu `{0:4876, 6:4146, 9:4044, 15:1214}` vs oracle
  `{0:4982, 3:5, 7:4080, 8:4027, 15:1186}`.
- **Pixel-pair confusion:** emu 6 ↔ orc 7 (4080), emu 9 ↔ orc 8 (4025), emu 0 ↔
  orc 0, emu 15 ↔ orc 15.  So the divergence is *one attribute bit*: `6(0110)`
  and `7(0111)` differ in bit 0; `9(1001)` and `8(1000)` differ in bit 0.  The
  emulator's `attr_pal` at the dungeon is the **identity** (`00 01 02 03 04 05 06
  07 10 11 12 13 14 15 16 17`, via `LOF_PAL2`), so the game's attribute-palette
  table (which must map the plane bits to grey) is **not being applied**.
  `seq_mask=01`, `gc_mode=00`, `bpp=4` — planes are read correctly, so this is
  purely the palette path, not write-mode/plane selection.  Confirms BLK-002's
  "not a missing sprite table".

Also: `--shot`'s RGB framebuffer is black in the viewport even though the EGA
planes have ink (48574 non-black pixels overall) — the shot path drops the
viewport colour.  Use `LOF_EDUMP` for verification, not `--shot`.

DP001 placement re-derived against the emulator geometry: best IoU 0.922 at
(10,11) (the drop-in's position), next-best 0.745 at (8,11) — so (10,11) is right.

## Round 19 — facing-patched ground truth + the composite rule (2026-11-08)

**The emulator can be driven to new views without keys, by patching the save's
facing byte** (`kSaveFacing` = 0x9a6; the Temple save has 2 = South).  Patch it,
re-run the golden recipe, and `LOF_EDUMP` yields a ground-truth frame:

```
cp game/GAMES/GAME1 /tmp/G1.orig
printf '\x00' | dd of=game/GAMES/GAME1 bs=1 seek=$((0x9a6)) count=1 conv=notrunc
LOF_EDUMP=planes.bin LOF_KEYAUTO=0x18,0x02 LOF_KEYGAP=8 LOF_CMDLEN=3 \
  LOF_NO_TIMER=1 ./lof-native --dir game --headless --frames 1500 --shot /dev/null
```

Results (view window, screen (10,11)):

| facing | base sprite | window ink | base IoU |
|--------|-------------|-----------|----------|
| 2 South (spawn) | `DP001` | 9404 | 0.922 |
| 0 North         | `DP036`/`DP060` | 12566 | 0.880 |

So the base view is a **function of facing**: facing 2 -> `DP001`, facing 0 ->
`DP036` (`DP060` is a byte-identical duplicate of `DP036`).  `DP001` (spawn) is a
*right-side* view and `DP036` (north) is an all-wall view; the window is 120x119.

**The composite rule (ink level).**  A greedy "place D5 sprites to maximise newly
covered target ink minus false positives" over the spawn window gives:

```
DP001@(0,0) 8690   DP042@(102,45) 230   DP056@(0,49) 110
DP044@(90,52) 79   DP022@(104,52) 48    DP051@(72,42) 60   DP022@(104,57) 32
=> ink IoU 0.966
```

i.e. the engine blits the full base view, then overlays the **small side pieces**
(`DP042/044/051/056/022`, 29x16..59x16) in a **horizon band (y ~42..57)** at the
window edges — exactly the residue characterised in Round 18.  The *north* view's
`DP036` is a superset (its base already has all the ink), so there the composite
adds nothing.  This confirms BLK-001's model (full view + per-cell side pieces)
and gives the piece set; the exact *selection* (which pieces, from which cells)
still needs a sweep of positions, not just facings.

**Still open.** Position (not just facing) drives the side-piece set; the save's
party x/y offsets are not yet identified, so a position sweep needs either those
offsets or the maintainer's `INT 21h AH=07` fix (BLK-007).  Ink-level the rule is
clear; index-exact placement has placement ambiguity (greedy FP), so it is not
worth baking into `port/dungeon.{h,cpp}` until the selection is pinned.

### Round 19b — facing sweep + independent confirmation against the DOSBox oracle

Swept all four facing bytes (0..3) on the Temple save and dumped planes for each:

| facing byte | base sprite in the window | window ink |
|-------------|---------------------------|-----------|
| 0 | `DP036`/`DP060` (all-walls) | 12566 |
| 1 | `DP036`/`DP060` (all-walls) | 12566 |
| 2 (the save's own) | `DP001` | 9404 |
| 3 | `DP036`/`DP060` (all-walls) | 12566 |

So only the *native* facing (2, South) shows the `DP001` view; the other three
show the all-walls view.  That is consistent with the base being the view of the
**cell straight ahead**: at the Temple spawn the party looks down a corridor
(open ahead), and turning to any other facing faces a wall.  It also means the
view is *not* a simple facing->sprite table — it depends on the map, i.e. the
engine reads the tiles ahead (matching the per-cell model).

**Independent confirmation.** Running the same greedy composite over the
**maintainer's real DOSBox capture** `ref/oracle/dungeon_slot1.png` gives the
*identical* pieces at the *identical* offsets as the emulator frame:

```
DP001@(0,0) 8690   DP042@(102,45) 230   DP056@(0,49) 110
DP044@(90,52) 79   DP022@(104,52) 48    DP022@(104,57) 32
=> spawn-oracle ink IoU 0.974
```

That the emulator and real DOSBox agree on the composite (same base + same side
pieces at the same coordinates) is strong evidence the rule is:

    view = full base sprite (per cell structure ahead)
         + the small side pieces (DP015..DP057) placed in the horizon band
           (window y ~40..60) at the window edges.

The maintainer's *moved* oracle frame reduces less cleanly (IoU 0.752; base
`DP037` + pieces), so a second (position) ground truth would help pin the exact
piece *selection*, but the base+side-piece *structure* is now confirmed on real
hardware.

### Round 20 — the save's party-position field (0x9a0) and view ground truth

`0x9a0..0x9a1` is a 16-bit little-endian field that differs between the two
shipped saves: GAME1 (Temple) = **125**, GAME2 (wilderness) = **745** (facing 2
and 0 respectively).  Patching it and re-running the golden recipe changes the
rendered viewport, so it is the party's position (a linear cell index candidate;
125 = 17 + 3*36, 745 = 25 + 20*36 for a 36-wide grid).

**Confirmed the view is map-driven, not a facing table.**  For the Temple, the
save's own value (125) gives the corridor view `DP001`, while adjacent values
(126, 89 — re-run sequentially, see Round 21) give the all-walls view `DP036`.
For the wilderness save the window matches `DP036`/`DP014` at IoU **1.000** in
`EGA/D0`, confirming the maintainer's ones-digit view-dir rule (DNG10 -> `D0`)
end-to-end.  So the base sprite is chosen from the local wall structure ahead,
exactly as BLK-001/BLK-006 describe.  (The 124/161/468/900/1205 values quoted in
an earlier draft came from *parallel* runs that raced on one save file and are
**withdrawn** — see Round 21.)

**Ground-truth mechanism (partial).**  Patching `0x9a0` *does* move the party for
the emulator, so position ground truth is reachable without keys — but the
value->cell mapping is not yet pinned (only the exact original value reproduces
the corridor), so it is not yet a turnkey sweep.  The maintainer's `INT 21h AH=07`
fix (BLK-007) or the position mapping would make it one.

### Round 22 — the DNG header is the party's entry cell + facing (pinned)

**The 4-byte DNG header is `[flag, entry_facing, entry_pos_hi, entry_pos_lo]`.**
The 16-bit `entry_pos` is **big-endian** (hi byte first) and is a row-major cell
index `y*36 + x` into the tile map.  It is the **party's start cell** and
`entry_facing` is the start facing.  This is not a guess: it matches the saves
exactly.

| file  | header        | entry_pos | (x,y)   | facing | save  | save 0x9a0 / 0x9a6 |
|-------|---------------|-----------|---------|--------|-------|--------------------|
| DNG05 | `00 02 00 7d` | 125       | (17,3)  | 2      | GAME1 | 125 / 2            |
| DNG10 | `00 00 02 e9` | 745       | (25,20) | 0      | GAME2 | 745 / 0            |

Both rows are byte-exact matches, and the endianness is forced: read little-endian
DNG10's word would be 59650 (out of the 1296-cell range), while big-endian gives
745.  So `0x9a0` (the save's party position, Round 20) *is* the cell index, and
the DNG header tells us where each level starts the party.  `flag` (h0) is 0 for
every level except DNG13 (4).

This closes the "position field semantics" question of Round 20/21: the engine
reads the position from the save, and the DNG header supplies the same encoding
for the level's entry.  (Which DNG file a given save loads is a separate
question — the *encoding* is what is pinned here.)  Note the view a cell renders
is **not** determined by the single cell ahead: DNG05 cell (17,3) facing 2 has a
wall at front and both side cells at y=4 yet renders the open corridor `DP001`,
while (18,3) renders the all-walls `DP036`, so the view depends on the wider
neighbourhood; the base-sprite selection rule is still BLK-006.

Implemented: `port/dungeon.{h,cpp}` now decodes the header as entry facing +
big-endian entry position (was previously kept verbatim as an opaque little-endian
`field`), exposes `entry_x()/entry_y()/entry_facing()` and a
`dungeon_place_entry()` that places the party at the header start;
`tools/dngs.py header` prints it; `tests/dungeon_test.cpp` pins DNG05=125/2 and
DNG10=745/0 and asserts the entry cell is not a wall.

### Round 21 — race correction + the viewport animates; spawn matches the oracle exactly

**Race warning (corrects Round 20's method).**  Two emulator runs must **not** run
in parallel on the same save: they both write `game/GAMES/GAME1`, so the second
run can load the first run's patched bytes.  My earlier "sweep" frames were
produced by parallel jobs and are therefore unreliable as *position* evidence.
All results below were re-taken **sequentially** (one `lof-native` at a time,
save restored between runs).

**`0x9a0` changes the rendered view, but its semantics are unresolved.**  With
the save at `0x9a0=125` the window is the `DP001` corridor view (ink 9298/9404 by
phase); with `0x9a0=126`, `89`, `130`, `500` or `1000` it is the all-walls
`DP036` view (ink 12566).  So the engine *does* read `0x9a0` and the view depends
on it — but **every** tested non-native value gives the same all-walls view, even
values whose `(x,y)` cell (assuming a 36-wide linear index) is open with the same
surroundings as the spawn.  Two readings are possible: (a) `0x9a0` is the party
position and the engine cross-checks it against another field, so a lone patch
lands on a "no valid view" fallback (`DP036`); or (b) `0x9a0` is not the raw
`(x,y)` index.  Either way it is **not yet** a turnkey position sweep, and I do
**not** claim a value->cell mapping.  What *is* solid: the base view is chosen
from the map, not from facing alone.

**The viewport animates.**  Holding the save fixed and varying only the frame
count (sequential) oscillates the window ink between two phases:

| frames | window ink | note |
|--------|-----------|------|
| 700    | 9298 | == oracle |
| 900    | 9404 | |
| 1200   | 9298 | == oracle |
| 1500   | 9404 | |

So the viewport is a two-phase animation (matching the `ega\anim\an*` files the
engine opens).  Frame 900/1500 capture the non-oracle phase; 700/1200 the oracle
phase.  This is why the earlier single-frame match looked like a 4.8% residue.

**Spawn shape is pixel-perfect vs the oracle.**  At the oracle phase (frames
700/1200) the emulator viewport has **ink IoU 1.0000** with
`ref/oracle/dungeon_slot1.png` (identical 9298 ink pixels).  The only remaining
difference is the per-pixel *index*: 8107 px differ by exactly +/-1 in EGA index
(4080 +1, 4027 -1) — the attribute-bit-0 palette issue of BLK-002, not a shape
error.  So BLK-001's composite is reproduced **exactly** by the engine; the
residual is purely BLK-002's palette.  Frame 1500's 9404-phase frame is ink IoU
0.9887 vs the oracle (the same view, one overlay piece animated in).

## Round 23 — structure-driven view ground truth (2026-11-12)

**The dungeon view is a function of the map *structure ahead*, and I can now
drive it deterministically by patching the DNG tile map** — no keys, no party
position field.  This supersedes the earlier "position sweep" attempts.

### The lever: patch the DNG tile layer, not the save

The engine reads `DNGS\DNG<loc>` at level load and renders from it.  Patching the
**save's** `0x9a0` (party position) has **no effect** on the emulator's view
(Round 20's 125-vs-126/89/108/120/124/468/900 results are **withdrawn**: all give
a byte-identical `DP001` view — those earlier "different view" readings were
parallel-run races on one save file).  What *does* change the view is the DNG
**header byte d[2] (facing)** and the DNG **tile layer**.

I added a byte-exact RLE re-encoder (verified: a no-op re-encode of `DNG05`
reproduces the original file byte-for-byte, 1464 bytes).  Patching a tile cell
and re-encoding lets me set the map structure under the emulator's feet.

### Results (Temple save, DNG05, spawn (17,3); recipe `LOF_EDUMP=… LOF_KEYAUTO=0x18,0x02
LOF_KEYGAP=8 LOF_CMDLEN=3 LOF_NO_TIMER=1 --frames 900`)

`d[2]` (facing) and the tile **directly ahead** both change the base view:

| change | ahead cell tile | view ink | nearest D5 sprite |
|--------|-----------------|----------|-------------------|
| native (facing 2) | 3 (wall) | 9404 | `DP001` (0.922) |
| ahead → 0 | 0 (open) | 4941 | `DP060` (0.346) |
| ahead → 1 | 1 | 9404 | `DP001` |
| ahead → 2 | 2 | 4941 | `DP060` |
| ahead → 6 | 6 | 14280 | `DP060` (**1.000**) |
| ahead → 7 | 7 | 4941 | `DP060` |
| ahead → 8 | 8 | 4941 | `DP060` |
| ahead+2 → 0 | 3 | 4941 | `DP060` |
| facing → 0 (ahead 7) | 7 | 13072 | `DP014` (0.997) |
| facing → 1 | 7 | 11540 | (composite) |
| facing → 3 | 7 | 12504 | (composite) |

So the engine classifies the cell ahead: **tile 3 (and 1) → the "closed" wall
view `DP001`**; **open cells (0/2/6/7/8) → an "open corridor" view**.  Changing
the *second* cell ahead (161 unchanged) still changes the view, so depth ≥2 is
read.  Changing only the cell at depth 2 (`162`) does **not** change the view, so
the base view is dominated by the **nearest** structure ahead.

### `dungeon_slot1_moved.png` explained (BLK-006)

Opening the cell ahead (`161 → 0`) makes the emulator viewport match the
maintainer's moved oracle at **ink IoU 0.998** (4941 vs 4951 ink).  So the moved
frame is simply the view with an **open cell ahead** — the composite-decode
"mess" (`DP037` + pieces, IoU 0.75) was an artefact of greedy placement over a
low-ink open view, not a different rule.

### Why this matters / what is still open

The base view is **not** a facing→sprite table and **not** driven by the save's
`0x9a0`; it is computed from the tile structure ahead.  Exact per-cell
`(neighbourhood → DPnnn)` selection is still not pinned (only two regimes are
confirmed: "wall ahead" → `DP001`, "open ahead" → `DP060`/`DP014`).  Pinning it
needs a **position** sweep, which needs the maintainer's input fix (BLK-007) —
the emulator does not consume patched position bytes, so I cannot place the party
at arbitrary cells to sweep the view without keys.

## Round 24 — base-view selection: the ahead-cell tile code (BLK-001)

Round 23 said the base view is "not a facing→sprite table".  It *is* a
**tile-code → sprite table**, keyed on the tile code of the cell **directly
ahead** of the party.  Method: patch DNG05's RLE tile layer
(`/tmp/tilepatch.py`, verified to change exactly the intended decoded cell),
run the analysis emulator with `LOF_EDUMP=`, and compare the 120x119 window at
(10,11) to every `EGA/D5/DP*` sprite by pixel IoU.

Measured map (D5 = Temple tileset; `ahead` = the single cell one step in the
facing direction):

| ahead tile | view ink | match |
|------------|----------|-------|
| 3, 12, 0, off-map | 9404 | `DP001` (0.922) — wall ahead |
| 4, 20, 36, 52 | 13199 | `DP058` (**1.000**) |
| 5, 21 | 12992 | `DP059` (**1.000**) |
| 6, 13 | 14280 | `DP060` (**1.000**) |
| 10 | 13072 | `DP014` (0.997) — doorway |
| 2, 7, 8 | 4941 | no single sprite; composite (`DP037` + pieces, IoU 0.75) |
| 9, 11, 14..19 | 9404 | `DP001` |
| 22, 26, 366 (nibbles 6/A/E/F on the *floor* code) | 9404 | `DP001` |

Two independent confirmations that the *ahead* cell is the key, not a
neighbourhood:

* Patches to the left/right and depth ≥2 cells leave the dump **byte-identical**
  (`md5` equal), so they do not enter the selection.
* The low nibble groups several codes: 4/20/36/52 all give `DP058`, 5/21 give
  `DP059`.  So the tile code is a **structure bitmask** (walls on N/E/S/W), and
  the low nibble is the "what is on the side" field.

Open question (still BLK-001): the exact bitmask → DPnnn rule.  The measured
`switch` is implemented in `dungeon_base_view_index`; the corridor case (2/7/8)
and unknown codes fall back to the wall view (`DP001`), which is correct for a
wall directly ahead and a harmless over-estimate elsewhere (a bare `DP037` blit
would leave the window mostly empty, since the engine's corridor view is a
multi-piece composite).  The four stride-16 groups
(4/20/36/52, 5/21, 6/13, 7/8) strongly suggest the code is
`(param << 4) | structure`; `0x02/0x07/0x08` are the "corridor straight ahead,
no side wall" case whose view is a **composite** the engine builds from a 64x59
wall-at-depth-2 piece (`DP037`) centred plus smaller side pieces.

Implemented: `dungeon_base_view_index` + `dungeon_ahead_cell` +
`render_dungeon_play` in `port/dungeon.{h,cpp}`; pinned by `test_base_view` in
`tests/dungeon_test.cpp`.

## Round 25 — registry migration (PR #89 rebase)

Merged `origin/main` into `work/agent-a` and migrated the workstream-A blocker
entries into the new per-lane registry (`docs/blockers/A.md`), per the
maintainer's PR #89 review:

| old | new | note |
|-----|-----|------|
| BLK-001 | BLK-A001 | first-person viewport composite (mine, In progress) |
| BLK-002 | BLK-A002 | emulator plane-0 blit — **Resolved** by the maintainer |
| BLK-006 | BLK-A003 | moved-view composite / side-piece selection (mine) |
| BLK-007 | BLK-A004 | emulator extended-key delivery (maintainer) |

The old single-file `docs/BLOCKERS.md` is now the static hub (taken from `main`
during the merge); no entries live there.  `BLK-A001` gained a Round-24 log with
the ahead-cell tile-code table.

## Round 26 — base-view rule fully decoded: the **low nibble** selects the sprite

Rebuilt the emulator sweep pipeline (the workspace reset had cleared `/tmp`).
`tools/dngs.py patch <in> <out> x y val` now writes a DNG with chosen tile cells
overridden, re-encoding with the same RLE scheme (round-trips byte-exactly), so a
chosen *ahead-cell* code can be fed to the emulator without moving the party.
DNG05's party spawns at (17,3) facing 2 (south), so cell (17,4) is directly
ahead; the `LOF_KEYAUTO=0x18,0x02 LOF_KEYGAP=8 --frames 1500` recipe opens save
slot 1 (Temple) and dumps the planes with `LOF_EDUMP=`.

Swept every tile code that occurs in `DNGS/` (26 distinct values).  The result is
unambiguous: **only the low nibble of the ahead tile code selects the base view;
the high nibble is ignored.**  Evidence: seven observed high-nibble variants of
low nibble 4 (`0x04,0x14,0x24,0x34,0x44,0x54,0xf4`) all give the *identical*
window matching `DP058` at IoU 1.000, and seven variants of low nibble 5
(`0x05,0x15,0x25,0x35,0x45,0x55,0x65`) all give `DP059` at 1.000.

| ahead low nibble | view | IoU | note |
|---|---|---|---|
| `0x4` | `DP058` | 1.000 | side passage |
| `0x5` | `DP059` | 1.000 | side passage variant |
| `0x6` | `DP060` | 1.000 | open room |
| `0xd` | `DP060` | 1.000 | open room variant |
| `0xa` | `DP014` | 0.997 | doorway |
| `0x0,0x1,0x2,0x3,0x7,0x8,0x9,0xb,0xc` | `DP001` | 0.922 | corridor/opening: multi-piece composite, no single full-size sprite |

The "corridor" set (`0x0/0x2/0x7/0x8/0x9`) is the perspective composite: its
window (ink ~4941) matches **no** full-size `DPnnn` (best 0.346); template
matching the smaller pieces finds `DP037` (64x59 wall-at-depth-2) at IoU 0.785
plus side pieces (`DP041` 0.757, `DP018` 0.754, `DP040` 0.738) — i.e. the engine
composes the corridor view from a centred wall piece and flanking pieces, which
is exactly the residual tracked by BLK-A003.

Implemented `dungeon_base_view_index` as `tile & 0x0f` (was the high-nibble
`param`-based table) and widened `test_base_view` to assert the invariance across
all 16 high nibbles for nibbles 4/5/6/13/10 and the wall fallback for the
corridor nibbles.  `make test` and `make port` green.

## Round 27 — the true solid-wall set is **{6,13}** (opacity sweep, ground truth)

The `{3,12}` wall set was provably wrong: DNG06's spawn cell (17,3) is tile 3,
so `{3,12}` left the level 0/1201 reachable (DNG00 8/684).  Connectivity alone
could not pin the set (marking *nothing* solid is trivially connected), so I
used a **direct occlusion oracle** instead of a global connectivity fit.

**Method (opacity sweep).**  Patch DNG05's ahead cell (17,4) to a chosen code
`X`, and *separately* vary the cell directly behind it (17,5 / 17,6).  Capture
the planes with `LOF_EDUMP=` (recipe `LOF_KEYAUTO=0x18,0x02 LOF_KEYGAP=100
--frames 1600`) and compare.  If `X` occludes, changing the cell behind it
cannot change the view; if `X` is see-through, it does.

| ahead code `X` | behind-cell change visible? | verdict |
|---|---|---|
| 0, 1, 2, 3, 7, 8, 12 | yes (diffs 3690..8279) | see-through |
| **6, 13** | **no (identical dump)** | **opaque → wall** |

So the solid set is exactly **{6,13}**.  This matches the render: 6/13 draw the
full-screen wall face `DP036`, which is **byte-identical to `DP060`**
(`md5 55df6376…`) and 100% ink — i.e. the Round-24 label "6/13 → DP060 open
room" was a mislabel of the same flat wall sprite (DP036 and DP060 are the same
file).  Tiles 3 and 12 are *not* walls (12 renders as a see-through corridor).

**Consequence for connectivity.**  With walls `{6,13}` only 11/29 levels are a
single connected region (mean largest component 0.935; worst DNG17 0.351).  That
is **not** a defect — the levels are genuinely multi-room (DNG17 is four rooms:
sizes 288/216/201/115).  A wrong wall set shatters a level into hundreds of
fragments, so the connectivity guard is "no shattering", not "fully connected".
Object/feature cells all sit on walkable tiles (DNG01 0/62, DNG17 0/327 on
walls), consistent with features marking squares, not walls.

Implemented in `dungeon_is_solid_tile` (`port/dungeon.cpp`), reclassified
`tile_colour`/`tile_symbol`, and rewrote `tests/dungeon_test.cpp` to the measured
model.  `make test` and `make port` green.

**Caveat (Rule 1).**  The sweep covered the ahead cell of one DNG05 corridor; it
is strong direct evidence for 6/13 and for 0/1/2/3/7/8/12, but codes that never
occur ahead of a party were not individually swept.  It is an oracle result, not
a decompiled one — flagged in `docs/GUESSWORK.md` (G-003).
