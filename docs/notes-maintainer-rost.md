# notes-maintainer-rost.md — the ROST per-record checksum (cheat protection)

**Status: DECODED and VERIFIED (2026-11-10, incoming-maintainer orientation session).**
This closes the open item the previous maintainer was interrupted on: the
"integrity/checksum word" at struct offset `0x19A` (410) of every saved-character
record. The algorithm below is derived from the decompilation and reproduces the
stored trailer on **23/23 records** across three independent save files.

## What the value is

Every `ROST/ROST` record is `414 = 410 (0x19A) + 4` bytes. The trailing 4 bytes
are a **32-bit little-endian checksum of the preceding 410 bytes** (`DX:AX` of the
routine). It is *not* a plain sum and *not* a CRC — it is an alternating
**ADD / XOR** fold with a carry into the high word. See "Algorithm" below.

## Where it lives (source of truth: the decompilation)

All addresses below are the `analysis/lof.asm` / disassembly values. The mapping
used: **`file_offset = asm + 0x54F0`**, equivalently `linear = asm + 0x10100`
(load segment `0x1010`). Two independent sites agree on the mapping, so it is not
in doubt.

### The checksum routine — asm `0x1BF18` (image; far-called as `0x1BF1:0x0008`)

Called as `checksum(off, seg, len)` returning `DX:AX`. It is *relocated* at load
time: the stored far pointer `0x1BF1:0x0008` resolves, after the MZ relocation,
to `(0x1BF1 + load_seg) : 0x0008`; with `load_seg = 0x119D` that is linear
`0x2D8E8`, i.e. asm `0x1BF18`. Disassembly (Turbo C frame):

```
1bf18:  push %bp ; mov %sp,%bp
1bf1b:  mov  $0xa,%ax ; lcall $0x2f0e,$0x26a      ; _chkstk(10)
1bf23:  push %si
1bf24:  movw $0x1,-0x6(%bp)                        ; acc_lo = 1
1bf29:  movw $0x0,-0x4(%bp)                        ; acc_hi = 0
1bf2e:  sub  %ax,%ax
1bf30:  mov  %ax,-0x8(%bp)                         ; i_hi = 0
1bf33:  mov  %ax,-0xa(%bp)                         ; i_lo = 0
1bf36:  jmp  1bf4d
1bf38:  mov  -0x2(%bp),%al   ; c            <-- main body per byte
1bf3b:  sub  %ah,%ah
1bf3d:  sub  %dx,%dx
1bf3f:  xor  %ax,-0x6(%bp)   ; acc_lo ^= c      (odd index)
1bf42:  xor  %dx,-0x4(%bp)   ; acc_hi ^= 0
1bf45:  addw $0x1,-0xa(%bp)  ; ++i (32-bit)
1bf49:  adcw $0x0,-0x8(%bp)
1bf4d:  mov  0xa(%bp),%ax ; cwtd                   ; len -> DX:AX (32-bit)
1bf51:  cmp  -0x8(%bp),%dx ; jl 1bf94              ; i >= len -> done
1bf56:  jg   1bf5d
1bf58:  cmp  -0xa(%bp),%ax ; jbe 1bf94
1bf5d:  mov  -0xa(%bp),%bx
1bf60:  les  0x6(%bp),%si                          ; ES:SI = buf
1bf63:  mov  %es:(%bx,%si),%al                      ; c = buf[i]
1bf66:  mov  %al,-0x2(%bp)
1bf69:  or   %al,%al ; jne 1bf71
1bf6d:  movb $0x1,-0x2(%bp)                         ; if (c == 0) c = 1   (skip NUL)
1bf71:  mov  $0x2,%ax ; cwtd
1bf75:  push %dx ; push %ax                         ; divisor = 2
1bf77:  push -0x8(%bp)                              ; hi of (i)
1bf7a:  push %bx                                    ; lo of (i)
1bf7b:  lcall $0x2f0e,$0x1f32                       ; 32-bit MOD -> DX:AX = i % 2
1bf80:  or   %ax,%dx ; jne 1bf38                     ; if (i % 2) -> XOR path
1bf84:  mov  -0x2(%bp),%al ; sub %ah,%ah ; sub %dx,%dx
1bf8b:  add  %ax,-0x6(%bp)   ; acc_lo += c       (even index)
1bf8e:  adc  %dx,-0x4(%bp)   ; acc_hi += carry
1bf91:  jmp  1bf45
1bf94:  mov  -0x6(%bp),%ax ; mov -0x4(%bp),%dx     ; return DX:AX
```

Note: **`i % 2` is computed with a 32-bit `DIV`** (`lcall 0x2f0e:0x1f32` with
divisor 2), not a test of bit 0 — but the result is the same parity.

### Where it is computed on save — asm `0x2A1E1`

The ROST writer (`0x2A0F0`…, called with the file handle) loops the records:

```
2a1c2:  lcall $0x18d,$0x2cd0    ; read 4 bytes  -> trailer -> [bp-4] ([bp-2])
2a1e1:  lcall $0x1bf1,$0x8      ; checksum(off=record, seg, len=0x19a) -> DX:AX
2a1e9:  cmp  -0x4(%bp),%ax      ; compare low  word
2a1ec:  jne  0x2a1f6
2a1ee:  cmp  -0x2(%bp),%dx      ; compare high word
2a1f1:  jne  0x2a1f6
2a1f3:  jmp  0x2a172            ; match -> next record
2a1f6:  ... movb $0xfc,%es:0xc(%bx,%si)   ; MISMATCH -> set record+0x0C = 0xFC
```

So the **tamper flag is byte `0x0C` of the 410-byte struct, value `0xFC`**. On a
clean record that byte is `0x00`.

### Where the flag is acted on — asm `0xB0CB`

Routine `0xAB6C` walks the 6 party slots and, at `0xB0CB`:

```
b0cb:  cmpb $0xfc,%es:0xc(%bx,%si)   ; record[0x0C] == 0xFC ?
b0d0:  je   0xb0d5
b0d2:  jmp  0xb160                    ; clean -> continue
b0d5:  ... prints the fraud message and takes the penalty
```

The message text lives at `ds:0x72CA` (file `0x472CA`):
`"very sorry, but \0 was caught \0trying to fraud. The penalty for this offence
\0is normally a \0..."` (the routine builds the name / penalty sentences around
it; the referenced fragment at `ds:0x734C` is `"fraud. The penalty for this
offence "`). The `(mov ax,0x7370)` at asm `0x1A498` is the *use* site that pulls
the tail of that message.

## Algorithm (canonical)

```
checksum(buf, n):
    acc = 1            # 32-bit, low word only used as 16-bit for the trailer
    for i in 0 .. n-1:
        c = buf[i]
        if c == 0: c = 1        # NUL bytes are treated as 1
        if i is even:  acc += c # ADD, carry propagates into the high word
        else:          acc ^= c # XOR (only the low byte can be non-zero)
    return acc & 0xFFFFFFFF
# trailer = little-endian uint32(acc); n = 410
```

The high word is always `0` for every real record seen (the ADD path never
carries out of the low 16 bits for these inputs), so in practice the trailer is a
16-bit value in the low two bytes with two zero bytes after it.

## Verification (23/23 records)

Reproduced the stored trailer exactly for:

- `game/ROST/ROST` — 11 records,
- `ROST1` (repo-root save, secured to `ref/rost/ROST1.cheat`) — 6 records,
- `ROST2` (repo-root save, secured to `ref/rost/ROST2.banked`) — 6 records.

Example (`game/ROST/ROST`): `0x068A 0x05EE 0x07BF 0x0895 0x06FF 0x0702 0x07FA
0x0891 0x0608 0x0933 0x0592` — all match the file bytes.

A brute-force over byte/word sums, XOR folds, CRC-16 variants, and `h = h*C + b`
(all constants) had previously found **no** match; that is consistent — the real
fold is *position-dependent* (parity of the index), which none of those cover.

## Consequence for the port (lane B task — not this lane)

**DONE (2026-11-16, eighteenth session).** The three requested follow-ups are
implemented: `port/char.{h,cpp}` has `rost_record_checksum()`, `load_rost()`
sets `RostRecord::intact`, and `tests/char_test.cpp` covers the algorithm plus
the fixtures. See `docs/blockers/B.md` (BLK-B005, Resolved).

`port/char.cpp::load_rost()` already reads `rec.tail` but does **not** validate
it. Implementing this algorithm belongs to **Workstream B** (`port/char.{h,cpp}`,
`tests/char_test.cpp`). Requested follow-up:

1. Add `uint32_t rost_record_checksum(const uint8_t* rec410)` to `port/char.*`
   (or `tools/`), matching the algorithm above.
2. Have `load_rost()` set a `bool intact = (tail == checksum(raw[0..409]))` (and,
   if the engine's `0x0C == 0xFC` tamper marker is ever surfaced, expose it too).
3. Add a `tests/char_test.cpp` assertion against the committed fixtures
   `ref/rost/ROST1.cheat`, `ref/rost/ROST2.banked`, and `game/ROST/ROST`.

No guess is involved: the algorithm is fully traceable to asm `0x1BF18` and the
stored trailers on 23 records.

## Reproduce

```sh
python3 - <<'PY'
def cks(b):
    acc=1
    for i,x in enumerate(b):
        c=x or 1
        acc = (acc+c) if i%2==0 else (acc^c)
    return acc & 0xFFFFFFFF
for path in ('game/ROST/ROST','ROST1','ROST2'):
    d=open(path,'rb').read(); n=d[0]
    ok=all(cks(d[1+i*414:1+i*414+410])==int.from_bytes(d[411+i*414:415+i*414],'little')
           for i in range(n))
    print(path, 'records', n, 'all match:', ok)
PY
```
