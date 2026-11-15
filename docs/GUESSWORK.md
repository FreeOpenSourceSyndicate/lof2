# GUESSWORK — the guess register

Every **guess** in this project is tracked here, per the rules in
[`docs/RULES.md`](RULES.md). A guess is any value/behaviour/layout/name/algorithm
that is not traceable to a source of truth (decompilation, data files, oracle
frames, or live play).

**Read this file at the start of every session** and act on each open item:
resolve it, advance it, or re-remind the owner. Nothing is ever deleted; a
resolved item moves to the "Resolved" section with how it was removed.

How to use each section:

| Section | Meaning | Required action |
|---------|---------|-----------------|
| **A. Cannot be removed — owner decision required** | Proven impossible to derive. | **Re-remind the owner every session** until an explicit order is given (Rule 3). |
| **B. Unclear — revisit (watchlist)** | Not yet known whether it can be derived. | Revisit every session; promote to A or to a task/blocker once clear (Rule 4). |
| **C. Owner-authorised guesses** | The owner explicitly allowed this specific guess. | Keep the exact authorisation wording + date; cite it in code (Rule 6). |
| **D. Can be removed — queued (normal task)** | Derivable; not yet done. | Do it, or file a blocker if another lane is needed (Rule 2). |
| **E. Known non-determinism (not a guess)** | Irreducible randomness by design. | Not a guess; listed so nobody mistakes it for one. |
| **F. Resolved / removed** | Guess has been removed. | Record how, keep for history. |

---

## A. Cannot be removed — owner decision required

*(none currently)*

---

## B. Unclear — revisit (watchlist)

### G-002 — 16×16 patch at the town screen's top-left corner (Lane M)

- **What:** the town screen (`--town`, `compose_town_screen()` in `port/lof.cpp`)
  matches the emulator frame at 99.2%; the residue is an 88-pixel 16×16 patch at
  rows 0..15 / cols 0..15 that the engine draws over `EGA/UP/MSCR`.
- **Why it looks like a guess:** the port currently just leaves `MSCR` there
  (i.e. it renders *something* not derived from the engine's actual draw), so the
  patch is an underived value.
- **Evidence:** the patch is not `MSCR`, not `TWN00`, not any single `EGA/**`
  asset, and not a shifted/wrapped copy of either (best match ≈0% on that patch;
  the oracle patch is a diagonal arrow shape). See
  `docs/notes-maintainer-town.md`.
- **New evidence (2026-11-13):** the *same* patch appears in both the town frame
  and the main-menu frame (the two frames are identical in rows 0..10 of the
  corner), so it is **not** a town-specific sprite. It also differs from `MSCR`
  in **both** DOSBox and the emulator, so it is not a decode artefact — it is
  something the engine draws over `MSCR`. Template-matching every `EGA/**` asset
  at every 4-px offset found **no** match (<25 mean-abs threshold). This points
  firmly at an engine-drawn sprite/algorithm (→ trace the routine).
- **Why unclear:** it may be removable by tracing the routine that draws the town
  screen (a small sprite blit / arrow) in `analysis/lof.asm`; not yet attempted.
- **Progress (2026-11-15, fourteenth session) — the draw site is found.**  A
  `LOF_PXWATCH=5,5 LOF_PXWATCH_N=40` run of the `town` scenario (and of the boot
  menu) shows the patch pixel is written by a **masked EGA sprite blitter** whose
  store is `mov word es:[bx+di],dx` at runtime `119D:0463` (image ~`0x1D30`; the
  surrounding code at image `0x1CF2..0x1D38` does the per-byte read-modify-write:
  it builds a run mask in AX from `ds:[bp]`, a shift `cl`, and the sprite byte
  `[si+0xa]`, then `and dx,ax; or dx,cs:[0x37e]; mov es:[bx+di],dx`).  The
  **same pixel is redrawn by two more stores** in the same segment
  (`119D:0B95`, `119D:0FC8`) — i.e. the patch is blitted more than once (layers).
  Next step: break at the blit entry (image `0x1CF2`) and capture the caller-set
  `si` (sprite source), `bx` (plane base), `cx` (width/height) to recover the
  sprite bytes, then reproduce the blit in `port/lof.cpp`.  The patch is
  **cosmetic** (88 px, 0.14% of the screen); it is the *only* residue besides the
  dynamic party panel, so it is not urgent.
- **To clear it:** capture the sprite + dest at the blit entry and reproduce it.
  If the source turns out to be a data-table sprite → move to **D**. If it is
  computed with no recoverable source → move to **A**.

### G-004 — `item_effect_name()` labels beyond the traced dispatcher values (Lane B)

- **What:** `item_effect_name()` gives 41 labels for the item effect byte
  (`+0x1f`).  The engine tests the byte at several sites; the returned strings
  are item-name-derived descriptions, not engine symbol names.
- **Source of truth:** every read of `es:0xfdb` (`+0x1f`) in the image.  The
  complete set of values the engine is **proven** to compare against, with a
  site: `1,2,3,10,19,23,30,31` (use-routine effect dispatch `0x0E89D` /
  `0x0EDB0`); `2,8,12,21,24,28,29,33` (equip/unequip handler `0x2995C` /
  `0x29FB1`); `9,13,15,20,27,40` (misc sites `0x14A09`, `0x13AF4`, `0xDF7E`,
  `0x14976`, `0xE274`, `0xE1BC`).  The engine's own messages fix the meaning of
  several: 1 "growing professionalism." (enchant weapon), 2 "feels himself
  safer." (enchant armour), 3/23 "goes green in the face!" (poison), 10 adds HP
  (`0x0EA90`), 15/40 "fits together." (combine paired items), 30/31 clear the
  poison state (`0xEBE4`/`0xECE2`).  Live play grounds one more: **34 (lantern)**
  — using a lantern with a flask of oil and a tinderbox consumed the oil and
  printed "… make light…" (the lantern's `+0x18` spell index 7 is "Permanent
  light").  Effects the engine never compares (5, 7, 35, 37, …) keep purely
  descriptive labels.
- **Why unclear:** for the values the engine **never** compares, the label is a
  grouping of the item table by effect (effect 5 = "Crystal ball", effect 8 =
  "Antimagic ring", …) — the engine stores no effect-name table, so those strings
  can only ever be descriptive naming choices, not engine symbols.  The
  equip/unequip stat deltas (effects `2,8,12,21,24,28,29,33`) **are** now
  derived statically (see `docs/notes-b.md`); the residual is the effect values
  the engine never tests.
- **To clear it:** for values the engine never tests, the descriptive name is
  already the correct answer (a naming choice, not a claim about the engine); no
  further derivation is possible or needed for them.

---

## C. Owner-authorised guesses

*(none)*

---

## D. Can be removed — queued (normal task)

*(none)*

---

## E. Known non-determinism (not a guess)

### RNG seeded once from `INT 1Ah` — fully decoded, and the oracle is deterministic

- **Fact (decoded from the decompilation, 2026-11-16):** the engine's generator
  is the **classic MSVC `rand()` LCG**: `state = state*0x343FD + 0x269EC3` and
  `rand()` returns `(state >> 16) & 0x7FFF` (`analysis/lof.asm` image `0x30ECC`
  is `rand`, `0x30EBA` is `srand` = store the seed in `state`, `0x2D73A` is a
  `rand_range(lo,hi)` wrapper). It is seeded **exactly once**, at startup:
  image `0x3E` (`lcall 0x18d,0x2ed1`) reads the BIOS clock (`0x47A6`: `int 0x1a`
  then `add cx,dx`) and passes the sum to `srand` (`lcall 0x2f0e,0x1dda`). There
  is **one** `srand` call site and **one** `rand()` call site in the image.
- **Consequence:** enemy placement/movement is a pure function of that single
  seed — it is *not* irreducible per-frame randomness. With the seed pinned, a
  combat frame is bit-exact and reproducible.
- **Emulator status:** the old `INT 1Ah` handler left `CX:DX` as whatever the
  caller held, so the seed was an accidental (but reproducible) `0x000B`. The
  handler now returns that value **explicitly** and honours `LOF_TICK=<n>` to
  choose a different seed. Verified: 6/6 runs identical, with and without
  `LOF_NO_TIMER`, across clock hours; `tests/golden_test.sh` stays green.
- This is **not** a guess — the algorithm is in the decompilation — it is listed
  so no agent mistakes the randomness for underived behaviour. (An earlier
  version of this entry claimed combat "cannot be pinned to a bit-exact golden
  frame"; that was wrong — it was a measurement artefact of running emulators
  concurrently, not the game.)

---

## F. Resolved / removed

### G-001 — town name "Thyn." hardcoded (Lane M)

**Resolved 2026-11-15 (fourteenth session) from the decompilation.**  The town
name is selected by the engine, not hardcoded, and the selector is fully decoded:

- The town routine (image `0x53E0`) prints `"You enter the proud town of"`
  (`ds:0x5A1D`), then tests the 32-bit cell `es:[0x460e]` (loaded via
  `[ds:0xaf48]`): `mov ax,es:[0x460e]; or ax,es:[0x4610]; je .cyldane;
  mov ax,0x3D29 ("Thyn. "); … .cyldane: mov ax,0x3D30 ("Cyldane. ")`, then
  `lcall 0x18d:0x16a8` (the string drawer).  So **nonzero → "Thyn.", zero →
  "Cyldane."** — the only two names in the EXE (`ds:0x5A39`/`ds:0x5A40`).
- The cell is filled by the save loader (image `0x1C952`):
  `mov ax,es:[bx+0x9a4]; cdq; mov es,[0xb00a]; mov es:[0x460e],ax;
  mov es:[0x4610],dx`.  So the selector is exactly **the 16-bit word at save
  offset `0x9a4`** (sign-extended into a dword).  Confirmed by watchpoints: the
  boot initialiser (image `0x179`) writes `1` (→ Thyn, no save), and the loader
  overwrites it with the save word (Thyn save → `1`, Cyldane save → `0`).
- The second `"Thyn"` (`ds:0x687D`, "The Grand Board of Trades greets you in
  Thyn.") is a **fixed immediate** (`mov ax,0x4B5F`, image `0x876D`), not a
  selector: the House of Trades greeting always says "Thyn" in the engine.

**Fix (this session):** `compose_town_screen()` now takes the name as a
parameter, chosen by `town_name_from_save()` from the word at `0x9a4`
(`port/lof.cpp`).  `port_town_test` pins both names: the Thyn frame must stay
pixel-exact against `ref/oracle/town.png`, and GAME4 (Cyldane) must render
"Cyldane." and match `ref/oracle/town_cyldane.png` in the message block (ink
mask, because that oracle uses a faded entry-route palette).  The predecessor's
"computed base + idx*7" hypothesis is superseded: the name is a plain
address-select on one save word, no table indexing.

### G-003 — `item_is_usable()` default for `spell == 0` non-potions (Lane B)

**Resolved 2026-11-14 by owner live play.**  Using an ordinary item in the
item-use menu prints "<item> is not useful in this situation." (+ a "(C)ontinue"
prompt), so the engine treats a `spell == 0` non-potion as **not usable**.
`item_is_usable()` returning `false` matches it, so the value is derived (live
play is a Rule-1 source of truth) rather than guessed.  The decompilation agrees:
the gate's verdict word `[bp-4]:[bp-2]` (image `0x0E562`) is left unwritten on
this path and the "is not useful in this situation." string (`ds:0x5C52`, loaded
at image `0x0E1A9`) is what the engine prints.

**Bonus from the same live-play report:** a **lantern** used with a flask of oil
and a tinderbox consumes the oil and prints "… make light…", grounding item
effect value 34 (lantern) as a light source; recorded under G-004.

*(The original watchlist entry moved here from section B.)*
