# notes-maintainer-decision.md — the combat decision screen (round prompt)

**Status: DECODED and IMPLEMENTED (2026-11-17, maintainer session).**
This closes `G-009` and `G-010` from `docs/GUESSWORK.md` and implements the
screen as `lof --decision`.  The frame matches `ref/oracle/combat_decision.png`
at **0.96** (viewport 1.000, text 0.993).

## The screen is the encounter composition plus a text block

The two oracle captures — `ref/oracle/combat.png` (the wandering-monster
encounter) and `ref/oracle/combat_decision.png` (the per-round prompt) — share
their viewport `(10..129, 11..129)`, roster `(6..135, 136..195)` and bottom strip
`(y >= 196)` **pixel-exactly** (measured 1.0000 for each region).  Only the
right-hand text block `(139..316, 92..187)` differs.  So the decision screen is
the encounter composition with new text, and `compose_decision_screen()` reuses
the same chrome.

## Address mapping (re-confirmed this session)

- `image = file - 0x54F0`; `ds base = image - 0x3AB10` (so `ds:0x0 = image
  0x3AB10`).  Verified: `ds:0x6FDB` -> image `0x437FB` = `"%ld"`, and
  `img.bin == LOF.EXE[0x54F0:]` byte-for-byte.

## The row layout — image `0x18734`

Per party slot `i` (loop counter at `[bp-0x1c]`, `cmp ... 6`), row
`y = 116 + 8*i`:

| step | image | what |
|---|---|---|
| index | `0x1875B` | `mov ax,[bp-0x1c]; inc ax` -> `"%ld"` (ds:0x6FDB), colour 8, x=139 |
| name  | `0x187A5` | `"%-10s "` (ds:0x6FDF) into a local buffer, then drawn at x=145 |
| rank  | `0x187DC` | see below; black, x=211 |
| action| `0x18843` | `action_names()[+0x0f]`, colour 8, x=235 |
| trail | `0x18880` | `"@ %ld"`, see below |

Cursor positioning is done by `lcall 0x18d:0x175a` with a column argument (4, 3,
5, 3, 2 in draw order); the drawer itself is `lcall 0x18d:0x16a8`.

### Row filter

`0x18798`: `cmpb $0,es:[bx]` (slot present) — skip if zero.
`0x1879E`: `cmpb $5,es:[bx+0x1f]`; `jae` skip — so `+0x1f` must be `< 5`.

### The rank column — `+0x24` is the combat rank

`0x187DC`..`0x18808`:

```
mov ax,0x19a ; imul [bp-0x1c]          ; 410-byte record stride
les bx,[es:0x30]                        ; the party array
mov bl,es:[bx+si+0x24]                  ; <-- the rank byte
sub bh,bh ; shl bx,1 ; shl bx,1         ; 4-byte table stride
push es:[bx+0xeb2] ; push es:[bx+0xeb0] ; far pointer from ds:0x0EB0
lcall 0x18d:0x16a8                      ; draw the abbreviation
```

`ds:0x0EB0` holds four far pointers to `KIL `/`ATK `/`DEF `/`RET `
(file `0x42BC0`, four 4-byte slots).  So the column is
`combat_abbrev_names()[char[+0x24]]`.

**`+0x24` is the combat rank, in the engine's own words.**  Read/write sites:
read at `0x197C7` and `0x1D323`, written at `0x197F0`, compared at
`0x1913C`/`0x1916C`/`0x1919C`/`0x191CC`.  Those compares gate the strings

| imm | string (ds) |
|---|---|
| 0 | `ds:0x713C` `(1)Enter Kill-Rank` |
| 1 | `ds:0x714F` `(2)Enter Attack-Rank` |
| 2 | `ds:0x7164` `(3)Enter Defence-Rank` |
| 3 | `ds:0x717A` `(4)Enter Retreat-Rank` |

and `0x197F0` stores the pressed digit minus `'1'`.  So the byte is a 0-based
rank selecting KIL/ATK/DEF/RET, and the engine spells all four out.  The old
"body/health state (2 dying, 3 dead)" annotation was a misreading of these very
compares as a death test; it is corrected in `notes-b.md` and `port/char.h`.

The shipped save confirms it: GAME2's six members carry `+0x24` =
`[0,0,1,2,2,2]`, exactly the oracle column `KIL KIL ATK DEF DEF DEF`.  The
predecessor's save-patch experiment (which was inconclusive because editing the
byte invalidates the record trailer) is unnecessary: the table lookup is read
straight out of the image.

### The action column

`0x18846`: `mov al,es:[bx+0xf]` -> index into the 5-entry far-pointer table at
`ds:0x0EC0` (file `0x42BD0`): `Defends    ` / `Attacks    ` / `Stalks     ` /
`Use object ` / `Cast Spell `.  All six GAME2 members have `+0x0f = 1`
("Attacks"), colour 8, x=235.

### The `+0x49` override

`0x18839`: `cmpb $0,es:[bx+0x49]`; if non-zero, `jmp 0x18720`, which draws the
fixed string `ds:0x6FF2` = "is out of action." at the action column and skips the
trailing number entirely.  None of the shipped saves set this byte.

### The trailing number

`0x18880`:

```
mov al,es:[bx+0x16] ; mov [bp-0x3c],al
cmp al,0xff
jne 0x18892            ; -> +0x16 + 1 via ds:0x6FE6
jmp 0x186d6            ; -> the +0x0d branch
```

The `+0x0d` branch is at `0x186F3`:

```
cmpb $0,es:[bx+0xd]  ; jl  skip      (signed)
cmpb $2,es:[bx+0xd]  ; jg  skip
cmpb $0,es:[bx+0xf]  ; je  skip
mov al,es:[bx+0xd] ; cbw ; inc ax    ; +0x0d + 1
... ds:0x6FEC = "@ %ld"
```

So the number is printed only when signed `+0x0d` is in `0..2` **and**
`+0x0f != 0`; otherwise nothing is drawn.

**Why it inks at x=307.**  The action word is a `"%-10s "` field, i.e. 10
content cells plus the trailing space, so the `"@ %ld"` cursor starts 10 cells
(60 px) past x=235 -> x=295.  `@` and the space each advance 6 px but have no
glyph in the text face, so the digit lands at `295 + 12 = 307` (measured ink
`x=308..311`).  All six GAME2 rows have `+0x0d = 0`, `+0x0f = 1`, `+0x16 = 255`,
hence `1` on every row.

## Header and footer

- `0x18CC4` draws `ds:0x6FAD` "Please make your decision" at y=92 (colour 8).
- `0x18CD5` draws `ds:0x6FC7` "for the next round!" at y=100 (colour 8).
- Footer `(A)ttack!` (`ds:0x7004`, drawn at `0x185FA`) at y=172 and
  `(Q)uick combat` (`ds:0x700E`, drawn at `0x1860A`) at y=180; both at x=139,
  black.  The positions are measured from the oracle and the port reproduces
  both rows at 1.0000.

## Verification

- `tests/port_decision_test.py` — whole-frame match, viewport, text, and the
  invariant that the drop-in's viewport/roster equal the encounter screen's
  (and that the same holds for the two oracles, so a broken derivation
  assumption is caught).
- `tests/char_test.cpp` — pins the rank mapping `[0,0,1,2,2,2]` ->
  `KIL KIL ATK DEF DEF DEF`, the action index, the `+0x1f < 5` guard and the
  trailing-field inputs against the real save.

## Residual differences (not bugs)

- The party/status panel residue (roster 0.859 vs the oracle) is the pre-existing
  emulator-vs-DOSBox difference, identical to the play and encounter screens
  (the port's roster is byte-identical across all three).
- The emulator capture paints a few glyph-descender pixels as index 12 (a
  capture artifact); the test therefore compares the text rows on their ink
  mask, which matches 1.0000.

## Open

`G-011` — the colour of the `+0x49` "is out of action." string is inferred
(index 8) because no oracle frame exercises that branch, and the two paths push
*different* flag arguments to the drawer (`0` for the action word at `0x18843`,
`1` for this path at `0x18720`), so the flag may well select a different
palette index.  All shipped saves have `+0x49 == 0`, so the reachable screens
are unaffected.
