# WORKSTREAMS — who owns what

The project is split so that agents can work **in parallel without touching each
other's files**. The rule that keeps builds green is simple:

> **One owner per file. Never edit a file you do not own.** If you need a change
> in someone else's file, add a request to your PR description (or open a tiny
> PR that only adds a function the owner can call).

Shared files that exactly one person owns at a time are marked **[SERIAL]**.
Everyone else must not edit them.

## Pick your own workstream (you do not need to be assigned one)

If you were dropped in without a workstream, choose one **yourself** from the
state of the repo. The authoritative list of open work is the checklist in
`docs/HANDOFF.md` ("Current status") plus the per-workstream **Definition of
done** below. To claim a workstream:

1. Read `docs/HANDOFF.md` and this file.
2. Pick the highest-value item that is still unchecked **and** not already
   claimed by an open branch/PR. To see what is claimed:
   `git ls-remote --heads origin` and the open PRs
   (`gh pr list --repo FreeOpenSourceSyndicate/lof2`).
3. Create your branch `work/<yourname>` and add a one-line claim at the top of
   your notes file (`docs/notes-<yourname>.md`): what you are doing.
4. Push the branch **immediately**, before writing code, so others can see it.
5. Do the work; push often; open a PR when a unit is green.

If everything is taken, pick the next unchecked item in `docs/HANDOFF.md`, or
grab a bug from the "Fixed bugs (do not regress)" list in `AGENTS.md` that needs
a regression test. There is always something; you never need to wait to be told.

Default mapping if you want a suggestion: first agent in → **A (dungeons)**,
second → **B (characters/items)**. The maintainer owns the main loop.

### Two rules of thumb

- **Prefer a workstream you already know.** If you have worked on one before, or
  the docs make one clearly familiar, stay in it. Your accumulated context is
  worth more than the marginal value of a different lane, and switching costs
  (re-reading formats, re-grounding against the oracle) are real. Only move on
  when your lane is genuinely done or blocked.
- **Work in long, uninterrupted stretches.** Do not stop after one small step to
  wait for the next prompt. Push frequently (reset safety), but keep going
  through the checklist until you are blocked, out of work, or truly finished.
  An idle agent costs the project far more than the occasional bit of rework.
  When your lane runs dry, take the next unchecked item from `docs/HANDOFF.md`
  or add a regression test for something in `AGENTS.md`'s "do not regress" list.

## File ownership map

| Path | Owner | Notes |
|------|-------|-------|
| `src/*` (emulator) | **Maintainer** | Oracle only; change deliberately |
| `port/lof.cpp` | **Maintainer** | Front-end + main loop **[SERIAL]** |
| `port/gfx.{h,cpp}` | **Maintainer** | EGA decoder + Screen, shared API **[SERIAL]** |
| `port/font.{h,cpp}` | **Maintainer** | Text rendering, shared API **[SERIAL]** |
| `port/data.{h,cpp}` | **Maintainer** | SNSC/NSC/TXT readers |
| `port/screens.{h,cpp}` | **Maintainer** | full-screen image helper |
| `port/dungeon.{h,cpp}` | **Agent A** | DNGS format + renderer |
| `port/char.{h,cpp}` | **Agent B** | characters/items/spells/combat |
| `tests/dungeon_test.cpp` | **Agent A** | your test |
| `tests/char_test.cpp` | **Agent B** | your test |
| `tools/dngs.py` | **Agent A** | offline format explorer |
| `tools/spec.py` | **Agent B** | offline format explorer |
| `docs/notes-a.md` | **Agent A** | running notes (push often) |
| `docs/notes-b.md` | **Agent B** | running notes (push often) |
| `docs/BLOCKED-*.md` | anyone | use when stuck (see HANDOFF) |
| `Makefile` | **Maintainer** | ask before changing |

If you need a new module, pick a name that no one else uses and register it here
in your PR.

## Workstream A — dungeons (`DNGS/`)

**Goal:** decode the 29 dungeon map files and render a walkable map.

**Files:** `port/dungeon.{h,cpp}`, `tools/dngs.py`, `tests/dungeon_test.cpp`,
`docs/notes-a.md`.

**Data:** `game/DNGS/DNG00..DNGxx`, 0.9–1.7 KB each.

**What is already known / strongly suspected** (verify before trusting):

- The first byte of every file is the RLE escape marker `0xff`. Streams look
  like `ff <count> <value>` runs (count is `count+?`; the escape run emits
  `value`). This is the *same* run-length idea as the EGA images but a different
  header — **confirm the header size and the count convention** by decoding a
  file and checking the output size against a plausible grid.
- Interior values seen are small and repetitive (`00 03 06 0c`), consistent with
  a per-cell tile/type code and a per-cell feature code. There is likely a width
  and height near the start.
- Use the emulator to ground truth: when LOF loads a dungeon it reads
  `DNGS/DNGnn` (watch `LOF_I21=1`) and draws to the EGA planes. Dump the planes
  (`LOF_EDUMP=out.bin`) and compare against your decoded grid.

**Definition of done:** `port/dungeon` decodes a DNGS file into a grid, a
`dungeon_test` asserts the grid dimensions and a couple of known cells, and the
drop-in can draw one dungeon map screen that matches the oracle frame.

## Workstream B — characters, items, combat (`SYST/SPEC*`, `ROST/`)

**Goal:** decode character/monster/item statistics and the combat tables, and
expose them as native structs.

**Files:** `port/char.{h,cpp}`, `tools/spec.py`, `tests/char_test.cpp`,
`docs/notes-b.md`.

**Data:** `game/SYST/SPEC0..SPEC7` (5–43 KB), `game/ROST/ROST`, `game/ROST/SHOP`,
`game/SYST/SNSC` (already parsed in `port/data.cpp` — reuse it, do not edit it).

**What is already known:**

- `SYST/SNSC` is 14 records x 78 bytes: singular name (19 bytes, NUL-padded),
  plural name (19 bytes), then a 40-byte stats block. `port/data.cpp` already
  reads this — call `load_snsc()`.
- `SPECn` are larger and likely hold the per-scenario item/spell/monster tables
  keyed by scenario. Determine the record size by looking for regular structure
  (repeated fixed-length records) and by cross-referencing `SNSC` names.
- `ROST`/`SHOP` are small and likely shop/roster tables.

**Definition of done:** `port/char` parses `SPECn` into typed records with
plausible fields (verify against the emulator's reads of the same file with
`LOF_I21=1` + `LOF_READ=1`), a `char_test` asserts record counts and a known
value, and a `--dump-spec` style debug command prints them.

## Working agreements

- Push to your own branch `work/<yourname>`; open a PR when a unit is green.
- Keep `make test` green; add your test to the `test:` target in `Makefile`
  **via a PR request to the maintainer** (do not edit `Makefile` directly if you
  are not the maintainer — instead add a standalone `tests/<yours>_test.cpp` and
  ask the maintainer to wire it in).
- Do not edit `src/`, `port/lof.cpp`, `port/gfx.*`, `port/font.*`,
  `port/data.*`, or `port/screens.*`.
- Commit small, commit often, push often. See `docs/HANDOFF.md` for why.
