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
2. **Emulator play-loop palette (workstream A / BLK-002).** *Corrected
   2026-11-11.* The emulator's final attribute palette at the menu, play and
   temple screens is the **ramp** `00..07 10..17`, not the standard EGA set, so
   brown renders yellow, blue renders cyan and dark-grey renders dark-green —
   the whole play loop, not just the dungeon. Re-rendering the emulator's own
   plane dump with the standard palette matches the oracle far better
   (menu 80% vs 59%, play 78% vs 58%, temple 75% vs 58%), and the drop-in
   (already standard) matches `ref/oracle/menu.png` at 99.8%. So the standard
   palette is correct and only the emulator is wrong. The game programs the
   palette via `INT 10h AH=10h AL=02h` from `4130:4834`/`4130:47DF` (the ramp
   table is the literal bytes at EXE `0x7012`); the standard set is nowhere in
   the EXEs and the emulator traces zero direct `0x3C0` writes, so the engine
   must transform the table — that transform is the thing to recover. Ground
   truth (repo owner, in-game): the Temple is **greyscale right after loading**
   (standard palette grey indices 0/7/8/15), and colour appears only when moving
   (small green specs on near walls). Full evidence: `docs/BLOCKERS.md` BLK-002
   and `docs/notes-maintainer-town.md`. **Do not change the drop-in** — it is
   correct.
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
