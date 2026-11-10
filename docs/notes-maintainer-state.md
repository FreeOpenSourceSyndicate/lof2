# notes-maintainer-state.md — session handover (2026-11-10)

Written by the incoming maintainer at the end of the first session, because the
previous maintainer was interrupted mid-work and their state was unclear. Read
this first, then `docs/HANDOFF.md` (the technical reference) and
`docs/WORKSTREAMS.md` (who owns what).

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
2. **Dungeon viewport palette (workstream A).** The emulator draws the temple in
   **colour** while the oracle is in **greys**; shape matches 99.6% but the RGB
   does not. The maintainer traced this to the EGA attribute/palette path (how
   plane bits map to attribute indices at render time), **not** a missing sprite.
   Re-record the oracle with the exact capture sequence before treating 100% RGB
   as the bar — the frame is a live animation (58% raw match at frame 1500, 66%
   at 2500).
3. **Font glyph transform (open, documented in `AGENTS.md`).** The engine's
   menu/town/panel glyphs are a *bold, double-struck* form with no exact match in
   `CHR1`/`CHR2`; the drop-in approximates with the `CHR1` cells. Next step:
   disassemble the glyph-blit routine that reads the `4130:` buffer.
4. **Combat / shop / item-use oracle path (workstream B blocker).** B has
   grounded everything reachable statically; the last runtime tables need a
   scripted path into a combat encounter / shop / item-use screen
   (`--keys`/`LOF_KEYAT`). See `docs/notes-b.md` "Blockers".

## Hygiene

- Do **not** read/clone/fetch the sibling repo `FreeOpenSourceSyndicate/lof`
  (poisoned). `check.sh` guards this.
- `game/`, `downloads/`, `GAME1` are git-ignored (proprietary); restore with
  `./bootstrap.sh`.
- Commit and push after every meaningful step — the workspace can reset.
