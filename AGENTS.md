# AGENTS.md — Legend of Faerghail native port

## Goal

Run the original 1990 DOS RPG *Legend of Faerghail* (reLINE Software) natively on
Ubuntu.  The game's two binaries — `LOF.EXE` (main game) and `VOR.EXE` (LZEXE-packed
loader/overlay manager) — are executed by a **native 8086/EGA emulator** written in
C/C++ in `src/`.  This is *not* a wrapper around the DOS binaries and *not* a
reimplementation of the game logic: it is a small, editable CPU + video emulator
that boots the original code, so the behaviour matches the originals.

## Build

```sh
make            # SDL2 window build if SDL2 present, else falls back
make headless   # no SDL2 (used for CI / headless debugging)
make test       # CPU self-test
```

Output binary: `./lof-native`.

## Run

```sh
./lof-native --dir game --headless --frames 300 --shot out.png   # render a frame
./lof-native --dir game                                          # interactive (SDL2)
```

`--dir` points at the game data directory (must contain `LOF.EXE`, `VOR.EXE`,
`EGA/`, `SYST/`, `DNGS/`, `DSKTAB.DFS`, ...).

### Debug env hooks (all optional)

| var | effect |
|-----|--------|
| `LOF_PLANT_LOFX=1`   | plant the copy-protection `lofx` value at 0x3E8 (bypass guard) |
| `LOF_DEBUG=1`        | per-run summary (instr count, mode, plane nonzero, int counts) |
| `LOF_VDBG=1`         | framebuffer/palette/seq/gc dump at end |
| `LOF_PAL2=1`         | per-register attribute palette dump |
| `LOF_EDUMP=<file>`   | dump the 4 EGA planes (4 x 0x10000 bytes) |
| `LOF_DUMP=<file>`    | dump VGA-style framebuffer |
| `LOF_VTRACE=1`       | plane write min/max/nonzero statistics |
| `LOF_WMODE=1`        | histogram of (map-mask, write-mode, set/reset, bitmask) |
| `LOF_PTRACE=1`       | EGA/VGA port OUT trace |
| `LOF_GC=1` `LOF_BM=1` `LOF_ATTR=1` `LOF_INT10P=1` | register-level traces |
| `LOF_I21=1` `LOF_I21ALL=1` | DOS INT 21h open/read trace / per-function counts |
| `LOF_MODE=1`         | trace INT 10h AH=00 video mode sets |
| `LOF_CLICK`/`LOF_KEYS` | scripted input for headless runs |

## Game data (not committed)

`downloads/` and `game/` are git-ignored (proprietary).  To restore:

- `downloads/lof_msdos.zip` — the **DOS** release (use this one).
- Extract to `downloads/ext_msdos/legfaer`, then copy to `game/`.
- `game/disk1.id`..`disk3.id` are dummy files created to satisfy the disk check.

Only the DOS version is used.  Prefer a German release if available; otherwise the
international DOS release.

## Reference captures

`tools/capture_dosbox.sh` runs DOSBox (EGA, 320x200) under Xvfb and grabs 320x200
frames via DOSBox's own screenshot key, so we can diff emulator output against real
hardware behaviour.  Requires `dosbox`, `xvfb-run`, `Xvfb`, `xdotool`, ImageMagick.

Reference frames used during development live in `/tmp/native/` (ephemeral) and
`ref/`.

## Architecture

- `src/cpu.cpp`   — 8086 interpreter (`cpu_step`), decoder, REP prefix handling.
- `src/ega.cpp`   — EGA registers, plane memory, write modes 0/1/2, palette.
- `src/machine.cpp` — memory, MZ loader (LZEXE unpack), INT 10h/16h/21h, ports.
- `src/main.cpp`  — CLI, frame loop, PNG writer, headless/SDL2 frontends.
- `src/emu.h`     — shared types.

## Fixed bugs (do not regress)

- CMP must not write back its destination; 0x83 is 16-bit; SUB/SBB/CMP flag order.
- VOR.EXE LZEXE unpacking (was printing "Packed file is corrupt").
- Copy-protection guard (`lofx` + disk ids).
- INT 10h AH=12h EGA detection (game must select EGA mode 0Dh, not VGA).
- 16-bit `out dx, ax` must drive the paired EGA data port with AH:
  - 0x3C4/0x3C5, 0x3CE/0x3CF, 0x3D4/0x3D5 → write AH to port+1.
  - 0x3C0 attribute controller: **index and data both go to 0x3C0**, so a 16-bit
    OUT writes AL (index) then AH (data) to 0x3C0.
- Graphics-controller register numbers: 7 = Read Map Select, 8 = Bit Mask
  (bit mask was wrongly mapped to 7, which forced a 0x3F mask → dropped pixels).
- EGA colour: primary guns weight 2, secondary weight 1 (index 7 = light gray).
- DOS INT 21h AH=3D must return the handle in AX (not preserve AH), else reads fail.
