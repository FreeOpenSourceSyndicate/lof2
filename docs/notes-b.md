# notes-b — Workstream B (characters / items / combat)

**Claim (2026-11-08):** Agent B. Working on `SYST/SPEC0..SPEC7`, `ROST/ROST`,
`ROST/SHOP`. Own files: `port/char.{h,cpp}`, `tools/spec.py`,
`tests/char_test.cpp`, this file.

**Claim (2026-11-08, branch `work/agent-b-snsstats`):** decoding the SNSC 40-byte
statistics block: the character-init routine's field mapping from an
`SNSC`/`NSCnn` record into the runtime 410-byte character struct, grounded in
`analysis/lof.asm` `1DA8E..1DCBB` and verified against `GAMES/GAMEn` +
`ROST/ROST` (Siegurd == class 0).

**Log (2026-11-08, party-status work):** the SNSC/NSC init routine was re-decoded
from the **raw image** (file offset `0x22F80..0x231A0`; the listing labels it
`1DA8E`) after finding desync gaps in the listing.  It seeds more than the five
fields previously recorded: `CharSeed` now also carries `c00` (+0x00 = 1),
`c14` (+0x14 = `rec[0x3a]`), `c1b` (+0x1b = 0x0c), `c64` (+0x64 = 0x1324) and
`c4e` (+0x4e = 100), with `kSnsField3a` and `kCharSeed14/1b/4e/64` added.
The character-sheet builder was also located from the raw image (file offset
`0x2BC8B`, listing `2676E`) and its `Weight`/`Rations`/`Gold` layout decoded (see
the sheet section below).  (An earlier note here called `+0x68`/`+0x69`
"inventory slot indices" — that was wrong; they are the maximum/current magic
points.  See the correction and the inventory-array section.)

**Log (2026-11-08, branch `work/agent-b-23`):** the SNSC seed mapping now also
carries the four `(value, level)` skill pairs — `char +0x142+2n = rec[0x3b+2n]`,
`char +0x143+2n = rec[0x3c+2n]`, n = 0..3 — copied by the init routine's tail
loop (1DC00/1DC40, confirmed from the raw EXE bytes).  Added `kSnsSkill3b`/
`kSnsSkill3c` + `kCharSkill142` to `SnsOffset`/`CharOffset`, extended `CharSeed`
with `c142[4]`/`c143[4]`, taught `tools/spec.py --sns` to print the pairs, and
asserted them in `char_test` (synthetic + real Ihl Kazar / Mask Bearer).

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
- (2026-11-08, branch `work/agent-b-items`) Decoded the character attribute
  block `+0x44..+0x48` and the level field `+0x17` from the sheet builder
  (`0x2676E`) and the training/level-up routines (`0x9144`, `0x95B0`), resolving
  the earlier `+0x46` "level vs Dexterity" conflict: `+0x46` is **Dexterity**,
  the level is `+0x17`.  Extended `CharSheet`/`CharOffset` in `port/char.{h,cpp}`,
  added `decode_char` coverage + a synthetic projection test in `char_test`, and
  taught `tools/spec.py --save` to print the attributes.  `make test` passes.
- (2026-11-08, branch `work/agent-b-exp`) Decoded the 32-bit **experience**
  accumulator at char `+0x6c` (low word) / `+0x6e` (high word): every reward
  site adds to it with an `add`/`adc` pair (e.g. `0x412`, `0xf28`) and the
  reward printer pushes the two words as one dword.  The character-sheet label
  block carries "Experience " + `%011ld` (DS:0xaf83) and the party table has an
  "Exp." column.  Added `kCharExp6c`/`CharSheet::exp` + `rd32`, projected it in
  `decode_char`, printed it in `tools/spec.py --save`, and asserted it on the
  synthetic struct and the live `GAMES/GAME1` characters (Decker 379, Siegurd
  699, Handy 231).  `make test` passes.
- (2026-11-08, branch `work/agent-b-weight`) **Resolved the item-table field
  `f2` (record `+0x14`): it is the item WEIGHT**, not cost/AC — proven by the
  carried-weight accumulator (image `0x1BB32`), the load check
  (`item+0x14 + char+0x66 <= char+0x64` at `0x19B54`/`0x2965f`) and the sheet's
  `Weight:` line, and reproduced exactly on 10/11 ROST characters and every named
  GAME1..3 character.  Renamed `ItemRecord::f2 -> weight`, added
  `kCharWeight64/66` + `CharSheet::weight_max/weight_cur` + `carried_item_weight`,
  corrected the item-field notes, and asserted it in `char_test`.  `make test`
  passes.

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

- Byte 0 = 0 header, then **185 stock bytes, one per item in item-table order**
  (the engine reads 186 then 4).  This is the emporium's **stock table**, not a
  price adjustment: `0` = the shop does not carry the item, `1..254` = finite
  units on hand, `0xFF` = the "always in stock" sentinel.
- Grounded from the buy/sell routines: the buy path reads the slot
  (`mov al, es:0x4616(bx)`, image `0xc130`) and decrements it
  (`dec es:0x4616(bx)`, image `0xc146`); the sell/restock path increments it
  (`inc es:0x4616(bx)`, image `0xb64f`), and refuses only once the count would
  reach `0xFF` (`cmp byte es:0x4616(bx),0xff; jb ok`, image `0xb644`).  So a
  larger value means *more* units, not a bigger discount.
- The shipped shop carries **items 1..44 only** (index 45 onward is all 0), e.g.
  Club 246, Hand axe 252, Full plate 248, and 24 items at `0xFF` (Short sword,
  Plate mail, Short arrows, ...).  Loader: `lcall 0x18d:0x2bb2` with path
  `ROST\SHOP` reads 186 bytes to `DS:0x4616`, then 4 bytes to a local.
- If `ROST\SHOP` cannot be opened the loader zero-fills the 186 bytes and then
  sets `0xFF` for every index `< 0x2d` (= 45) except 0 (image
  `0xa72f..0xa77b`) — a fresh game stocks the first 44 items, matching the file.
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
- `+0xeb` (`class_eb`) is an SNSC class index (`0xff` = none): the consumer at
  `1F86B` scales it by 78 into the SNSC base and reads `stats[0x0a]`.  Only 3 of
  665 records set it (0, 1, 2).
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
the array `es:[0x30]` with the usual `imul …,0x19a`) read/write these directly.
The attribute block and level field are grounded independently in the character
**sheet builder** (`0x2676E`) and the **training/level-up** routines
(`0x9144`, `0x95B0`):

| off | size | note |
|----:|-----:|------|
| 0x00 | byte | `present`; non-zero = slot in use |
| 0x01 | 14 | NUL-terminated name |
| 0x10 | word | erase pass runs only when `!= -1`; **live** chars carry `-1` |
| 0x12 | word | paired with 0x10 (`> 0`); cleared with the slot |
| 0x17 | byte | **level / rank**; level-up does `inc es:[bx+si+0x17]` (0x9144) then prints "X enters the %ld rank." (DS:0x4ea6); sheet prints it under "Rnk " |
| 0x18 | byte | class flag: 1 PC / 2 monster (SNSC seed) |
| 0x1b | byte | **profession index** 0..11 into `kProfessionNames` (the display routine at image `0x2FB4A` reads it, rejects `> 0x0b` and switches on it via the jump table at `CS:0x93B6`); the SNSC seed writes `0x0c` = "no profession" (only Siegurd, an unfinished slot, keeps it) |
| 0x1c | byte | base stat 45..75 (SNSC seed) |
| 0x1d | byte | set to `0x0a`, overridden to `5*dex46-0x41` when `dex46>0x0f` (1BA40) |
| 0x1e | byte | **total defence / armour class**: seeded `100 - rec[0x2d]`, then the armour routine (0x210E8) overwrites it with `stat1d + Σ equipped-item AC`; the sheet reads it as the defence stat |
| 0x20 | word | maximum hit points |
| 0x22 | word | current hit points |
| 0x6b | byte | **used inventory-slot count** (0..48); add routine `inc es:[bx+si+0x6b]` (`0xc11a`/`0x2ed8a`), remove `dec` (`0x2e36b`); the armour routine (0x210E8) loops the `+0x82` slots only up to this. Equals the non-empty-slot count in every shipped record. Exposed as `CharSheet::inv_used` |
| 0x6c | dword | **experience** (32-bit: low word `+0x6c`, high word `+0x6e`) |
| 0x44 | byte | **Constitution** |
| 0x45 | byte | **Strength** |
| 0x46 | byte | **Dexterity** |
| 0x47 | byte | **Intelligence** |
| 0x48 | byte | **Wisdom** |
| 0x56 | byte | speed 1..4 (SNSC seed) |

**Attribute block (`+0x44..+0x48`) — this corrects an earlier note that called
`+0x46` "level".** `+0x46` is **Dexterity**, not level; the level is `+0x17`.
Evidence (all in `analysis/lof.asm`, cross-checked against raw EXE bytes):

- The sheet builder copies the 410-byte struct onto its stack (`repne movsw`
  0xcd words at `2679D`), so `[bp-0x19e+off]` *is* char field `+off`.  It then
  prints, in order: "Str " (`+0x45`), "Con " (`+0x44`), "Dex " (`+0x46`),
  "Int " (`+0x47`), "Wis " (`+0x48`) — each followed by its `%02d` value read
  from exactly that stack slot.
- The training routine (`0x95B0`..) increments `+0x45` for "Strength improves!",
  `+0x46` for "Dexterity improves!", `+0x48` for "Wisdom improves!".  There is
  no "Level improves!" string (level is raised by the `inc +0x17` at `0x9144`).
- The Con→HP bonus in the char-init pass (`0x856D`..`0x85AD`) reads `+0x44`
  (`Con > 14` adds `Con-14` HP; the `+0x17` read just above it is doubled for
  HP).  `+0x17` is the level, `+0x1f` gates the bonus (0 = a player character).

Exposed as `CharOffset` + `CharSheet` in `port/char.h`; `char_test` asserts them
on a save and on a synthetic struct.  The rest of the struct (the play-loop
stat view at `es:[bx+0x27xx]`) is only reachable from the main loop and is left
to the maintainer.

The sample save `GAMES/GAME1` reads cleanly: Decker rnk 1 / Con 14 / Str 19 /
Dex 14 / Int 12 / Wis 15; Siegurd (the un-rolled template) is all zeros.

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

Field semantics (`price`, `weight`, `damage` and `f6` are grounded; `f0`/`f3`/
`f4`/`f5` are only partially named — their exact combat/shop use lives in the
play-loop code, so treat them as unverified):

- **`f1` (record `+0x12`) = item price in gold.**  The equipment list prints it
  with `%4d$` (image `0xc474`, format `%4d$ %3dlb` at DS:0x44ab) and the shop
  buy/sell routines read the same field (image `0xb745` / `0xb122`, with
  condition/Charisma scaling).  Values run 5..3800.
- **`f2` (record `+0x14`) = item weight.**  Grounded three ways (see the
  "item weight" section below): the carried-weight accumulator, the load check,
  and the character-sheet `Weight:` line.  This supersedes the earlier note that
  called `+0x14` the *cost* (weapons) / *AC bonus* (armour) — the numbers were
  right, the label was wrong.  Values run 5..3500 (`Club 100`, `Short sword 50`,
  `Broadsword 80`, `Leather armour 150`, `Full plate 650`); the equipment list
  and the sheet display `weight / 10` as pounds.
- melee weapons (class 1): `f0` = weapon class code 0..5 (not a hands count);
  `f4`'s high byte is the **damage** (exposed as `ItemRecord::damage`, subtracted
  from the target's HP at image `0x13ebd`); e.g. Short sword `(f0=1, price=10,
  wt=50, f4=0x0600 => dmg 6)`, Broadsword `(2,18,80,dmg 8)`, Two handed swrd
  `(2,25,456,dmg 11)`.
- **`+0x17` (the HIGH byte of `f3`) = initial quantity / charges.**  The
  item-add routine copies it straight into the inventory slot's count byte
  `+0x84` (`mov al, es:[bx+0xfd3]` at image `0xbf5d`, stored at `+0x84`), so a
  fresh stack starts at this count: arrows/bolts `10`, rations `1`, tinderbox
  `200`; for charged magic items it is the charge count (staves 9..25, e.g.
  Striking staff 23, Polymorph wand 17).  It is 0 for ordinary non-stackable
  gear.  **Consequence for armour:** the armour routine reads only the LOW byte
  of `+0x16` (`mov al, es:[bx+0xfd2]` at image `0x2116b`), so `defense_from_items`
  must mask `f3 & 0xff` — otherwise a charged staff would contribute a huge
  bogus AC.  Exposed as `ItemRecord::charges`.
- armour (class 2): `f3` = AC (scaled by condition, image `0x2116b`), `f4` =
  armour sub-type; e.g. Leather `(f0=4, price=15, wt=150, f3=35)`.
- projectiles (class 4): `+0x17` = ammo count (10, see the charges bullet above);
  `f5` = the paired item's 1-based index (e.g. Short arrows `f5 = 20` = Short bow
  at index 19); the launcher's `f5` points back (Short bow `f5 = 21`).  `f5` also
  pairs Flask of oil <-> Lantern and the quest items (Emerald <-> Corona,
  Ring <-> Keystaff).
- `f6` is a **profession-eligibility mask** (12 bits, max 4095).  The equipment
  list tests it against the character's profession index `+0x1b` as
  `test f6, 1<<char[+0x1b]` (image `0xc539..0xc553`), i.e. bit *n* = profession
  *n* (Warrior..Monk), **not** the 14-entry SNSC class.  `item_usable_by_class()`
  reads it.  Verified on the shipped roster: every item a character has equipped
  passes the mask for that character's profession (Connar/Gorth/… all `True`).

**Use-effect index (off 31).**  Grouping the 185 items by this byte yields a
clean 1:1 behaviour table, so it *is* a use-effect id, not an equip slot.  The
values present in this release (and the items that carry them):

| eff | meaning | example items |
|----:|---------|---------------|
| 0 | plain gear | Club, Warhammer, all ordinary weapons/armour |
| 1 / 2 | enchant weapon / armour | Enchant weapon, Enchant armour |
| 3 / 23 | poison I / II | Poison I, Poison II |
| 4 | gunpowder | Gunpowder |
| 5 | crystal ball | Crystal ball |
| 7 | tool | Spellbook, Holy symbol, Thieves' picks, Anvil, Sledge-hammer |
| 8 | antimagic | Antimagic ring |
| 9 | catchpole hilt | Catchpoles hilt |
| 10 | healing | Healing potion |
| 11 / 12 / 13 / 33 | special/artifact weapon | Inquisitor, Blade of power, Stonesword, Lightbringer |
| 14 | book half | 1st/2nd Bookhalf |
| 15 | quest jewel | The Emerald, The Corona, The Keystaff, The Ring |
| 16 | scroll/ball | Papyrus scroll, Mithril ball |
| 17 / 18 | death mask / sarcophagus | Death mask, Sarcophagus |
| 19 | protection scroll | Scroll, Magic protect., True scroll, maps |
| 20 | holy water | Holy water |
| 21 | dragon armour | Dragon chain/plate/leather |
| 24 | healer staff | Healers' staff |
| 27 | scroll of power | Scroll of power |
| 28 / 29 | protecting / shelter ring | Protecting ring, Ring of shelter |
| 30 / 31 | antidote I / II | Antidote I, Antidote II |
| 32 | spade | Spade |
| 34 / 35 | lantern / torch | Lantern, Torch |
| 36 | talisman | Talisman |
| 37 | climbing gear | Rope, Crystal wedge |
| 38 | strength/food | Strength potion, 12 Rations |
| 39 | map | Blue map, Small map, Old map, … |
| 40 | raw material | Dry clay, Some water |

Exposed as `item_effect_name(effect)` (descriptive label) and
`items_with_effect(effect)` (the indices that carry it); `dump_items()` prints
the label next to each item.  The labels are *descriptive*, derived from the
item names in each group — the exact per-effect handler lives in the play loop
and is left un-named.

### Handoff notes for the play loop (maintainer domain)

- Character struct: only the head is written by the init/erase routine; the rest
  (stats, inventory, equipment, spell list) is read/written through the play
  loop's global view `es:[bx+0x27xx]` — hundreds of `mov ax,0x19a` sites follow
  the town/shop code.  Naming those fields needs the live cursor/loop state, so
  it is left to the maintainer rather than guessed.
- Spells **are** a fixed table (correcting an earlier guess): 141 records of
  36 bytes at image `0x44430` (data segment `0x4430`) — a 22-byte NUL-padded
  name then 14 data bytes.  Records are grouped by caster class (magician,
  druid, priest, ...); the final 14 carry German names (unused/debug).  It is
  transcribed into `port/char_spells.inc` (regenerate with
  `tools/spec.py --emit-spells`) and exposed as `spells()` / `spell_by_index()`.
  The 14 data bytes decode as five little-endian words then four bytes: the
  first word is a **bitmask** (`1,2,4,8,16,32`), byte 10 is small (1..10,
  likely a power/spell level), byte 11 is 2/3, byte 12 is a target flag
  (0 self/buff, 1 single, 2 area) and byte 13 is a modifier.  Their exact use is
  in the play loop, so only the layout is asserted.
  **data byte 8 is a spell-effect code**: grouping all 141 spells by it gives
  clean families (`0x0f` = the seven heal spells, `0x02` = light, `0x04` =
  paralyse, `0x0c` = fear, `0x0e` = summon elemental, `0x37` = map, `0x3e` =
  enchant, `0x00` = plain damage), so it is the id the play loop switches on.
  Exposed as `spell_effect_name(code)` / `spells_with_effect(code)` (labels are
  descriptive, derived from the grouped spell names).
- The item table (above) is indexed by `shl ax,1` twice + `add` (×32), consistent
  with the max record size seen on a single stack frame.

### Character-generation name tables (LOF.EXE)

The "add a new character" flow's choices are baked into LOF.EXE as fixed
tables (transcribed into `port/char_names.inc`, regenerate with
`tools/spec.py --emit-names`; eyeball with `--names`):

| table | offset | records | values |
|-------|-------:|---------|--------|
| sex | `0x41e28` | 2 x 12 | Female, Male |
| race | `0x41e4a` | 6 x 12 | Human, Half-Elf, Elf, Halfling, Dwarf, Half-Orc |
| profession | `0x41e98` | 12 x 12 | Warrior, Barbarian, Rogue, Smith, Scout, Priest, Druid, Magician, Illusionist, Paladin, Healer, Monk |
| alignment | `0x41f28` | 2 x 12 | Lawful, Chaotic |
| skills | `0x41d52` | 9 packed | Negotiating, Attack, Defence, Concentration, Pick-pocketing, Stalking, Trap detecting, Trap disarming, Lock picking |
| languages | `0x41dbe` | 8 packed | Common, Animal, Orc, Lizard, Dwarven, Elven, Dark, Magic |
| status (short) | `0x42190` | 8 x 6 | Good, Mad, Ill, Poisn, Blnd., Stun., Stone, Dead |
| status (long) | `0x421c0` | 8 packed | Good, Mad, Ill, Poisoned, Blind, Stunned, Stoned, Dead |
| gender/profession | `0x41f87` | 12 packed | female Warrior, ..., Priestess, ..., Healer |
| race adjective | `0x4202e` | 6 packed | Human-, Half-Elven-, Elven-, Halfling-, Dwarven-, Half-Orc- |
| alignment (lower) | `0x42013` | 2 packed | "lawful ", "chaotic " |
| profession abbrev | `0x420da` | 13 x 3 | W, Ba, Ro, Sm, Sc, Pr, Dr, Ma, Il, Pa, He, Mn, ?? |
| profession (padded) | `0x42101` | 13 x 11 | Warrior..Monk, then "??" |
| actions | `0x42204` | 5 x 12 | Defends, Attacks, Stalks, Use object, Cast Spell |
| morale | `0x423f8` | 5 x 13 | Good, Satisfactory, Mediocre, Alarming, Bad |
| combat abbrev | `0x421f0` | 4 x 5 | KIL, ATK, DEF, RET |
| scenario title | `0x42240` | 8 x 22 | Valley of Faerghail … The Mountain |
| facing | `0x422f0` | 4 x 22 | Facing North/East/South/West |
| time of day | `0x42348` | 8 x 22 | Midnight, Dawn, Early morning, Morning, Midday, Afternoon, Sunset, Evening |

The scenario titles above double as the location line the play loop blits, so
`scenario_names()` (index = scenario number) names the region each
`SYST/SPECn` / `SYST/NSC0n` pair belongs to.  The adjacent **facing** and
**time-of-day** tables are the map/status screen's heading and clock lines
(each a 22-byte centred cell); exposed as `facing_names()` /
`time_of_day_names()`.

Exposed as `sex_names()`/`race_names()`/`profession_names()`/`alignment_names()`/
`skill_names()`/`language_names()`/`status_short_names()`/`status_long_names()`/
`gender_profession_names()`/`race_adjective_names()`/`alignment_lower_names()`/
`profession_padded_names()`/`action_names()`/`morale_names()`/
`combat_abbrev_names()`/`scenario_names()` with `kSexCount`..`kScenarioCount`.  The 12-byte records are 11 chars + NUL
(zero-padded between groups); the packed tables are NUL-terminated strings
(their trailing spaces are significant — they are fixed-width columns).

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

The SNSC class index a special/NPC record refers to lives in SPEC `+0xeb`
(`0xff` = none).  The consumer at `analysis/lof.asm` `1F86B` does
`imul 0x4e` (78) by `+0xeb`, adds the SNSC base `es:[0x37b0]`, and reads
`es:[bx + si + 0x48]` = `stats[0x0a]` (record byte 48) as a byte argument to
the far routine `212f:6e00` (the same routine used elsewhere to push a
byte/word value).  So record byte 48 is a per-class value the message code
prints/uses; only 3 of 665 SPEC records set `+0xeb`, so it is a rarely used
field.  The other 39 stat bytes have no comparable single-byte consumer here.

The 40-byte statistics sub-block (record bytes 38..77) is a class template: a
PC (Siegurd/Eljot) starts from a 14-value seed vector
(`02 00 01 03 01 01 0c 0a 41 00 0c 00 01 01`), while monsters (Dragon, Warrior,
Werewolf) carry combat stats inline.

#### Decoded: the SNSC/NSC stats block -> character struct (verified)

The class record is 78 bytes; the 40-byte stats block is at record byte 38.  The
character-init routine (`analysis/lof.asm` 1DA8E..1DCBB) copies five of those
bytes into the runtime 410-byte character struct.  The mapping (record offset ->
character offset), grounded in the disassembly and confirmed **byte-for-byte**
against the shipped `GAMES/GAMEn` + `ROST/ROST` saves (Siegurd == SNSC class 0):

| record off | stats idx | char off | transform | meaning |
|-----------:|----------:|---------:|-----------|---------|
| `0x4c` | 38 | `+0x18` | copy | class flag: 1 for PCs, 2 for big monsters |
| `0x2e` | 8  | `+0x1c` | copy | base stat (45..75) |
| `0x2d` | 7  | `+0x1e` | `100 - n` | seeded defence, then overwritten by the armour routine (0x210E8) with the total AC |
| `0x2d` | 7  | `+0x1d` | `100 - n` | same seed, then overridden by the Dexterity rule (1BA40) |
| `0x36` | 16 | `+0x56` | copy | speed (1..4) |
| `0x2c` | 6  | `+0x20`/`+0x22` | `rand(1..n/2) + n/2` | seeds both max and current hit points (`hp_seed`) |
| `0x3a` | 20 | `+0x14` | copy | per-class word value (Siegurd 18) |
| `0x2a` | 4  | `+0x7a+idx` | set 2 | picks which `+0x7a+n` slot is initialised |
| `0x3b+2n` | 17+2n | `+0x142+2n` | copy | skill *value* (n = 0..3) |
| `0x3c+2n` | 18+2n | `+0x143+2n` | copy | skill *level* (n = 0..3) |

The routine also writes fixed values (char `+0x00`=1, `+0x1b`=0x0c, `+0x20`=
`+0x22`=`rand(1..rec[0x2c]/2)`, `+0x64`=0x1324, `+0x4e`=0x64).  The four
`+0x142`/`+0x143` skill pairs come from `rec[0x3b+2n]`/`rec[0x3c+2n]` (the loop at
1DC00/1DC40).  All of these are in `CharSeed` (`c00`/`c14`/`c1b`/`c64`/`c4e` +
`c142[4]`/`c143[4]`) with the `SnsOffset` enum; `tests/char_test.cpp` checks both
the record bytes and the resulting live-character fields.

Example (class 0 Siegurd): `rec[0x4c]=1 -> char[0x18]=1`,
`rec[0x2e]=65 -> char[0x1c]=65`, `rec[0x2d]=10 -> char[0x1e]=90`,
`rec[0x36]=4 -> char[0x56]=4`; class 4 Dragon: `char[0x18]=2`, `char[0x1c]=45`.
The skill pairs are non-zero only for classes 2 (Ihl Kazar), 3 (Mask Bearer) and
4 (Dragon), which is where they are asserted.

The named-field layout for the *remaining* stat bytes (0x26..0x4d) is only
readable through the play loop (the runtime stat struct at `es:[bx+0x27xx]` is in
the maintainer's main-loop domain), so only the low-risk offsets are named and
the rest is left as a follow-up.

### Character-sheet inventory fields (`Weight`/`Rations`/`Gold`) — decoded

The character-sheet builder is the routine that copies the 410-byte struct onto
its stack (`repne movsw`, `cx = 0xcd`) at **file offset `0x2BC8B`** (the listing
labels it `2676E`; again the listing drifts, so the bytes are the ground truth).
Because it copies the struct to `[bp-0x19e]`, a `[bp+disp]` read with
`disp + 0x19e == off` *is* char field `+off`.

The sheet prints, in order: `Rnk ` (`+0x17`), `Hit point ` (`+0x20`/`+0x22`),
`Magic.... ` (`+0x68`/`+0x69`), `State...  ` (`+0x1b`), `Str`/`Con`/`Dex`/`Int`/
`Wis` (`+0x45`/`+0x44`/`+0x46`/`+0x47`/`+0x48`), `Experience ` (`+0x64`/`+0x66`),
`Weight... ` (`+0x64`/`+0x66`), then `Rations.. : ` and ` Gold:`.  The `Hit point`
line is the current/maximum HP pair (`+0x22`/`+0x20`); the `Magic.... ` line is
the current/maximum magic-point pair **`+0x69`/`+0x68`** (see the correction and
the inventory-array section).  The `State...  ` label prints the **profession
name** derived from `+0x1b` (see the `+0x1b` row above and `CharSheet::profession()`);
the display routine at image `0x2FB4A` switches on `+0x1b` (rejecting `> 0x0b`)
to pick the name, so `+0x1b` is a profession index, not a state flag.

The `Rations`/`Gold` values themselves are **not** simple struct fields.  They
are the result of iterating the character's inventory-slot list (a run of
`+0x112`-based slots at a base slot index `+0x5a`), summing a per-item weight and
accumulating a gold count that is rendered with the 32-bit format `%05ld$`.
Decoding that item-table layout (the 185-record, 32-byte table inside `LOF.EXE`
at image `0x42cec`) is a separate follow-up; the sheet's `Rations`/`Gold` are
left unmodelled rather than guessed.

> **Correction (2026-11-08).**  An earlier revision of this section (and of
> `kCharInv68`) called `+0x68`/`+0x69` "inventory slot indices the hit-point and
> magic lines resolve item names from".  That was wrong.  The pair is the
> character's **maximum / current magic points** (`+0x68` = max, `+0x69` = cur;
> the sheet's "Magic.... " line).  The routine at image `0xEBF0..0xEC87` fills
> both from a per-(profession,level) table (`table_seg:[0xa38 + 16*+0x1b +
> +0x17]`, level < 16) plus Wisdom/Intelligence bonuses; the spell routine at
> `0x137AD` clamps the current down to the maximum and the rest routine at
> `0x6440` restores it.  See the "inventory array" section below.

Note `+0x64`/`+0x66` (a 32-bit value the sheet labels `Experience`/`Weight`)
versus `+0x6c`/`+0x6e` (the reward-accumulator dword exposed as `CharSheet::exp`):
they are distinct; the reward sites add to the `+0x6c` pair.

### Monster rosters (`SYST/NSC0n`) — new

The engine's monster table for scenario `n` is `SYST/NSC0%ld` (format string at
image `0x4a363`). The file is a count byte followed by that many 78-byte
records in the **same layout as SNSC** (singular + plural name, then the 40-byte
stats block), so `port/data.cpp`'s `load_nsc()` reads it unchanged. This release
ships scenarios 0..7 (25/36/36/131/80/68/142/60 monsters). B exposes
`monster_names(dir, scenario)` (the singular names, in table order); the 40-byte
stat block is the same un-named template as SNSC and is left as a follow-up.
(`SYST/NSC0n` is deliberately *not* auto-probed as `NSC%02d`, since only
scenarios 0..9 have a single-digit file name.)

### Status after unblock

- [x] Oracle-confirmed ROST (414 = 410+4) and SHOP (190) strides; ROST `tail`.
- [x] SPEC dispatch-type semantics (all 25 observed values) + `spec_type_name`.
- [x] Save-game (`GAMES/GAMEn`) parser + format (header `\0FOL`, 410 stride).
- [x] Item table (185 x 32 bytes) transcribed from LOF.EXE -> `port/char_items.inc`.
- [x] Spell table (141 x 36 bytes) transcribed from LOF.EXE -> `port/char_spells.inc`.
- [x] Magic-point table (12 professions x 16 levels) transcribed from LOF.EXE ->
      `port/char_magic.inc`; `magic_table_value()` + `CharSheet::magic_max()`
      reproduce the stored `+0x68` for every live character.
- [x] 14 SNSC classes exposed as `kClassName[]`; `item_usable_by_class()`.
- [x] Character-generation name tables (sex/race/profession/alignment/skills/
      languages/status) -> `port/char_names.inc` + accessors.
- [x] Character-sheet display tables (female professions, race adjectives,
      lower alignments, padded/abbrev profession columns, actions, morale).
- [x] Monster rosters (`SYST/NSC0n`) exposed via `monster_names()` (reuses
      `load_nsc`).
- [x] Scenario titles + combat abbreviations (`scenario_names()`,
      `combat_abbrev_names()`).
- [x] Item use-effect names (`item_effect_name()`, `items_with_effect()`).
- [x] Spell effect codes (`spell_effect_name()`, `spells_with_effect()`).
- [x] SPEC keyword/password (`SpecRecord::keyword`) + `+0xeb` class reference.
- [x] Status-line facing / time-of-day tables (`facing_names()`,
      `time_of_day_names()`).
- [x] SNSC/NSC 40-byte stats-block -> character-struct seed mapping
      (`sns_char_seed()`, `SnsOffset`, `CharSeed`); verified against the saves.
      Includes the four `+0x142`/`+0x143` skill pairs (`c142[4]`/`c143[4]`,
      `rec[0x3b+2n]`/`rec[0x3c+2n]`), asserted on Ihl Kazar / Mask Bearer.
- [x] Character-struct projection (`CharSheet`, `decode_char()`) plus the
      Dexterity-derived `+0x1d` rule (`char_stat1d_from_dex()`): the HP pair
      `+0x20` (max) / `+0x22` (current) is proven by the sheet builder at
      28054/28067 (`100*cur/max` as a percentage), and `+0x1d` by the routine at
      1BA40 (`<16 -> 10`, else `5*dex-65`).  Both verified against every
      in-use save/ROST character.  `--dump-char [FILE]` prints the sheets.
- [x] Character attribute block `+0x44..+0x48` (Con/Str/Dex/Int/Wis) and level
      `+0x17` (`kCharCon44`..`kCharWis48`, `kCharLevel17`, `CharSheet` fields),
      grounded in the sheet builder (0x2676E) and the training/level-up routines
      (0x95B0/0x9144).  Resolves the earlier `+0x46` "level vs Dexterity"
      conflict: `+0x46` is Dexterity, the level is `+0x17`.
- [x] 32-bit **experience** accumulator `+0x6c`/`+0x6e` (`kCharExp6c`,
      `CharSheet::exp`), grounded in the reward add/adc pairs and the sheet
      label "Experience " + `%011ld` (DS:0xaf83); asserted on the live save.
- [x] Tests assert element types/indices, the 665-record total, ROST tail values,
      all-present roster slots, and the save header/records/trailer.
- [x] Party-status view of the struct: `+0x24` body state, the attribute
      modifiers `+0x5b`/`+0x5c`/`+0x5e`/`+0x5f` and the flag `+0x6a`, grounded in
      the party-status builder `27F4E..280A4` (adds each modifier to a base
      field `+0x1c`/`+0x1e`/`+0x17`; `+0x6a` gates a `+50` to the HP byte) and
      the death routine `1919C`/`191CC`.  Exposed as `kCharBody24`/`kCharFlg5b`…
      /`kCharFlag6a` and `CharSheet::body24/mod5b/mod5c/mod5e/mod5f/flag6a`;
      asserted on a synthetic struct and on `GAMES/GAME1`.
- [x] Character sheet's `Weight`/`Rations`/`Gold` lines: gold is the 32-bit
      `+0x72`/`+0x74` (`kCharGold72`), rations the byte `+0x70` (`kCharRations70`);
      the carried-weight base is `10*rations + gold` (weight routine 0x20F7E).
      `CharSheet::gold`/`::rations70` + `weight_base()`; asserted on Connar.
- [ ] Remaining B: the full named stat-field semantics of the SNSC 40-byte block
      and the remaining 410-byte character-struct fields — the exact *names* the
      engine prints for the `+0x5b`-family modifiers still need the live status
      line (maintainer domain).

## Character-struct projection (`CharSheet`) — 2026-11-08

The engine's character-sheet builder (analysis/lof.asm around 27Fxx-28080)
copies a handful of fields out of the 410-byte (0x19a) struct into a display
buffer, and the copies pin the semantics:

- `char +0x20` / `+0x22` are the **maximum / current hit points** (words).  At
  28057 it stores `+0x22` directly (the current value it shows), then computes
  `100 * [+0x22] / [+0x20]` and stores the quotient as the HP percentage.  So
  `+0x22` is the numerator (current) and `+0x20` the divisor (maximum).  Every
  in-use character in `GAMES/GAMEn` and `ROST/ROST` satisfies `current <= max`
  (e.g. Handy 9/11, Connar 0/11).  Siegurd's max of 96 is exactly the value the
  init routine writes (`rec[0x2e]=65` base, `+0x20 = +0x22 = rand(1..0x41/2)
  + 0x41 = rand(1..32)+65`, upper half 96).

- `char +0x1d` is the **Dexterity-derived stat** set by the routine at 1BA40: a
  character whose Dexterity (`+0x46`) is below 16 keeps the value 10 the init
  routine wrote; from Dexterity 16 up it becomes `5*dex - 0x41` (dex 16 -> 15,
  19 -> 30).  Verified against all six party members and the full roster.

- `char +0x1e` is the **total defence (armour class)**, computed by the armour
  routine at image `0x210E8` (the accumulation is `0x2114E..0x211DA`; it stores
  the result to `+0x1e`).  It starts from the Dexterity stat `+0x1d` and adds,
  for every **equipped** inventory slot, the item's armour AC (item field `f3`,
  record `+0x16`) scaled by the item's condition (slot byte `+0x85`, the 4th
  byte):

      v = cond + 10;  v -= v % 20;  ac += f3 * v / 100;

  A slot counts only when its aux byte (`+0x83`, the 2nd byte) is non-zero
  (worn / wielded) — a merely carried item adds nothing.  The running total is
  clamped to `0x5f`.  Crucially the routine first checks the slot flag word
  `+0x10`: it recomputes `+0x1e` only when `+0x10 == -1`, so for a live PC the
  saved `+0x1e` is exactly this formula (verified on 10/11 roster members; the
  one exception, Siegurd, carries `+0x10 == 0` and is a stale snapshot).  The
  sheet's defence line reads `+0x1e` (`%2d%%`, string DS:0xa566) and the builder
  computes `attr_a = 100 - (mod5b + stat1e)` from it.  This is exposed as
  `defense_from_items()`; the earlier "body stat `100 - rec[0x2d]`" label was
  only the seed value before the armour pass.

`decode_char()` projects these plus the already-known fields (present, name,
flags10/counter12, level17, class18, base1c, stat1e, con44/str45/dex46/int47/
wis48, speed56) into a `CharSheet` with `hp_percent()` reproducing the builder's
integer division exactly.  Note the save carries spare slots with `+0x00 != 1`
(garbage); only `present == 1` slots are real characters.

Correction (2026-11-08): an earlier revision of this section called `+0x46`
"level" and named the helper `char_stat1d_from_level`.  `+0x46` is **Dexterity**;
the level is `+0x17` (see the "410-byte character struct — grounded offsets"
table above and the sheet builder at 0x2676E).

## Party-status view of the character struct — 2026-11-08

The town/play loop's **party-status builder** walks the party and packs a
**12-byte status row per member** into a display block at `es:[si+0x179..0x184]`
(segment `[0xb0b0]`); the party renderer at `0x134A2` draws it.

> **Listing caveat.** `analysis/lof.asm` *labels* this builder `27F4E`, but its
> disassembly has **desync gaps** through the middle (the literal markers
> `... ; (data / unreached bytes skipped)` between `27F71` and `27FA7`, and
> again at `27FAC`/`27FAD`), so the lines inside those gaps are mis-decoded —
> e.g. the bogus `and bl, byte ptr [bx + di - 0x3dcd]` at `27FAD`.  The tail
> re-syncs and is correct.  Trust the raw image, not the listing, in this range.
> The builder's real file offset is **`0x2D436`**; verify a read against raw
> bytes, e.g. the `+50` gate is `26 80 7f 6a 00 74 0a` / `26 80 84 7c 01 32`
> (`cmp byte es:[bx+0x6a],0` / `add byte es:[si+0x17c],0x32`) at file `0x2D4B8`.
> (The listing's `27F4E` sits ~`0x1A9xx` below the file offset; the base is not
> a single constant across routines, which is why the raw image is the oracle.)

Display row (offsets into the display block), all read straight from the
410-byte struct:

| disp | builder (image) | source | value |
|-----:|-----------------|--------|-------|
| `+0x176` | `2D474` | const | `2` (slot-in-use marker) |
| `+0x177` | `2D47A` | loop idx | party index |
| `+0x178` | `2D484` | `[+0x18]` | class flag (verbatim) |
| `+0x179` | `2D570` | loop idx | party index (again) |
| `+0x17a` | `2D55C` | `[+0x22]`,`[+0x20]` | `100 * hp_cur / hp_max` |
| `+0x17b` | `2D4F7` | `[+0x24]` | combat rank, verbatim (see below) |
| `+0x17c` | `2D49B` | `[+0x22]` | `hp_cur >> 3` (sar 3, `+0x32` if `+0x6a`) |
| `+0x17d` | `2D50B` | `[+0x5b]`,`[+0x1e]` | `100 - (mod5b + stat1e)`, `-0xa` if `[+0x0f]==0` |
| `+0x17e` | `2D523` | `[+0x5c]`,`[+0x1c]` | `mod5c + base1c` |
| `+0x17f` | `2D537` | `[+0x5f]`,`[+0x17]` | `mod5f + level17` |
| `+0x180` | `2D547` | `[+0x22]` | current HP (word copy) |
| `+0x184` | `2D436`/… | `[+0x19a*idx+0x30]` | pointer to the struct (dword) |

Grounded struct reads (all directly from the 410-byte struct):

| off | builder (image) | arithmetic | meaning (bounded) |
|----:|-----------------|------------|-------------------|
| `+0x24` | `2D4F7` | copied to display `+0x17b` | **combat rank 0..3** = KIL/ATK/DEF/RET. Written at `197F0` from the `(1)Enter Kill-Rank`..`(4)Enter Retreat-Rank` menu (`ds:0x713C..0x717A`, compared at `1913C`/`1916C`/`1919C`/`191CC`); read at `197C7`/`1D323`. (An earlier version of this row read those compares as a dying/dead test — wrong.) |
| `+0x22` | `2D49B`/`2D547`/`2D55C` | `100*[+0x22]/[+0x20]`, `>>3` | current HP |
| `+0x20` | `2D560` | divisor of the HP percentage | maximum HP (already known) |
| `+0x5b` | `2D50B` | `100 - ([+0x5b]+[+0x1e])` -> `+0x17d` | attribute modifier |
| `+0x5c` | `2D523` | `[+0x5c]+[+0x1c]` -> `+0x17e` | attribute modifier |
| `+0x5f` | `2D537` | `[+0x5f]+[+0x17]` -> `+0x17f` | attribute modifier |
| `+0x0f` | `2D579` | `== 0` -> `-= 0xa` on `+0x17d` | status-line flag |
| `+0x6a` | `2D4B8` | when `!= 0`, `+= 50` to `+0x17c` | flag |

So the modifier bytes are **bounded signed values added to a base field** (the
builder adds them to `+0x1c`/`+0x1e`/`+0x17`).  The character generator zeroes
all of them (`1B8C0..1B92E`), so a fresh save carries 0.  The encounter code
reads `signed(+0x5e) + Strength` and compares it against `0x11` (image `0x13E10`
/ `0x1BD50`; an early `E9 …` branch in the listing range was another desync
artifact, not a real `+= 2`), and has an `inc byte es:[bx+0x5e]` bump (image
`0x1BD63`).

`+0x6a` correlates with the class slot on the shipped save (Decker 0, Handy 1,
Merlin 2, Cassandra 1, Assanla 0) and is *not* zeroed by the generator, so it is
a per-character flag rather than a pure stat.  `+0x0f` is `1` for every live
character.  The exact name of each modifier
(the game's own label for it) is not yet pinned; they are exposed descriptively
as `kCharFlg5b/5c/5e/5f`, `kCharFlag6a`, `kCharFlg0f` and
`CharSheet::mod5b/5c/5e/5f`, `body24`, `flag6a`, `flag0f`; the derived display
row is exposed as `CharSheet::hp_pct()/hp_mod()/attr_a()/attr_b()/attr_c()`.
`char_test` asserts them on a synthetic struct and on `GAMES/GAME1` (modifiers
all 0; Handy body 1 flag 1 row `[81 1 51 50 100]`, Merlin body 2 flag 2).

`port/char.{h,cpp}` expose the projection so the front-end (maintainer) can
consume it without re-deriving the arithmetic: `party_row(CharSheet, index)`
builds one `PartyRow`, and `party_status(SaveGame)` builds the whole live party
(slots with `present == 1`, in order).  `char_test` asserts the six-member
`GAME1` projection.

## Inventory array (+0x82) and magic points (+0x68/+0x69) — 2026-11-08

The 410-byte character struct carries its **inventory** as a flat array of **48
four-byte slots based at `+0x82`**, ending at `+0x82 + 48*4 = +0x142` — exactly
where the four seeded `(value, level)` skill pairs begin (`+0x142`).  This is the
array the character-sheet's `Weight`/`Rations`/`Gold` lines iterate.

Ground truth is the **item-add routine** at raw image offset `0xBF00` (the
listing desyncs here, so use the raw bytes):

- It scans for the first free slot with `cmp byte es:[bx+si+0x82],0` over
  `si = slot * 4`, `slot = 0..47` (`si` from `0` up, `cmp si,0x30` at `0xBEED`).
- On a hit it stores the 1-based item index at `+0x82` and a per-item byte at
  `+0x84`, and writes `100` to `+0x85`.

So a slot is `{ +0x82 item index (0 = empty), +0x83 equipped, +0x84 charges,
+0x85 condition }`.  All four bytes are now grounded in the raw image (the
earlier "aux1/aux2" names were guesses):

- **`+0x83` = equipped flag** (1 = worn/wielded, 0 = carried only).  Set to 1 by
  the equip routine (`mov byte es:[bx+si+0x83],1` at image `0x2f45e`); cleared to
  0 by the unequip / "take off all" routine (image `0x2f136`, `0x2f170`,
  `0x2f1a8`, `0x2f1e2`, `0x2f21a`, `0x2f278`, `0x2f2b0`, ...).  The armour
  routine counts a slot only when this is non-zero (`cmp byte es:[bx+0x83],0`,
  image `0x21139`).  On the shipped data it is `0`/`1` only.
- **`+0x84` = remaining count / charges.**  Seeded from item `+0x17` when the
  item is added (`mov es:[bx+si+0x84],cl` after `mov al, es:[bx+0xfd3]`, image
  `0xbf5d`).  The "use" path decrements it (`dec byte es:[bx]` at image
  `0x13c53`, where `bx` points at `+0x84`) and refills `+0x85` from it:
  `+0x85 = 100 * slot[+0x84] / item[+0x17]`, i.e. the condition byte doubles as
  the **charge percentage**; when the count hits 0 the slot's condition is reset
  to `100`.  The attack path (image `0x1950d`) uses the same decrement.  This is
  why `+0x84` is `0` on the shipped party (nothing is partially used).
- **`+0x85` = condition / durability, 0..100** (100 = pristine).  Seeded to 100
  on add (`movb $0x64, es:[bx+si+0x85]`, image `0xbf9d`).  Combat damage subtracts
  `2` (`sub byte es:[bx+si+0x85],2`, image `0x1abda`); the armour formula scales
  AC by `(cond+10) - (cond+10)%20` (image `0x21141..0x2117c`); the shop sell
  routine divides the price by it (`div 0x19` after the `+0x85` read, image
  `0xb152`).  Worn armour in the shipped saves is slightly damaged (`96`/`98`).

Decker's `GAME1` pack decodes cleanly as Leather armour (27), Small shield (34),
Short sword (13), The Amulet (120); Handy's as Leather armour (27), Warhammer
(8), Anvil (43), Sledge-hammer (44), The Amulet (120) x2.  Exposed as
`InvSlot{item, equipped, charges, condition}` + `CharSheet::inv[48]`
(`kCharInv82`, `kCharInvCount`, `kCharInvStride`); `char_test` asserts the
synthetic layout and Decker's/Handy's real slots.

### Magic points `+0x68` (max) / `+0x69` (cur)

`+0x68` is the **maximum** magic points and `+0x69` the **current** value.  The
direction is proven by the spend/rest routines: the spell routine at image
`0x137AD` clamps the current down to the maximum (`if +0x68 != 0 && +0x69 <
+0x68 then +0x69 = +0x68`, bytes `26 80 7f 68 00 74 71 26 8a 47 68 26 38 47 69
73 67 26 88 47 69`), and the rest routine at `0x6440` restores it
(`26 8a 47 68 26 88 47 69` = `+0x69 = +0x68`).  The sheet prints
`(+0x69, +0x68)` = (current, maximum).

The pair is filled by the routine at image `0xEBF0..0xEC87` (file offset
`0xEBF0`; the listing labels it `0xEC00`).  It computes the character's record
pointer, then:

```
if (level(+0x17) < 16)
    prof = +0x1b;  base = table_seg:[0xa38 + 16*prof + level]
else
    base = 0
if (base != 0 && Wisdom(+0x48) > 15) base += 1
if (base != 0 && Intelligence(+0x47) > 15) base += 2
+0x68 = base ; +0x69 = base
```

The table is a **12 professions x 16 levels** byte block, `table_seg:[0xa38]`
where `table_seg` is the segment word the loader stores at `DS:0xaf70` (the
routine does `mov es,[0xaf70]` then `mov al, es:0xa38[bx]`).  The block itself
ships in the EXE at file offset **`0x42748`** (= DS `0x2748`); transcription is
into `port/char_magic.inc` (regenerate with `tools/spec.py --emit-mp`).

> **Correction (2026-11-08).**  An earlier revision said the table was indexed
> by the SNSC *class* and was "14 classes x 16 levels", which left Handy/Cassandra
> one point off.  The index byte is `+0x1b` — the **profession**, not the class —
> and the block is 12 x 16.  With that fix the formula reproduces the stored
> `+0x68` for **every** live character in `GAMES/GAME*` (24/24) and `ROST/ROST`,
> including Siegurd (`+0x1b = 12` is out of range -> base 0, so his saved 0 is
> correct).  Merlin (prof 7, level 1, Int 19, Wis 12): `table[7][1] = 1`, +2 Int
> -> 3.  Handy (prof 3): `table[3][1] = 1` -> 1.  Cassandra (prof 10): `1` +1
> Wis -> 2.  The caster rows (professions 3,5,6,7,8,10) climb with level; the
> fighter rows (0,1,2,11) are flat 0.

Because the pair is TRANSIENT (spell/rest mutate `+0x69`), `CharSheet` projects
the saved value verbatim as `mag_max68`/`mag_cur69`; `CharSheet::magic_max()`
*recomputes* the fill value from `prof1b`/`level17`/`int47`/`wis48` and matches it
for every live character.  Exposed as `kCharMagMax68`/`kCharMagCur69`,
`kMagicProfCount`/`kMagicLevels`, `magic_table_value()` and
`CharSheet::magic_max()`; asserted on the synthetic struct, on `GAME1` and
against the whole save.

`+0x25` is a small **load/carry** byte (`10` for most of the shipped party, `15`
for Cassandra) — exposed as `kCharLoad25`/`CharSheet::load25`.

## Boundary: the sheet's `Weight`/`Rations`/`Gold` lines (runtime segment) — 2026-11-08

The character-sheet routine that prints `Weight... `, `Rations.. : ` and
` Gold:` is at raw image offset **`0x2BFxx`–`0x2C6xx`** (its label block lives at
DS `0x920d`..`0x9302`, DS base `0x41d00`; the "Experience " label it also prints
is at DS `0x9283` = raw `0x4af83`).  It iterates the character's inventory
array (the `+0x82` slots decoded above) and, for each occupied slot, reads a
**per-item stats table**:

```
bx = stored_item_index * 32
es = [0xb094]                       ; a RUNTIME segment, not the EXE image
name   = es:[bx + 0xfbc]            ; 16-byte item name
weight = es:[bx + 0xfd0] (word) / 10
field  = es:[bx + 0xfd3] (byte)
class  = es:[bx + 0xfda] (byte)
flag   = es:[bx + 0xfd8] (word)
```

`[0xb094]:0xfbc` is **past the end of the EXE file** (`0x41d00 + 0xb094 + 0xfbc`
= `0x4dd50` > file size `0x4cf10`), so the segment `[0xb094]` is not the image
at that address.  **But** the offsets the sheet uses inside a record match the
**static item table** exactly: `name@0`, `+0x14@20` (= `f2`), `+0x1c@28` (= `f6`),
`+0x1e@30` (= the item class byte).  Four independent offset matches make it
near-certain that `[0xb094]` is the **loaded item table** (the engine copies the
185 x 32-byte table into a runtime segment at load).  So the sheet's per-item
reads map onto the item record as:

```
weight  = f2 (record +0x14, word) / 10
byte    = record +0x17 (the high byte of f3)
flag    = f6 (record +0x1c, word)
class   = the item class byte (record +0x1e = 30)
name    = the 16-byte name (record +0)
```

That pins the sheet's per-item reads onto the item record.  (The ` Gold:` line
prints the character's own 32-bit gold counter `[bp-0x12c]` = char `+0x72`;
see the 2026-11-08 resolution below, which supersedes the earlier guess that it
was a party counter.  `Rations.. : ` formats the char `+0x70` byte with `%02d`.)

### Resolved (2026-11-08): `+0x14` is the item **weight**

The open question above is now closed by three independent, mutually-consistent
pieces of evidence, so the earlier "cost / AC bonus" label for `+0x14` is
**wrong** and has been corrected:

1. **The carried-weight accumulator** (image `0x21030`, storing the sum at
   `0x210d4`).  It reads the character's item-count byte
   `+0x6b`, then for each of the `+0x82` inventory slots it does
   `ax = es:[item_index * 32 + 0xfd0]` (the item record's `+0x14`, since
   `0xfd0 = 0xfbc + 0x14`) and accumulates it into a 32-bit running total,
   finally storing the low word at char `+0x66`.  The accumulator also seeds the
   total with a per-character base `10 * byte[+0x70] + word[+0x72]` (rations and
   gold; see the 2026-11-08 resolution below).  Because the
   inventory byte is a **1-based** index (see below), the record fetched for
   slot value `v` is `items()[v-1]`; the routine's `* 32` matches that.
2. **The load check** at image `0x19B54` / `0x2965f`: an action is refused while
   `item[+0x14] + char[+0x66] > char[+0x64]` — i.e. the item's `+0x14` plus the
   already-carried total must not exceed the character's capacity `+0x64`.
3. **The character sheet** (`Weight... %04d / %04d` at DS:0x92c0) prints the
   pair `(char[+0x66], char[+0x64])` **after dividing each by 10** (`div 0xa`
   right before the format call at image 0x2c21d / 0x2c1f0), so the sheet shows
   pounds.  The compact `Weight:%04d/%04d` at DS:0x92c8 is the same pair.  This
   means the raw `+0x14` values (5..3500) are tenths of a pound, and the earlier
   note in this file claiming the `/10` was *absent* was itself wrong.

**Display, not identity.**  The `/10` does not affect the field *identity*: the
accumulator sums the raw `+0x14` values and the sheet only divides for display.
The equipment list (`%4d$ %3dlb` at DS:0x44ab, image 0xc474) confirms the same
convention: it prints `price` with `%4d$` and `weight / 10` with `%3dlb`.

**Oracle check.**  Recomputing `char[+0x66]` as
`10*byte[+0x70] + word[+0x72] + Σ items()[v-1].weight` reproduces the stored
value **exactly for 10 of the 11 ROST characters** (Connar 289, Stiletto 343,
Lord Krynn 386, Shadow 374, Assanla 245, Cassandra 278, Merlin 223, Handy 573,
Decker 273, Siegurd 23) and for `GAMES/GAME2`/`GAME3` (Decker 290).  The
mismatches are single stale snapshots — Gorth (off by 10), `GAME1` Decker (off
by 9), and the `GAME5` Decker record, whose inventory/`+0x70` bytes are
themselves inconsistent in the file — i.e. the same class of snapshot drift the
earlier note flagged for the ROST records generally; none affects the field
identity.  `CharSheet::weight_base()` exposes the base so callers can
reconstruct the total.

**Capacity `+0x64`.**  The maximum carried weight is set by the routine at image
`0x981d`: `+0x64 = 350 * Strength(+0x45)` while Strength ≤ 15, else
`100 * Strength + 3400` (`mul 0x64; add 0xd48`).  Every shipped save matches
(Str 19 → 5300, 17 → 5100, 13 → 4550, 11 → 3850); the init routine seeds
`+0x64 = 0x1324` (4900) first and this routine overwrites it.

**Exposed.**  `ItemRecord::f2` was renamed to `ItemRecord::weight`; char offsets
`kCharWeight64`/`kCharWeight66` and `CharSheet::weight_max`/`weight_cur` were
added, projected by `decode_char`, and `carried_item_weight(CharSheet)` sums the
inventory item weights.  `char_test` asserts the item weights (Club 100, Short
sword 50, Full plate 650), the two char fields, and that
`carried_item_weight(connar) + base == connar.weight_cur` on the real ROST.
`tools/spec.py --emit-items` now documents record `+0x14` as the weight.

### Resolved (2026-11-08): the sheet's `Weight`/`Rations`/`Gold` lines read char `+0x64/+0x66/+0x70/+0x72`

The `Weight`/`Rations`/`Gold` lines are no longer a boundary.  The sheet
builder (image `0x2BC5E`, which copies the 410-byte record into a `bp`-relative
scratch buffer at `bp-0x19e`) reads the fields straight out of the character
record:

```
Weight... :   bp-0x13a  (char +0x64, max)  /  bp-0x138 (char +0x66, current)
Rations.. :   bp-0x12e  (char +0x70, byte, printed with %02d)
 Gold:        bp-0x12c  (char +0x72, 32-bit, printed with %05ld$)
```

`0x19e - 0x64 = 0x13a`, `0x19e - 0x66 = 0x138`, `0x19e - 0x70 = 0x12e`,
`0x19e - 0x72 = 0x12c` — the scratch copy sits at `bp-0x19e`, so char offset `N`
is at `bp-(0x19e-N)`.  The `Gold:` line pushes the **32-bit** pair `[bp-0x12a]`
(high word = char `+0x74`) then `[bp-0x12c]` (low word = char `+0x72`) into the
`%05ld$` format.  The `Hit point`/`Magic` lines read char `+0x20/+0x22` (HP max /
cur) and `+0x68/+0x69` (MP max / cur) from the same scratch buffer, which
cross-checks the frame base.

**This corrects two earlier claims.**  The note above that "the ` Gold:` line is
**not** summed from items … it prints the party's own 32-bit gold counter
(`[bp-0x1a6]`+0x31)" was wrong: `bp-0x12c` is the character's own gold, not a
party counter.  And the `+0x70` byte is the character's **rations** (its
`Rations.. : %02d` line), not a "base weight / 10".

**The weight base is `10*rations + gold`.**  The carried-weight routine (image
`0x20F7E`, storing at `0x210D4`) reads `word es:[bx+0x72]` into its running
total, then `byte es:[bx+0x70]`, multiplies that byte by 10
(`shl;shl;add;shl` = `*10`) and adds it, then sums every inventory item's weight
(`item +0x14`).  So:

```
char[+0x66] = 10*char[+0x70] + word[+0x72] + Σ items()[v-1].weight
```

i.e. each ration weighs 10 weight units and each gold piece 1 (the units are
tenths of a pound: a ration 1 lb, a coin 0.1 lb).  `CharSheet::weight_base()`
returns this base, `carried_item_weight()` the item sum.

**Gold is 32-bit.**  The shop routine (`0x25760`) checks affordability with a
32-bit compare of `es:[bx+0x74]:es:[bx+0x72]` against `0x14` (20) and refuses
with "Not enough gold." (DS:0x834d) when below; the buy path (`0x257b4`) does
`subw $0x14, es:[bx+0x72]; sbbw $0, es:[bx+0x74]`.  So `+0x72`/`+0x74` is a
little-endian 32-bit gold counter (`kCharGold72`).

**Oracle check.**  `10*+0x70 + word[+0x72] + Σ item weight` reproduces the
stored `+0x66` exactly for the same 10 of 11 ROST characters plus
`GAMES/GAME2`/`GAME3`; only the three known stale snapshots (Gorth, `GAME1`
Decker, `GAME5` Decker) differ.

**Exposed.**  `kCharGold72` (dword) and `kCharRations70` (byte) replace the
earlier `kCharWeightBase70`/`kCharWeightBase72`; `CharSheet::gold` /
`::rations70` are projected by `decode_char`, `weight_base()` returns
`10*rations + gold`, and `char_test` asserts Connar `rations 1 / gold 29`,
the synthetic `10*rations + gold` identity, and the real-ROST reconstruction.

### Resolved: the stored item index base is 1-based

The `+0x82` byte is the item-table index.  It is **1-based** (`item_by_index()`,
`items()[v-1]`): the shipped party's canonical starting kits read cleanly only
that way — Decker Leather armour (27) + Small shield (34) + Short sword (13) +
The Amulet (120); Handy Leather armour (27) + Warhammer (8).  A 0-based reading
gives nonsense (27 -> Studded leather, 120 -> Broadsword+, two Broadswords in one
pack).  `char_test` now asserts `item_by_index(dki.inv[0].item)->name ==
"Leather armour"` and that `item_by_index(0) == nullptr` (0 is the empty marker),
so the 1-based contract is pinned.  `ROST/SHOP` (185 stock bytes) is separately
0-based over the item table (its `byte 0` is Club's stock, 246), so the two
tables use different bases.

## Play-screen party panel: the "dark stat number" — 2026-11-08

The maintainer's town/play-loop notes left exactly one panel residue undecoded:
the *dark* number drawn under each character's profession line, to the left of
the white status word ("GOOD").  It is a **workstream-B character-field**
question and is now resolved without guesswork.

**Format.** The number is `decimal(+0x1e)` followed by one trailing symbol
glyph.  The symbol is CHR2 **band-1 cell 37** (a `%`), and the digits are CHR2
**band-0 cells `27+d`** (note CHR2's digit base is 27, whereas CHR1's is 26 — the
two strips differ).  Glyphs use the panel's 5x5 face, same as the names.

**Grounding.**
- The SNSC-derived init routine (`analysis/lof.asm` `1DA40`/`1DB0A`, raw image
  `0x22F80..`) seeds the char field `+0x1e` from the class record's `+0x2d`
  (`+0x1e = 100 - rec[0x2d]`, the same seed `+0x1d` starts from before the
  Dexterity rule at `1BA40` overrides `+0x1d`; `+0x1e` keeps the raw seed).  The
  panel draws `+0x1e`, not `+0x1d` — the two differ on the shipped party.
- The shipped party (GAME1/GAME2) carries `+0x1e` = 50, 90, 50, 20, 40, 65.
- Rendered `decimal(+0x1e)` + `%` reproduces the dark glyphs in the DOSBox
  capture `ref/oracle/play.png` **pixel-exact for all six characters**
  (`50% 90% 50% 20% 40% 65%`), checked cell-by-cell against the decoded CHR2
  strip.  This is the only reading consistent with both the field values and the
  capture; no other byte in the struct matches the observed glyph pattern.

**Exposed.** `panel_stat_text(uint8_t stat1e)` returns the display string
(`"<n>%"`); `char_test` asserts the six shipped values.  `stat1e` itself is
already decoded into `CharSheet::stat1e` / `SaveChar::stat1e`, so this closes the
panel-number question end to end.

## Combat damage routine (image 0x18FC6..0x196E0) — 2026-11-08

The melee-attack handler that the play loop calls lives in the raw image around
`0x18FC6`–`0x196E0` (the listing desyncs, so these offsets are raw).  It is
reached from the action dispatcher when a character's action byte `+0x0f == 3`
(image `0x19343`), i.e. the "Attacks" action.  The parts that touch the item
table and the character struct are:

1. **Charge / quantity check** (`0x194A8`).  The weapon slot is read as
   `char + 0x82 + slot*4`; the routine first tests `+0x84` (the slot's remaining
   count): if it is exactly `1` *and* the item's `+0x17` (initial charges) is
   non-zero, it delegates to a special routine (`lcall 0x212f:0x7b6e`) — this is
   the "last arrow / bolt" path.  Otherwise it decrements `+0x84`
   (`dec byte es:[bx]`, image `0x1950d`) and refills the slot's condition byte as
   the charge percentage (`+0x85 = 100*slot[+0x84]/item[+0x17]`, image
   `0x19553..0x1955c`); when `+0x84` hits 0 it is reset to `0x64` (100).
2. **Base damage** (`0x1959E`).  It loads the weapon's **damage byte** item
   `+0x19` (`mov al, es:[bx+0xfd5]`, image `0x195AD`), stores it, and calls
   `lcall 0x2a7d:0x2f6a` (a random-roll helper) then `inc`s the result — so the
   per-hit damage is `roll(item[+0x19]) + 1`.  `item[+0x19]` is exposed as
   `ItemRecord::damage` (the high byte of item `+0x18`).
3. **Strength / class bonus** (`0x1904B..0x1907E`).  Before the roll the routine
   builds `-0x2e = ( (Str(+0x45) + mod5e(+0x5e))  +  base(-0xa) + (-0x3a) ) / 3`
   (`add` of `+0x45`, `+0x5e`, two other stack operands, then `idiv 3`) and adds
   a Strength-over-15 bonus: when `Str + mod5e > 15` it adds `(Str+mod5e-15)*5`
   to the accumulator (image `0x1905F..0x1906C`).  So Strength (`+0x45`) plus the
   Strength modifier (`+0x5e`) drives the to-hit/damage bonus.
4. **Class-based multiplier** (`0x195D7..0x19634`).  The attacker's class field
   `+0x1a` selects a scaling: `+0x1a == 1` adds `damage/3`, `+0x1a == 2` adds
   `damage/4` (both signed, image `0x195D7`/`0x195FB`), then the total is
   divided by `+0x55` (`mov es:0x55(%bx,%si),%al; idiv`, image `0x19627`).
   `+0x1a` is a class/category byte seeded per class (the ROST has values 1..5;
   it is **not** the profession index, which is `+0x1b`).
5. **Action-time divisor `+0x55`** (not the SNSC speed).  `+0x55` is a per-action
   byte set to `1` by the normal action handlers (image `0x10046`, `0x156f0`) and
   to `2` by one slow/hasted action (image `0x1c62d`); on the shipped data every
   character carries `1`, so it is normally a no-op divisor.  The **SNSC-seeded
   speed** is `+0x56` (`mov es:[bx+si+0x56],cl` from `rec[0x36]`, image
   `0x230c1`; e.g. Siegurd `4`, humanoid slots `1`), exposed as
   `CharSheet::speed56`.

Fields this grounds for the character struct (all read here, previously unnamed):
`+0x0f` action byte (`3` = attack), `+0x0e` target slot, `+0x1a` class/category
scaling byte, `+0x45` Strength, `+0x55` action-time divisor, `+0x56` SNSC speed, `+0x5e` Strength modifier.

### Item use = the spell table, not a separate "item-use table" (BLK-B003)

The read `es:[bx*0x24 + 0x2712]` (image `0x0E59E`, `0x0F072`, `0x17C6F`,
`0x1947E`, `0x19690`) is **not** an item-use table.  It is the 141-record spell
table.  With `DS` base `0x41d10` (verified: e.g. the spell name `"Burning hands"`
at file `0x44430` is `DS:0x2720`, and `"is not useful in this situation."` at
file `0x47962` is `DS:0x5c52`), `DS:0x2712` resolves to **file `0x44422`**.  The
spell table's record base is `DS:0x26FC` = file `0x4440C`; `0x2712 - 0x26FC =
0x16`, so the read lands on the byte just *past* a record's 14-byte parameter
block.  A live
1 MB dump of the town path (`LOF_SEGDUMP`) is byte-identical to the EXE image at
`0x44422`, so the table is static EXE data (see BLK-B002 resolution).

The use routine at `0x0E562` is the ground truth.  It:

1. `shl bx,5` (`0xE56F`) to reach the character's 32-byte item record; `+0x18`
   is the item's **spell index** (`mov al, es:[bx+0xfd4]`, `0xE575`).  This is
   exactly `ItemRecord::spell` (item record `+0x18`).
2. `imul 0x24` and `mov al, es:[bx + 0x2712]` (`0xE59E`) — reads the spell
   record's first parameter byte.
3. `test al,3` / `test al,4`: if **either** is set it clears the "no-use" flag
   and proceeds to use the item; if both are clear the item is rejected and the
   routine prints "is not useful in this situation." (image `0xE15D`).
4. Separately, `es:[bx+0xfda]` (item `+0x1f`, the use *category*) is compared
   with `9` (`0xE5C5`, `0xE618`): **potions** (`class 9`) also clear the
   "no-use" flag, i.e. they bypass the spell-parameter gate.

So the engine's own byte is `spells()[spell-1].data[0]`, and the predicate is
`(data[0] & 0x07) != 0` **or** the item is a potion.  `ItemRecord::spell` and
`item_use_index()`/`item_use_flags()`/`item_is_usable()` now model this; the
old `port/char_itemuse.inc` (transcribed from file `0x42712` = `ds:0xa02`, which
no instruction references) is deleted.

**Item `+0x18` is the spell index, not an "armour sub-type".**  Spot checks:
Torch → spell 2 "Light"; Healing potion → spell 80 "Healing I"; Healing II →
spell 92 "Healing II"; Ring of power → 129 "Regeneration I"; Healers' staff →
116 "Restoration"; Staff of light → 84 "Fear undead".  Every mapping names the
spell the item is documented to cast.  Item `+0x19` remains weapon damage.

**Usable items** under the full predicate (24): Torch, Lantern, Holy water,
Healing potion, Poison I/II, Antidote I/II, Healing II/III, Strength potion,
Heroes' drink, Potion of Kybol/haste, Ring of Kybol, Ring of power, Healers'
staff, Gain level, Drain level, Ring of shelter, Gunpowder, Magic protect.,
Talisman, Gnokem Whisky.

### The `spell==0` "not-usable" default is an uninitialised-read quirk (BLK-B003 follow-up)

The gate at image `0x0E562` keeps its verdict in the word `[bp-4]:[bp-2]`
(0 = usable).  It is written **only** inside the two branches that clear it —
`0xE588/0xE58D` (spell != 0) and `0xE5CF/0xE5D2` (potion, class 9) — and read at
`0xE5D5/0xE5D8` (`or [bp-4],[bp-2]; je 0xE5E0`).  In the `spell == 0`,
non-potion path the branch at `0xE586` jumps straight to `0xE5BA`, so the flag is
**never written** and the read at `0xE5D5` consumes whatever the previous
stack frame left there.  The routine prologue (`0xD9FC`..`0xDA0A`) initialises
`[bp-6]/[bp-8]`, not `[bp-4]/[bp-2]`.

Both callers (`0xF52A`, `0x13EA6`) drive the routine from the item menus.  Caller
2 only reaches it for **potions** (class 9), where the flag *is* cleared, so the
uninitialised case is unreachable there.  Caller 1 can offer any carried item, so
for a `spell == 0` non-potion the engine's verdict is a stack-dependent quirk.

**Resolved by live play (2026-11-14):** the owner used an ordinary item and the
engine printed "<item> is not useful in this situation." (+ a "(C)ontinue"
prompt), so the engine treats a `spell == 0` non-potion as **not usable**.
`item_is_usable()` returns `false`, which matches; the guess is cleared (G-003 →
section F of `docs/GUESSWORK.md`).  The owner also reported that a **lantern**
used with a flask of oil and a tinderbox consumes the oil and prints "… make
light…" — grounding item effect value 34 (the lantern's `+0x18` spell index 7 is
"Permanent light").

### The effect byte (`+0x1f`) is consumed by real dispatchers, not just a label

`ItemRecord::effect` (`es:[bx+0xfdb]`) is **not** only a display label: the
engine compares it directly at 33 sites.  The complete set of values it is
**proven** to test, with a site each:

- **use-routine effect dispatch** `0x0E89D`: `1` → `0xE8CA`, `2` → `0xE95E`,
  `3` → `0xE9C2`, `10` → `0xEA90`, `19` → `0xEB32`, `23` → `0xEB4E`,
  `30` → `0xEBE4`, `31` → `0xECE2` (the `>10` cases via the second dispatch
  `0x0EDB0`).
- **equip / unequip handler** `0x29FB1` (equip) / `0x2995C` (unequip): `2`, `8`,
  `12`, `21`, `24`, `28`, `29`, `33`.  Each is a symmetric stat delta on the
  wearer:
  | effect | equip (`0x29FB1`) | unequip (`0x2995C`) |
  |--------|-------------------|---------------------|
  | 2  | `+0x5b = cl` | `+0x5b = 0` |
  | 8  | `+0x5a += 4` | `+0x5a -= 4` |
  | 12 | `+0x59 += 1` | `+0x59 -= 1` |
  | 21 | `+0x52 = 1`  | `+0x52 = 0` |
  | 24 | `+0x53 = 1`  | `+0x53 = 0` |
  | 28 | `+0x5a += 1` | `+0x5a -= 1` |
  | 29 | `+0x5a += 2` | `+0x5a -= 2` |
  | 33 | `+0x56 += 1` | `+0x56 -= 1` |
  (`+0x56` is the sheet's speed byte and `+0x5b` its attribute modifier — both
  named in `port/char.h`; `+0x52`/`+0x53`/`+0x59`/`+0x5a` are further
  character-struct bytes this handler toggles, not yet separately named.)
- **misc sites:** `9` (`0x14A09`), `13` (`0x13AF4`), `15` (`0x0DF7E`), `20`
  (`0x14976`), `27` (`0x0E274`), `40` (`0x0E1BC`).

The engine's **own messages** fix the meaning of several values:

- `1` → " sees no effect." (already professional) / " feels his growing
  professionalism." — *enchant weapon*.
- `2` → " feels himself safer." — *enchant armour*.
- `3` and `23` → " didn't take much to it, and goes green in the face!" —
  **poison**; the handler subtracts the item's `+0x19` from HP and sets the
  character's poison field (`+0x1f = 3`, `+0x57 = 4`, `+0x58 = 0x12`).
- `10` → the `0xEA90` handler adds the item's `+0x19` to HP and clamps to max
  (`+0x20`) — **healing**.
- `30` / `31` → clear the poison state (`if char[+0x1f] == 3 { char[+0x1f] = 0;
  char[+0x57] = 0; char[+0x58] = 0 }` / `== 4`) — **antidote I / II**.
- `15` (quest jewels, `0x0DF7E`) → a **combine** handler: it takes the item's
  paired index (`+0x1a`, `f5`), prints "<name> and <name> fits together."
  (`ds:0x5c3c`/`0x5c42`; or "That doesn't seem to work." `ds:0x5c21` if the pair
  is absent), removes both items and adds the result.  It special-cases item
  indices `0x5e`/`0x5f` (Corona/Keystaff), matching the `f5` pair chain
  Emerald↔Corona↔Keystaff.
- `40` (raw materials, `0x0E1BC`) → the same combine shape ("That doesn't seem
  to work." `ds:0x5c73` if the pair is absent); combines paired index `f5`
  (Dry clay ↔ Some water).

So the labels for `1,2,3,8,10,12,19,21,23,24,28,29,30,31,33` are grounded;
the rest are item-name-grouping descriptions (the engine stores no effect-name
table).  **Live play grounds one more:** using a **lantern** (effect 34) with a
flask of oil and a tinderbox consumes the oil and prints "… make light…", so 34
is a light source (the lantern's `+0x18` spell index 7 is "Permanent light").
The distinction is recorded in `docs/GUESSWORK.md` (G-004).

### The spell effect code (data byte 8) is a real jump-table dispatch

`SpellRecord::data[8]` (`es:[spell*0x24 + 0x271A]`, image `0x0F5D8`) is the id
the cast routine switches on.  The routine does `sub ax,1`, bounds-checks
`cmp ax,0x3e; jbe`, then `add ax,ax; jmp *cs:0x2C1E(bx)` (`0x0F5E2`..`0x0F5ED`)
— a 63-entry (`0x3f`) jump table.  So `spell_effect_name()`'s labels describe a
code the engine really dispatches on (the labels themselves are descriptive
family names, not engine symbols).  Same shape as the item effect byte above.


## Blockers (Workstream B) — 2026-11-08

Sorted by who is needed to unblock. Everything B can ground from the static
image (`LOF.EXE` disassembly + the data files) is already done; the items below
need runtime state or another lane.

### Needs the maintainer (play loop / oracle)
- **MEDIUM — wire B's grounded model into `lof.cpp`.** `defense_from_items()`,
  the slot semantics (`equipped`/`charges`/`condition`) and the combat-damage
  model (above) are ready to use; consuming them is `lof.cpp` (SERIAL) work.
- **LOW — remaining char-struct fields.** `+0x0e` (target slot), `+0x0f`
  (action byte), `+0x1a` (class/category scaling) are read by the combat routine
  but their *display* names need the live status/combat UI.

### Needs workstream A (dungeons)
- **MEDIUM — dungeon/event screens reachable.** B's SPEC dispatch types
  (7/9/0x0c/0x0e/0x11/0x19/25) drive the dungeon event system that A owns; their
  handler ground truth (e.g. the type-7 door countdown, the type-0x19 guardian
  riddle) can only be confirmed once A can drive those screens in the oracle.

### Needs B (me) — nothing currently unblocked
The remaining SNSC/NSC 40-byte stat-block names (`rec[0x28]`, `0x29`, `0x2b`,
`0x30`..`0x33`, `0x44`..`0x4e`) are consumed by the monster **runtime record**
builder (`0x2CF0E`) and the combat routine (`0x1904B`), i.e. play-loop state, so
naming them statically would be guesswork. Left as a documented follow-up.

## Encounter / combat screen (2026-11-08)

### Reproduction (deterministic)

The first wandering-monster encounter is reachable from the *wilderness* (not
from the GAME5 dungeon save, which sits in a static, monster-free dungeon room).
The maintainer's post-town path reproduces it exactly:

```
cp GAME4 game/GAMES/GAME4        # slot 4 = the Cyldane save (also installs via bootstrap.sh)
LOF_CMDLEN=3 LOF_NO_TIMER=1 LOF_I21=1 \
  LOF_KEYAUTO=0x18,0x03,0x50,0x48,0x2e LOF_KEYGAP=50 \
  ./lof-native --dir game --headless --frames 760 --shot combat.png
```

The shot is **pixel-identical** to `ref/oracle/combat.png` (mean abs diff
`0.00`). The whole path is deterministic because `LOF_NO_TIMER=1` pins the only
source of variability (the BIOS clock hook that drives wandering-monster
spawns). GAME5 (dungeon slot 5) was *not* needed for combat: dungeon rooms have
no wandering monsters in this build.

### Where the enemies come from

During the encounter the engine opens exactly one roster: **`SYST/NSC00`** —
the scenario-0 (`Valley of Faerghail`) monster table — reading its 1-byte count
(25) then all 25 78-byte records (`LOF_I21=1` trace). It also reads
`DNGS/DNG10` and `SYST/SPEC0`, but the *names* printed on the encounter screen
come from `NSC00`. Enemies are generated on load/encounter, never stored in the
save.

`SYST/NSC0n` layout is identical to `SYST/SNSC` (see `port/data.h`): a count
byte, then N records of `19-byte singular name | 19-byte plural name | 40-byte
statistics block`. The encounter prints the **plural** name. Scenario 0 record
12 is `Mercenary`/`Mercenaries`; the first combat met four of them:

```
You unknowingly meet
4 Mercenaries (Rnk 2)
6 Mercenaries (Rnk 1)
What will you do ?
(A)ttack
(G)reet your opponents
(T)alk to them
(W)ithdraw at once
```

New API: `monster_roster(dir, scenario)` returns the full `SnsRecord` vector
(plural names + stats), and `combat_group_line(count, plural, rank)` formats one
group's line, e.g. `combat_group_line(3, "Ogres", 1) == "3 Ogres (Rnk 1)"`.

### Combat strings (LOF.EXE only — no data file carries them)

All the encounter / round-result text lives in `LOF.EXE`'s data segment as
packed NUL-terminated strings. The offsets in the table below are **file
offsets** into `LOF.EXE` (verified against the raw EXE, 2026-11-10); the
*image* offset is `file - 0x54F0` (the MZ header is `0x54F0` bytes = 1359
paragraphs), and the runtime `ds` offset is the image offset. They are
transcribed into `port/char_names.inc` by `tools/spec.py --emit-names` and
exposed via `port/char.h`. (An earlier note called these "image offsets" — they
are file offsets.)

| file off | string | use |
|----------|--------|-----|
| `0x490BD` | `You unknowingly meet`      | intro variant 0 |
| `0x490D2` | `Without warning you meet`  | intro variant 1 |
| `0x490EB` | `You are surprised by:`     | intro variant 2 |
| `0x49101` | `You still see in front of you:` | intro variant 3 |
| `0x49120` | `%ld `                      | count field of a group line |
| `0x49125` | ` (Rnk %ld)`                | rank field of a group line |
| `0x49130` | `What will you do ?`        | menu prompt |
| `0x49143` | `(A)ttack`                  | action 0 |
| `0x4914C` | `(G)reet your opponents`    | action 1 |
| `0x49163` | `(T)alk to them`            | action 2 |
| `0x49172` | `(W)ithdraw at once`        | action 3 |
| `0x48D2D` | `The result of this round:` | melee round table title |
| `0x48D47` | `Name       HP   W P B Attack` | melee column header |
| `0x48D64` | `---------- ---- - - - -------` | melee rule |
| `0x49048` | `The result of this round:` | withdraw table title |
| `0x49062` | `Name       Gld. Rt Exp. HP   ` | withdraw column header |
| `0x49080` | `---------- ---- -- ---- -----` | withdraw rule |

The `(W)ithdraw` round table (`0x49048`) lists per-character `Gld.` (gold), `Rt`
(rating?), `Exp.` and `HP`; the melee round table (`0x48D2D`) lists `HP`, the
four rank flags `W P B` (weapon / protection / bow?) and the chosen `Attack`.
The per-character row formats follow at `0x4909E` (`%-10s ` name), `0x490A5`
(`%3ld$ ` gold), `0x490AC` (`%2ld `), `0x490B2` (`%4ld `), `0x490B8` (`%5ld`).
These column semantics are *read off the header text* and are not yet confirmed
against a live withdraw screen — treat the per-column meaning as unverified
while the strings themselves are exact.

Also present in the same block: the pre-combat action menu `(A)ttack!` /
`(Q)uick combat` (`0x48D14`/`0x48D1E`), the per-character order menu
`(D)efend` / `(A)ttack` / `(S)neak` / `(U)se object` / `(P)erform magic` /
`(1)Enter Kill-Rank` / `(2)Enter Attack-Rank` / `(3)Enter Defence-Rank` /
`(4)Enter Retreat-Rank` / `(C)ontinue` (`0x48E15`..`0x48EA0`), the aim prompt
`Who has ` + ` aimed at  (1-6) ?` (`0x48EAB`/`0x48EB4`), and the loot lines
`You find ` + `an artefact:`/`a weapon:`/`armour:`/`a shield:`/… (`0x48EE3`..).

### Status of the combat *logic* (still open)

The encounter **screen** (text + menu + monster picture) is now grounded. The
action *dispatcher* and the damage routine remain play-loop work: they live in
the image's code segment and their data references are not reachable by a plain
immediate scan (the engine addresses its strings via segment-relative pointers,
so `b8 <lo> <hi>` searches do not hit them). Grounding the dispatcher needs the
maintainer to expose a combat-round debug dump (cf. the item-use table request
above) — the static image alone gives the strings, not the branching.

---

## Combat action dispatcher — grounded (2026-11-10, unblocks BLK-B004)

This closes the "still open" note above: the action *dispatcher* and the round
*state* are now grounded against real in-game state, not guessed.

### 1. Where the live party struct is (the key that unblocked it)

The combat routines address each character through a far pointer, so the struct
is **not** at the save-file's own buffer. Resolve it exactly as the code does
(ds = `0x4C92` when LOF is resident):

```
es  = word [ds:0xafa6]        ; = 0x468E
bx  = word es:[0x30]          ; offset part  -> 0x3794
seg = word es:[0x32]          ; segment part -> 0x5C92
base = 0x5C92*16 + 0x3794     ; = 0x600B4
```

The same far pointer is loaded via `[ds:0xb036]`. Character `i`'s 410-byte
record is at `0x600B4 + i*0x19A`, name at `+1` (14 bytes, NUL-padded) — matching
the 410-byte record layout. A *second* copy lives at `0x468E:0x908` (= linear
`0x471E8`); the two carry the same values, but the combat routines write the
`0x600B4` copy (verified with `LOF_WATCH`).

Reproduce: `LOF_WATCH=600B0:60A00` shows the writes; `LOF_SEGDUMP=…,5C92:3794:9C4`
dumps the whole party.

### 2. Per-character combat fields (grounded by watchpoints)

| off | field | evidence |
|-----|-------|----------|
| `+0x0f` | **action byte** | `(D)efend` set char0 `1 -> 0` (writer `1DAF:B853`); default is `1` |
| `+0x0d` | **selected flag** | `0x00` normally, becomes `0xff` when the character is configured |
| `+0x0e` | **target** | the `Who has … aimed at (1-6) ?` prompt fills it |
| `+0x70` | *not* the action | a per-character byte the round setup fills from `rand(5)+1` (image `0x1DB78`); independent of `+0x0f` |

The action **enum** is the already-derived `kActionNames` table
(`port/char_names.inc`): `0=Defends, 1=Attacks, 2=Stalks, 3=Use object,
4=Cast Spell`. Live confirmation: the default action is `1` (Attacks); `(D)efend`
sets `0`. (Note: the disassembly listing shows `cmp byte ptr es:[bx+si+0xf],N`
at image `0x13BA2`/`0x13E53`, i.e. the executor reads `+0xf` — consistent.)

### 3. The config menu and its keys (from the play flow)

After `(A)ttack` at the encounter the engine shows the per-character config
screen (strings image `0x48E15`..): `(D)efend (A)ttack (S)neak (U)se object
(P)erform magic` / `(1)Enter Kill-Rank (2)Enter Attack-Rank (3)Enter
Defence-Rank (4)Enter Retreat-Rank` / `(C)ontinue`, plus the aim prompt
`Who has ` + ` aimed at  (1-6) ?`. The menu is **keyboard driven**: a digit
`1`..`6` selects the character, then an action letter sets that character's
action; `(C)ontinue` commits and runs the round. Confirmed end-to-end in the
emulator:

```
# encounter (A) -> config (A) -> pick char 1 -> (D)efend -> (C)ontinue
LOF_CMDLEN=3 LOF_NO_TIMER=1 LOF_DUMP=/tmp/round.mem \
  LOF_KEYAT="50:0x18,100:0x03,150:0x50,200:0x48,250:0x2e,650:0x1e,780:0x02,900:0x20,1020:0x2e" \
  ./lof-native --dir game --headless --frames 1200 --shot /tmp/round.png
```

`LOF_KEYAT` is required: the walk consumes keys before the encounter appears
(~frame 450–650), so `LOF_KEYAUTO` lands them too early.

### 4. The round-result table

After the round the engine draws `The result of this round:` + the melee table
`Name       HP   W P B Attack` (title/header/rule at image `0x4383D`/
`0x43857`/`0x43874` — the earlier `0x48D2D/0x48D47/0x48D64` were *file*
offsets, not image offsets) and fills one row per party member (names verified
present in the frame: `Decker`…`Cassandra`). The withdraw table (`0x49048`) lists
`Gld. Rt Exp. HP` instead. **The per-column semantics are now resolved** — see
"Meaning of the columns" below (W = weapon damaged, P = shield/armour damaged,
B = an ability improved, Attack = Passed/Failed).

**IMPLEMENTED (session 37, 2026-11-20) — `--round-result`.**  Composed by
`compose_round_result_screen()` in `port/lof.cpp` and guarded by
`tests/port_round_result_test.py` against the committed oracle
`ref/oracle/round_result.png` (text **ink mask 1.0000**; the residual
index-12 pixels are the emulator's text artefact, present in every text oracle
and absent from DOSBox).  The oracle is deterministic; capture with:

```
LOF_CMDLEN=3 LOF_NO_TIMER=1 \
LOF_KEYAT="50:0x18,100:0x03,150:0x50,200:0x48,250:0x2e,650:0x1e,780:0x02,900:0x20,1020:0x2e" \
  ./lof-native --dir game --headless --frames 1200 --shot /tmp/round_result.png
```

Layout (image `0x1867D`..`0x189F6`), all measured on that frame and explained
by the drawer's newline flag (see GUESSWORK G-011 correction):

| element | y | x | colour |
|---|---|---|---|
| `The result of this round:` | 92 | 139 | 8 |
| `Name       HP   W P B Attack` | 108 | 139 | 0 |
| `---------- ---- - - - -------` | 116 (ink 119) | 139 | 0 |
| row *n* (one per **present** member, slot order) | 124+8n | 139 | 8 |
| `(C)ontinue` | 180 | 139 | 0 |

The row is the engine's `printf` sequence, so the columns fall out of the
formats: `"%-10s "` name (11 cells → x=205), `"%4ld "` HP change right-aligned
in four 6-px cells, `"* "`/`"- "` for W/P/B at x=235/247/259, and
`"Passed"`/`"Failed"`/`"-"` at x=271 (the `-` is the out-of-action case,
`char +0x49 != 0`).  Row values are the engine's four per-round arrays (see
below), **not** save fields; `--round-result delta:w:p:b:a` takes them per
present member (a = 0 Failed, 1 Passed, 2 out-of-action) and the bare flag
renders the pinned oracle round.

**Font cells recovered by this screen:** `:` = CHR1 band 1 cell 37 and
`*` = band 1 cell 30 (both were missing from `port/font.cpp`; pinned by
`tests/font_test.cpp`).

### 5. Reproduce / verify

Deterministic: the whole path is md5-stable (`LOF_DUMP` identical across runs)
with `LOF_NO_TIMER=1`. Screens: `ref/oracle/combat.png` (encounter, pixel-exact),
and the config / round frames render reproducibly. The party-struct dumps used
here are `/tmp/round*.mem` (ephemeral) — regenerate with the command above.

### Still open after this

- The `+0x70` byte's meaning (it is randomised at round setup; may be the
  `Attack` column value). Not needed to dispatch.
- The `(S)neak`/`(U)se object`/`(P)erform magic` paths did **not** change `+0xf`
  in my runs (they likely open a sub-prompt first) — a live-play confirmation
  would settle their enum values (they are already in the derived table as
  2/3/4).
- The per-column semantics of the round table (`W P B`, `Attack`).

## Round resolution — located, not yet derived (2026-11-20, session 37)

The decision screen only *collects* the actions; the round is resolved by a
separate routine, and the flow is now pinned end to end:

| step | site | what |
|---|---|---|
| 1 | image `0x1865A` | `lcall 0x212f:0x6c24` = image **`0x27F14`** — rebuild the party-status rows (the builder `docs/char` describes) |
| 2 | image `0x18663` | `call 0x17982` with one arg `[bp+6]` — **the round resolution** |
| 3 | image `0x18669` | if `es:[0x3de9] == 0` skip the table (`jmp 0x18A7D`), else draw the round-result table |

### The combat rows (verified from a live capture, 2026-11-20)

`0x17982` and its per-attack callee `0x1370C` work on two blocks of **12-byte
rows** in the *character segment* `es = [ds:0xafa6]` (= `0x468E` in the shipped
build):

* party rows: `es:[0x176 + 12*i]`, `i = 0..5`
* defender rows: `es:[0x258 + 72*g + 12*j]` (6 rows per group `g`)

Captured with `LOF_TRACE_IP=27a82 LOF_CAP=…` (see the address trap in
`AGENTS.md`; the resolution entry is image `0x17982` + `0x10100`) and read out of
the 1 MB dump.  Field meanings verified against `GAME2`:

| off | meaning | evidence |
|---|---|---|
| +0x00 | marker: `2` = party, `1` = defender, `0` = empty | the six party rows carry 2, the mercenaries 1 |
| +0x01 | character-record index (`0x19a` stride from `[0xafa6]:0x30`) | 0..5 for the party; 12 = the Mercenary record |
| +0x02 | class (`1` PC, `0` for two of the party rows) | matches `char[+0x18]` |
| +0x04 | power (weapon damage), e.g. 100, 81 | per-member |
| +0x05 | rank term used by the to-hit formula (0,0,1,2,2,2) | per-member |
| +0x07 | defence, e.g. 40, 10, 50, 80, 60, 35 | per-member |
| +0x08 | offence, e.g. 100, 65 | per-member |
| +0x0a | **current HP** (word) | Decker 10, Siegurd 96, Handy 9, Merlin 5, Cassandra 14, Assanla 13 — exactly `GAME2`'s `+0x22` |

The captured defender rows for the Mercenary encounter were
`01 0c 01 00 32 01 ec 14 64 01 08 00` and `01 0c 01 00 64 01 ec 14 64 01 10 00`
(idx 12, class 1, hp 8 and 16), so the encounter builder fills the same layout.

### `0x1370C(attacker, target, flag)` — what is derived

Called from `0x17982` (`0x17D13`, quick path `0x17B8F`) as
`push seg; push off` twice, then `push cs; call`.  The callee sees
`[bp+6]:[bp+8]` = **attacker** far pointer, `[bp+0xa]:[bp+0xc]` = **target**,
`[bp+0xe]` = flag (non-zero = the forced/unarmed path at `0x137BD`).  Returns
AX: `1` when the attacker's action `+0x0f == 0`, `0` when a marker/HP guard
fails.

Prelude (all verified byte-level):

```
roll       [bp-0x6]  = rand(100) + 1                     (0x13744)
base       [bp-0x3a] = 120 - 20*(atk[+0x05] + tgt[+0x05]), clamped >= 1
attack     [bp-0xa]  = atk[+0x08] - tgt[+0x07],          clamped >= 1
power      [bp-0x32] = atk[+0x04],                        clamped >= 1
```

Then the item/skill pass (`0x13B29`..`0x13B8E`):

```
[bp-0x32] *= 25 or 50 (percent)   ; via the 32-bit helper 0x1b83:0x5fe
si = atk_record[+0x5e] + atk_record[+0x45]   ; strength bonus
if si > 15: [bp-0xa] -= 5*si - 75
hit  [bp-0x2e] = ([bp-0xa] + [bp-0x32] + [bp-0x3a]) / 3
```

Hit test (`0x14183`): `cmp [bp-0x2e], roll` → `jge` = hit; then
`call 0x1c40:0x12c` draws the hit/miss line.  The `+0x0f == 3/4` (use object /
cast spell) branch instead calls **`0x1299E(attacker, target, …)`** at `0x145F4`
with `(base+power)/2 / record[+0x55]` as the magnitude.

**Still open:** the plain-damage path (after a hit, `0x1423F`..), `0x1299E`, the
monster path at `0x159F8`, the quick loop at `0x17BAA`, and the W/P/B array
writes.  With the row layout and the prelude now pinned, these can be derived
one at a time and checked against the same capture technique.

### Damage chain, kill path and miss flag (verified 2026-11-20)

`[bp-0x2e]` is the **damage**, not a hit score: after the 25/50 % scaling it is
adjusted by the character's `+0x1a == 2` (+25 %, `0x14112`) and then divided by
`record[+0x55]` (`0x1413E`).  `[bp-0x48]` starts as that damage and is clamped to
the victim's current HP (`0x14151`: `if victim[+0x0a] < [bp-0x48] then
[bp-0x48] = victim[+0x0a]`).  The **same** value is then compared with
`roll = rand(100)+1` at `0x14290` — so the engine's "to hit" is
`damage >= roll`, which is why a strong party hits almost always.

Branches after `0x14290`:

* **miss** (`0x14295`): print `ds:0x68f3` `", but misses."` and set
  `[0xafd6]:[record+0x3d48] = 1` — this byte is the source of the round table's
  `Attack` column (`Failed`).
* **kill** (`0x142C2`): if the damage equals the victim's remaining HP →
  `victim[+0x0a] = 0`, `victim[+0x00] = 0` (row emptied), the per-group counter
  `[0xafca]:0x3d48 + 3*group` is decremented, and the rewards are paid:
  `[0xafcc]:0x3740 += rand([0x5ee8 + 2*victim[+0x46]]) + 1` (gold),
  `[0xafd0]:0x16e += victim[+0x31]` (score),
  `attacker_record[+0x6c] += victim[+0x44]` (experience).
* **hit, survives** (`0x1443C`): prints `" and hits "`/`" hits "`, `"for "`, the
  number, then multiplies by the damage-type factor
  `[0xafc6]:0xf37 + target_combat_record[+0x2c]` (`0x144DD`).

Messages read from the file (`ds` = `0x4C92`, so file = `ds_operand + 0x41D10`):
`0x68E7` `" shoots at "`, `0x68F3` `", but misses."`, `0x6901` `" hits "`,
`0x6908`/`0x692B` `"for "`, `0x6920` `" and hits "`, `0x6935` `"."`.

Two HP write sites: `0x15822` `dec word record[+0x22]` (one point, followed by
`dec word attacker_row[+0x0a]` at `0x15834` — the branch that has not been
reached yet) and the kill's `victim[+0x0a] = 0`.

### Traps found while capturing (do not repeat)

* The character records are at the **far pointer stored at `[ds:0xafa6]:0x30`**
  (`les bx,DWORD PTR es:0x30`), not at `[0xafa6]+0x30`; in the shipped build it
  is `5C92:3794`.  Reading `0x468E:0x30` as an offset yields garbage.
* At the resolution entry for the standard script
  (`0x18,0x03,0x50,0x48,0x2e,0x1e,0x02,0x20,0x2e`) the actions are
  `Decker = 0` (Defends), the other five `= 1` (Attacks) — so actions *are* set.
* **The melee damage clamp is not reached with `GAME2`.**  Measured: `0x13C10`
  (the `record[+0x0f] == 4` cast branch) hits twice, `0x14151` (the clamp)
  never.  The capture also shows `word record[+0x10]` is `0xFFFF` for Decker,
  Handy, Merlin, Cassandra, Assanla and `0x0000` for Siegurd (whose
  `+0x142 + 2*n` table is empty), so the `0x1385B`..`0x138D1` "auto-cast" scan
  is **not** the explanation for the other four.  Next step: trace the region
  `0x13D00`..`0x14160` on one member to find the actual divergence (the block
  that computes `[bp-0x2e]` at `0x14112`..`0x14144` is entered from above it).

## BLK-B004 — the round-result printer FOUND (2026-11-17, twenty-first session)

**The `W P B` columns are decoded structurally.** The printer for the melee
round table is at **image `0x186A3`** (file `0x1DB93`), inside the combat
round function **`0x1810E`** (it prints `"The result of this round:"` at
`0x18681`). It is reached from the same code that prints the config menu
(`0x1865A` `lcall 0x212f,0x6c24`), so the whole round flow is one routine.

### How to find it (reproducible method)

The printer runs with **`ds = 0x4C92`**, so its DS-relative string operands are
`image_offset − 0x3C820` (e.g. header image `0x43857` → operand `0x7037`).
Searching the image for `b8 <disp16>` with `disp = image_off − 0x3C820` finds
every string the printer loads. The six flag operands are:

| column | image off | DS operand | strings |
|--------|-----------|-----------|---------|
| header | `0x43857` | `0x7037` | `Name       HP   W P B Attack` |
| rule   | `0x43864` | `0x7054` | `---------- ---- - - - -------` |
| W *    | `0x4389f` | `0x707f` | `* ` | 
| W -    | `0x438a2` | `0x7082` | `- ` |
| P *    | `0x438a5` | `0x7085` | `* ` |
| P -    | `0x438a8` | `0x7088` | `- ` |
| B *    | `0x438ab` | `0x708b` | `* ` |
| B -    | `0x438ae` | `0x708e` | `- ` |
| Attack | `0x438b1`/`0x438b8`/`0x438bf` | `0x7091`/`0x7098`/`0x709f` | `Failed`/`Passed`/`-` |

### The four per-character arrays (party segment `0x468E`)

The printer reads, for character `i` (0..5), four bytes from the **party
segment** (`es = [ds:0xafdc] = [ds:0xafd4] = [ds:0xafd6] = 0x468E`):

| column | address | printed |
|--------|---------|---------|
| **W** | `es:[i*2 + 0x5608]` | `* ` if ≠ 0, else `- ` (image `0x18933..0x1895B`) |
| **P** | `es:[i*2 + 0x5609]` | `* ` if ≠ 0, else `- ` (`0x1895C..0x18983`) |
| **B** | `es:[i + 0x224]` | `* ` if ≠ 0, else `- ` (`0x18986..0x189AB`) |
| **Attack** | `es:[i + 0x3d48]` | `-` if char `+0x49 ≠ 0` (out of action); else `Failed` if `[0x3d48] ≠ 0`; else `Passed` (`0x189AE..0x189F6`) |

The row also prints, before HP: the `+0x24`-indexed name (`es=[0xafde]`,
`[bx*4+0xeb0]`), the `+0x0f` action name (`es=[0xafe0]`, `[bx*4+0xec0]`),
`@ %ld` from `+0x16`, and the HP delta `char[+0x22] − snapshot` (`%4ld `).

### The arrays are zeroed each round, then set during resolution

At the top of the combat function the four arrays are **zeroed** for all six
characters (image `0x18198`–`0x181B8`):
```
18198: es = [0xafd4]; mov byte es:[bx+0x224], al        ; B  = 0
1819F: es = [0xafdc]; mov byte es:[si+0x5609], al       ; P  = 0
181A8:                mov byte es:[si+0x5608], al       ; W  = 0
181B0: es = [0xafd6]; mov byte es:[bx+0x3d48], al       ; Attack = 0
```
Then the round-resolution path sets individual bits to 1.

### Setter sites (all `mov byte es:[..], 1`, `es=[0xafdc]`/`[0xafd4]`)

> **CORRECTION (2026-11-18).** The first version of this table resolved the
> setter strings with the **wrong DS base `0x40000`**, which made the setters
> look like *town* routines ("learn new spells", "welcome to our Great Temple",
> "pick-pocketing improves!"). Those strings do not exist at the operands the
> setters load. The base is **`0x41D10`** (`file = DS_operand + 0x41D10`),
> proven two ways: (a) the printer's own title operand `0x701d` → file `0x48d2d`
> = `"The result of this round:"`; and (b) a runtime `LOF_TRACE_IP` at the W
> setter (`1DAF:7CDA`) shows **`ds = 0x4C92`**, and `0x4C920 − 0xAC10 = 0x41D10`
> (`0x40000` corresponds to a DS of `0x4AC1`, which never occurs). With the
> correct base the setters are **combat** messages, and the table below is the
> fixed one. (The earlier "Key structural fact" paragraph — that the flags are
> shared town/combat bits — was an artefact of the wrong base and is **void**.)

Enclosing functions are the `55 8b ec` prologue boundaries (reliable; a `retf`
scan is not, because the big resolver is one prologue with several `retf`s).

| flag | set at image | enclosing fn | strings (correct base `0x41D10`) |
|------|--------------|--------------|----------------------------------|
| **W** | `0x156CA` | `0x1370C` | `"The weapon of "` … `" gets stuck in the armour and is "` … `"damaged!"` |
| **W** | `0x15955` | `0x1370C` | `"Unfortunately, the weapon "` … `" is not durable "` … `" enough, and breaks with a loud bang!"` |
| **P** | `0x17290` | `0x1370C` | `"The armour rusts!"` / `"The shield rusts!"` / `"The weapon rusts!"` |
| **P** | `0x17658` | `0x1370C` | `"However, the shield of "` … `" has been hit "` … `"and damaged!"` |
| **P** | `0x1777A` | `0x1370C` | `"However, the armour of "` … `" has been hit "` … `"and damaged!"` |
| **B** | `0x12846` | `0x11DD6` | `"(Defensive ability grows!)"` |
| **B** | `0x13D7D` | `0x1370C` | `"(Sneaking ability improves!)"` |
| **B** | `0x1471C` | `0x1370C` | `"(Concentration improves!)"` |

(Addresses are the `es:`-prefixed form, matching the listing; the bare form of
each is one byte later. The `0x12846`/`0x156CA`/`0x15955`/`0x17658` sites are
decoded in `analysis/lof.asm`; `0x13D7D`/`0x1471C`/`0x17290`/`0x1777A` fall in
regions the listing skips as "unreached", so they were recovered by a byte scan
for the `26 c6 87 <disp16> 01` pattern.)

**The columns are equipment-condition / ability events.** Each `W`/`P` setter
is immediately followed by a per-slot condition write on the *same* character,
and the slot index is a local that the rust dispatch **names**:

| slot local | item | rust message |
|-----------|------|--------------|
| `[bp-0x38]` | **weapon** | `"The weapon rusts!"` (`0x1725A`) |
| `[bp-0x30]` | **shield** | `"The shield rusts!"` (`0x1722A`) |
| `[bp-0x3e]` | **armour** | `"The armour rusts!"` (`0x171ED`) |

`char[slot*4 + 0x85]` is the slot's condition byte (0..100; `port/char.h`).
The paired writes are `sub byte es:[bx+si+0x85],2` (damaged, −2) or
`mov byte es:[bx+si+0x85],0` (destroyed). So:

- **`W`** = **the character's *weapon* was damaged** this round — `0x156CA`
  (`sub …,2` on the weapon slot) and `0x15955` (`…,0`, the weapon breaks).
- **`P`** = **a *protective* item (shield or armour) was damaged** — `0x17658`
  (shield −2), `0x1777A` (armour −2); the `0x17290` rust variant zeroes the
  weapon/shield/armour slot and prints the matching "rusts" line.
- **`B`** = **an ability improved** — the three setters print
  `(Defensive ability grows!)` / `(Sneaking ability improves!)` /
  `(Concentration improves!)` (i.e. a successful skill use in the round).
- **`Attack`** = Passed/Failed (below).

### Runtime confirmation

`LOF_WATCH=4bee8:4bef4` (W/P arrays) over the deterministic combat drive
(`LOF_CMDLEN=3 LOF_NO_TIMER=1 LOF_KEYAT="50:0x18,100:0x03,150:0x50,200:0x48,250:0x2e,650:0x1e,780:0x02,900:0x20,1020:0x2e"`)
shows the arrays are zeroed at round setup and a **W bit is set to 1** at
`1DAF:7CDA` = image `0x156CA` — i.e. the W flag is genuinely set during the run.
That run's `LOF_TRACE_IP=257ca` also prints **`ds = 0x4C92`** at the setter,
which is what pins the string base to `0x41D10` (see the correction above).

### Meaning of the columns (corrected 2026-11-18)

| column | byte | meaning | evidence |
|--------|------|---------|----------|
| **W** | `es:[i*2 + 0x5608]` | the character's **weapon was damaged** this round | setters `0x156CA`/`0x15955` pair with a write to the **weapon** slot's condition (`[bp-0x38]`), messages `"The weapon of … gets stuck … damaged!"` / `"…the weapon … breaks with a loud bang!"` |
| **P** | `es:[i*2 + 0x5609]` | a **protective item (shield/armour) was damaged** this round | setters `0x17658`/`0x1777A` pair with the **shield** (`[bp-0x30]`)/**armour** (`[bp-0x3e]`) slot, messages `"However, the shield/armour of … has been hit and damaged!"`; `0x17290` is the rust variant |
| **B** | `es:[i + 0x224]` | an **ability improved** this round | setters `0x12846`/`0x13D7D`/`0x1471C` print `"(Defensive ability grows!)"` / `"(Sneaking ability improves!)"` / `"(Concentration improves!)"` |
| **Attack** | `es:[i + 0x3d48]` | the action **Passed/Failed** | `-` if out of action (`char +0x49 ≠ 0`); else `Failed` if `[0x3d48] ≠ 0`; else `Passed` |

**Owner live-play corroboration (2026-11-17, expanded report):** the owner
described the table in full:

```
[name]                -7 - * - Passed
[name]                -5 - - - Failed
```

One row per living party member.  Columns: **HP** = the change in that member's
HP this round; **W P B** = `*`/`-` flags (the event happened / did not);
**Attack** = `Passed`/`Failed` for whether the member's chosen action succeeded.
The owner confirmed **`P = *` ⟺ armour −2%** (exactly the `P` mechanism above,
armour slot −2) and that **`B` is not used ammunition** (tested) — consistent
with the derived "ability improved" meaning.

**Still open (one confirmation):** the owner's test rounds never damaged a
**weapon** and never showed a `B=*` cell. So the `W` and `B` readings are derived
from the decompilation (message + paired slot) but not yet seen in a populated
frame. Owner task **OT-003** now asks only for (a) a round in which a **weapon**
is damaged and (b) a round in which an ability message (`Defensive ability
grows!` / `Sneaking ability improves!` / `Concentration improves!`) appears,
reporting the `W` and `B` cells. That closes BLK-B004 with no guesswork.

**Engine behaviour (owner, 2026-11-17) — quick vs regular rounds.**  The
round-result **table** is displayed only when **quick combat** is selected; the
per-action **messages** (the weapon-damage and ability lines above) are displayed
only in a **regular** (non-quick) round.  The two are therefore never on screen at
the same time — which is why the owner could not cross-check a `*` flag against
its message.  **OT-003 was therefore closed as not-needed** (the `W`/`B` meanings
are decompilation-derived and stand; Rule 1).  (Whether the melee round table is
*only* the quick-combat summary is not yet confirmed — the printer at `0x186A3`
runs in both modes' round function.)
