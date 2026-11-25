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
