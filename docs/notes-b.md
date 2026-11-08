# notes-b — Workstream B (characters / items / combat)

**Claim (2026-11-08):** Agent B. Working on `SYST/SPEC0..SPEC7`, `ROST/ROST`,
`ROST/SHOP`. Own files: `port/char.{h,cpp}`, `tools/spec.py`,
`tests/char_test.cpp`, this file.

## Goal (from WORKSTREAMS.md)

Decode character/monster/item statistics and combat tables, expose them as
native structs.

**Definition of done:** `port/char` parses `SPECn` into typed records with
plausible fields (verify against the emulator's reads with `LOF_I21=1` +
`LOF_READ=1`), a `char_test` asserts record counts and a known value, and a
`--dump-spec` debug command prints them.

## Log

- (start) Game data restored via `./bootstrap.sh`. Workstream B is unclaimed:
  no `port/char.*`, no `tests/char_test.cpp`, no `work/*` branch, no open PR.

## Format findings (from data + `analysis/lof.asm`)

### `SYST/SPEC%ld` — scenario "special" record table (the main B deliverable)

- File = **1 count byte + N x 238-byte records** (238 = `0xee`). Verified
  exactly for all 8 files (e.g. SPEC0: `1 + 24*238 = 5713`). The record stride
  `0xee` is confirmed in the disassembly: `mov ax,0xee; imul [bp-0x44];
  les bx,[0x7d04]; ... es:[bx+si+OFF]` — `[0x7d04]` is the loaded-SPEC buffer.
- LOF loads it as `SYST\SPEC%ld` (format string at image `0x49dd7`, `%ld` is the
  scenario index).
- Fields confirmed by disassembly accesses `es:[bx+si+off]` on that buffer:
  - `+0x0b` / `+0x0f` : flags set/cleared per event.
  - `+0x48` (byte)      : used as argument to `lcall 0x212f:0x6e00`.
  - `+0x4b` (byte)      : boolean flag (0/1).
  - `+0x4c` (word)      : value, read for type 7.
  - `+0x4e` (word)      : compared `< 0`; read into a 32-bit accumulator.
  - `+0xc9` (byte)      : index; `0xff` = none, else 0..12. Read for the
    "you feel a strong force" prompt and compared `== 0xff`.
  - `+0xda` (word)      : value.
  - `+0xe2` (word)      : **behaviour/dispatch type** — compared against
    0, 1, 4, 7, 9, 0x0c, 0x0e, 0x11, 0x19. This is the record's event type.
  - `+0xe4` (word)      : counter (`dec`, compared `>= 1`).
  - `+0xe6`, `+0xe8` (word): two more arguments used by type 7.
- Text: two NUL-terminated strings.
  - `text_a` at `+0x0e`, region `[0x0e,0x4a)` (max 60 bytes; longest seen 59).
  - `text_b` at `+0x50`, region `[0x50,0xc6)` (max 118 bytes; longest seen 117).
  - Bytes `0x00..0x0d` (except a few short "key" strings) and `0xc6,0xc7,0xc8`
    are zero padding.

### `ROST/ROST` — the 11-strong party roster

- File = **1 count byte + N x 414-byte records**. `1 + 11*414 = 4555` exactly.
- `+0x00` = present flag (0x01), `+0x01` = C-string name (max 13 bytes; also a
  short "er"/"r"/"ter" suffix string at `+0x08`). The rest is stats + equipment
  (per-record 414-byte block; many zero bytes).

### `ROST/SHOP` — 190 bytes

- Byte 0 = 0 header, then **185 signed bytes, one per item in item-table order**
  (the engine reads 186 then 4; the first 33 line up exactly with the item
  classes: Club `-10`, War scythe `-2`, … Full plate `-8`, Small shield `-1`).
  Values are in `-1..-10` (Club alone is `-10`), so it is a per-item shop
  adjustment / availability table; the rest are `0`.
- Last word `0x1606` at offset 186 (the separately-read 4-byte trailer).

### Progress

- [x] SPEC count/stride + all field offsets verified against `analysis/lof.asm`.
- [x] ROST count/stride verified (11 x 414).
- [x] `port/char.{h,cpp}`, `tools/spec.py`, `tests/char_test.cpp` (wired in
      `tests/local.mk`).
- [x] `make test` green: synthetic layout test + real-data assertions on
      SPEC0/SPEC6/SPEC7/ROST/SHOP (counts 24/182/41, record names, SHOP values).

### Correction (verified)

The offsets `+0x0b`, `+0x0e`, `+0x0f`, `+0x48` initially suspected as SPEC
fields belong to a *different* structure — an NPC/class buffer with stride
`0x19a` (see `mov ax,0x19a; imul [bp-0x44]` at `1F0A0`) or `0x4e`. They are NOT
on the 238-byte `[0x7d04]` SPEC buffer, and `+0x0e`/`+0x0f` actually lie inside
the `text_a` string. The parser therefore exposes only the fields that appear
in a `mov ax,0xee`-strided function, all of which sit past the text regions.

### Verification

- Record counts match the files byte-exactly: `len == 1 + count*238` for all 8
  SPEC files and `1 + 11*414` for ROST — an independent confirmation of both
  strides.
- The `0xee` stride and the `+0xc9`/`+0xe2`/`+0xe4`/`+0x4c`/`+0x4e` accesses are
  read straight out of `analysis/lof.asm` on the loaded-SPEC buffer.
- The emulator's own INT 21h trace (`LOF_I21=1`) shows it opening
  `SYST\SNSC`; reaching the dungeon/event screens that open `SYST\SPECn`
  requires scripted menu input (an open follow-up in HANDOFF.md), so the SPEC
  ground truth here is the static disassembly + exact file-size match.

### Request for the maintainer (do not edit `port/lof.cpp` myself)

`lof.cpp` is `[SERIAL]` and owned by the maintainer. It now already exposes
`--dump-spec` (thanks, maintainer, PR #6). No further front-end change is needed
for the current B scope.

## Unblock + oracle grounding (after maintainer PR #6/#7)

The maintainer landed `O`+Enter → town (the first in-game state) and wired
`--dump-spec`. B is now grounded against real in-game reads:

- Oracle trace (`LOF_I21=1`, `--keys 0x18,0x1c`) shows the engine reading
  `ROST\ROST` as **11 × (410 + 4) bytes** and `ROST\SHOP` as **186 + 4** bytes.
  This *independently confirms both strides* (414 and 190) that I derived from
  the file sizes — the split read (410 then 4) is exactly the 4-byte tail I saw
  at offsets 410..413.
- The event dispatcher in `analysis/lof.asm` at `1F740` confirms the SPEC layout
  on the 238-byte buffer `[0x7d04]`: it prints `text_b` at `+0x50`, and for
  dispatch **type 0x19** reads `index_c9` at `+0xc9`
  (`mov al, es:[bx+si+0xc9]; cwde; ... call`). Both match my parser.

### SPEC dispatch-type semantics (`+0xe2`, oracle-derived)

Correlating the type word with each record's text across all **665** records
(SPEC0..7) gives a clean 1:1 behaviour mapping. The engine's explicit handler
tests are 1, 7, 9, 0x0c, 0x0e, 0x11, 0x19; the rest (0,2..6,8,10,12..18,20..22,
24) are simple text/scenery records. Encoded as `SpecType` +
`spec_type_name()` in `port/char.{h,cpp}`:

| type | count | meaning | evidence |
|-----:|------:|---------|----------|
| 0  | 586 | pure description (scenery, gravestones) | dominant; text only |
| 1  | 9 | recruit ("may I join you (Yes/No)") | text_b |
| 2  | 9 | spring/fountain | text_b |
| 3  | 3 | spring breaking through ground | text_b |
| 4  | 1 | battlefield | text_b |
| 5  | 1 | rumbling passageway | text_b |
| 6  | 1 | (no text) | — |
| 7  | 10 | "would you like to go through?" door | handler `cmp …,7` |
| 8  | 12 | trade for gold (Yes/No) | text_b |
| 9  | 1 | notice/sign; `arg_4c` = text id | handler `cmp …,9` |
| 10 | 1 | deep/clear spring | text_b |
| 11 | 7 | teleport | text_b |
| 12 | 1 | "press it?" button | handler `cmp …,0xc` |
| 13 | 2 | inscribed stone / cauldron | text_b |
| 14 | 1 | figure blocks the way | handler `cmp …,0xe` |
| 15 | 3 | (often no text) | — |
| 16 | 1 | hazardous crossing (lava) | text_b |
| 17 | 4 | item/puzzle interaction | handler `cmp …,0x11` |
| 18 | 2 | damaging trap | text_b |
| 19–22 | 1 each | button variants | text_b ("Click.") |
| 24 | 1 | massive stone door with a circle | text_b |
| 25 | 5 | elemental guardian riddle; `index_c9` selects it | handler `cmp …,0x19` + reads `+0xc9` |

`SPEC0[15..18]` are the four elemental guardians with `index_c9 = 6,7,8,9`
(Water, Earth, Air, Fire). Index 10 (Woodkeeper) also appears, matching the SNSC
class order for classes 5..13.

### SPEC field semantics grounded in the dispatcher

The event dispatcher (`analysis/lof.asm` ~`1F88F`, `1F98C`, `20AD7`) confirms the
record field meanings on the 238-byte buffer:

- `+0x4e` (`arg_4e`) is read **signed** and `jl`-gated (`cmp word …,0; jl`):
  negative (`-1`) means "no text window"; the engine then reads `+0x4c`
  (`arg_4c`) and pushes it as the text id for the window it opens.
- `+0xe4` (`counter`), `+0xe6` (`arg_e6`) and `+0xe8` (`arg_e8`) are the type-7
  (door) handler's countdown/target fields.
- Record init (`1B95A`..) zeroes `+0x4a/+0x4b/+0x4c` and sets `+0x4e = 0x64`,
  confirming the field map at the head of the 0xa4..0xee region.

In this release `arg_4e` is `-1` in **659 of 665** records (only six have a value:
four are 3, two are 0) — asserted in `char_test`.

### Saved games (`GAMES/GAMEn`) — new

The engine's save system is grounded from `LOF.EXE` strings: `GAMES\GAME%d`
with prompts "Load which gamestanding:", " is not a valid game:",
" gamestanding loaded!" (and the save mirror). A save file is:

    off 0  : header `00 46 4F 4C`  (0x00 'F' 'O' 'L')
    off 4  : N x 410-byte (0x19a) character records — the SAME struct ROST uses
    end    : trailing block (10 bytes here)

GAME1..GAME5 are all 13134 bytes = 4 + 32*410 + 10, i.e. **32 character slots**.
The six party members appear first: Decker, Siegurd, Handy, Merlin, Cassandra,
Assanla (present = 0x01). `SaveGame`/`load_save()` in `port/char.{h,cpp}` parse
this; `tools/spec.py --save GAME1` dumps it.

### 410-byte character struct — grounded offsets

The character-init / erase routines (`analysis/lof.asm` `1B95A`..`1BA8E`, over
the array `es:[0x30]` with the usual `imul …,0x19a`) read/write these directly:

| off | size | note |
|----:|-----:|------|
| 0x00 | byte | `present`; non-zero = slot in use |
| 0x01 | 14 | NUL-terminated name |
| 0x10 | word | erase pass runs only when `!= -1`; **live** chars carry `-1` |
| 0x12 | word | paired with 0x10 (`> 0`); cleared with the slot |
| 0x1d | byte | set to `0x0a`, overridden to `5*level46-0x41+1` when `level46>0x0f` |
| 0x46 | byte | clamped/derived stat driving 0x1d |

Exposed as `CharOffset` in `port/char.h`; `char_test` asserts them on a save.
The rest of the struct (the play-loop stat view at `es:[bx+0x27xx]`) is only
reachable from the main loop and is left to the maintainer.

### Item table (transcribed from LOF.EXE) — new

The item table is **not** in any data file: the engine keeps it inside `LOF.EXE`
at image offset `0x42cec` (data segment `0x2cec`) as **185 records of 32 bytes**:

    off 0 : 16-byte NUL-padded name ("Club", "Short sword", …)
    off 16: seven little-endian int16 fields
    off 30: byte item class, byte use-effect index

Item class (off 30): 0 misc/key, 1 melee, 2 armour, 3 shield, 4 projectile,
5 staff, 6 scroll/book/map, 8 ring, 9 potion, 10 treasure, 11 bow/sling.
Off 31 is **not** an equip slot: it is 0 for ordinary gear and `1..40` only on
magic/consumable items (potions, rings, wands, special weapons), i.e. a
use-effect index.

Because the drop-in must run without the original EXE, the table is transcribed
into `port/char_items.inc` (regenerate with `tools/spec.py --emit-items`, which
reads the EXE once as a dev aid) and exposed as `items()` / `item_by_index()` /
`dump_items()` in `port/char.{h,cpp}`.

Field guesses (weapons line up cleanly; armour's third field is the raw AC bonus):

- melee weapons (class 1): `f0` = hands, `f1` = damage, `f2` = cost,
  `f4` = class/penalty mask; e.g. Short sword `(1,10,50,…,1536)`, two-handed
  sword `f0 = 2`.
- armour (class 2): `f1` = hands-ish cost, `f2` = raw AC, `f3` = equipped AC;
  e.g. Leather `(4,15,150,35)`, Full plate `(1,2000,650,90)`.
- projectiles (class 4): `f3` = ammo count; e.g. Bullets `(2,2,2,2560,1024…)`.
- `f6` is a class-eligibility mask (magic items set the high bits, e.g. 4095).

### Handoff notes for the play loop (maintainer domain)

- Character struct: only the head is written by the init/erase routine; the rest
  (stats, inventory, equipment, spell list) is read/written through the play
  loop's global view `es:[bx+0x27xx]` — hundreds of `mov ax,0x19a` sites follow
  the town/shop code.  Naming those fields needs the live cursor/loop state, so
  it is left to the maintainer rather than guessed.
- Spells are stored as a **name pool** (NUL-terminated strings around image
  `0x44478`..), not as fixed records like the item table; their effects live in
  the interpreter code next to each name, so there is no data-only spell table
  to transcribe.
- The item table (above) is indexed by `shl ax,1` twice + `add` (×32), consistent
  with the max record size seen on a single stack frame.

### ROST record structure (refined)

The file stride is 414, but the engine treats a character as a **410-byte
(0x19a) struct plus a separate 4-byte word** — the loader reads 410 then 4 per
record (`LOF_I21` trace) and the runtime character stride is the pervasive
`imul …,0x19a`. `RostRecord` now exposes that 4-byte `tail` (little-endian):
Connar=0x68a, Siegurd=0x592, etc.

### SNSC is confirmed (already parsed by `port/data.cpp`)

The 78-byte SNSC stride is confirmed in the loader at `analysis/lof.asm`
`1B1E1`: `mov ax,0x4e; imul [bp-2]; add ax,[es:0x37b0]; …` loops
`cmp [bp-2],0xe` (14 classes). `port/data.cpp` already reads it; B reuses it and
does not duplicate it. The class order is exposed as `kClassName[]` (Siegurd,
Eljot, Ihl Kazar, Mask Bearer, Dragon, Earth, Fire, Water, Air, Angel,
Woodkeeper, Phantomfighter, Warrior, Werewolf) — the same order the item `f6`
eligibility mask and SPEC `index_c9` use.

The 40-byte statistics sub-block (record bytes 38..77) is a class template: a
PC (Siegurd/Eljot) starts from a 14-value seed vector
(`02 00 01 03 01 01 0c 0a 41 00 0c 00 01 01`), while monsters (Dragon, Warrior,
Werewolf) carry combat stats inline. The named-field layout is only readable
through the play loop (the runtime stat struct at `es:[bx+0x27xx]` is in the
maintainer's main-loop domain), so only the low-risk offsets are named and the
rest is left as a follow-up.

### Status after unblock

- [x] Oracle-confirmed ROST (414 = 410+4) and SHOP (190) strides; ROST `tail`.
- [x] SPEC dispatch-type semantics (all 25 observed values) + `spec_type_name`.
- [x] Save-game (`GAMES/GAMEn`) parser + format (header `\0FOL`, 410 stride).
- [x] Item table (185 x 32 bytes) transcribed from LOF.EXE -> `port/char_items.inc`.
- [x] Tests assert element types/indices, the 665-record total, ROST tail values,
      all-present roster slots, and the save header/records/trailer.
- [ ] Remaining B: named stat-field semantics of the SNSC 40-byte block and the
      410-byte character struct — both need the play loop (maintainer domain).
