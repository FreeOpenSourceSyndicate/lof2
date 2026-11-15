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

## Round 28 — full ahead-code sweep: the base-view map is corrected, and the view is ray-composed

Rebuilt the capture pipeline again (workspace reset clears `/tmp`) and swept
**every** ahead-cell code for DNG05's spawn (17,3) facing 2, i.e. cell (17,4),
holding the cell behind it (17,5) at its original 0.  Each run:
`tools/dngs.py patch` the tile cell, `cp` into `game/DNGS/DNG05`,
`LOF_EDUMP=… LOF_KEYAUTO=0x18,0x02 LOF_KEYGAP=100 ./lof-native --dir game
--headless --frames 1600 --shot /dev/null` (~4 min/run), then diff the 120x119
window against every `EGA/D5/DP*`.

**Corrected map** (IoU against the best single sprite):

| ahead code | view | IoU | note |
|---|---|---|---|
| `0x4` | `DP058` | 1.000 | side passage |
| `0x5` | `DP059` | 1.000 | side passage variant |
| `0x6`, `0xd` | `DP060` (= `DP036`) | 1.000 | looking at a wall: flat face |
| `0xa` | `DP014` | 0.997 | doorway |
| `0x1`,`0x3`,`0xb`,`0xc` | `DP001` | 0.91 | textured wall/room |
| `0x0`,`0x2`,`0x7`,`0x8` | **none** | 0.35 | open corridor, composed view |
| `0x9` | **none** | 0.32 | corridor variant |

Round 26's table said the corridor nibbles fall back to `DP001` at IoU **0.922**;
the direct sweep does not reproduce that — the corridor nibbles give a distinct,
stable image (ink 4941; identical for 0/2/7/8, md5 `e165034f…`) that matches
**no** single sprite.  (`cmd_patch` is verified correct: patching `(17,4)` to 0
changes decoded tile byte 161, exactly `y*36+x`.)  Round 26's 0.922 reading is
superseded.

**It is not a sprite composite either.**  Greedy + exact-cover decomposition over
all 51 `EGA/D5/DP*` and all 24 `EGA/ANIM/AN*` pieces tops out at 57% ink with
large false positives, and a **colour-exact** search (sprite pixels must equal
target pixels, not just be non-zero) finds *zero* placements.  It is also not the
drop-in's own DDA ray-caster (IoU 0.17 — the ray-caster draws a symmetric
wall band; the engine's corridor view is asymmetric with ceiling/floor detail).

**The view is ray-composed, not a pure function of the ahead cell.**  With the
ahead cell fixed at 0, changing the depth-2 cell (17,5) changes the window
(ink 4941 → 5111 for both `3` and `6`).  So the engine marches the whole column
and composes per-depth geometry; `dungeon_base_view_index` (which only looks one
cell ahead) is therefore a documented approximation.  This is the true residual
of BLK-A003.

**Positive result: the emulator reproduces the moved oracle.**  The corridor
capture (ahead = 0) matches the maintainer's `ref/oracle/dungeon_slot1_moved.png`
at symmetric-difference **10 pixels** (ink 4941 vs 4951) — i.e. the emulator's
dungeon view is faithful to real DOSBox, so BLK-A004's "the party cannot be moved
/ the moved frame cannot be reproduced" is stale: patching the tile layer is an
equivalent and much cheaper way to reach any view.

Implemented: corrected `dungeon_base_view_index` (0x1/0x3/0xb/0xc → 1 explicitly;
0x0/0x2/0x7/0x8/0x9 fall back to 1 as the documented approximation) and
`test_base_view` now pins the full map.  `make test` and `make port` green.
The corridor case stays BLK-A003 (Open); the fallback is recorded as G-004.

### Round 28 addendum — the corridor view is frame-stable (not an animation phase)

Captured the ahead=0 corridor view at frames 1300, 2000 and 2600 (same
`LOF_KEYAUTO=0x18,0x02` sequence): all three dumps are **md5-identical**
(`e165034f…`), and identical to the frame-1600 capture.  So the corridor view is
a fixed composed image, not an animation phase — BLK-A003's alternative
"animation phase mismatch" hypothesis is ruled out.  It is a deterministic
function of the map column.

One more disproof of the composite theory: a **colour-exact** placement of
`DP037` (the earlier "wall-at-depth-2" candidate) over the corridor window has
match 170 / mismatch 3048 at *every* offset — the ink overlap the earlier greedy
search found was entirely spurious.  So the corridor image is composed by the
engine (geometry it draws per depth), and reproducing it needs the decompiled
view routine (Lane M) — a cross-lane dependency, logged in BLK-A003.

## Round 29 — the view is depth-scaled; the feature byte is irrelevant

Two questions left open by Round 28, both now settled empirically by patching
DNG05's ahead cell (party at (17,3) facing S, ahead cell (17,4)) and diffing the
120x119 window at (10,11) of `LOF_EDUMP=`.

### The feature byte does not enter the view

Round 28's sweep only patched the **tile** layer.  The ahead cell (17,4) also
carries a non-zero **feature** byte (34), so it was unclear whether the window
was driven by the tile, the feature, or both.  Using a new `dngs.py patchcell`
(tile + feature) I fixed the tile and varied the feature:

| ahead (tile, feature) | md5 |
|-----------------------|-----|
| (0, 0)   | `e165034f…` |
| (0, 12)  | `e165034f…` |
| (0, 132) | `e165034f…` |
| (3, 0)   | `694ac649…` |
| (3, 34)  | `694ac649…` |

The feature byte has **no effect**; only the tile code matters.  So the Round 28
map stands and `dungeon_base_view_index` needs no feature input.

### The base view is a function of the distance to the first *view-opaque* cell

Keeping the ahead cell open (tile 0) and moving the first wall further down the
column shows the window is not binary "wall / not wall" but **depth-scaled**:

| first opaque cell | md5 | shape |
|-------------------|-----|-------|
| depth 1 (ahead)   | `694ac649…` | full-screen wall texture = `DP001` (IoU 0.917) |
| depth 2           | `58116d1d…` | composed perspective wall face (ink 5111) |
| depth 3           | `e165034f…` | composed corridor (ink 4941) |
| depth 4           | `e165034f…` | identical to depth 3 — **saturates** |

The depth-2 image is a **trapezoidal wall face** (top of the face at the centre,
widening downward to the floor) — exactly what a perspective renderer draws for
a wall one cell further away.  Depth >= 3 saturates: the far wall's projected
face is small enough that the engine draws the same corridor image.

### `tile 3` is view-opaque but movement-passable

The clean way to reproduce the depth-2 image is to put a **tile-3** cell at
depth 2 while leaving the ahead cell open: the window becomes `58116d1d…`,
identical to the depth-2 wall case.  So the set of cells the *view* treats as
opaque includes tile 3, which `dungeon_is_solid_tile` (the movement test) does
not.  The two opacity sets differ.  A lateral side-opening (tile 8) beside an
open corridor does not change the view (`e165034f…`).

### What this means for the drop-in

`dungeon_base_view_index` looks only at the ahead cell, so it is **exact when the
ahead cell is itself the first opaque cell** (the common case — walls, doors,
side passages) and a documented approximation otherwise.  The depth-2 and
depth->=3 images are engine-drawn (best single-sprite IoU 0.36/0.35), so
reproducing them still needs the decompiled view routine (BLK-A003, Lane M).  I
extended `test_base_view` to pin that `dungeon_base_view_index` stays
depth-independent, so it cannot silently drift into a half-correct depth model.

## Round 30 — decompilation consulted; the corridor view is procedurally drawn, not a sprite composite

Per `docs/RULES.md` Rule 1 the decompilation is the primary source, so I read
`analysis/lof.asm` and the emulator's DOS traces. Findings:

### The wall view is exactly `DP001`; the corridor view is not a sprite at all

Colour-accurate template search of the 120x119 window (DNG05 entry, facing S,
ahead cell tile 0 = open corridor vs tile 3 = wall):

| view | best single sprite | colour accuracy |
|------|--------------------|-----------------|
| wall ahead (tile 3) | `D5/DP001` | **98.8%** |
| corridor (tile 0) | none — best `D5/DP035` | 20% (noise) |

The corridor matches **no** single `EGA/**` sprite (best 20% colour / 0.346 ink
IoU, confirming Round 28) and — new this round — **zero colour-exact
containment placements** exist across all 51 `D5/DP*` sprites, i.e. no sprite
fits anywhere in the corridor with every ink pixel the same colour.  (The same
search finds **hundreds** of 100%-*ink* containment placements, which is why
Round 28's ink-only reading looked like a composite; the colours never agree.)

### The engine bulk-loads the whole tileset at level entry

`LOF_I21=1` file-open trace: entering the dungeon opens **all 51 `EGA/D5/DP*`
plus all `EGA/ANIM/AN*`** in one pass (the loader at image `0x1f5c` / wrapper
`0x236e`), so per-view sprite *selection* is an in-memory pointer choice, never
a per-frame file open.  The string table at image `0x3b3e8` (`ega\d0\dp000` ..
`dp069`, `ega\anim\anNN`) is loaded through that wrapper.

### The corridor silhouette is shared art, but the content is per-context

`D0/DP001`, `CHAR1/CH26` and `CHAR2/CH25` all reproduce the corridor *silhouette*
at 46-58% pixel-exact but never its colours (and CH26/CH25 are byte-identical to
each other).  So the same drawing routine produces the corridor shape in several
contexts with different palette content — a **procedural room draw**, not a blit
of a named image.  This fits the engine's own debug strings `Enter
dungeon-number?` / `Enter level-number?` / `Enter room-number?` / `Akt. room:
%d`: a dungeon is drawn **room by room** from the room's tile data.

### Consequence for the drop-in

The model is now: **wall cells blit the pre-scaled sprite (`DP001` at depth 1);
open/corridor cells are drawn procedurally by the room routine.**  The
procedural routine is what BLK-A003's residual needs, and it is inside the
GFA-compiled body (no adjacent lookup table is statically visible near the
loader), so it still needs the routine-level decompilation.  `dungeon_base_view_index`
is unchanged (exact for the wall case, documented fallback for the corridor).

## Round 31 — corrected address map; the view routine and its 6-sprite table (decompilation)

This round fixes the **address map** used in earlier rounds and decodes the view
routine at the instruction level. All offsets below are *image offsets* into
`LOF.EXE`'s loaded image.

### Address map (verified)

For the **main program** segment (`cs = 0x119D` at run time):

    image_offset = reloc_seg * 16 + offset - 0x10100

Cross-checks: the view routine `119D:22A2` -> image `0x3B72` (its bytes match the
routine below); the RLE sprite loader `119D:0A5B` -> image `0x232B` (the decoder
at `0x22F2`). Earlier rounds used `- 0x2F0E0` / `- 0x1010`, which do **not**
reproduce these two sites; the `- 0x10100` form does. The GFA helper segments
(`0x18D`, `0x2F0E`, `0x212F`) are loaded separately and are *not* covered by
this map, so `lcall 0x18d:0x16a8` cannot be resolved with it.

### The view routine — image `0x3B72` (`lcall 0x18d,0x22a2`)

```
inc cs:[0xe5e]                     ; view nesting counter
call 0x30f6 ; call 0x5008          ; save/restore register block
save cs:[0x1414]/cs:[0x1416]; clear them
ax,bx,cx,dx,si = 10,11,0x81,0x82,1 ; viewport rect (10,11)-(0x81,0x82)
lcall 0x18d,0x1fb2                 ; fill that rect (frame)
ax,bx,cx,dx,si = 5,0x87,0x85,0xbf,1
lcall 0x18d,0x1fb2                 ; fill a second rect
cs:[0x376]=10 ; cs:[0x378]=11      ; blit origin (10,11)
dx = 0x3120 ; ds = dx              ; relative data segment
ax = mode * 6 ; bx = ax
dx = [bx + 0x49f3]                 ; *** sprite SEGMENT for this mode ***
cs:[0x39e]=1 ; cs:[0x1fa0]=1
lcall 0x18d,0x3a0                  ; blit the sprite into the viewport
lcall 0x18d,0x1c0
rect (10,11)-(0x82,0x82) ; call 0x312e ; lcall 0x18d,0x1204 ; call 0x312e
dec cs:[0xe5e] ; retf
```

The mode argument is the word pushed at each of the nine `lcall 0x18d,0x22a2`
call sites (0..4). **Mode 3 is the walk/`E` (enter/continue) view**: the site at
`119D:7646` pushes `3` before the call, and `119D:74F0`.. handles movement.

### The sprite table `ds:0x49f3` (runtime `4130:49f3`)

Six 6-byte entries: `[sprite_segment word][0x0100][word]`. Runtime dump
(`LOF_SEGDUMP=4130:49f3:20`):

    entry 0  seg 0x6DCD   entry 1  seg 0x6F8C   entry 2  seg 0x714B
    entry 3  seg 0x730A   entry 4  seg 0x74C9   entry 5  seg 0x7688

**Entry 0 (`0x6DCD`) is `EGA/D5/DP001`, byte-exact.** Dumping the six contiguous
segments (`LOF_SEGDUMP=6dcd:0:0xC000`) and reading the first as a raw EGA image
(magic `0x1234` at offset 0, `w=15` -> 120 px, `h=119`, `size=7150 = 10+4*w*h`)
reproduces all 7140 plane bytes of `EGA/D5/DP001` (100 %). So **the segments hold
*uncompressed* EGA images** (header + 4 planes, no RLE) — the loader at image
`0x232B` is a straight copy, not an RLE decoder, for these. This confirms the
wall case in `dungeon_base_view_index` (ahead tile 6/13 -> `DP001`, index 1) is
sourced, not a guess.

Entries 1..5 were not yet re-dumped at the right instant (they are overwritten
between loads), so their sprite identities remain open; only entry 0 is pinned.

### Corridor view (BLK-A003) — still open, now narrower

The corridor (ahead cell open, tile 0) is **not** entry 0: entry 0 == DP001
(8715 ink) while the corridor is 4941 ink. It is therefore one of entries 1..5,
or drawn by the fill routine `0x18d:0x1fb2`. No `EGA/**` file reproduces it
(best colour match 63 %, best full-size IoU 0.35), consistent with Round 30.
The open work is to catch the memory image of entry 1..5 while it is live; the
table, the loader and the routine are now all located.

## Round 32 — the dungeon view sprite table (index -> dp number) located

`dngs.py`/the port currently treat a "view index" as a DP number directly
(`DP%03d`). The engine does not: it indexes a **table of filenames**. The table
lives in LOF's data segment `0x3120` at offset `0x4A29` (image offset `0x35C29`),
6 bytes per entry `[seg=0][0x0100][filename offset]`, where the filename strings
are the `ega\d0\dpNNN` block at image `0x361E1` (`ds:0x4FEE`). Decoded
index -> file:

```
 0 dp001    1 dp014    2 dp015    3 dp016    4 dp017    5 dp018
 6 dp019    7 dp020    8 dp021    9 dp022   10 dp023   11 dp024
12 dp025   13 dp026   14 dp027   15 dp028   16 dp029   17 dp030
18 dp031   19 dp032   20 dp033   21 dp034   22 dp035   23 dp036
24 dp037   25 dp038   26 dp039   27 dp040   28 dp041   29 dp042
30 dp043   31 dp045   32 dp045   33 dp046   34 dp047   35 dp048
36 dp049   37 dp050   38 dp051   39 dp052   40 dp053   41 dp054
42 dp055   43 dp056   44 dp057   45 dp061   46 dp058   47 dp059
48 dp060   49 dp002   50 dp003   51 dp004   52 dp005   53 dp006
54 dp007   55 dp008   56 dp009   57 dp010   58 dp011   59 dp012
60 dp013   61 dp062   62 dp063   63 dp064   64 dp065   65 dp066
66 dp067   67 dp068   68 dp069
```

The port's `dungeon_base_view_index` returns **dp numbers** (1, 14, 58, 59, 60)
and feeds them straight to `DP%03d`, so it is internally consistent and selects
the right files; it is simply a different (coarser) parametrisation than the
engine's 69-entry table. No port change is needed for the wall/door cases.

The routine that consumes the table is image `0x4198` (runtime `119D:28C8`): it
reads three parallel arrays indexed by a view id — `X=cs:[0x28b6+2i]`,
`Y=cs:[0x28bc+2i]`, `spr=cs:[0x28c2+2i]` — then `dx = ds:[0x4A29 + spr*6]` and
calls the blitter `lcall 0x18d,0x3a0`. This routine did **not** fire in our
headless run, so the wall/corridor screens we captured are drawn by the sibling
routine at image `0x3B72` (the `ds:0x49F3` six-slot sprite cache). Both routines
load sprites into the same segment cache, so which one runs depends on game
state; pinning that state remains open.

### Corridor (BLK-A003) — procedural, now with strong evidence

Exhaustive matching of the captured corridor viewport (frame 697, ink 4941)
against **every** 120x119 `EGA/**` file gives a best colour match of 0.633
(`CHAR2/CH65`, `CHAR1/CH61`) and a best shape-IoU of 0.35 (`DP036`/`DP060`); the
seven full-size `D5` sprites top out at shape-IoU 0.346. No single file (and no
reduced-plane mask) reproduces it. Together with the 3D-perspective structure
visible in the ink map (converging side walls, receding floor), this confirms the
corridor view is **composited/ray-drawn**, not a sprite blit — the port's
procedural DDA renderer (`render_dungeon_viewport`) is the right model.

## Round 33 — the real first-person view routine (img 0x3B72) decoded

The routine that draws the play viewport is at image `0x3B72` (runtime
`119D:3B72`; called via `lcall 0x18d,0x22a2`). Its address is `0x18D0 + 0x22A2`,
NOT `0x22A2` — the relocation applies to the *segment*, so the image offset of a
`lcall SEG,OFF` target is `0x18D0 + OFF` (equivalently `SEG*16 + OFF - 0x10100`
with SEG=0x18D). Every earlier "runtime 119D:22A2 = img 0x22A2" note was off by
0x1900. Sequence:

1. `inc cs:[0xe5e]` (frame counter), `call 0x30f6`, `call 0x5008`.
2. Saves `cs:[0x1414]`/`cs:[0x1416]` (the two EGA write-mode bytes), zeroes them.
3. Draws rect `(0x0a,0x0b)-(0x81,0x82)` with `lcall 0x18d,0x1fb2` (this is the
   top **grid/ceiling** band), then rect `(0x05,0x87)-(0x85,0xbf)` (the
   **floor** band). Both fill with the base colour; `cs:[0x1414]` selects OR vs
   AND per column.
4. Sets the blit origin `cs:[0x376]=0x0a`, `cs:[0x378]=0x0b`.
5. `ds=0x3120; mul cl` (cl=6) → `bx`; `dx = ds:[bx+0x49f3]` → the view sprite
   **segment**; `lcall 0x18d,0x3a0` blits it.
6. Draws another rect and `lcall 0x18d,0x1c0`.

### The six-slot view sprite table (ds:0x49F3, image 0x35BF3)

```
mode 0 -> seg 0x6DCD   mode 1 -> seg 0x6F8C   mode 2 -> seg 0x714B
mode 3 -> seg 0x730A   mode 4 -> seg 0x74C9   mode 5 -> seg 0x7688
```
(Entries also carry a second word 0x0100 and a pointer into the `ega\twn\...`
name block; the segment is the field the blitter uses.) The segments are spaced
447 paragraphs apart. `mode` is the value the view routine computes at step 5
from `call 0x5008` (a 6-register pop helper) and the pushed call argument.

### Ground truth: a solid wall ahead draws DP001 (verified 94.51%)

Dumping all six cache segments at frame 699 of a wall run, `seg 0x6DCD` (= mode
0) is byte-for-byte the wall screen (94.51%, remainder = the rect overlay). The
same segment holds `ega\d5\dp001` (verified byte-exact in Round 31). So the wall
view is a straight DP001 blit — which is exactly what the port's
`dungeon_base_view_index` returns for a wall ahead (`case 0x6/0xd -> 60`, and the
fallback `1`). The port's ahead-cell -> DP mapping is therefore **validated for
the wall case**.

### Corridor still composed

The corridor run's cache holds no valid view sprite (all six segments are stale
`twn`/garbage headers) and no 120x119 EGA file matches it, so the open-corridor
view is drawn by the rect/`0x1fb2` path plus something not captured by the
sprite cache — consistent with the Round 32 "composed, not a sprite" conclusion.
The port falls back to DP001 for open cells (BLK-A003); that fallback is wrong
for a corridor but harmless for the wall/door cells that are the play loop's
common case.

## Round 34 — base-view selection VERIFIED by emulator sweep (ground truth)

Swept the ahead-cell tile of DNG05's spawn (cell 17,4, facing up) over all 16
low nibbles, running the original in the emulator and comparing each captured
viewport against every `D5/DPnnn` file:

```
nibble : best-matching view sprite   port's dungeon_base_view_index
  0    : DP067  57.65% (open)       -> 1   (fallback; BLK-A003)
  1    : DP001  94.51%              -> 1   OK
  2    : DP067  57.65% (open)       -> 1   (fallback; BLK-A003)
  3    : DP001  94.51%              -> 1   OK
  4    : DP058 100.00%              -> 58  OK
  5    : DP059 100.00%              -> 59  OK
  6    : DP036 100.00% (=DP060 in D5) -> 60 OK (ambiguous, see below)
  7    : DP067  57.65% (open)       -> 1   (fallback; BLK-A003)
  8    : DP067  57.65% (open)       -> 1   (fallback; BLK-A003)
  9    : DP067  85.88% (open)       -> 1   (fallback; BLK-A003)
 10    : DP014  99.73%              -> 14  OK
 11    : DP001  94.51%              -> 1   OK
 12    : DP001  94.51%              -> 1   OK
 13    : DP036 100.00% (=DP060 in D5) -> 60 OK (ambiguous, see below)
 14    : DP001  94.51%              -> 1   OK
 15    : DP001  94.51%              -> 1   OK
```

So the port's mapping is **independently confirmed** for every nibble that
selects a real view sprite (1/3/4/5/10/11/12/14/15 and the 6/13 wall case). The
`94.51%` (not 100%) for the DP001 cases is the rect overlay the view routine
paints after the blit, not a mismatch.

The only ambiguity is nibble 6/13: the emulator blits a sprite byte-identical to
`D5/DP036` **and** `D5/DP060` (those two files are byte-identical in theme D5 —
md5 `82699bf5de`). The port returns 60; the engine's index cannot be told apart
in D5. In every other theme `DP036 != DP060` (checked D0–D4,D6,D7), so the port
could differ from the engine there — but the shipped/tested configuration is D5,
where the two are the same image, so the rendered output is correct. Documented
as an open ambiguity rather than a bug.

Open cells (nibbles 0/2/7/8/9) resolve to `DP067` (57.65–85.88%, never 100%) —
this is the composed corridor (BLK-A003), where `DP067` is only a partial fit.
The port falls back to `DP001` there; a better fallback would be `DP067`, but it
still would not be pixel-exact. No change made (it would be a guess, not
decompilation-grounded).

## Round 35 — view routine fully decoded; loader identified

Decoded the remaining helpers the view routine (img 0x3B72) calls, closing the
"is there a hidden second draw path?" question for the *viewport sprite*:

- `lcall 0x18d,0x1c0` (img 0x1A90) is a **CRTC/status retrace wait** (`in al,0x3da`
  / `in al,0x3ba` polling) — no drawing.
- `call 0x30f6` / `call 0x312e` (img 0x30F6/0x312E) are **register save/restore**
  (AX,BX,CX,DX,BP,ES,DS,SS,SI,DI,FLAGS to `cs:[0x180e..0x1824]`).
- `lcall 0x18d,0x1204` (img 0x2AD4) is a **rectangle plane-copy** (a `rep movsb`
  loop over 4 planes) — it moves the just-blitted window, it does not draw the
  corridor.

So the viewport draw is exactly: fill ceiling rect `(0x0a,0x0b)-(0x81,0x82)`,
fill floor rect `(0x05,0x87)-(0x85,0xbf)`, blit `table[mode]`, plane-copy. The
corridor (BLK-A003) is therefore **not** produced by this routine at all — it
must come from the tile-renderer path the view routine drives indirectly, not
from a sprite blit. (This confirms Round 30/32/33 from the other direction.)

### The sprite loader (img 0x1F87, reached via `call 0x06a8`-style call)

```
call 0x226c              ; open file -> handle
cs:[0x688] = handle
ah=42 al=02 int21        ; seek END -> DX:AX = size; cs:[0xa1c] = size (low word)
ds = cs:[0x35a2]         ; source segment (set by caller)
ah=42 al=00 int21        ; seek START
ah=3f cx=0x80e8 dx=0     ; read 33000 bytes into ds:0
ah=3e int21              ; close
call 0x22f2              ; RLE-decode the 33000-byte buffer
cs:[0x68a]++             ; file index counter (preload order)
```

The preloader opens the `ega\d5\dpNNN` files in **table order** (observed:
dp001, dp014, dp015, ... dp060, dp067, dp068, dp069), one per frame, so each of
the 6 cache segments ends up holding its table entry. This explains why the
cache segments are in flux across frames and why a single-frame dump can catch
one mid-load (header not yet written).

## Round 36 — negative result: the 6-slot table is not the depth selector

Tested the hypothesis that the view routine's 6-slot table index (`mode`,
`mul cl` at img 0x3C3A / runtime `119D:236A`) encodes the wall depth, which would
explain the Round-29 depth-scaled views. Built three DNG05 variants with the
first opaque cell at depth 1/2/3 and traced `119D:236A`: **ax = 3 in all three**
(same table slot → same sprite segment 0x730A). So the table index is *not* the
depth. The index appears fixed (=3, "dungeon view") for the walk screen; the
depth-dependent content must be applied by another routine, or the slot segment
is reloaded per cell by the preloader before the blit. This is the same open
question as BLK-A003 — no further progress from static disassembly alone.

## Round 37 — BLK-A003 corridor: definitively procedural, geometry measured

Captured the depth-scaled views (ahead cell at depth 1/2/3/4, DNG05 spawn
(17,3) facing S) via `LOF_EDUMP=` and measured the viewport:

| first opaque cell | ink px | first row | shape |
|---|---|---|---|
| depth 1 | 14280 | 0 | full-screen flat wall face |
| depth 2 | 5111 | 9 | small **trapezoid** in the centre, widening downward |
| depth 3 | 4941 | 18 | corridor, **saturates** |
| depth 4 | 4941 | 18 | identical to depth 3 (byte-for-byte) |

Depth-2 row extents run `(44,55)` at y=12 widening to `(4,115)` at y=114 — a
perspective tunnel: the far wall is a small centred rectangle whose edges
diverge toward the viewer. Depth ≥3 saturates because the far wall is beyond the
horizon.

**Decisive test:** slid every `D5/DP*` + every `ANIM/*` sprite (75 files, any
size ≤ window) over the depth-2 and depth-3 windows at every offset and counted
**colour-exact** coincident ink pixels — best = **0**. So no sprite (full-size or
piece), at any position, shares a single colour-exact pixel with the corridor
view. Combined with Round 30 (template search), this **conclusively rules out a
sprite composite** for the corridor: it is drawn procedurally, per-pixel, by the
room routine. No further static analysis is possible without the room routine's
code location, which is inside the GFA-compiled body with no adjacent table.

BLK-A003 remains open; the port's `DP001` fallback for open cells (documented
guess G-004) is unchanged, as any replacement would itself be a guess.

## Round 38 — BLK-A003 UNBLOCKED: the "procedural" corridor is `EGA/CHAR2/CHnn`

**Decisive experiment.** Zeroed `EGA/CHAR2/CH25` on disk (all-zero 120x119 EGA)
and re-ran the corridor capture: the viewport went from **ink 4941 to ink 0**
(4941 differing px). Same for depth-2: zeroing `EGA/CHAR2/CH40` took it from
**5111 to 0**. So the corridor view is *not* procedurally drawn — it is an
ordinary sprite blit from `EGA/CHAR2/`:

| first opaque cell | sprite file |
|---|---|
| depth 1 (wall) | `EGA/D5/DP001` |
| depth 2 | `EGA/CHAR2/CH40` |
| depth 3+ | `EGA/CHAR2/CH25` |

Why the earlier searches missed it: Round 30/37 only scanned `EGA/D5/*` and
`EGA/ANIM/*`. `EGA/CHAR2/` is a third sprite bank (the room/interior art).
`CHAR2/CH25`'s colour histogram is **byte-identical** to the corridor view's
(exactly 9339/1567/497/616/164/363/1105/259/196/174 across colours
0/2/4/6/7/8/10/12/14/15), and a pure-translation alignment of CH25 onto the
window reaches 4479/4941 = 90.7% colour-exact (best dx=24, dy=-17; the residual
is the ceiling/floor rect bands the view routine paints around it).

**The selection routine.** The CH25 load is issued from runtime `119D:2AC4`
(image `0x4394`) via `lcall 0x18d,0x68c` (the sprite loader, image `0x1F5C`).
The routine starts at image `0x4321` (runtime `119D:2A31`):
```
ds = 0x3120; call 0x5008          ; read args
cmp ax, 0x16
jl  small                          ; ax < 0x16 -> table A, else table B
small: dx = cs:[0x7e2 + idx*2]     ; idx = cs:[0x240e]
       byte [0x5561] = 0x33
big:   dx = cs:[0x7f2 + idx*2]     ; dx = string pointer into the data segment
       dx = cs:[0x2a3e + idx*2] + 0x30
       byte [0x5561] = dl
cs:[0xa06] = cs:[0x2408]           ; sprite destination segment
lea dx, [0x5559]                   ; path buffer (holds "ega\char2\ch25")
cs:[0xa7c] = dx
push cs:[0xa06]
lcall 0x18d, 0x68c                 ; load + blit
```
So the room routine picks a `ega\char2\chNN` filename from a pointer table
indexed by `cs:[0x240e]`, writes it to the buffer at `0x5559`, and blits it. The
remaining sub-task is decoding `cs:[0x240e]` (the room/depth index) and the
`0x7e2`/`0x7f2` pointer tables — a bounded, non-guessy task now that the files
are identified. This supersedes the BLK-A003 "procedural room draw" conclusion.

## Round 39 — the full room-art selection chain (BLK-A003 closed)

Traced the CH-number argument through the whole call graph. Runtime addresses
(image offset = runtime_linear - 0x10100; segment 0x18D, offset as noted):

1. **Caller** `ip 0x2685B` (img 0x2812B) pushes the CH number (`[bp+6]`) and calls
   `lcall 0x18d, 0x2a4e` — the **room routine** (img 0x431E).
2. **Room routine** `ip 0x2A4E`: reads the arg into `ax` (`call 0xce2` ->
   `mov ax,[bp+8]`), then
   - `cmp ax, 0x16` -> `jl` picks table `cs:[0x7e2]`, else `cs:[0x7f2]`;
   - formats the string `ega\char2\chNN` into the buffer at `0x5559` (the `NN`
     digits come from `cs:[bx+0x2a3e]`, i.e. `ax % 10` / `ax / 10`; the literal
     `ega\char2\ch` prefix is at `0x5559`), then
   - `lcall 0x18d, 0x68c` (sprite loader img 0x1F5C) loads + blits it.
3. **Wrappers** `sub_26820` (ip 0x26820): if a cached room differs, push
   `[bp+8]` (=1), `lcall 0x18d,0x19a6` (store the room id in `ds:[0x9540]`),
   `call sub_2680F` (clear) and the room routine.
4. **Room-art callers** (each pushes the CH number then `call sub_26820`):
   - `ip 0x239CE` -> `ax=0x13` (19)
   - `ip 0x24220` -> `ax=0x13` (19)
   - `ip 0x2491B` -> `ax=[bp-8]` — computed from a 0x19a-stride record at
     `es:[bx+di+0x10]`, `imul si,0x4e`, then `es:[bx+di+0x48]` (a per-scenario
     room-art table)
   - `ip 0x27769` -> `ax=0x12` (18)

**Measured CH number vs. dungeon geometry** (DNG05 spawn (17,3) facing S, ahead
cell patched):

| ahead opaque cell | CH number | sprite |
|---|---|---|
| depth 1 (wall, tile 6) | (DP path) | `EGA/D5/DP001` |
| depth 2 | 40 (0x28) | `EGA/CHAR2/CH40` |
| depth 3 | 25 (0x19) | `EGA/CHAR2/CH25` |
| depth 4 | 25 (0x19) | `EGA/CHAR2/CH25` |

So the dungeon-view room art is chosen by a room-index that is a function of the
party's *position* in the level (the same CH is reused across depth 3/4, and the
caller at 0x2491B derives it from a per-scenario record), not purely by depth.
The `CHAR2/CHNN` bank is the interior/room art; `D5/DPnnn` is the door/wall art.

**Status:** BLK-A003 (the "corridor composite" blocker) is **closed** — there is
no procedural composite and no side-piece selection to reverse; the view is one
`CHAR2/CHnn` (or `DPnnn`) blit. Port work remaining is a small table
`(room-index -> CHnn)` plus the existing `render_dungeon_view` blit, which is a
lane-A code change, not a reverse-engineering task.

## Round 40 — the room-art view composition (partial)

Re-captured the corridor (DNG05 ahead cell (17,4)=0) and depth-2 ((17,5)=6)
frames and aligned `CHAR2/CH25`/`CH40` onto them.

**Fixed blit offset.** `CHAR2/CH25` (120x119) lands on the screen at (-14,28),
i.e. viewport (-24,17); the top CH25 rows (0-3) and left columns fall off the
screen. At that offset 4480/4941 = 90.7% of CH25's ink is colour-exact.

**Row structure of the residual.** Per-CH25-row match:
- rows 4-36 and row 100: **100%** colour-exact.
- rows 38-98: a **horizontal band of mismatch that widens to the left as y
  grows** (the `XXXXX` wedge in the diff map) — i.e. the engine overwrites that
  region with its own draw, leaving CH25 visible only to the right.
- rows 104-106: the engine draws over CH25 (the diff shows a solid band).

So the engine's open-cell view = **blit `CHAR2/CHnn` at a fixed offset, then
paint a wedge/band** (the side-wall + far-wall geometry) over part of it. This is
the same `render_dungeon_view` rect-fill structure decoded in Round 35; the
"middle image" it paints is not yet identified.

**Data quirk:** `EGA/CHAR2/CH25` and `EGA/CHAR1/CH26` are **byte-identical**
(same 4039-byte file). So the CH number alone does not pin the bank; the engine's
room routine always reads `CHAR2` (Round 38-39), so `CHAR1` is a duplicate/alias.

Status: the blit offset and the band-occlusion structure are known; the exact
occlusion polygon (and whether the far-wall art is a second sprite) remains open.

## Round 41 — the room-art routine, fully decoded (Task 2)

**The routine** is at image `0x431E` (`119D:2A4E`). Disassembly (image offsets;
runtime offset = image − 0x18D0):

```
431E  call 0x30f6            ; save regs
4321  mov dx,0x3120
4324  mov ds,dx
4326  call 0x5008            ; -> ax = [bp+8] = the room number (NN)
4329  cmp ax,0x16            ; NN < 22 ?
432C  jge 4346
432E  mov bx,cs:[0x240e]     ; room-art INDEX (separate from NN)
4333  shl bx,1
4335  mov dx,cs:[bx+0x7e2]   ; art segment (NN<22 table)
433A  mov cs:[0x2bae],dx
433F  mov byte [0x5561],0x33 ; bank digit '3'
4344  jmp 4363
4346  mov bx,cs:[0x240e]
434B  shl bx,1
434D  mov dx,cs:[bx+0x7f2]   ; art segment (NN>=22 table)
4352  mov cs:[0x2bae],dx
4357  mov dx,cs:[bx+0x2a3e]   ; bank digit table
435C  add dx,0x30            ; -> ASCII '0'..'5'
435F  mov [0x5561],dl        ; overwrite the bank digit in "ega\char_\ch__"
4363  mov dx,cs:[0x2408]
4368  mov cs:[0xa06],dx
436D  call 0x5008            ; -> ax = [bp+8] = NN again
4370  mov bx,1
4373  call 0x4fbb            ; itoa(NN): writes ASCII digits to cs:[0x36de/df]
4376  mov al,cs:[0x36de]
437A  mov ah,cs:[0x36df]
437F  mov [0x5565],al        ; 10s digit of NN
4382  mov [0x5566],ah        ; 1s digit of NN
4386  lea dx,[0x5559]        ; the filename buffer
438A  mov cs:[0xa7c],dx
438F  push cs:[0xa06]
4394  lcall 0x18d,0x68c      ; sprite loader (reads the buffer)
4399.. retf (returns 1 if the cached room changed)
```

**The filename** is assembled in the buffer at `0x5559` as
`ega\char<B>\ch<NN>` where:
- **`<NN>` = the routine's argument** `[bp+8]`, formatted by the itoa helper at
  image `0x4FBB` (divisors `cs:[0x36e1]` = {10000,1000,100,10}) into
  `cs:[0x36de]`/`cs:[0x36df]`, then copied to buffer offsets `+0xC`/`+0xD`.
  Measured: corridor **NN=25**, depth-2 **NN=40**.
- **`<B>` = the bank digit** = `cs:[0x2a3e + cs:[0x240e]*2] + 0x30`. The table
  `cs:[0x2a3e]` is `[0,1,1,1,1,2,2,2,…]`; with the measured `cs:[0x240e]=5`
  this gives `'2'`, i.e. `ega\char2\ch25` — matching the `LOF_I21=1` open trace
  exactly. (For `NN>=22` the routine instead hardcodes bank `'3'`.)

**Callers** of the wrapper `sub_26820` (image `0x280F0`), each pushing NN then
`call sub_26820`:
- image `0x252A5` → NN `0x13` (19)
- image `0x25AF7` → NN `0x13` (19)
- image `0x29040` → NN `0x12` (18)
- image `0x261F2` → NN **computed** (not a constant)

The measured NNs (25, 40) match none of the constants, so the dungeon-view art
is drawn by the **computed caller at image `0x261F2`**, which derives NN from a
per-scenario record:

```
261AD  mov ax,0x19a
261B0  imul word ptr [bp+6]        ; * (some index)
261B3  mov di,ax
261B5  mov es,[0xb06c]
261B9  les bx,es:[0x30]
261BE  mov si,es:[bx+di+0x10]
261C2  cmp si,0x7c5 / 0xffff      ; guard: empty slot
261CD  mov ax,0x4e
261D0  imul si
261D2  mov bx,ax
261D4  mov es,[0xb088]
261D8  les di,es:[0x37b0]
261DD  mov al,es:[bx+di+0x48]      ; <-- NN = this scenario-record byte
261E1  sub ah,ah
261E3  mov [bp-8],ax
261EE  push [bp-8]                 ; NN
261F2  call sub_26820
```

So NN is a byte field at `+0x48` of a `0x4e`-stride record in the scenario data
(`es:[0xb088]` segment, table at `es:[0x37b0]`), selected by an index that
itself comes from a `0x19a`-stride record in the `es:[0xb06c]` segment (field
`+0x10`).

**Status:** the routine, the filename rule (`ega\char<B>\ch<NN>`), the bank
table and the NN formatting are all decoded and verified against the real file
open. Remaining: the exact identity of the `+0x48` scenario field and the
`0x19a`/`0x4e` record semantics (which data file backs `es:[0xb06c]` /
`es:[0xb088]`).

### Round 41 addendum — the runtime caller of the room-art wrapper

The executed caller (traced at linear `0x2C949`, module `1DAF`, corridor case):

```
1DAF:CE86  mov es,[0xafc0]
1DAF:CE8A  mov al, es:[0x259]      ; current location/room id (byte)
1DAF:CE8E  cwde
1DAF:CE8F  mov si, ax
1DAF:CE91  mov ax, 0x11a           ; record stride = 0x11a (282) bytes
1DAF:CE94  imul si                 ; bx = id * 0x11a
1DAF:CE96  mov bx, ax
1DAF:CE98  mov es,[0xafc0]
1DAF:CE9C  les di, es:[0x52]       ; table base pointer
1DAF:CEA1  mov al, es:[bx+di+0x48] ; <-- NN = byte at +0x48 of the record
1DAF:CEA5  sub ah, ah
...
1DAF:CEDB  cmp ax, 0xffff          ; -1 == "no art"
1DAF:CEFB  push [bp-0x40]
1DAF:CEFE  push [bp-0x42]          ; = NN
1DAF:CF01  lcall 0x212f, 0x6e00    ; -> sub_26820 (the wrapper)
```

So **NN = a byte field at offset `+0x48` of a `0x11a`-stride record**, indexed by
`es:[0x259]` (the party's current location/room id), in the scenario data
segment `es:[0xafc0]` (table base at `es:[0x52]`). `NN == 0xffff` means "no room
art". This is the source of the `25`/`40` the room routine formats.

This is the same shape as the static match at image `0x261F2` (a table lookup,
`+0x48` field, `0xffff` guard) — the dungeon view art is a per-location data
lookup, not a depth computation. **Task 2 is complete:** the routine, the
filename rule, the bank table, the NN formatting and the NN source are all
decoded and verified against the real file open (`ega\char2\ch25`).

## Round 42 — Task 2b solved: the NN table is `SYST/NSC<dd>` (verified)

Round 41 ended with the NN read pinned to a `0x11a`-stride record in segment
`es:[0xafc0]`, field `+0x48`, indexed by `es:[0x259]`. Task 2b asked *which data
file backs that segment*. Answer: **`SYST/NSC<dd>`, the scenario NPC/monster
table**, where `<dd>` is the same scenario number as `DNGS/DNG<dd>`.

**Method.** Dumped 16 KiB of segment `0x5C92` at the corridor spawn
(`LOF_SEGDUMP`), located the record base, and matched the record bytes against
every `SYST/*` file. The record's first 19 bytes are the singular creature name;
`NSC05` rec0 is `"Lizard priest"`, and the anchor string at `0x42CC+...` matches
`game/SYST/NSC05` byte-for-byte.

**Disk vs memory layout.** On disk an NSC file is `count` (one byte) followed by
`count` records of **78** bytes; in memory the engine expands each record to
**282 (0x11A)** bytes, but the first 78 bytes are bytewise identical. Verified
for every NSC file: `size == 1 + count*78` (NSC00..NSC07 all conform). So the
room-art byte is at disk offset `1 + i*78 + 0x48` for record `i`.

**Causal proof (decisive).** Patched `NSC05` record 16's `+0x48` byte from `25`
to `30`, re-ran the corridor capture, and the emulator's open trace changed from
`ega\char2\ch25` to **`ega\char2\ch30`** (and read 3958 bytes). Reverting the
patch restores `ch25`. So the NN the room routine formats is exactly this byte.

**What the numbers mean.** `NSC05`'s `+0x48` is a *per-NPC* value: 26 = Lizard
priest, 25 = Lizard warrior, 40 = Ogre, 5/7/8/23/32/34/41/44/45/51 = other
creatures. The Temple spawn's location id (`es:[0x259] = 16`) selects a Lizard
warrior record, whose NN is 25 -> `ega\char2\ch25`.

**Corroboration.** The corridor viewport's colour histogram is *exactly* CH25's
(`{0:9339, 2:1567, 4:497, 6:616, 7:164, 8:363, 10:1105, 12:259, 14:196,
15:174}`), i.e. the viewport contains precisely CH25's 4941 ink pixels. Round 38
already showed zeroing `CHAR2/CH25` empties the viewport; together with the
causal NSC patch this closes the chain end to end: **location id -> NSC<dd>
record -> +0x48 -> `ega\char2\chNN` -> viewport blit.**

**Port + tooling.** `port/dungeon.cpp` now exposes
`dungeon_room_art_number(dir, scenario, location)`, which reads the disk record
directly (validating the count and rejecting out-of-range locations with `-1`).
`tools/dngs.py nsc <file> [location]` dumps the records and their room-art
number. `tests/dungeon_test.cpp::test_room_art` asserts the measured values
(location 16 -> 25, 0 -> 26, 10 -> 25, 35 -> 40) and the error paths.

**Open (out of scope for 2b):** how the engine *uses* the resolved NN to pick
the final blit (the room routine's `ax < 0x16` branch and the two pointer tables
`cs:[0x7e2]`/`cs:[0x7f2]`), and the meaning of the remaining ~200 bytes of the
in-memory 282-byte record.

## Round 43 — the room routine's branch and bank rule, verified

Round 41 gave the routine's shape; reading `img 0x431E` (`119D:2A4E`) opcode by
opcode pins it exactly, and the path template is now located:

- **Template.** `ega\char0\ch00` is a literal in the image at **img `0x36759`**
  (`"...ook\0ega\char0\ch00\0\xff\xff"`). The routine writes the bank digit into
  the buffer's offset +8 and the two NN digits into +12/+13 (via the `itoa`
  helper `call 0x4fbb`, which fills `cs:[0x36de]`/`cs:[0x36df]`).
- **Branch on NN** (`call 0x5008` returns the arg = NN):
  ```
  cmp ax, 0x16
  jge big
  ; NN < 22:  bank digit byte [0x5561] = 0x33 ('3')  -> ega\char3\chNN
  big:
  ; NN >= 22: bx = cs:[0x240e]<<1
  ;           byte [0x5561] = cs:[bx+0x2a3e] + 0x30  (bank digit from table)
  ```
  The bank table `cs:[0x2a3e]` (img `0x430E`) is `[0,1,1,1,1,2,2,2,...]`, so
  with the measured index `cs:[0x240e] = 5` the digit is `'2'` -> `ega\char2`.
- **Verified causally.** Patched `NSC05` rec16 `+0x48` (NN) to **5** and re-ran:
  the emulator opened **`ega\char3\ch05`**. NN=25 gives `ega\char2\ch25`. So the
  `NN < 0x16` branch is real and selects the `CHAR3` bank.
- **The bank index is independent of NN.** `cs:[0x240e]` is *written* by a
  setter at img `0x3CE0` (`call 0x5008; mov cs:[0x240e],ax`) from its own
  argument (and reset to 0 at img `0x3DD0`), and read by the room routine (img
  `0x432E`/`0x4346`) and several other sites. So the full selection is
  `(bank_index, NN) -> ega\char<B>\ch<NN>`, with NN from `SYST/NSC<dd> +0x48`
  and the bank index supplied separately. `dungeon_room_art_path` currently
  fixes the bank at `CHAR2` (the value observed for the DNG05 spawn); threading
  the bank index through is the remaining lane-A step.
- **Pointer-table stores** (`cs:[0x7e2]`/`cs:[0x7f2]` -> `cs:[0x2bae]`) are the
  GFA/VB runtime's active-pointer bookkeeping, not a second sprite bank; the
  only bank effect is the path byte at +8.

## Round 44 — location id confirmed at the exact runtime segment

`LOF_TRACE_IP` matches on `cs*16+ip` (linear), so the Round 41 note's `119D:CE8A`
was really **`1DAF:CE8A`** (linear `0x2A97A`) — the same code, the right segment.
Tracing there for the Temple corridor gives `es=0x468E`. Dumping that segment:

```
es:[0x259] = 16          ; the location id the record is indexed by
es:[0x52]  = 0x42CC      ; the record-table base (matches the earlier dump)
```

So at the spawn the engine indexes `NSC05` record **16** and reads `+0x48` ->
**25** -> `ega\char2\ch25`. This closes the loop at the exact runtime state: the
location id is engine state at the party's current position, not a save byte (it
was 16 here and is not simply the map id). Wiring `dungeon_room_art_number` into
the play view therefore needs that id threaded through from the play loop; the
resolver itself is done and tested.

## Round 45 — CORRECTION: `NSC<dd> +0x48` is the position-selected **encounter/area sprite**, not corridor art

Rounds 38-44 established the chain `position -> SYST/NSC<dd> record -> +0x48 ->
ega\char<B>\ch<NN> -> viewport`. Rounds 38/42 read that as "the corridor room
art". Decoding the routine that *sets* the id (`img 0x27780`, segment `0x3788`,
linear `0x37880`) and re-reading the NSC records shows a sharper picture:

- **`+0x48` is a per-record sprite number, and the records are named creatures.**
  `dngs.py nsc game/SYST/NSC05`: records 0-9 are "Lizard priest" (`+0x48`=26),
  10-17 "Lizard warrior" (`+0x48`=25), 18 "Priest" (`+0x48`=5). So the number is
  *the creature/encounter sprite*, one per NPC/monster record.
- **The id is chosen from the party's map position**, not from a room structure.
  The routine reads the party's map cell (`es:[0x4612]`/`es:[0x4613]`), indexes
  the `NSC<dd>` records (stride `0x11a`), compares each record's `+0x2c` (the
  level/floor field) against the current level, and writes the winning record's
  creature id into the party's `es:[0x259]` (`img 0x27A6A`, `mov es:[si+0x259],
  al`, where `al` came from the record). The room routine then reads that id and
  resolves `NSC05[id].+0x48` -> the CH sprite to display.
- **The captured corridor viewport *is* the selected sprite.** The frame's
  viewport ink is exactly 4941 px == CH25's ink, and Round 38's zeroing test
  (zero `CHAR2/CH25` -> viewport ink 0) plus the NSC05 patch test (patch `+0x48`
  -> both the loaded file *and* the viewport change) confirm it end to end.
- **So the Temple spawn's "dungeon view" is CH25** — the sprite the engine
  associates with the Lizard-warrior encounter at that position. The earlier
  "corridor room art" label was wrong: there is no separate corridor bitmap; the
  engine blits the position-selected NPC/area sprite.

**What this changes:** the mechanism (`NSC<dd> +0x48` -> `ega\char<B>\ch<NN>`)
and its code (`dungeon_room_art_number`, `dungeon_room_art_path`) are unchanged
and still correct. What was wrong was the *interpretation* (room vs creature).
`port/dungeon.{h,cpp}` comments and the test wording are updated to say
"position-selected encounter/area sprite".

**Still open (lane A, next):** the exact `position -> record index` rule inside
`img 0x27780` (the `+0x2c` level test and the map-cell lookup through
`es:[0x54]`), which is what a drop-in needs to pick the sprite for a walk move
without the emulator. Until then the play view keeps the `DP001` fallback.

## Round 46 — the id->sprite mechanism, end to end (and what is left)

Full chain, now traced in both directions:

1. **Producer** (`img 0x276CA`, the routine R): reads the party's map cell
   (`es:[0x4612]`/`es:[0x4613]`, segment `ds:[0xb08a]`), walks the `NSC<dd>`
   records (stride `0x11a`), tests `+0x39` (present flag) and `+0x2c` (an
   ordering/level key) against the current context, and writes the selected
   record's id into the party's `es:[0x259]` (`img 0x27A6A`) together with
   `es:[0x25a]` (from the record's `+0x4c`) and `es:[0x25d]`.
2. **Consumer** (`img 0x1A876`): `al = es:[0x259]`; `si = al * 0x11a`;
   `al = es:[di + si + 0x48]` where `di = es:[0x52]` (the record table base).
   That byte is the sprite number. It caches it in `[bp-0x42]` and, when it
   changes, calls the loader (`lcall 0x212f:0x6e00`).
3. **Sprite**: the loader formats `ega\char<B>\ch<NN>`; `B` is 3 for `NN < 0x16`
   and the runtime bank (2 at the DNG05 spawn) otherwise. So `NSC05[16].+0x48 =
   25` -> `ega\char2\ch25`, exactly the file the emulator opens and the sprite
   whose 4941 ink pixels equal the Temple-spawn viewport.

Records are named creatures, so `+0x48` is a per-record **creature sprite**
(NSC05: Lizard warrior -> 25, Lizard priest -> 26, Priest -> 5, Orc -> 41, ...).

**Still open (lane A).** The producer's `position -> record id` rule is not yet
pinned: at the Temple spawn `es:[0x259] = 16` and `NSC05[16]` is a Lizard warrior
(`+0x2c=3`, `+0x44=65`), while the only records whose placement byte `+0x44`
equals the spawn cell 125 are 60-62 (Priest, `+0x2c=5`, sprite 5). So the id is
*not* a plain "record whose placement cell == party cell" lookup — it depends on
the runtime record array R builds (with the `+0x39`/`+0x2c` tests), which needs
the encounter state. Until that rule is derived the play view cannot pick the
sprite for an arbitrary walk move without the emulator, so it keeps the `DP001`
fallback.

**Delivered for the drop-in.** `dungeon_encounter_sprite(dir, scenario, location)`
returns the exact sprite path for a known id (test covers 16->CHAR2/CH25,
0->CHAR2/CH26, 18->CHAR3/CH05, and the error cases). The play view needs only the
id; the mechanism it feeds is complete.
