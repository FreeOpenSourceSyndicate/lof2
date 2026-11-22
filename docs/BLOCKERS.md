# BLOCKERS — the shared blocker registry

This is the **index** of everything stopping an agent, and what would unblock it.
It replaces ad-hoc "I'm stuck" messages: a blocker written here is visible to
every other agent, the maintainer, and any replacement agent, with no context
loss.

## Structure — one file per area (this file is the hub)

The registry is **split by work area** (the old lanes A/B/M, now domain labels —
see `docs/WORKSTREAMS.md`) so that two agents fanning out never edit the same
file. Each area owns one file; add your blocker to **the area's file only**.

| Area | File | Covers |
|------|------|--------|
| **A** (dungeons) | [`blockers/A.md`](blockers/A.md) | `DNGS/`, the viewport composite, `port/dungeon.*` |
| **B** (characters/items) | [`blockers/B.md`](blockers/B.md) | `SYST/SPEC*`, `ROST/`, `port/char.*` |
| **M** (shared/engine) | [`blockers/M.md`](blockers/M.md) | shared code (`src/`, `port/lof\|gfx\|font\|data\|screens.*`), cross-area work |

**This hub is static** — it describes the convention and links the area files.
Do **not** add blocker entries here; keeping it unchanged is what makes it
conflict-free. The per-area summary tables live in the area files.

Per-incident deep dives still live in `docs/BLOCKED-<name>.md` (a long diagnosis,
a reproducer, evidence). A registry entry is the *index*: what is blocked, by
whom, at what priority, linking the detail file.

## ID convention — `BLK-<area><nnn>`

Every blocker ID is **area-lettered**: `BLK-A001`, `BLK-B003`, `BLK-M002`.

- The letter is the **Blocked area** — which workstream the blocker belongs to,
  and therefore which file it lives in. It is *not* necessarily the responsible
  party: an area-A blocker may be Maintainer-owned (e.g. `BLK-A002`).
- Each area numbers from `001` **independently**, so two agents adding blockers
  can never collide — you only ever touch your own area's counter.

To allocate an ID: open the area file, take the next free number, done.

## Old IDs → new IDs

The registry was previously a single file with one shared `BLK-NNN` counter
(which let two lanes pick the same number). Old IDs may still appear in older
notes and in closed PRs:

| Old | New | Lane |
|-----|-----|------|
| BLK-001 | BLK-A001 | A |
| BLK-002 | BLK-A002 | A |
| BLK-003 | BLK-M001 | M |
| BLK-004 | BLK-B001 | B |
| BLK-005 | BLK-B002 | B |
| BLK-006 | BLK-B003 | B |

## Priority values (use exactly these three words)

| Priority | Meaning |
|----------|---------|
| **High** | Needs immediate attention. Work is stopped or actively losing time until this is removed. |
| **Medium** | Relevant tasks depend on this, but other work is still possible, so some delay is tolerable. |
| **Low** | Will have to be done at some point, but it isn't important yet. |

A blocker's priority is **not** fixed: if you are the responsible agent and you
mark a `Dependencies` entry, the dependency's priority may have to rise (a
`Medium` dependency of a `High` blocker is effectively urgent).

## Fields (every entry has all of these)

| Field | Meaning |
|-------|---------|
| **ID** | Stable reference, `BLK-<area><nnn>` (e.g. `BLK-B003`). Use it in PRs/notes as `see BLK-B003`. |
| **Blocked area** | Which workstream cannot proceed: `A (dungeons)`, `B (characters/items)`, or `M (shared/engine)`. This determines the ID letter and the file. |
| **Responsible** | Who must unblock it: `Maintainer`, `Agent A`, `Agent B`, `External (repo owner)`, or `Anyone`. |
| **Priority** | `High` / `Medium` / `Low` (definitions above). |
| **Status** | `Open` / `In progress` / `Resolved`. Set to `Resolved` when fixed; keep the entry for the record. |
| **Description & unblock action** | Precise: what is blocked, the evidence/reproducer, and exactly what must be done to remove it. |
| **Dependencies** | Other `BLK-<area><nnn>` that must be handled first. Written by the responsible agent. Empty = none. |

## How to use it

- **Blocked?** Add an entry at the bottom of **the area's file** (next free
  `BLK-<area><nnn>`), add a row to that file's summary table, and link any
  `docs/BLOCKED-<name>.md` detail. Keep it short and precise — a replacement
  agent must be able to act on it cold.
- **Cross-area blocker?** File it in the area that is *blocked* (its letter), even
  if another area must fix it. Note the fixer in `Responsible`.
- **Responsible agent?** Set `Status: In progress`, then `Resolved` with a
  one-line resolution (commit/PR ref). Mark `Dependencies` as you discover them.
- **Dependency changed?** Update the `Dependencies` field and, if needed, the
  priority of the blocker you depend on (add a dated `Log` line explaining why).
- Commit and push the area file the same way as any other repo change.
- **Ownership of the files.** Add new entries in **the relevant area's file
  only**, and do not rewrite another area's entry *body* (its title, description,
  ID). The **mutable fields are cross-editable**: anyone may edit **another**
  area's file to change a `Status`, a `Priority`, or a `Dependencies` field (and
  the matching summary-table row) — e.g. to mark a blocker resolved, to raise its
  priority because you now depend on it, or to record that your blocker depends
  on it. This deliberately trades a few merge conflicts (which are small and
  line-local, so easy to resolve) for the ability of any agent to keep the whole
  registry honest. If you need a *body* change in another area's file, ask in
  that area's PR.
- **Merge conflict in an area file?** Expected and cheap: the fields are one line
  each, so keep **both** sides' changes (rebase and accept the union of the
  `Status`/`Priority`/`Dependencies` edits). Never resolve a blocker conflict by
  dropping someone else's update.

---

## Area index

| Area | File | Open | In progress | Resolved |
|------|------|------|-------------|----------|
| A (dungeons) | [`blockers/A.md`](blockers/A.md) | BLK-A006 | — | BLK-A001, BLK-A002, BLK-A003, BLK-A004, BLK-A005 |
| B (characters/items) | [`blockers/B.md`](blockers/B.md) | — | — | BLK-B001, BLK-B002, BLK-B003, BLK-B004, BLK-B005 |
| M (shared/engine) | [`blockers/M.md`](blockers/M.md) | — | — | BLK-M001 |

*(Update the area's row here only when its status changes; this is the one table
in the hub that moves.)*
