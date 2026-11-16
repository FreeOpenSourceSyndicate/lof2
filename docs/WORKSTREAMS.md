# WORKSTREAMS — who owns what (single-lane model)

> **Read `docs/RULES.md` first** (the owner's standing rules + changelog),
> `docs/GUESSWORK.md` (the guess register), and `docs/OWNER-TASKS.md` (work only
> the owner can do). In particular, **never guess** and **do not edit files you
> do not own**: if a task would require guessing and removing the guess needs
> another area's files, that is a **blocker** (`docs/BLOCKERS.md`) — see
> `docs/RULES.md` Rules 2 and 8.

## The model: one lane, three areas

The project used to be split into **three parallel lanes** (A = dungeons,
B = characters/items, M = shared/engine) so that up to three agents could work
at once without touching each other's files. That split is **retired**: in
practice only one line (the maintainer's) had unblocked work, so the lane
boundaries mostly produced hand-offs, not parallelism, and items whose nominal
owner was idle (e.g. `BLK-A001`, `BLK-B005`) had nobody feeling responsible.

From now on there is **one lane — the Maintainer lane (`M`)** — that owns the
whole tree. The old lanes survive only as **areas** (domain labels), kept for
transparency and to make fan-out safe:

| Area | Old lane | Domain | Files |
|------|----------|--------|-------|
| **M — engine & front-end** | M | shared code, main loop, emulator | `src/*`, `port/lof.cpp`, `port/gfx.*`, `port/font.*`, `port/data.*`, `port/screens.*`, `Makefile`, `tests/cpu_test.cpp`, `docs/notes-maintainer-*.md` |
| **A — dungeons** | A | `DNGS/` format + first-person viewport | `port/dungeon.*`, `tools/dngs.py`, `tests/dungeon_test.cpp`, `docs/notes-a.md` |
| **B — characters / items / combat** | B | `SYST/SPEC*`, `ROST/` | `port/char.*`, `tools/spec.py`, `tests/char_test.cpp`, `docs/notes-b.md` |

The **area** of a file is just a label (it tells you which domain a change
belongs to, and which notes/blocker file to read). It is **not** an ownership
boundary any more.

### Working alone (the normal case)

There is **one worker**: the Maintainer lane. It may edit **any** file. It works
the single queue in `docs/HANDOFF.md` ("Current status") top to bottom, in long
uninterrupted stretches, committing and pushing after every meaningful step.

### Fanning out (0–2 extra agents)

If the owner starts extra agents, they **do not get assigned a lane** — they take
**items off the same single queue**. Because two agents must never edit the same
file at once, the one rule that replaces "stay in your lane" is:

> **Claim the item, and do not start a second item that touches the same area
> (or the same file) as an item another active agent is already working.**

Concretely, to take an item:

1. Read `docs/HANDOFF.md` and this file; pick the highest-priority **unclaimed**
   item from the queue.
2. Create your branch `work/<yourname>` and push it **immediately**, before
   writing code, with a one-line claim at the top of your notes file — this is
   what makes the claim visible to the other agents.
3. Check `git ls-remote --heads origin` and `gh pr list --repo
   FreeOpenSourceSyndicate/lof2` first: if another live branch/PR already claims
   the same area, take a different item.
4. Do the work; push often; open a PR when a unit is green; squash-merge it
   yourself once `make check` is green (Rule 10).

The queue in `docs/HANDOFF.md` lists, per item, the **area** it touches, so two
agents can always pick two items in different areas and never collide. When an
area has only one item left, run one agent.

### A note on the retired lanes

The lane definitions (A/B/M) and everything built on them — the
`BLK-<lane><nnn>` blocker IDs, the per-lane blocker files
(`docs/blockers/{A,B,M}.md`), and the per-lane notes (`docs/notes-a.md`,
`docs/notes-b.md`, `docs/notes-maintainer-*.md`) — are **kept as-is** for
transparency and history: existing commits, PRs and notes reference them, so
erasing them would lose the audit trail. New blocker IDs may still use a letter
(the area the blocker sits in) but there is no longer a *lane* to "stay in".

## File ownership map (areas, for fan-out safety)

The table below is the **same map as before**, with the Owner column relabelled
to the area. It is no longer a hard edit boundary for a single worker; it is the
**fan-out collision map**: when more than one agent runs, agents pick items whose
areas are disjoint, and this table tells them which files those areas cover.

| Path | Area | Notes |
|------|------|-------|
| `src/*` (emulator) | M | Oracle only; change deliberately |
| `port/lof.cpp` | M | Front-end + main loop **[SERIAL]** |
| `port/gfx.{h,cpp}` | M | EGA decoder + Screen, shared API **[SERIAL]** |
| `port/font.{h,cpp}` | M | Text rendering, shared API **[SERIAL]** |
| `port/data.{h,cpp}` | M | SNSC/NSC/TXT readers |
| `port/screens.{h,cpp}` | M | full-screen image helper |
| `port/dungeon.{h,cpp}` | A | DNGS format + renderer |
| `port/char.{h,cpp}` | B | characters/items/spells/combat |
| `tests/dungeon_test.cpp` | A | dungeon test |
| `tests/char_test.cpp` | B | character test |
| `tests/cpu_test.cpp` | M | CPU/emulator self-test |
| `tools/dngs.py` | A | offline format explorer |
| `tools/spec.py` | B | offline format explorer |
| `docs/notes-a.md` | A | running notes (push often) |
| `docs/notes-b.md` | B | running notes (push often) |
| `docs/notes-maintainer-*.md` | M | maintainer running notes |
| `docs/blockers/{A,B,M}.md` | area | blocker registries (see hub) |
| `docs/BLOCKERS.md` | M | registry hub — convention + index (static; do not add entries) |
| `docs/BLOCKED-*.md` | anyone | per-incident detail; link it from a registry entry |
| `Makefile` | M | serial |

If you need a new module, pick a name no one else uses and register it here in
your PR.

## The areas in detail

### Area A — dungeons (`DNGS/`)

**Goal:** decode the 29 dungeon map files and render a walkable first-person map.

**Files:** `port/dungeon.{h,cpp}`, `tools/dngs.py`, `tests/dungeon_test.cpp`,
`docs/notes-a.md`.

**Data:** `game/DNGS/DNG00..DNGxx`, 0.9–1.7 KB each. Format is decoded and
verified across all 29 files (4-byte header, then a 36×36 tile map and a 36×36
object/feature layer); see `docs/notes-a.md` for the exact layout.

**Remaining:** the first-person viewport is a static `DP001` tile, but the
engine composites the view from the full-view sprites plus smaller side pieces
per cell (`BLK-A001`). Ground truth: `tools/dngs.py oracle <png> game/EGA <d> <i>`
scores a composite against `ref/oracle/dungeon_slot1*.png`.

**Definition of done:** `port/dungeon` decodes a DNGS file into a grid, a
`dungeon_test` asserts the grid dimensions and known cells, and the drop-in draws
one dungeon map screen that matches the oracle frame.

### Area B — characters, items, combat (`SYST/SPEC*`, `ROST/`)

**Goal:** decode character/monster/item statistics and the combat tables, and
expose them as native structs.

**Files:** `port/char.{h,cpp}`, `tools/spec.py`, `tests/char_test.cpp`,
`docs/notes-b.md`.

**Data:** `game/SYST/SPEC0..SPEC7`, `game/ROST/ROST`, `game/ROST/SHOP`,
`game/SYST/SNSC` (parsed in `port/data.cpp` — reuse it, do not edit it).

**Remaining:** the combat action **dispatcher** / round resolution (`BLK-B004`,
needs an emulator-side round dump from area M) and the ROST per-record checksum
in `port/char.cpp` (`BLK-B005`, algorithm already decoded in
`docs/notes-maintainer-rost.md`).

**Definition of done:** `port/char` parses `SPECn` into typed records, a
`char_test` asserts record counts and known values, and `--dump-spec` prints
them.

### Area M — engine, front-end, emulator

**Goal:** keep the analysis emulator (the oracle) and the drop-in front-end
(`port/lof.cpp`) working; own the shared/serial code.

**Remaining:** the in-game play loop (movement + combat wiring in `--ui`), the
Inn/Temple `D5` interiors (shared with area A's viewport work), and the cosmetic
`G-002` patch. See the queue in `docs/HANDOFF.md`.

## Working agreements

- Push to your own branch `work/<yourname>`; open a PR when a unit is green.
- Keep `make test` green. `Makefile` is area M; if you are not the maintainer,
  add a standalone `tests/<yours>_test.cpp` and ask the maintainer to wire it in.
- Commit small, commit often, push often. See `docs/HANDOFF.md` for why.
