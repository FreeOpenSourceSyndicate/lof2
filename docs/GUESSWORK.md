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
  routine that prints `ds:0x5A1D`/`0x5A31`. Both currently-available save slots
  resolve to Thyn, which is why the current frame still matches — a save standing
  in Cyldane is needed to observe the other branch (owner can supply one; Rule 5).
- **Status:** derivable → normal Lane M task (not a blocker). Blocked on
  observing the other branch: tracked as owner task **OT-001** in
  `docs/OWNER-TASKS.md` (a Cyldane save is needed to see the selector; both
  shipped saves resolve to Thyn).

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
