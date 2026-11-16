# RISKS — read before you touch anything

This project runs with agents on the same repo. The hazards below have already
caused real loss and are treated as first-class engineering concerns.

## Risk 1 — workspace reset

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

## Risk 2 — the red tree (unrecoverable errors)

The classic way an agent gets stuck is editing a shared file until the build or
tests break, with no green state left to return to.

**Mitigations:**

1. Know the files your item touches (see `docs/WORKSTREAMS.md`) and keep to
   them; if other agents are running, do not touch a file another is editing.
2. Before a risky change, note the current commit: `git rev-parse HEAD`.
3. If the tree goes red and you cannot fix it within a few minutes:
   `git checkout -- <files>` or `git stash`, confirm `make test` is green again,
   then add an entry to the area's blocker file under `docs/blockers/` (A, B or
   M — see the hub `docs/BLOCKERS.md`) describing the problem (and a
   `docs/BLOCKED-<name>.md` file if it is long).
4. `make test` green is the shared contract; never push a branch that breaks it.

## Risk 3 — accidentally running the original binaries

The deliverable must be a *re-implementation*. The emulator (`src/`) exists only
to observe the originals during development. The shipped `port/` binaries must
read only game **data** files (`EGA/`, `SYST/`, `DNGS/`, `ROST/`, `*.SAV`), never
`LOF.EXE` / `VOR.EXE`. Reviewers reject any `port/` change that opens an `.EXE`.
