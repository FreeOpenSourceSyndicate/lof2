# Legend of Faerghail — native Ubuntu drop-in

A native, editable C/C++ re-implementation of the 1990 DOS RPG *Legend of
Faerghail* (reLINE Software). The deliverable is a pair of drop-in binaries —
`lof` and `vor` — that read the game's **data** files and reproduce the engine.
They never execute the original `LOF.EXE` / `VOR.EXE`.

```sh
./bootstrap.sh      # restore the (git-ignored) game data + build + test
make check          # health check: poison guard, build, tests, git hygiene
```

## If you are an agent (or starting one)

**Read these, in order. They are short and they tell you everything.**

1. `docs/HANDOFF.md` — orientation, the two hard constraints, and the rules for
   surviving a workspace reset or an unrecoverable error.
2. `docs/RISKS.md` — the four risks (especially the poisoned sibling repo).
3. `docs/WORKSTREAMS.md` — the work split, file ownership, and **how to pick your
   workstream yourself** (no one needs to assign it to you).

You do not need any prior context. `AGENTS.md` holds the deep technical
reference; `docs/` holds the operating instructions.

## Layout

| Path | What it is |
|------|------------|
| `src/` | 8086 + EGA **emulator** — an oracle that runs the originals (dev aid only) |
| `port/` | the **deliverable** — native drop-in that reads data files |
| `tools/` | Python helpers (`ega.py`, `dngs.py`, `compare.py`, `decompile.py`) |
| `analysis/` | annotated disassembly listings |
| `game/`, `downloads/` | proprietary game data (git-ignored; `bootstrap.sh` restores) |
