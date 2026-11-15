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
- **To clear it:** trace the town-screen draw path in the decompilation. If the
  sprite/algorithm is found → move to **D** (or a blocker if another lane owns
  the routine). If it cannot be derived → move to **A**.

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

### G-001 — town name "Thyn." is hardcoded (Lane M)

- **What:** `compose_town_screen()` hardcodes the string `"Thyn. Which building
  do you"`. The town name is **variable**.
- **Source of truth:** `LOF.EXE` string table. Verified offsets (2026-11-14;
  `file = ds_offset + 0x40000`):
  - `ds:0x5A1D` = `"You enter the proud town of"` (file `0x45A1D`);
  - `ds:0x5A39` = `"Thyn. "` (file `0x45A39`), `ds:0x5A40` = `"Cyldane. "`
    (file `0x45A40`) — the **only** two town names, NUL-separated;
  - `ds:0x5A4A` = `"Which building do you want to visit?"` (file `0x45A4A`);
  - a *second* `"Thyn"` at `ds:0x687D` ("The Grand Board of Trades greets you in
    Thyn.", file `0x4687D`).
  - **Neither `0x5A39` nor `0x5A40` is loaded by a plain `mov ax, imm`** anywhere
    in the image (searched for `B8 39 5A` / `B8 40 5A`): the selector is computed,
    not a fixed pointer. **Nor is either address stored as data anywhere**: a
    search for the little-endian word `0x5A39` finds it **zero** times, and
    `0x5A40` only as instruction immediates (e.g. `26 C6 40 5A …` =
    `mov byte es:[bx+si+0x5a], …`), never as a pointer. So the name address is
    built entirely from a computed base + offset. The town routine
    (`image 0xC22B`) loads the *fragments* `0x5A31` ("town of"), `0x5A4C`
    ("ich building…"), `0x5A69` ("isit?") into AX and calls the string drawer
    (`lcall 0x18d:0x16a8`), so the name is chosen upstream of it.
- **Superseded (wrong):** the earlier entry claimed the names are at
  `ds:0x5A56`/`0x5A5E`; those are the *middle* of the "Which building…" string.
  See `docs/notes-maintainer-town.md` for the corrected mapping.
- **Fix:** take the name as a parameter of `compose_town_screen()`; select it from
  the save/scenario. The selector is discoverable by tracing the call site of the
  routine that prints `ds:0x5A1D`/`0x5A31`.
- **New evidence (2026-11-15, owner-supplied Cyldane save).** OT-001 is resolved:
  `game/GAMES/GAME4` stands the party in front of **Cyldane**; `O`,`4`,`Up`
  enters the town and the frame reads *"You enter the proud town of Cyldane."*
  (oracle `ref/oracle/town_cyldane.png`). So the selector **can now be traced**.
  - **Both town names are static and identical in both RAM dumps** (`ds:0x5A39`
    "Thyn." and `ds:0x5A40` "Cyldane." are present verbatim at the same addresses
    in the Thyn and Cyldane town screens): the engine does not rewrite the string,
    it **selects between the two addresses**. So the selector is the address of
    the name passed to the print routine.
  - Neither address appears as a literal anywhere (confirmed: no `B8 39 5A` /
    `B8 40 5A` immediate; word `0x5A39` occurs **zero** times in the image;
    word `0x5A40` only as instruction bytes). The address is computed (base +
    `0x5A39` + `idx*7`), consistent with the two names being `7` bytes apart
    (`"Thyn. \0"`). The town-index byte that drives it is not yet pinned; the
    town routine is `image 0xC22B` (prints `0x5A31`/`0x5A4C`/`0x5A69`), so the
    selector is upstream of it. Next: trace the callers of `0xC22B` with the
    Cyldane save loaded and find where the name pointer is formed.
- **Status:** derivable → normal Lane M task (not a blocker). OT-001 (owner task)
  is **resolved** (Cyldane save delivered); the hardcode removal is the remaining
  Lane M work. Both `GAME2` (Thyn) and `GAME4` (Cyldane) now reproduce in the
  oracle for side-by-side tracing.

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
