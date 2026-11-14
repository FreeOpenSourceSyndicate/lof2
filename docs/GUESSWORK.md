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

### G-003 — dungeon solid-wall set `{6,13}` is oracle-derived, not decompiled (Lane A)

- **What:** `dungeon_is_solid_tile()` in `port/dungeon.cpp` treats tiles 6 and 13
  as the only walls (everything else, including the void tile 0, is walkable).
- **Source of truth:** the emulator **opacity sweep** (Round 27,
  `docs/notes-a.md`): patching the ahead cell to code `X` and varying the cell
  *behind* it leaves the view byte-identical only for `X in {6,13}`; the render
  for those is the full-screen wall face `DP036` (== `DP060`, 100% ink).  This is
  source-of-truth #3 (oracle frames), so it is not a *chosen* value.
- **Why it is still on the watchlist:** the sweep covered the ahead cell of one
  DNG05 corridor.  It is direct evidence for 6/13 and for the tested see-through
  codes (0/1/2/3/7/8/12), but codes that never occur directly ahead of a party
  were not individually swept, and the *generality* (all 29 levels) is inferred
  from consistency, not traced in the engine.
- **To clear it:** confirm the wall test in the decompilation (`analysis/lof.asm`,
  the move/collision path near image `0xD83`), which is a **Lane M** routine — if
  it is not removable by Lane A it becomes a blocker (`docs/blockers/A.md`).
  Cross-checking is also possible with more oracle sweeps (patch the *party
  cell*, not the ahead cell) at no decompile cost.

### G-004 — open-corridor dungeon view falls back to `DP001` (Lane A)

- **What:** `dungeon_base_view_index()` returns `DP001` for the open-cell ahead
  codes `0x0/0x2/0x7/0x8/0x9`, because the engine's actual view for those cells
  is a composed image that no single `DPnnn` reproduces (BLK-A003).
- **Source of truth:** the Round-28 ahead-code sweep (`docs/notes-a.md`) shows
  the true window for those codes (ink 4941; identical for 0/2/7/8) matches no
  single sprite (best IoU 0.35 across all 221 120×119 EGA files), no `DPnnn`/
  `ANIM` piece composite (colour-exact search finds zero placements), and not the
  drop-in's ray-caster (IoU 0.17).  So `DP001` is a **chosen** fallback, not a
  decoded value.  Round 29 sharpens it: the engine projects the first
  *view-opaque* cell at its depth (depth 1 → `DP001`, depth 2 → a trapezoidal
  wall face, depth >= 3 → the corridor image, saturating); `DP001` is only exact
  at depth 1.
- **Why it is a guess:** it is not the engine's image.  It is the closest
  single-sprite approximation for the corridor case until the composition rule is
  decoded (BLK-A003).
- **To clear it:** decode the engine's per-depth geometry composition (Lane M
  decompile of the view routine, or further oracle sweeps that vary the whole
  column) so the corridor view can be rendered natively.

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
