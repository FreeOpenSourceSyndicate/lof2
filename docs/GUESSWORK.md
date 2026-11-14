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
- **Why unclear:** it may be removable by tracing the routine that draws the town
  screen (a small sprite blit / arrow) in `analysis/lof.asm`; not yet attempted.
- **To clear it:** trace the town-screen draw path in the decompilation. If the
  sprite/algorithm is found → move to **D** (or a blocker if another lane owns
  the routine). If it cannot be derived → move to **A**.

### G-003 — `item_is_usable()` default for `spell == 0` non-potions (Lane B)

- **What:** the item-use gate (image `0x0E562`) keeps its verdict in the word
  `[bp-4]:[bp-2]`, written **only** in the `spell != 0` branch (`0xE588/0xE58D`)
  and the potion branch (`0xE5CF/0xE5D2`).  In the `spell == 0`, non-potion path
  the branch at `0xE586` skips both, so the flag is never written and the test at
  `0xE5D5/0xE5D8` reads **uninitialised stack**.  `item_is_usable()` returns
  `false` for that case — a defensible default, not a derived value.
- **Source of truth:** `analysis/lof.asm` `0x0E562`–`0x0E5E0`, routine entry
  `0xD9FC`, callers `0xF52A` and `0x13EA6`.
- **Why unclear:** caller `0x13EA6` reaches the routine only for potions (class
  9), where the flag is cleared, so the uninitialised read is unreachable there.
  Caller `0xF52A` can offer any carried item, but whether a `spell == 0`
  non-potion is ever actually selected is a **live-play** question (Rule 5).
- **To clear it:** play the item-use menu and use a `spell == 0` non-potion; if
  the engine accepts it, the flag defaults to "usable" and the predicate must
  change.  Otherwise move to **A** (or leave as the documented default).

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
  some: 1 "growing professionalism.", 2 "feels himself safer.", 3/23 "goes
  green in the face!" (poison), 10 adds HP (`0x0EA90`), 30/31 clear the poison
  state (`0xEBE4`/`0xECE2`).  Effects the engine never compares (5, 7, 34, 35,
  37, …) keep purely descriptive labels.
- **Why unclear:** the *labels* are still derived by grouping the item table by
  effect value (effect 5 = "Crystal ball", effect 8 = "Antimagic ring", …); the
  engine stores no effect-name table, so the strings can only ever be
  descriptive.  Some handlers live in the play loop, so fully characterising
  each effect needs Lane M to drive those screens (a blocker).
- **To clear it:** trace each effect value's handler to a concrete stat change /
  message and align the label; for values the engine never tests, keep the
  descriptive name (the label is then a naming choice, not a claim about the
  engine).

---

## C. Owner-authorised guesses

*(none)*

---

## D. Can be removed — queued (normal task)

### G-001 — town name "Thyn." is hardcoded (Lane M)

- **What:** `compose_town_screen()` hardcodes the string `"Thyn. Which building
  do you"`. The town name is **variable**.
- **Source of truth:** `LOF.EXE` string table at file `0x45A1D`
  (`"You enter the proud town of"`, then NUL-separated `"Thyn. "` and
  `"Cyldane. "`), `0x45A4A` (`"Which building do you want to visit?"`), then the
  building labels.
- **Fix:** take the name as a parameter of `compose_town_screen()`; select it from
  the save/scenario. The selector is discoverable by tracing the call to the
  routine that prints `ds:0x52D`. Both currently-available save slots resolve to
  Thyn, which is why the current frame still matches.
- **Status:** derivable → normal Lane M task (not a blocker).

---

## E. Known non-determinism (not a guess)

### RNG seeded from the BIOS tick counter (Lane B / combat)

- **Fact:** the engine seeds its generator from `INT 1Ah` (TIME): `analysis/lof.asm`
  image `0x47A6` does `int 0x1a` then `add cx,dx` as the seed. Enemy
  placement/movement is therefore non-reproducible **by design**.
- **Consequence:** combat cannot be pinned to a bit-exact golden frame; any
  combat oracle must inject a fixed seed. This is **not** a guess — the algorithm
  is in the decompilation — it is listed so no agent mistakes the randomness for
  underived behaviour.

---

## F. Resolved / removed

*(none yet)*
