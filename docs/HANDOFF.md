# HANDOFF — read this first

You are one of several agents re-implementing the 1990 DOS RPG *Legend of
Faerghail* as native Ubuntu C/C++. This file tells you everything you need to be
productive in the first ten minutes, and how to survive a workspace reset or a
dead-end error. Deep detail lives in `AGENTS.md`; the work split lives in
`docs/WORKSTREAMS.md`.

> **Incoming maintainer? Read `docs/notes-maintainer-state.md` first** — it
> records the last session's landing of the play-loop work and the current open
> items.

## 0. Sixty-second orientation

```sh
./bootstrap.sh            # restores game data + builds + tests (idempotent)
make                      # build the analysis emulator  -> ./lof-native
make port                 # build the drop-in binaries   -> ./lof ./vor
make test                 # CPU self-test + font test
make check                # health: poison guard, build, tests, git hygiene
```

### Minimal prompt to start (or replace) an agent

Anyone can spin up a fresh agent with just this:

> You are working in `FreeOpenSourceSyndicate/lof2`. Start by running
> `./bootstrap.sh` and reading `README.md`, then `docs/HANDOFF.md`,
> `docs/RISKS.md`, and `docs/WORKSTREAMS.md` — they tell you everything,
> including how to pick your own workstream. Prefer a workstream you already
> know. Work in long, uninterrupted stretches — keep pulling items off the
> checklist rather than stopping after one step. Do not edit files you do not
> own. Commit and push after every meaningful step (the workspace can reset).
> Review and merge your own PRs. Never read the sibling repo
> `FreeOpenSourceSyndicate/lof`. If you get blocked, add an entry to **your
> lane's** blocker file under `docs/blockers/` (A = dungeons, B = characters,
> M = maintainer; the ID convention and fields are defined in the hub
> `docs/BLOCKERS.md`) instead of just stopping.

That prompt plus the repo is all a replacement agent needs — no prior context.
The agent picks its own workstream from `docs/WORKSTREAMS.md` (see "Pick your own
workstream"); you do not have to point it at one.

### How to work (do this, do not wait to be told)

- **Prefer the workstream you already know.** Stay in the lane whose formats and
  code you have already learned; switching wastes the context you built. Move on
  only when your lane is done or blocked.
- **Work in long, uninterrupted stretches.** Push often for reset-safety, but do
  not stop after one small step to await the next prompt. Keep pulling items off
  the checklist in this file until you are blocked or the work is finished. Idle
  agents cost the project the most time; when your lane runs dry, take the next
  unchecked item here, or add a regression test for an entry in `AGENTS.md`'s
  "do not regress" list.
- **You own your PR end to end** — create it, review it, and squash-merge it when
  `make check` is green (see below). Do not wait for the coordinator to merge.

### Merging your PR

You may review and merge pull requests yourself — you do not need the maintainer.

1. Keep the branch green: `make check` must pass.
2. `gh pr create` (or the API) against `main`.
3. Wait for the mergeability check, then merge:
   ```sh
   gh pr merge <n> --repo FreeOpenSourceSyndicate/lof2 --squash
   ```
   (or the async API: `PUT /repos/.../pulls/<n>/merge-async`). Do not merge a
   branch whose `make check` fails.

- `src/`  — an 8086/EGA **emulator**. It runs the *original* `LOF.EXE`/`VOR.EXE`
  so we can observe behaviour and verify our re-implementation. It is a
  **development oracle only** — the shipped drop-in must not use it.
- `port/` — the **deliverable**: native C/C++ that reads the game *data* files
  and reproduces the engine. Never executes the original EXEs.
- `tools/` — Python helpers (`ega.py` decoder, `decompile.py`, capture script).
- `analysis/` — annotated disassembly listings (`lof.asm`, `vor.asm`).
- `game/`, `downloads/` — proprietary game data, **git-ignored** (see risks).

## 1. The two hard constraints

1. **Never execute `LOF.EXE` / `VOR.EXE` from the deliverable** (`port/`). Reading
   their bytes for analysis is fine; running them is the emulator's job only.
2. **Never look at the sibling repo `FreeOpenSourceSyndicate/lof`.** It took the
   wrong approach and is treated as poisoned. Do not clone, fetch, curl, browse,
   or read it. If a task seems to require it, it does not — see
   `docs/RISKS.md`.

## 2. You WILL hit these two risks — plan for them

### Risk A — your workspace can be reset without warning

Everything you have not committed and pushed is gone. Mitigation:

- **Commit and push after every meaningful step**, not at the end of the task.
  Small commits are cheap; lost work is not.
- Never rely on `/tmp` for anything you need to keep. Put durable notes in the
  repo (e.g. `docs/notes-<yourname>.md`) and push them.
- `game/` and `downloads/` are git-ignored, so a reset loses them. `./bootstrap.sh`
  restores them in one command. Run it first thing after any reset.
- Your *branch* is safe once pushed. Prefer pushing your own branch
  (`work/<yourname>`) over long-lived local-only work.

### Risk B — you can hit an error you cannot recover from

The usual cause is a change to shared code (`src/`, `port/`, `Makefile`) that
breaks the build or the tests, leaving you with no green state to fall back to.
Mitigation:

- **Never leave the tree red.** If you cannot fix a break quickly, `git stash` or
  `git checkout -- <file>` to return to the last green commit, then push a note.
- **Stay in your lane.** Each agent owns specific files (see
  `docs/WORKSTREAMS.md`). Do not edit files owned by another agent without
  coordinating in the PR; that is how builds break.
- **Keep `make test` green.** It is the shared contract. If your work needs a new
  test, add it to the test target in `Makefile`.
- If you are truly stuck, **add an entry to your lane's blocker file** —
  `docs/blockers/A.md` (dungeons), `docs/blockers/B.md` (characters/items) or
  `docs/blockers/M.md` (shared/maintainer) — with the blocked lane, responsible
  agent, priority, precise description + what unblocks it, and dependencies (the
  fields and the `BLK-<lane><nnn>` ID convention are in the hub
  `docs/BLOCKERS.md`) and push it. Add a `docs/BLOCKED-<yourname>.md` detail file
  when the diagnosis is long, and link it from the registry entry. A fresh agent
  (or the maintainer) can then pick it up with no context loss — that is the
  whole point of the "any agent replaceable" design.

## 3. How to prove your work is correct

The emulator is the oracle. For any screen or behaviour you implement in `port/`:

1. Run the original under the emulator and capture a frame:
   `LOF_CMDLEN=3 ./lof-native --dir game --headless --frames 300 --shot /tmp/oracle.png`
2. Render the same thing with the drop-in:
   `./lof --dir game --headless --shot /tmp/mine.png`
3. Compare (exact or near-exact):
   `python3 tools/compare.py /tmp/oracle.png /tmp/mine.png`

A subsystem is "done" when the drop-in matches the oracle frame for the relevant
screen, and `make test` still passes.

## 4. Current status (update this list as you go)

- [x] Analysis emulator boots LOF to the main menu (EGA mode 0Dh).
- [x] **Emulator renders all four EGA planes correctly** (BLK-A002, fixed
      2026-11-12): the emulator frame is byte-exact against `ref/oracle/menu.png`
      (all planes 100%). The cause was `port_out16` dropping a zero data byte;
      see `AGENTS.md` "Fixed bugs".
- [x] Scripted input drives the menu: `--keys`/`LOF_KEYFRAME` inject keys, and
      letters switch screens (e.g. `L` opens the character screen). The menu is
      keyboard-driven, not mouse-only — an earlier note said otherwise; it was
      wrong (only Enter had been tried).
- [x] Oracle golden-frame regression test — `tests/golden_test.sh` (`make golden`,
      also run by `check.sh`). Guards the emulator output every drop-in screen is
      verified against.
- [x] EGA image format decoded (11-byte header) — `tools/ega.py`, `port/gfx.cpp`.
- [x] Font strip decoded + menu text — `port/font.cpp`.
- [x] Intro sequence (TITL -> CRED -> menu) in the drop-in.
- [x] Data readers: `SYST/SNSC`, `SYST/NSCnn`, `SYST/TXTn` — `port/data.cpp`.
- [x] Emulator: menu key map complete; **the in-game screen is reachable**:
      `O` then Enter loads the save and the engine reads `ROST/ROST`,
      `ROST/SHOP` and the town graphics (`EGA/TWN/TWN00..05`) — the town/play
      screen. Scripted as the `town` scenario in `tests/golden_test.sh`.
- [x] Drop-in exposes workstream B's reader: `./lof --dir game --dump-spec`
      (decoded `SYST/SPEC0..7`).
- [x] Drop-in: play-screen static composition (`--play`) — HUD background +
      viewport tile + the **full** party panel (name, profession, `+0x1e` stat
      number, `GOOD` status).  The panel is pixel-exact; the screen matches the
      DOSBox capture at ~94.2% (residue = the engine's dynamic viewport sprite).
      Guarded by `tests/port_play_test.py`.
- [x] Drop-in: reproduce the in-game (town) screen — `--town`.  It is
      `EGA/UP/MSCR` + `EGA/TWN/TWN00` at (10,11) + the engine's message and
      building menu drawn in `CHR1` at x=139 (rows 92..188).  Static match
      99.2% vs the emulator frame (viewport + every text row pixel-exact; the
      residue is a 16x16 top-left patch and the dynamic party panel).  Guarded
      by `tests/port_town_test.py` (oracle `ref/oracle/town.png`).
- [ ] Building screens (Emporium/Temple/Inn/Bank/…): the `shop`/`temple_bldg`
      golden scenarios capture the Emporium/Temple, but the screen is a
      different (more complex) layout; the first capture was incomplete.  Next
      session: capture with a longer key gap and decode.  See
      `docs/notes-maintainer-town.md`.
- [ ] Dungeon subsystem (`DNGS/`) — workstream A.
- [ ] Character/items/combat (`SYST/SPEC*`, `ROST/`) — workstream B. **Unblocked:**
      the SPEC/ROST reads now happen in the oracle (`town` scenario), so B can
      ground the remaining stat/combat semantics against real in-game state.
- [x] Front-end UI state machine (`--ui`) — maintainer.  Keyboard-driven intro
      (title -> credits -> menu) with the menu hot-keys wired: `O` loads a save
      and enters the play screen, `Q`/Esc quits.  Input via `Window` (SDL2);
      `--ui-keys` scripts a key sequence for head-less regression testing
      (`tests/port_ui_test.py`).  In-game movement/combat remain to do.
- [x] Native walk loop (`--walk`) — maintainer.  Loads the save's dungeon
      (`DNGS/DNG<d><l>` from the save's location byte), places the party and
      replays a scancode sequence through agent A's walk model
      (0x48 step, 0x50 turn 180, 0x4b/0x4d turn), then renders the play screen
      with the level map in the viewport.  `--walk-keys` scripts the input;
      guarded by `tests/port_walk_test.py`.  The first-person viewport is still
      the static `DP001` tile (the engine's per-cell sprite composition is
      workstream A's remaining task).
