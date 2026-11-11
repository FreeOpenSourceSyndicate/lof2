# notes-maintainer-itemuse.md — the "runtime item-use table" is the static spell table

Investigated 2026-11-12 while picking up **BLK-B002** ("expose the runtime
item-use table from a live engine"). Result: the premise of BLK-B002 is
**wrong**. The table the engine reads at `es:[idx*0x24 + 0x2712]` is the
**spell table**, and it is **static EXE data**, byte-identical at run time — no
live-engine dump is
needed. It is the same table workstream B already transcribed as
`port/char_spells.inc`.

This note records the evidence so the finding is not re-litigated.

## Method

1. Built the emulator with a new `LOF_SEGDUMP` hook (`src/main.cpp`) and a `ds`/
   `ss` field on the `LOF_DEBUG` summary line (both maintained below).
2. Ran the `town` golden path head-lessly with a full 1 MB memory dump:
   ```
   LOF_KEYAUTO=0x1e,0x03,0x2e,0x2e LOF_KEYGAP=250 LOF_CMDLEN=3 LOF_NO_TIMER=1 \
     LOF_DEBUG=1 LOF_DUMP=/tmp/mem_town.bin \
     ./lof-native --dir game --headless --frames 1600 --shot /dev/null
   ```
3. Compared the live `ds:` image against the raw `LOF.EXE` image.

## Evidence

**Addresses.** `LOF.EXE` has an MZ header of `0x54F0` bytes, so *image offset* =
*file offset* − `0x54F0`. The static data segment lands at image base
`0x3C820`: the engine's `ds:` offsets map to image `0x3C820 + off` (confirmed by
the runtime dump, where `ds = 0x4C92` and the bytes at `ds:off` equal the image
at `0x3C820 + off`).

**The read.** Every use/attack handler reads a 36-byte (`0x24`) stride table at
`es:[idx*0x24 + 0x2712]` (image `0x0E59E`, `0x0E5F8`, `0x0E7EB`, `0x0F072`,
`0x0F0E6`, `0x0F55F`, `0x0F58F`, `0x0F5D8`, `0x1095D`, `0x11E6C`, `0x11E8D`,
`0x17C6F`/`0x17C70`, `0x1947E`/`0x1947F`, `0x194D6`, `0x19690`).

**It is the spell table.** `ds:0x2712` is 14 zero bytes (the null row 0) followed
by `"Burning hands"`; the records are `[22-byte NUL-padded name][14 data bytes]`
with the name at `ds:0x2720 + i*0x24` and the data at `ds:0x2736 + i*0x24`
(equivalently: `[14 data][22 name]` from base `0x2712`). 141 records. The
routine around it prints the spell prompts and effect messages from the same
data segment (`0x5d45` "This map is nearly the same…", `0x5d87` "Who has",
`0x5f10` "Who should speak out a spell (1-6)?", `0x5cd9`/`0x5d26` "covers all …"),
and the index is a **spell slot**, not an item index (an item index times
`0x24` would run far past the 141-record table).

**It is static.** The whole `141 * 36`-byte region is **byte-identical** between
the live dump and the raw image at `0x3EF32` (file `0x44422`). So the table is
loaded straight from the EXE; it is *not* populated at run time.

**B's transcription is correct.** `port/char_spells.inc` (`kSpellNames` +
`kSpellData`) matches the image byte-for-byte, and its `byte 8 == 0x0f` family is
exactly the heal spells (`Healing I/II/III`, `Cure light/serious wounds`),
confirming the name↔data pairing. So the "item-use" bytes BLK-B002 wanted are
already in the repo.

**`port/char_itemuse.inc` is mis-sourced.** It was extracted from *file* offset
`0x42712` (= image `0x3D222` = `ds:0xa02`). No instruction in the image
references a `disp16` base of `0xa02` (the only two literal `0x0a02` occurrences
are incidental code/data bytes). The handlers read the **spell** table at
`0x2712`, so `item_use_flags()` / `item_is_usable()` are built on bytes the
engine never reads. An item's own use state comes from its **item record**
(32-byte stride, engine base `es:[bx + 0xfda]`, name at `+2`; read with
`shl bx,5`), not from a 36-byte table.

## Consequence

- **BLK-B002 is resolved as "no such runtime table":** the table is static and
  already transcribed; nothing needs a live engine.
- **New follow-up (for Agent B):** re-derive `char_itemuse.inc` (or drop the
  API) — see BLK-B003 in `docs/BLOCKERS.md`.
- The `LOF_SEGDUMP` hook stays: it is the general "dump any segment" debug aid,
  useful for the next runtime-table question.

## Addendum 2026-11-12 — the offset conflation (and the conflict with PR #91)

Open PR #91 (workstream B) reaches a *different* conclusion for BLK-B002: that the
"item-use table" is static at "image `0x42712` / data segment `0x2712`", and that
a runtime dump only looks different because the **MZ loader relocates** it. The
relocation observation is real, but the **offset is conflated**, and the table is
not the target of the `0x2712` read. Both facts are checkable:

1. **`image 0x42712` and `ds 0x2712` are different addresses.** The real MZ header
   is `0x54F0` bytes, so *file* `0x42712` = *image* `0x3D222` = *ds* `0x0A02`.
   The label "image `0x42712` / ds `0x2712`" only holds if one subtracts `0x40000`
   (256 KB) instead of the header — `0x42712 - 0x40000 = 0x2712`. That is the
   root cause of the conflation. `ds:0x2712` is actually image `0x3EF32`
   (file `0x44422`).

2. **The `es:[bx*0x24 + 0x2712]` read resolves to the spell table.** Every
   stride-`0x24` read of `[bx + 0x2712]` first does `mov es, [0xafae]`; the image
   word at `ds:0xafae` is `0x3C82`, which the loader relocates to the runtime
   `0x4C92` (= `ds`). So `es:[bx*0x24 + 0x2712]` = linear `0x4C92*16 + 0x2712`
   = image `0x3EF32` — the **spell** table (`[14 data][22 name]`, "Burning
   hands"), not the `[22 name][14 data]`/dispatch bytes in `char_itemuse.inc`.
   The surrounding strings confirm it is the spell-cast path: `ds:0x5cd9`
   "covers all and slowly dissipates", `ds:0x5d26` "covers all, but has no
   effect", `ds:0x5f10` "Who should speak out a spell (1-6)?".

3. **What the `char_itemuse.inc` bytes really are.** The region at image
   `0x3D222` (`ds:0x0A02`) *is* genuinely relocated at run time: 426 bytes
   differ from the image, and the changed words are `0x3C82 → 0x4C92` far-pointer
   *segment* words (dispatch lists into the code/data segment). So PR #91's
   relocation analysis is correct **for image `0x3D222`** — but that table is
   **not** reached by any `disp16` base of `0x0A02` (verified: zero such
   references), and it is certainly not what `es:[bx*0x24 + 0x2712]` reads.
   Whether/where the engine uses image `0x3D222` is still open (it may be a
   dispatch table reached via a computed far pointer); it is **not** the
   "item-use table".

**Net:** `main`'s merged finding (PR #93) stands — the `0x2712` read is the
spell table. PR #91 should be corrected before merge: its relocation note is
worth keeping, but it must not claim image `0x42712`/`ds 0x2712` as the read
target, and `port/char_itemuse.inc` remains mis-sourced (BLK-B003).

**Numbering collisions — now structurally impossible.** The registry used to be a
single file with one shared `BLK-NNN` counter, so two lanes could pick the same
number (PR #89 picked `BLK-006`/`BLK-007` for dungeons while `main` used
`BLK-006` for this item-use mis-source). The registry is now split **one file per
lane** with **lane-lettered** IDs (`docs/blockers/{A,B,M}.md`, IDs
`BLK-<lane><nnn>`), so lanes no longer share a counter and cannot collide. See
the hub `docs/BLOCKERS.md` for the convention and the old→new ID map
(`BLK-005 → BLK-B002`, `BLK-006 → BLK-B003`, etc.).
