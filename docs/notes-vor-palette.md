# VOR credits/title palette (P-14) — findings

Status: research in progress (no code change committed yet). Owner: M-6 for
`port/vor.cpp` + `port/gfx.cpp`; do not edit those without coordinating.

## What is known (from AGENTS.md + this investigation)

* `port/vor.cpp` renders `EGA/IN/DRAG` and `EGA/IN/CRED` with the port's
  **standard** palette (`Palette` default = `kEgaAttr` in `port/gfx.cpp`,
  `00 01 02 03 04 05 14 07 38 39 3A 3B 3C 3D 3E 3F`).
* VOR.EXE programs its **own** palette at run time via `INT 10h AH=10h AL=02h`
  (set all 16 registers from `ES:DX`), from **five 17-byte tables** at image
  offsets `0x4834, 0x4845, 0x4856, 0x4867, 0x4878` (16 registers + overscan;
  the intro fades between them). So the colours VOR shows are NOT the standard
  set, and the port's DRAG/CRED RGB is therefore wrong even though the
  *indices* are right.
* The existing `tests/port_vor_test.py` only checks the port's RGB maps back to
  the same **indices** as `ega.indices(asset)` (via the STOCK table). It passes
  regardless of the actual palette, so it does **not** catch the palette error.

## What I found in the VOR memory image (needs confirmation)

Ran `LOF_DUMP=/tmp/vor.mem ./lof-native --dir game --exe game/VOR.EXE
--headless --frames 3 --shot /dev/null`, then scanned the 1 MB dump for the
5-table structure (5 x 17 bytes at a 0x11 stride, every byte <= 0x3F). The one
clean hit is a **fade set** (dark -> bright) at linear **0x19EFF**, exactly
0x11 apart:

```
T0 @0x19EFF  00 00 00 00 00 00 00 00 00 00 00 00 00 00 04 10 00
T1 @0x19F10  00 00 10 10 00 10 00 10 00 01 02 01 04 05 06 10 00
T2 @0x19F21  00 10 02 03 10 05 10 07 00 03 03 11 06 05 06 07 00
T3 @0x19F32  00 10 02 03 04 05 06 07 10 03 12 03 06 15 14 07 00
T4 @0x19F43  00 01 02 03 04 05 06 07 10 11 12 13 14 15 16 17 00   <- bright/final
```

This is a plausible VOR fade set, BUT it is **not yet proven** to be the
`INT 10h AH=10h` source:
* The load base is unknown, so I cannot yet map linear 0x19EFF back to image
  offset 0x4834 (0x19EFF - 0x4834 = 0x156CB, not a round paragraph segment).
* The bright table `T4` is the "naive i+8" set (`00..07,10..17`), which under
  `rgb_from_attr()` makes index 6 dark-yellow — the same artefact AGENTS.md
  notes for the *menu*. That may be correct for VOR or may mean this is a
  different 5-table structure. Must be confirmed against DOSBox.

## To finish (concrete next steps)

1. Capture the real CRED page under DOSBox (`tools/capture_dosbox.sh`, press a
   key after the dragon to reach the credits) and save `ref/oracle/vor_cred.png`
   — this is the ground-truth palette. Also capture the DRAG title.
2. Decide which of the 5 fade tables is active at CRED (most likely the bright
   `T4`, if VOR does not re-program for the still). Confirm by matching the
   DOSBox CRED RGBs to `rgb_from_attr()` of each candidate table.
3. Implement: add a VOR-specific `Palette` (built from the confirmed table) and
   render DRAG/CRED with it in `port/vor.cpp` — do **not** change the default
   `kEgaAttr` (other screens rely on it).
4. Replace/extend `tests/port_vor_test.py` to compare the port's **RGB** (not
   just indices) against the DOSBox capture, so a palette regression is caught.

## Why this stalled

The emulator's `--exe VOR.EXE` run reported a 640x350 framebuffer (not the
expected 320x200) and produced no `[3C0]` attribute-write trace, so the
programmed palette could not be read directly from the emulator. A DOSBox
capture (step 1) is the reliable way to pin the exact colours before writing
code — no value should be guessed.

## M-7 update 2026-11-24 — tables located; the routine decoded

The five tables ARE in the unpacked VOR image, but **not** at linear 0x19EFF (that
was a coincidental dump structure — M-6's byte pattern is not present in the
image).  The real offsets (unpacked image, located by searching for the bright
T4 byte pattern and checking the 0x11 stride):

```
T1 @image 0x55c6  00 00 10 10 00 10 00 10 00 01 02 01 04 05 06 10 00
T2 @image 0x55d7  00 10 02 03 10 05 10 07 00 03 03 11 06 05 06 07 00
T3 @image 0x55e8  00 10 02 03 04 05 06 07 10 03 12 03 06 15 14 07 00
T4 @image 0x55f9  00 01 02 03 04 05 06 07 10 11 12 13 14 15 16 17 00  <- bright
```
(T0 is the all-black table; the fade routine starts at `ds:0x4834` = T0 which is a
zero region, then `+0x11` to T1..T4.  So image `0x55c6` = T1, i.e. `ds:0x4845`.)

The palette routine is **image `0xF83`** (`analysis/vor.asm` line 1507):
```
0xF83  lea si,[0x4834]        ; T0
0xF87  mov cx,5               ; five tables
0xF8A  mov ah,0x10; mov al,2  ; INT 10h AH=10h AL=02h, ES:DX = ds:si
0xF8E  push ds; pop es; mov dx,si; int 0x10
0xF94  [6x lcall 0x1039:0x1c0 — delay]
0xFB4  add si,0x11; loop      ; next table
```
So VOR **fades T0->T1->T2->T3->T4 and leaves T4 active** (the last loop
iteration).  There is no other palette-program in VOR (grep: only this one
`lea si`/`AH=10h AL=2` block), so both DRAG and CRED render under **T4 =
`00 01 02 03 04 05 06 07 10 11 12 13 14 15 16 17`**.

**Remaining ambiguity (still needs DOSBox to settle):** AGENTS.md notes the same
programmed set `00..07 10..17` renders as the STOCK colours in DOSBox for the
*menu*.  If DOSBox likewise renders T4's attrs (0x10..0x17, and 0x06) as
stock-equivalent for VOR, then the port's existing STOCK (`kEgaAttr`) is already
correct and P-14 is a non-issue; if DOSBox honours T4 literally (index 6 = 0x06
yellow-green, 8..15 = 0x10..0x17), the port needs a VOR-specific T4 palette.
This dichotomy can only be resolved by capturing the CRED page under DOSBox
(`tools/capture_dosbox.sh`) and comparing RGB per index — do not guess.
