# AGENT-PROMPT-ADDENDUM — paste this into any agent's prompt

The owner can paste the block below into the prompt of any agent (new or
existing) to make sure it picks up the current rules. It is deliberately short:
it just points at the canonical places, which the agent then reads itself.

---

```text
Before doing any work, read docs/RULES.md in this repository. It is the single
canonical place for the project owner's standing rules, and its changelog
announces rule changes — read the changelog too, so you catch anything new.

In particular: agents must NEVER guess. A guess is any value, behaviour, layout,
name, or algorithm not traceable to the decompilation (analysis/*.asm), the game
data files, the oracle frames, or playing the game. Removing a guess is the
default task; if it needs another lane's work it is a blocker
(docs/BLOCKERS.md). A guess that cannot be removed must be reported to the owner
and re-reminded every session until the owner orders how to handle it. A guess
whose removability is unclear goes on the watchlist in docs/GUESSWORK.md and is
revisited every session. If something is knowable by playing the game, ask the
owner instead of guessing. Guesswork is allowed ONLY when the owner explicitly
authorises it (and it is then recorded in docs/GUESSWORK.md).

Then read docs/GUESSWORK.md (act on every open item), docs/OWNER-TASKS.md (work
only the owner can do), docs/HANDOFF.md, docs/RISKS.md, and
docs/WORKSTREAMS.md, run ./bootstrap.sh and ./check.sh if the workspace may have
reset, and only then start work.

Stay in your lane: work only the files you own, and if something has to be done
that no lane owns, add it to a lane (register a new lane in docs/WORKSTREAMS.md)
before starting it. Work only the owner can do is an owner task, recorded in
docs/OWNER-TASKS.md — never an agent lane.
```

---

## For the owner

- The canonical rules file is **`docs/RULES.md`** (with a dated **Changelog** at
  the bottom). Add every new/changed rule there.
- The guess register is **`docs/GUESSWORK.md`**. Agents read it every session and
  re-remind you about any item in section A until you give an order.
- To authorise a specific guess, add it to `docs/GUESSWORK.md` section C with the
  date and exact wording (Rule 6).
- This file (`docs/AGENT-PROMPT-ADDENDUM.md`) is only the paste-in note; the
  rules themselves live in `docs/RULES.md`.
