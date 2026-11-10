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




