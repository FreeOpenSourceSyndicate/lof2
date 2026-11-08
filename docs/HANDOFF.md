# HANDOFF — read this first

You are one of several agents re-implementing the 1990 DOS RPG *Legend of
Faerghail* as native Ubuntu C/C++. This file tells you everything you need to be
productive in the first ten minutes, and how to survive a workspace reset or a
dead-end error. Deep detail lives in `AGENTS.md`; the work split lives in
`docs/WORKSTREAMS.md`.

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
> `FreeOpenSourceSyndicate/lof`.

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
- If you are truly stuck, push a branch with a `docs/BLOCKED-<yourname>.md` note
  describing exactly what fails and what you tried. A fresh agent (or the
  maintainer) can then pick it up with no context loss — that is the whole point
  of the "any agent replaceable" design.

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
- [x] EGA image format decoded (11-byte header) — `tools/ega.py`, `port/gfx.cpp`.
- [x] Font strip decoded + menu text — `port/font.cpp`.
- [x] Intro sequence (TITL -> CRED -> menu) in the drop-in.
- [x] Data readers: `SYST/SNSC`, `SYST/NSCnn`, `SYST/TXTn` — `port/data.cpp`.
- [ ] Emulator: scripted input to drive the menu into the game (oracle follow-up).
- [ ] Dungeon subsystem (`DNGS/`) — workstream A.
- [ ] Character/items/combat (`SYST/SPEC*`, `ROST/`) — workstream B.
- [ ] Main game loop / UI state machine — maintainer.
