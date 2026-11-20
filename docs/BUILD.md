# BUILD — how to build, test, and install Legend of Faerghail (native drop-in)

This is the single, complete build guide. `README.md` links here; `AGENTS.md`
has the deeper technical reference (file formats, engine internals). If you only
want a working build, read "Quick start" and stop.

Everything here is plain `make` + `g++` (C++17). There are **two** things you can
build, and it helps to know which you want:

| Binary | What it is | Source | Reads the original `.EXE`? |
|--------|-----------|--------|----------------------------|
| `lof-native` | the **analysis emulator** — an oracle for development and golden tests | `src/` | yes (dev aid only, never shipped) |
| `lof`, `vor` | the **drop-in** — the actual deliverable | `port/` | **never** (reads data files only) |

The drop-in is what replaces `LOF.EXE` / `VOR.EXE` in the game folder. The
emulator is a development aid for producing the decompilation and reference
frames; it is not part of the deliverable.

---

## Quick start

```sh
./bootstrap.sh      # restore game data (if missing) + build everything + run tests
```

`bootstrap.sh` is idempotent and is the recommended first step on a fresh
checkout or a reset workspace. It will:

1. download the (git-ignored) game data from the Internet Archive if `game/` is
   empty;
2. file any owner-supplied save in `saves/inbox/` or at the repo root into
   `saves/` and install the well-known fixtures into slots (`saves/README.md`);
3. install the Python test deps (`numpy`, `Pillow`) if missing;
4. `make` (emulator), `make port` (drop-in), `make test`.

Use `./bootstrap.sh --check` to see what is missing **without changing
anything** (no downloads, no builds, no slot installs).

---

## Prerequisites

| Need | For | Install (Debian / Ubuntu) |
|------|-----|---------------------------|
| `g++` (C++17) + `make` | everything | `apt-get install -y build-essential` |
| Python 3 + `numpy` + `Pillow` | the frame tests (`make golden-port`, `make check`) | `pip install numpy pillow` |
| `libsdl2-dev` + `pkg-config` | a live SDL2 window (optional) | `apt-get install -y libsdl2-dev pkg-config` |

Only `g++` + `make` are required to *build* the drop-in. SDL2 is optional: if it
is absent, the build falls back to a head-less front-end and `--ui` runs scripted
input only. `numpy`/`Pillow` are only needed to *verify* screens against the
oracle PNGs.

**Game data is required at run time** (not to build). It is proprietary and
git-ignored (`game/`, `downloads/`); `bootstrap.sh` restores it. The drop-in
reads only data files — `EGA/`, `SYST/`, `DNGS/`, `ROST/`, `DSKTAB.DFS`, saves —
never `LOF.EXE` / `VOR.EXE` (see `docs/RULES.md`, the hard constraint).

---

## Build targets

Run these from the repository root.

| Command | Builds | Notes |
|---------|--------|-------|
| `make` | `lof-native` (emulator) | SDL2 window if SDL2 is present, else head-less |
| `make headless` | `lof-native` | forces the no-SDL2 build (CI / headless debugging) |
| `make port` | `lof` and `vor` (drop-in) | **the deliverable** |
| `make dropin` | `lof`, `vor`, then copies them into `game/` | installs under the names `START.BAT` invokes |
| `make test` | — | builds + runs the CPU self-test, font test, and per-area tests |
| `make golden` | — | emulator regression sweep (12 deterministic scenarios) |
| `make golden-port` | — | drop-in screen tests vs the DOSBox oracle PNGs |
| `make golden-noexe` | — | proves the drop-in runs with the original `.EXE`s absent |
| `make asm` | — | regenerate `analysis/*.asm` from the originals (needs `lof-native`) |
| `make check` | — | full health check: repo, build, tests, golden, git hygiene |
| `make clean` | — | remove built binaries and objects |

Typical full build + verify:

```sh
make port          # build the drop-in binaries ./lof and ./vor
make test          # unit tests
make golden-port   # screen tests (needs numpy + Pillow + game data)
make check         # everything, including the emulator golden sweep
```

`make` and `make port` honour `CXX` and `CXXFLAGS`, e.g.
`make port CXX=clang++ CXXFLAGS="-O2 -std=c++17"`.

### Test-time knobs

- `GOLDEN_JOBS=N` — how many golden scenarios run concurrently (default `nproc`;
  `GOLDEN_JOBS=1` forces the old serial order). Parallelism does not change the
  asserted hashes.
- Re-record the golden hashes only after an **intended** emulator change:
  `./tests/golden_test.sh --record` (writes `tests/golden.txt`).

---

## Run

### The drop-in (the deliverable)

```sh
./lof --dir game --screen UP/MSCR --shot out.png   # render one named screen
./lof --dir game                                    # interactive (SDL2 window)
./vor --dir game                                    # VOR intro (dragon + credits)
```

`--dir` points at the game data directory (default: the folder holding the
binary). Both accept `--headless` and `--shot FILE.png` for verification. See
`AGENTS.md` for the full flag list (`--play`, `--ui`, `--walk`, `--town`, ...).

To install the drop-in into the game folder so `START.BAT` runs it:

```sh
make dropin     # copies ./lof and ./vor into game/
```

### The analysis emulator (dev aid only)

```sh
./lof-native --dir game --headless --frames 300 --shot out.png   # render a frame
./lof-native --dir game                                          # interactive (SDL2)
```

This runs the **original** `LOF.EXE` / `VOR.EXE` (with `--vor` for the
`START.BAT` order) purely to observe behaviour and produce the decompilation. It
is never shipped and the drop-in never depends on it.

---

## Troubleshooting

- **`make golden-port` fails with an import error** — install the Python deps:
  `pip install numpy pillow`.
- **No window / `--ui` ignores keys** — SDL2 is absent; install `libsdl2-dev`
  and `pkg-config`, then `make port` again. Head-less builds still pass the tests.
- **`game data missing — run ./bootstrap.sh`** (from `check.sh`) — the data is
  git-ignored; run `./bootstrap.sh` to restore it.
- **Golden test looks slow** — the emulator sweep is ~10 min at 4-way; raise
  `GOLDEN_JOBS` on a bigger machine.
- **`make check` is the source of truth** for "is the tree healthy" — it runs
  build + tests + golden + git hygiene and must be green before a PR is merged
  (`docs/RULES.md` Rule 10).
