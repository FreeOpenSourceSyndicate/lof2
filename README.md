# Legend of Faerghail — native Ubuntu drop-in

A native, editable C/C++ re-implementation of the 1990 DOS RPG *Legend of
Faerghail* (reLINE Software). The deliverable is a pair of drop-in binaries —
`lof` and `vor` — that read the game's **data** files and reproduce the engine.
They never execute the original `LOF.EXE` / `VOR.EXE`.

```sh
./bootstrap.sh      # restore the (git-ignored) game data + build + test
make port           # build the drop-in binaries ./lof and ./vor
make check          # health check: build, tests, git hygiene
```

Full build instructions — prerequisites, every `make` target, SDL2 vs head-less,
running the drop-in vs the emulator, and troubleshooting — are in
**[`docs/BUILD.md`](docs/BUILD.md)**.

## If you are an agent (or starting one)

**Read these, in order. They are short and they tell you everything.**

1. `docs/RULES.md` — **the owner's standing rules and their changelog.** Read
   this first, every session. In particular: **never guess** (see
   `docs/GUESSWORK.md`, the guess register, which you also read every session)
   and **do not edit a file another active agent is working on** (see
   `docs/OWNER-TASKS.md` for owner-only work).
2. `docs/HANDOFF.md` — orientation, the hard constraint, the **single work
   queue** you pull items from, and the rules for surviving a workspace reset or
   an unrecoverable error.
3. `docs/RISKS.md` — the three risks (workspace reset, the red tree, running the
   original binaries).
4. `docs/WORKSTREAMS.md` — the one-lane model, the **areas** (old lanes, kept for
   transparency), the file map, and **how to pick your own work item** (no one
   needs to assign it to you).

You do not need any prior context. `AGENTS.md` holds the deep technical
reference; `docs/` holds the operating instructions. If the owner asks you to add
rules to an agent's prompt, the paste-in block is
`docs/AGENT-PROMPT-ADDENDUM.md`.

## Layout

| Path | What it is |
|------|------------|
| `src/` | 8086 + EGA **emulator** — an oracle that runs the originals (dev aid only) |
| `port/` | the **deliverable** — native drop-in that reads data files |
| `saves/` | user-created save games (project assets, Rule 12; content-addressed) |
| `tools/` | Python helpers (`ega.py`, `dngs.py`, `compare.py`, `decompile.py`) |
| `analysis/` | annotated disassembly listings |
| `docs/` | rules, handover, work queue, and the build guide (`docs/BUILD.md`) |
| `game/`, `downloads/` | proprietary game data (git-ignored; `bootstrap.sh` restores) |

Build instructions live in **[`docs/BUILD.md`](docs/BUILD.md)**.
