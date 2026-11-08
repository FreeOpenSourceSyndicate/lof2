# AGENTS.md — Legend of Faerghail native drop-in

## Goal

Play the 1990 DOS RPG *Legend of Faerghail* (reLINE Software) natively on Ubuntu
**without executing the original executables**.

The deliverable is a pair of drop-in binaries — `lof` and `vor` — that sit in the
game folder in place of `LOF.EXE` / `VOR.EXE`. They **must not run the original
EXEs**; they read the game's *data* files (`EGA/`, `SYST/`, `DNGS/`, `ROST/`,
`DSKTAB.DFS`, `*.SAV`, ...) and reproduce the original behaviour as faithfully as
possible by re-implementing the engine in editable C/C++. The drop-in must work
even if `LOF.EXE` / `VOR.EXE` are absent, so that a player who owns the game data
can still play.

## IMPORTANT: what NOT to do

- **Do NOT execute the original `LOF.EXE` / `VOR.EXE`.** The emulator (below) is
  a *development aid* for decompilation and reference only; the shipped drop-in
  must not depend on it or on the original binaries at run time.
- **Do NOT look at the sibling repo `FreeOpenSourceSyndicate/lof`.** It is
  unreliable and its approach is a known dead end; treat it as poisoned.

## Original-binary analysis (reference only)

The 8086/EGA emulator in `src/` exists solely to *run the originals once*, so we
can observe behaviour and produce a decompilation (`tools/decompile.py`,
`analysis/*.asm`). Findings from that analysis drive the re-implementation.

- `VOR.EXE` is **both** the intro/title screen (title graphic + a text page) **and**
  the copy-protection step: it plants the `lofx` signature in the INT 0xFA vector
  slot, and `LOF.EXE`'s startup guard aborts unless that signature is present.
  It prints "Please reboot your system without resident programs." only when a
  prior instance is already resident. So VOR *does* show content and must be
  reproduced (it is not copy-protection-only).
- `LOF.EXE` loads its graphics from `EGA\...` (e.g. `ega\in\titl`, `ega\char0\ch00`,
  `ega\up\up00`..), its disk table from `DSKTAB.DFS`, and system data from
  `SYST\SNSC`.

## Build

```sh
make            # SDL2 window build if SDL2 present, else falls back
make headless   # no SDL2 (used for CI / headless debugging)
make test       # CPU self-test
```

Output binary: `./lof-native`.

## Run (analysis emulator — dev aid only)

```sh
./lof-native --dir game --headless --frames 300 --shot out.png   # render a frame
./lof-native --dir game                                          # interactive (SDL2)
```

`--dir` points at the game data directory (must contain `LOF.EXE`, `VOR.EXE`,
`EGA/`, `SYST/`, `DNGS/`, `DSKTAB.DFS`, ...).  When `--dir` is omitted the data
directory defaults to the folder holding the executable.

Useful flags: `--exe <path>` (load a specific program), `--vor` (run VOR first,
then LOF — the original START.BAT order), `--shot-frame N` (capture frame N).

This emulator is a **development aid** for producing the decompilation and
reference captures. It is not the deliverable.

## Drop-in binaries (the deliverable)

The real deliverable is `lof` (and `vor`) — native re-implementations that read
the game *data* files and reproduce the original behaviour **without executing
`LOF.EXE`/`VOR.EXE`**.  They must work when the original EXEs are absent, and the
code must stay editable so the game can be modified after the port.

Roadmap:

1. Keep the analysis emulator working (below) to observe/annotate the originals.
2. Extract the data-file formats (EGA graphics, `DSKTAB.DFS`, `SYST\SNSC`, saves).
3. Re-implement, in order: VOR title screen -> LOF boot/menu -> the play loop.
4. `make dropin` installs the re-implementation into `game/` as `lof` and `vor`.

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

- `downloads/lof_msdos.zip` — the **DOS** release (use this one).  It is
  byte-identical to archive.org item `msdos_Legend_of_Faerghail_1990`
  (`Legend_of_Faerghail_1990.zip`, md5 `52eedcec208a1ae4677c6abf5ce57eb0`).
- Extract to `downloads/ext_msdos/legfaer`, then copy to `game/`.
- `game/disk1.id`..`disk3.id` are dummy files created to satisfy the disk check.

Only the DOS version is used.  A German-language DOS build does not exist
(reLINE shipped the DOS release in English/international; the German release was
Amiga/Atari ST only), so the international DOS release is the correct choice.

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
- BIOS INT 16h must return the scan code in AH and the ASCII value in AL; games
  key off either byte.  DOS INT 21h AH=01/07/08 return the ASCII value in AL
  (extended keys return AL=0, with the scan code on the following read).
- `scancode_ascii` table: 0x2C is `z` (the row is `zxcvbnm,./`, no leading `\`),
  0x29 is `` ` ``, 0x2B is `\`.  Covered by `make test`.
- `reset_cpu_state` must clear transient CPU state (`halted`, prefix/repeat
  flags), not just the registers — otherwise a second program (LOF after VOR)
  inherits the first one's halt flag.  Covered by `make test`.
