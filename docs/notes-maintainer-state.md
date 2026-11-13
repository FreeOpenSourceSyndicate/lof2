# notes-maintainer-state.md — session handover (2026-11-10)

Written by the incoming maintainer at the end of the first session, because the
previous maintainer was interrupted mid-work and their state was unclear. Read
this first, then `docs/HANDOFF.md` (the technical reference) and
`docs/WORKSTREAMS.md` (who owns what).

## Session 2026-11-12 (sixth session) — TL;DR

Environment HEALTHY. HEAD `97c9167`, `main` clean, 0 ahead / 0 behind.

- **Town screen decoded + shipped** — `./lof --dir game --town`.  Composition:
  `EGA/UP/MSCR` + `EGA/TWN/TWN00` at (10,11) + the engine message/building menu
  in `CHR1` at x=139 (rows 92..188).  Every glyph template-matches a `CHR1`
  cell exactly; recovered two unmapped cells (`.`=band1 cell 34, `?`=band1
  cell 35) now in `port/font.cpp`.  Static match 99.2% vs the emulator frame
  (`ref/oracle/town.png`); residue = a 16x16 top-left patch + the dynamic party
  panel.  Guarded by `tests/port_town_test.py` (wired into `make golden-port`).
  Commits `921665a`, `44f38fb`, `97c9167`.
- **Open PRs (both other lanes' DRAFTS; left to their authors):** #99
  (`work/agent-b-itemuse-fix`, BLK-B003, **merges cleanly**) and #89
  (`work/agent-a-blockers`, **conflicts** on `docs/BLOCKERS.md`).
- **Next session (lane M):** the building screens (Emporium/Temple/…).  The
  `shop`/`temple_bldg` golden scenarios reach them, but a 1700-frame capture was
  **partially drawn** — capture with `--frames` ≥3000 and a larger `LOF_KEYGAP`
  first.  Then the 16x16 town corner patch (source unidentified; not any single
  `EGA/**` asset).

## Session 2026-11-12 (fifth session) — TL;DR

Environment verified HEALTHY (`./check.sh` → HEALTHY at the end of the session).

- **BLK-A002 RESOLVED — the plane-0 bug was a dropped zero-byte port write.**
  Root cause: `Machine::port_out16` only drove the paired EGA data port when
  `AH != 0` (guard `if (v & 0xFF00)`), so a 16-bit `out dx, ax` that set a
  register to **0** was silently dropped and the register kept its old value.
  The game's plane blitter cycles GC *Read Map Select* `0,1,2,3` / SEQ *Map
  Mask* `1,2,4,8` across four passes (`mov al,idx; mov ah,data; out dx,ax`);
  the **first** pass sets Read Map Select **0**, that write was dropped, so the
  pass read the still-selected plane 3 and copied it into plane 0. Hence plane 0
  of *every* frame was wrong while planes 1–3 were perfect.
  - How it was found: scanning each plane for its best base offset in the
    emulator's plane memory — planes 1–3 matched the oracle at base `0x0000`
    (100%), plane 0 matched best at `0x1F40` (88%). The earlier "double buffer"
    probe (`A1F40`) was a *symptom* of this bug, not a renderer/page bug.
  - Fix: always write the data byte (a zero data byte is a legitimate value).
  - Verified against `ref/oracle/menu.png`: **all four planes 100.00%** (plane 0
    80.29% → 100.00%; overall 80.29% → 100.00%). Golden hashes re-recorded
    (`tests/golden.txt`). Recorded in `AGENTS.md` (do-not-regress) and
    `docs/blockers/A.md` (BLK-A002 → Resolved). PR **#98** (merged).
- **Play-screen check:** the play oracle (`ref/oracle/play.png`) still differs
  (overall 0.7787, planes 1–3 ~98% not 100%), but that is a **different
  animation/game-state frame**, not a plane bug — the diff is spread over the
  dynamic viewport + party region, and the menu oracle (same emulator run, one
  screen earlier) is byte-exact. Do not chase it as an emulator bug; if it
  matters, regenerate the play oracle from the same save/animation phase.
- **Open PRs left as-is (not mine to merge):** #89 (Agent A, `work/agent-a-blockers`)
  and #91 (Agent B, `work/agent-b-itemuse-static`). Both are DRAFT and both
  conflict on `docs/BLOCKERS.md`; #91 also carries the disputed item-use offset
  (see `docs/blockers/B.md` BLK-B003 + the maintainer addendum). They need their
  owners to rebase/renumber; do not merge them without that.

## Session 2026-11-12 (fourth session) — TL;DR

Environment verified HEALTHY (`./check.sh`: repo, game data, both builds,
`make test`, oracle goldens, drop-in goldens, clean tree all ok).

- **BLK-A002 reframed — it is an EGA plane-0 (blue) bug, not the palette.**
  Re-derived the emulator's index image from its own plane dump (`LOF_EDUMP`)
  and compared to the oracle's index image (oracle RGB inverted through the
  standard palette; all 64000 oracle pixels invert cleanly): planes **1/2/3 are
  byte-identical** (100.000%), plane 0 matches **80.29%**, 12616 pixels differ
  and **every** difference is plane-0-only. So re-palletting the dump can never
  exceed ~80% (the index image itself is 80.3% right — exactly the figure the
  previous "standard palette" comparison reported). The wrong palette (ramp
  `00..07 10..17`) is a **separate cosmetic** issue. Engine blits one plane at a
  time (`119D:0184` cycles GC Read Map Select + SEQ Map Mask; `119D:0F90` /
  `119D:1250` `rep movsb` plane 0 under mask `0x01`); prime suspect is the
  `gc_latch` handling in `src/ega.cpp` `Ega::write`. **Emulator-only** — the
  drop-in is correct (99.8% vs the oracle); do not change it. PR **#94** (docs
  only) merged. Detail: `docs/blockers/A.md` BLK-A002 and
  `docs/notes-maintainer-town.md` ("BLK-A002 CORRECTED").
- **Next step for BLK-A002:** instrument `gc_latch` around the plane-0
  `rep movsb` (which write mode is in effect for the `119D:0184` loop) and fix
  the plane-0 write path; verify against `ref/oracle/menu.png`.
- No code changed this session (diagnosis + docs only). Two agent drafts remain
  open and unmerged (#89 needs a rebase/renumber; #91 has the conflated-offset
  problem noted above).

## Session 2026-11-12 (third session) — TL;DR

Environment verified HEALTHY (`./bootstrap.sh` + `make test`). Picked up the
in-progress **BLK-B002** (was BLK-005) and closed it.

- **BLK-B002 resolved — the "runtime item-use table" is the static spell table.**
  A live 1 MB dump of the town path (new `LOF_SEGDUMP` hook) is byte-identical to
  the raw `LOF.EXE` image at `0x3EF32`: the `es:[idx*0x24+0x2712]` table is the
  141-record spell table, already transcribed correctly in `port/char_spells.inc`.
  No live-engine dump was ever needed. Full evidence:
  `docs/notes-maintainer-itemuse.md`. PR **#93** (merged).
- **BLK-B003 opened (Agent B).** `port/char_itemuse.inc` / `tools/spec.py
  emit_itemuse` extract from *file* `0x42712` = image `0x3D222` = `ds:0xa02`,
  which **no instruction references**; `item_use_flags()`/`item_is_usable()` are
  therefore built on bytes the engine never reads. Item state comes from the
  32-byte item record (`es:[bx + 0xfda]`, `shl bx,5`).
- New emulator hook `LOF_SEGDUMP=<file>,<seg>:<off>:<len>` (dump any linear
  range) and `ds=`/`ss=` added to the `LOF_DEBUG` line; documented in `AGENTS.md`.
- Note: `check.sh` is slow here (~11 golden scenarios, some 1600 frames) and can
  exceed a 10-minute command timeout; `make test` + the `boot` golden hash are
  the fast checks.
- **Two open PRs reviewed (not merged — they need fixes):**
  - **#91** (Agent B, `work/agent-b-itemuse-static`): also claims BLK-B002 but
    with a **conflated offset** — it treats "image `0x42712`" as "ds `0x2712`".
    The MZ header is `0x54F0`, so image `0x42712` is really `ds:0x0A02`, and the
    `es:[bx*0x24+0x2712]` read lands on the **spell** table. Its relocation
    analysis is real but for image `0x3D222`. Left a review comment asking it to
    retarget; detail in `docs/notes-maintainer-itemuse.md` (addendum).
  - **#89** (Agent A, `work/agent-a-blockers`): adds its own `BLK-006`/`BLK-007`,
    which collided with `main`'s `BLK-006` under the old single-counter scheme.
    **Resolved structurally** this session: the registry is now split one file
    per lane with lane-lettered IDs (`docs/blockers/{A,B,M}.md`), so no two lanes
    can collide. Left a comment on #89 to rebase and renumber its entries to the
    lane scheme (`BLK-A...`).

## Session 2026-11-11 (second session) — TL;DR

Environment verified HEALTHY (`./check.sh`). No open PRs at start. This session:

- Diagnosed BLK-A002 (was BLK-002): the **emulator's whole play-loop palette is the wrong
  ramp** (see item 2 below and `docs/BLOCKERS.md`). The drop-in is already
  correct; do not change it. PR #90 (docs only).
- Corrected the palette item in this handover (it carried the superseded
  "dungeon viewport RGB" framing).
- Slot-2 load remains genuinely slow (a full `--dir game` play run exceeds
  several minutes); `--frames 100` is ~18 s. Use `LOF_NO_TIMER=1` and small
  frame counts for triage.

## TL;DR — what changed this session

The previous maintainer's work was **unlanded**: 48 commits on
`work/maintainer-town` (PR #50, still a draft) on top of a `main` that had moved
on by 28 commits. Nothing was lost, but the tree everyone builds from (`main`)
did **not** have the save-load fix, so it could not reach the in-game screen.

This session landed it and cleared the PR queue. `main` now contains:

| PR | What | State |
|----|------|-------|
| #50 | The maintainer's play-loop work (see below), merged with `main` | **merged** |
| #82 | Workstream A: dungeon-screen oracle check + SPEC-index blocker resolved | **merged** |
| #85 | Workstream B: character-sheet gold/rations fields + weight base | **merged** |
| #86 | `check.sh`/`bootstrap.sh` hardening (this session) | **merged** |

`main` tip: `3cfce11` (then #86's squash). No open PRs remain.

## What PR #50 actually contained (now on `main`)

It was labelled "REP CMPS/SCAS fix" but is the whole play-loop milestone:

- **Emulator (the oracle):**
  - `REP CMPS/SCAS` now leave the remaining count in `CX` (`c.cx--`, not
    `c.cx = 0`). Without it the MS-C `strlen` returns 0xFFFE and a `sprintf`
    overflows a stack local, `retf`-ing to `0000:0000` on save load.
  - DOS 8.3 name resolution matches space-padded names (`GAME  2` -> `GAME2`),
    so the engine can open `GAMES\GAME%3d` saves. **This was the real save-load
    blocker**, not the allocator.
  - Adaptive key feed (`LOF_KEYAUTO`/`LOF_KEYGAP`/`LOF_KEYLOG`), exact-frame key
    scheduling (`LOF_KEYAT`), INT 21h AH=0Ch, clock override (`LOF_HOUR`).
  - New golden frames: `temple` (slot 1) and `play`/wilderness (slot 2).
- **Drop-in (`port/`, the deliverable):**
  - Play screen (`--play SUB/DPNNN`): `EGA/UP/MSCR` + first-person viewport tile
    at (10,11) + the full six-member party panel from the save (panel is
    **pixel-exact**; whole screen ~95% vs DOSBox, residue = dynamic overlays).
  - Interactive front-end (`--ui`, `--ui-keys`): title -> credits -> menu -> `O`
    loads a save -> play screen; `Q`/Esc quits.
  - Walk loop (`--walk`, `--walk-keys`): arrow keys step/turn over the decoded
    dungeon map.
  - VOR preloader (`EGA/IN/DRAG` + `EGA/IN/CRED`), pixel-exact.
  - Correct standard EGA attribute palette; the compact small-caps panel face.

### Merge-conflict resolution (recorded so it is not re-litigated)

Merging current `main` into the branch hit three conflicts:

1. `tests/char_test.cpp` — took `main`'s renamed inventory fields
   (`equipped`/`condition`), and pointed the wilderness-inventory assertions at
   `sv2` (GAME2) because the maintainer renumbered the save slots.
2. `port/char.h` — both sides added a `kCharProf1b` enum; removed the duplicate.
3. `docs/BLOCKED-agent-a.md` — kept the maintainer's "UNBLOCKED" text.

## Save-slot layout (important — differs from the raw archive)

The maintainer renumbered the shipped saves: **slot 1 = the Temple save** (root
`GAME1`, installed by `bootstrap.sh`), **slot 2 = the wilderness save**
(`GAME2`). The archive's own `GAMES/GAME1` is a *different* file (byte
0x9a2/0x9a4: root = 5,0; archive = 0,1). `bootstrap.sh` installs the committed
root `GAME1` as slot 1, so do not "restore" the archive copy over it.

## Verification status

- `make test` green (cpu / font / dungeon / char).
- `make golden` green (boot / look / open / add / dism / erase / temple / play).
- `make golden-port` green (menu / play / ui / walk / vor).
- `./check.sh` → HEALTHY (run again at the start of your session; see below).

## Environment (fresh/reset workspace)

`./bootstrap.sh` now installs the Python deps the frame tests need
(`numpy`, `Pillow`) and warns if SDL2 dev files are absent. If `check.sh` ever
prints `golden-port failed` with no other clue, read
`/tmp/lof_golden_port.log` — a missing dep or a build error lands there.

## What is genuinely open (next session's work)

Pick a workstream from `docs/WORKSTREAMS.md`. The highest-value open items:

1. **First-person viewport composition (workstream A).** The drop-in's `--walk`
   viewport is still the static `DP001` tile; the engine composites per-cell
   sprites (full views + smaller side pieces). Ground truth: `tools/dngs.py
   oracle` scores a composite against `ref/oracle/dungeon_slot1*.png`. The
   spawn view is `DP001` (0.952); the *moved* view matches no single sprite
   (best 0.576), confirming a composite. See `docs/notes-a.md`.
2. **Emulator play-loop plane 0 (workstream A / BLK-A002).** *Reframed
   2026-11-12; the "palette" framing was a red herring.* The emulator's frame at
   the menu, play and temple differs from the DOSBox oracle **only in EGA plane 0
   (blue)**: comparing the emulator's own index image (`LOF_EDUMP`) to the
   oracle's index image (oracle RGB inverted through the standard palette), planes
   **1/2/3 are byte-identical** and plane 0 matches 80.29% — 12616 differing
   pixels, every one plane-0-only. The emulator *also* ends up with the wrong
   attribute palette (ramp `00..07 10..17`), but that is a **separate cosmetic**
   issue: re-palletting the dump cannot exceed ~80%, because the index image
   itself is only 80.3% right (that is exactly the "standard palette" figure the
   old note reported, so the old "only the palette is wrong" conclusion was
   wrong). The engine blits one plane at a time (`119D:0184` cycles GC Read Map
   Select + SEQ Map Mask; `119D:0F90`/`119D:1250` `rep movsb` plane 0 under mask
   `0x01`), so the bug is in the plane-0 write path — prime suspect the
   `gc_latch` handling in `src/ega.cpp` `Ega::write` (write modes 1/2 take the
   value from the latches). **Emulator-only:** the drop-in (`port/gfx.cpp`) is
   correct (99.8% vs the oracle) — do not change it. Full evidence:
   `docs/blockers/A.md` BLK-A002 and `docs/notes-maintainer-town.md`
   ("BLK-A002 CORRECTED").
3. **Font glyphs (resolved — `BLK-M001`).** The engine blits `CHR1`/`CHR2`
   cells directly (no bold/double-struck transform); the port already does, and
   its menu text is pixel-exact. The "bold, double-struck" note was an artefact
   of the wrong-palette era. See `docs/blockers/M.md` and `AGENTS.md`.
4. **Combat / shop / item-use (workstream B).** *Updated 2026-11-12.* The
   "runtime item-use table" is settled — it is the **static spell table**, already
   transcribed (BLK-B002, resolved; PR #93). What remains for B is **BLK-B003**:
   `port/char_itemuse.inc` is mis-sourced (image `0x3D222`, unreferenced) and
   must be re-derived from the item record or dropped. Shop stock/prices still
   want a scripted shop scenario (`--keys`/`LOF_KEYAT`). See `docs/notes-b.md`
   "Blockers" and `docs/notes-maintainer-itemuse.md`.

## Hygiene

- Do **not** read/clone/fetch the sibling repo `FreeOpenSourceSyndicate/lof`
  (poisoned). `check.sh` guards this.
- `game/`, `downloads/`, `GAME1` are git-ignored (proprietary); restore with
  `./bootstrap.sh`.
- Commit and push after every meaningful step — the workspace can reset.

### Guesswork audit (2026-11-12, sixth session)

All guesses are now tracked in the canonical register **`docs/GUESSWORK.md`**
(read it every session, per `docs/RULES.md`). The Lane-M findings from this
audit were moved there:

- **G-001** (queued task) — the town name "Thyn." is hardcoded in
  `compose_town_screen()`; the town name is a variable (a string table in
  `LOF.EXE` at file `0x45A1D`, names `"Thyn. "` and `"Cyldane. "`). Derivable →
  normal task.
- **G-002** (watchlist) — the 16×16 town top-left patch; not `MSCR`, not `TWN00`,
  not any single `EGA/**` asset, not a shift/wrap of either. Removability unclear
  → revisit until clear.
- **E (not a guess)** — the RNG is seeded from the BIOS tick counter
  (`analysis/lof.asm` image `0x47A6`, `int 0x1a` then `add cx,dx`), so combat is
  non-reproducible by design; listed so nobody mistakes it for a guess.
- **Party-panel live state** (time of day, morale, rations) is engine state a
  save does not carry; the port deliberately draws the labels only. This is
  correct-by-construction, not guesswork.
