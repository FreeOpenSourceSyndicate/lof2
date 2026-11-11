# BLOCKERS — the shared blocker registry

This is the **index** of everything stopping an agent, and what would unblock it.
It replaces ad-hoc "I'm stuck" messages: a blocker written here is visible to
every other agent, the maintainer, and any replacement agent, with no context
loss.

## Structure — one file per lane (this file is the hub)

The registry is **split by work lane** so two agents never edit the same file.
Each lane owns one file; add your blocker to **your lane's file only**.

| Lane | File | Owner | Covers |
|------|------|-------|--------|
| **A** (dungeons) | [`blockers/A.md`](blockers/A.md) | Agent A | `DNGS/`, the viewport composite, `port/dungeon.*` |
| **B** (characters/items) | [`blockers/B.md`](blockers/B.md) | Agent B | `SYST/SPEC*`, `ROST/`, `port/char.*` |
| **M** (maintainer) | [`blockers/M.md`](blockers/M.md) | Maintainer | shared code (`src/`, `port/lof\|gfx\|font\|data\|screens.*`), cross-lane work |

**This hub is static** — it describes the convention and links the lane files.
Do **not** add blocker entries here; keeping it unchanged is what makes it
conflict-free. The per-lane summary tables live in the lane files.

Per-incident deep dives still live in `docs/BLOCKED-<name>.md` (a long diagnosis,
a reproducer, evidence). A registry entry is the *index*: what is blocked, by
whom, at what priority, linking the detail file.

## ID convention — `BLK-<lane><nnn>`

Every blocker ID is **lane-lettered**: `BLK-A001`, `BLK-B003`, `BLK-M002`.

- The letter is the **Blocked lane** — which workstream the blocker belongs to,
  and therefore which file it lives in. It is *not* necessarily the responsible
  party: a lane-A blocker may be Maintainer-owned (e.g. `BLK-A002`).
- Each lane numbers from `001` **independently**, so two agents adding blockers
  can never collide — you only ever touch your own lane's counter.

To allocate an ID: open your lane file, take the next free number, done.

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
| **ID** | Stable reference, `BLK-<lane><nnn>` (e.g. `BLK-B003`). Use it in PRs/notes as `see BLK-B003`. |
| **Blocked lane** | Which workstream cannot proceed: `A (dungeons)`, `B (characters/items)`, or `M (maintainer)`. This determines the ID letter and the file. |
| **Responsible** | Who must unblock it: `Maintainer`, `Agent A`, `Agent B`, `External (repo owner)`, or `Anyone`. |
| **Priority** | `High` / `Medium` / `Low` (definitions above). |
| **Status** | `Open` / `In progress` / `Resolved`. Set to `Resolved` when fixed; keep the entry for the record. |
| **Description & unblock action** | Precise: what is blocked, the evidence/reproducer, and exactly what must be done to remove it. |
| **Dependencies** | Other `BLK-<lane><nnn>` that must be handled first. Written by the responsible agent. Empty = none. |

## How to use it

- **Blocked?** Add an entry at the bottom of **your lane file** (next free
  `BLK-<lane><nnn>`), add a row to that file's summary table, and link any
  `docs/BLOCKED-<name>.md` detail. Keep it short and precise — a replacement
  agent must be able to act on it cold.
- **Cross-lane blocker?** File it in the lane that is *blocked* (its letter), even
  if another lane must fix it. Note the fixer in `Responsible`.
- **Responsible agent?** Set `Status: In progress`, then `Resolved` with a
  one-line resolution (commit/PR ref). Mark `Dependencies` as you discover them.
- **Dependency changed?** Update the `Dependencies` field and, if needed, the
  priority of the blocker you depend on (add a dated `Log` line explaining why).
- Commit and push the lane file the same way as any other repo change.
- **Ownership of the files.** Add and edit entries in **your own lane's file
  only**. The *single* exception: you may edit **another** lane's file **only to
  add or change a `Dependencies` field** on an entry (and its summary-table row)
  — e.g. to record that your blocker now depends on, or no longer depends on,
  that lane's blocker. Do not rewrite another lane's description, status, or
  priority; if you need a non-dependency change there, ask in that lane's PR.

---

## Lane index

| Lane | File | Open | In progress | Resolved |
|------|------|------|-------------|----------|
| A (dungeons) | [`blockers/A.md`](blockers/A.md) | BLK-A001 | BLK-A002 | — |
| B (characters/items) | [`blockers/B.md`](blockers/B.md) | BLK-B003 | — | BLK-B001, BLK-B002 |
| M (maintainer) | [`blockers/M.md`](blockers/M.md) | BLK-M001 | — | — |

*(Update your lane's row here only when its status changes; this is the one table
in the hub that moves.)*
