# Internal game clock + per-tick party regen — research decode

Status: **research only** (no `port/*.cpp` changes). Owner of code: M-7.
This file records what the disassembly proves about the engine's internal clock
and the per-tick regeneration, so M-7 can implement the P-4 clock model from it.
Every claim is tied to an image offset (flat `LOF.EXE` offset, as in
`analysis/lof.asm` / `tools/lofdis.py`).  Confirmed vs open is stated per item.

Disassembly command used:

    python3 tools/lofdis.py <image_offset> <length>

(The raw `analysis/lof.asm` listing drifts by a byte or two in places; the
capstone dump is authoritative.)

## 1. The clock variable — CONFIRMED

The in-game time-of-day is a **32-bit value in the engine's data segment** at

    es:[0x3b0c]   (low word)  /  es:[0x3b0e]  (high word)

(segment `es` loaded from `[0xb072]`).  This is the same word the save loader
fills from the save's dword at **0x9b6** (see `docs/notes-b.md` §"time of day"),
which indexes the 8-entry phase table (Midnight, Dawn, Early morning, Morning,
Midday, Afternoon, Sunset, Evening).

Advance + wrap, decoded at image `0x25C01`..`0x25C24`:

```
25C01  add word ptr es:[0x3b0c], 1     ; value += 1
25C07  adc word ptr es:[0x3b0e], 0
25C0D  cmp word ptr es:[0x3b0e], 0     ; if high word ...
25C12  jl  ...                          ;   <0  -> keep (impossible for +1)
25C14  jg  0x25c1e                      ;   >0  -> wrap
25C16  cmp word ptr es:[0x3b0c], 7      ; if low word > 7 ...
25C1C  jbe ...                          ;   <=7 -> keep
25C1E  mov es:[0x3b0e], 0               ; WRAP:
25C24  mov es:[0x3b0c], 0
```

So the value is a **phase index in 0..7**; one "tick" adds **1** phase, and a
tick past 7 rolls the day back to 0 (Midnight).  **One tick = one phase.**

## 2. The per-tick routine — CONFIRMED (structure), image ~0x25440..0x25Fxx

The routine that advances the clock is a single large function (one `bp` frame
spanning `0x25440`..`0x25F..`; the `retf` of the *previous* function is at
`0x25AEE`, so do not split it at `0x25AF0`).  After the "who is present /
resting" selection loop it does, at `0x25BE2`:

```
25BE2  lcall 0x18d,0x1b19   ; returns ax (a small value)
25BE7  inc ax               ; -> [bp-0x10]  (rest/step count? OPEN, see §5)
25BE9  mov [bp-0x10], ax ; [bp-0xe]=dx
25BF5  es=[0xb084]; mov es:[0x79f6],0 ; mov es:[0x79f4],0   ; clear two scratch cells
25BFD  es=[0xb072]
25C01  ...clock += 1 (see §1)...
25C2A  loop counter [bp-0x1c] = 0        ; per-member loop
```

### Per-member regen (loop at 0x25C34.., member = [bp-0x1c], stride 0x19a)

Decoded from `0x25C34`..`0x25D30`:

* `es:[rec+0x57] = 4`  (a status/timer byte, set every tick).
* **HP:** `HP_cur(+0x22) += array[member]`, then
  `if HP_cur(+0x22) > HP_max(+0x20) : HP_cur = HP_max`  (`0x25C67`, `0x25C88`..`0x25C92`).
  `array[member]` is the dword at `[bp-0x44 + member*4]`.
* **MP:** `if MP_max(+0x68) != 0 and MP_cur(+0x69) < MP_max`:
  `MP_cur(+0x69) += max(1, MP_max >> 3)`  (`0x25CDF`..`0x25D22`).
  (i.e. one eighth of max per tick, at least 1.)

### The HP-regen array `[bp-0x44]` — CONFIRMED to be zero in this path

The only writes to `[bp+si-0x44]`/`[bp+si-0x42]` in the whole image are the
**zeroing** at `0x254BB`/`0x254BE`:

```
25CB9  sub ax,ax
25CBB  mov word ptr [bp+si-0x42], ax
25CBE  mov word ptr [bp+si-0x44], ax
```

Nothing ever stores a non-zero regen amount into that array.  Therefore in
**this** routine `HP_cur += 0` (HP is not healed by the plain tick); only the
clamp and the **MP** regen take effect.  The full HP/MP restore is the *inn
rest* routine (`0x820a`..`0x8680`, see `docs/notes-b.md`), which is a different
function — resting at the inn does not merely run this tick.

Consequence for the port: a "time passes" step on the map should advance the
phase by 1 and apply **MP regen only** (`+ max(1, MPmax>>3)`, clamped) plus
`+0x57 = 4`; it should **not** heal HP.

## 3. What the `0x13` argument is — OPEN

At the routine prologue (`0x25AF0`):

```
25AF0  mov ax,0x13 ; cdq ; push dx ; push ax ; push cs ; call 0x280f0 ; add sp,4
25AFD  lcall 0x1b39,0x3ee
```

`0x13` (=19) is passed to `0x280f0` together with `cs`.  Whether this is a
message id, a step count, or the number of ticks to run is **not yet proven** —
do not guess it in code.  (It is the most likely knob for "how many phases a
rest/wait adds"; trace it with `LOF_TRACE_IP` around `0x280f0` if needed.)

## 4. Frequency (per step vs per wait vs per rest) — OPEN

The tick advances exactly **one phase per call**.  What is not yet established is
how many times the map loop calls it per forward step, and whether the inn-rest
(`0x820a`) calls it a fixed number of times.  To finish this, capture the clock
word `es:[0x3b0c]` before/after one forward step and before/after one inn rest
with the analysis emulator (read it with `LOF_SEGDUMP=<file>,<seg>:0x3b0c:2`,
where `<seg>` = the `[0xb072]` segment; or watch it with `LOF_WATCH=3b0c:3b10`).
Record the deltas here.

## 5. Cross-references

* Phase-name table + save field: `docs/notes-b.md` §"time of day"
  (`ds:0x0f04` / save dword 0x9b6 / names at `ds:0x2348`..).
* Inn-rest HP/MP restore routine: `docs/notes-b.md` (`0x820a`..`0x8680`).
* The map "tired, won't go on" gate and the town-gate phase gate:
  `docs/notes-maintainer-town.md` §"Internal clock".

## Summary for the implementer (M-7)

1. Model the clock as a phase index 0..7; **+1 per tick, wrap >7 -> 0**.
2. A plain map tick applies **MP regen only**: `MP_cur += max(1, MP_max>>3)`
   (clamped to MP_max), and sets member byte `+0x57 = 4`.  No HP change.
3. The inn rest is a separate routine that restores HP/MP — do not fold it into
   the tick.
4. Open knobs to pin before wiring gameplay timing: the `0x13` argument to
   `0x280f0` (§3) and the per-step / per-rest call count (§4).

## Implemented by M-7 (2026-11-24)

`world_tick(SaveGame&)` in `port/char.cpp` implements one tick exactly as §1–§2:
clock phase +1 wrap >7->0 (synced into `g.time_of_day` and the save dword at
0x9b6), then per present member `+0x57 = 4` and MP regen
`MP_cur += max(1, MP_max>>3)` clamped — **no HP change**.  CLI:
`lof --time-tick N --save S --amulet-out O` (N ticks).  Pinned by
`char_test::test_world_tick` (phase wrap, MP regen + clamp, MP-max-0 no-op, HP
unchanged, build_save persists the clock) and `check_time_tick` in
`port_building_test.py` (GAME4: clock 2->5, casters +1/tick, HP unchanged,
+0x57=4).  The OPEN knobs from §3/§4 (the `0x13` arg and the per-step/per-rest
tick count) are NOT wired — `--time-tick` takes an explicit count; the map/rest
call frequency still needs the `LOF_WATCH`/`LOF_SEGDUMP` capture in §4.
