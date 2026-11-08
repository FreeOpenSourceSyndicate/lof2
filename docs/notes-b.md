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

- Byte 0 = 0; bytes 1..45 = 45 **signed** adjustment values (`-1..-10`),
  probably a per-item price/availability table; then zero padding; last word
  `0x1606` at offset 186.

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

`lof.cpp` is `[SERIAL]` and owned by the maintainer. Please wire
`--dump-spec` into the front-end: include `char.h`, and on `--dump-spec` call
`load_spec(dir + "/SYST/SPEC" + n)` for n=0..7 and `dump_spec(stdout, t, label)`.
(The same dump is already runnable now via `./char_test --dump-spec`.)
