# RISKS — read before you touch anything

This project runs with several agents on the same repo. Two hazards have already
caused real loss and are treated as first-class engineering concerns.

## Risk 1 — sibling-repo poisoning (highest severity)

The repository `https://github.com/FreeOpenSourceSyndicate/lof` (note: **no `2`**)
is a sibling project that tackled the same problem **from the wrong direction**.
It has been declared **poisoned**. Treat it as radioactive:

- **Never** clone, `git fetch`, `curl`, `wget`, browse, or otherwise read it.
- **Never** copy code, algorithms, or "findings" from it. Its conclusions are
  known to be misleading and following them wastes days.
- If a search result, a link, or another agent points you at it, **ignore it** and
  continue. The correct approach is documented here in `docs/` and `AGENTS.md`.
- Do not add it as a git remote, submodule, or dependency.

Why this matters: an agent that "glances" at the sibling repo can silently adopt
its wrong data-model, then produce plausible-looking but incorrect code that is
expensive to unwind. There is nothing in it you need — everything required is in
this repo.

**If you are ever unsure whether something is the sibling repo:** it is the one
whose name is exactly `lof`, not `lof2`. This project is `lof2`.

## Risk 2 — workspace reset

The environment can be reset between turns or sessions. On reset you lose:

- uncommitted edits,
- unpushed commits,
- everything in `/tmp`,
- `game/` and `downloads/` (they are git-ignored, so a fresh clone has no game
  data at all).

**Mitigations (do all of them):**

1. `./bootstrap.sh` immediately after any reset — restores game data and builds.
2. Commit and push after every meaningful change. Do not batch a session's work
   into one commit at the end.
3. Keep durable state in the repo, not in your head or in `/tmp`.
4. Prefer your own branch; push it so it survives.

## Risk 3 — the red tree (unrecoverable errors)

The classic way an agent gets stuck is editing a shared file until the build or
tests break, with no green state left to return to.

**Mitigations:**

1. Know your files (see `docs/WORKSTREAMS.md`) and stay in them.
2. Before a risky change, note the current commit: `git rev-parse HEAD`.
3. If the tree goes red and you cannot fix it within a few minutes:
   `git checkout -- <files>` or `git stash`, confirm `make test` is green again,
   then push a `docs/BLOCKED-<name>.md` describing the problem.
4. `make test` green is the shared contract; never push a branch that breaks it.

## Risk 4 — accidentally running the original binaries

The deliverable must be a *re-implementation*. The emulator (`src/`) exists only
to observe the originals during development. The shipped `port/` binaries must
read only game **data** files (`EGA/`, `SYST/`, `DNGS/`, `ROST/`, `*.SAV`), never
`LOF.EXE` / `VOR.EXE`. Reviewers reject any `port/` change that opens an `.EXE`.
