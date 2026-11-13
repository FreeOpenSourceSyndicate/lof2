# RULES — read this first, every session

This is the **single place** where the project owner's standing rules live, and
where **rule changes are announced**. Every agent reads this file before doing
any work, and re-reads the changelog at the bottom to catch changes.

If you are an agent starting work: read this file, then `docs/HANDOFF.md`, then
`docs/RISKS.md`, then `docs/WORKSTREAMS.md`. If a rule here conflicts with any
other document, **this file wins** — say so in your notes and follow this file.

- Owner of the rules: **the repository owner**. Agents do not change the intent
  of these rules; they only record that a change happened (see the changelog).
- Anything labelled **MUST** / **NEVER** is a hard constraint, not advice.

---

## Rule 1 — No guesswork. Ever.

**Agents MUST NOT guess.** A *guess* is any value, behaviour, layout, offset,
name, or algorithm in code, tests, or docs that is **not traceable to a source
of truth**. The sources of truth, in order, are:

1. the **decompilation** (`analysis/*.asm`) — the primary source;
2. the **game data files** (`EGA/`, `SYST/`, `DNGS/`, `ROST/`, `*.SAV`, ...);
3. the **emulator / DOSBox oracle frames** (`ref/oracle/`, golden tests);
4. **playing the game** (ask the owner — see Rule 5).

A number that was *recovered* (template-matched, decoded, traced) is not a
guess. A number that was *chosen because it looked right* is a guess, even if it
makes a test pass. "It works" is not "it is derived".

The only exception is **Rule 6** (explicit owner authorisation).

## Rule 2 — Removing a guess is the default task. If another lane is needed, it is a blocker.

When you find a guess, your job is to **remove it** by finding the source of
truth. If removing it needs work that **another lane owns** (see
`docs/WORKSTREAMS.md` for file ownership), then it is **a blocker**: file it in
the shared blocker registry (`docs/BLOCKERS.md`, your lane's file) with the
`BLK-<lane><nnn>` ID, the evidence, and exactly what would unblock it. Do not
"work around" it and do not let the guess stand silently.

A guess that you can remove yourself is a normal task, not a blocker — but it
MUST be removed before you call the work done.

## Rule 3 — A guess that cannot be removed must be reported to the owner, repeatedly.

If, after honest effort, the guess **cannot** be taken out of the work, then:

- You MUST record it in `docs/GUESSWORK.md` under **"Cannot be removed — owner
  decision required"**, with full details: what the guess is, where it is, why
  it cannot be removed, what evidence you have, and the options.
- You MUST **inform the owner** (in your reply / handover) and **keep reminding
  the owner every session** until the owner gives an explicit order on how to
  handle it.
- Until ordered otherwise, do the most defensible thing (leave it as a
  documented gap, or gate it behind a flag) — never silently ship it as if it
  were derived.

An entry stays in this state **until the owner orders a resolution** (which may
be "allow the guess", "leave the gap", "drop the feature", ...). Record the
order in `docs/GUESSWORK.md` and in the changelog below.

## Rule 4 — If it is unclear whether the guess can be removed, put it on the watchlist.

When it is **not yet clear** whether a guess can be taken out, you MUST add it
to `docs/GUESSWORK.md` under **"Unclear — revisit"** with what is known and what
would settle the question. Revisit the watchlist **at the start of every
session** (it is part of this file's checklist). When it becomes clear:

- the guess **can** be removed → promote it to a normal task (Rule 2), or a
  blocker if another lane is needed;
- the guess **cannot** be removed → promote it to "owner decision required"
  (Rule 3).

Nothing leaves the watchlist without being resolved one of these ways.

## Rule 5 — When unsure and it is knowable by playing, ask the owner.

If something is unclear from the compilation and data but **can be known from
playing the game**, do not guess and do not stall: **ask the owner**. The owner
can play and answer. Log the question and the answer in your lane notes.

## Rule 6 — Guesswork is allowed ONLY when the owner explicitly authorises it.

The owner may, in writing (in this file's changelog, in an issue, or in chat),
authorise a specific guess. Such an authorisation:

- is **specific** (names the value/behaviour and the scope) — it is not blanket;
- MUST be recorded in `docs/GUESSWORK.md` under **"Owner-authorised guesses"**
  with the exact wording and date, and cited in a code comment at the guess site;
- is **revocable** — the owner may withdraw it, and it then returns to Rule 2/3.

Absent an explicit authorisation, Rule 1 applies.

## Rule 7 — Record rule changes here, dated.

Any change to these rules is appended to the **Changelog** below, newest first,
with the date and a one-line summary. This file is the canonical announcement
point; other documents may summarise but MUST point here.

---

## Session checklist (do this at the start of every session)

1. Read this file (and the changelog for anything new).
2. Read `docs/GUESSWORK.md` — resolve, advance, or re-remind on every open item.
3. Read `docs/HANDOFF.md`, `docs/RISKS.md`, `docs/WORKSTREAMS.md`.
4. Run `./bootstrap.sh` and `./check.sh` if the workspace may have reset.
5. Only then start work. If you would have to guess, stop and apply Rules 2–6.

---

## Changelog (newest first)

### 2026-11-12 — Guesswork policy established

- Added this file (`docs/RULES.md`) as the canonical rules + announcement point.
- Rule 1: **no guesswork**; guesses must be traceable to the decompilation,
  data files, oracle frames, or live play.
- Rule 2: removing a guess is the default task; if it needs another lane, it is a
  **blocker** in `docs/BLOCKERS.md`.
- Rule 3: a guess that cannot be removed MUST be reported to the owner and
  **re-reminded every session** until the owner orders how to handle it.
- Rule 4: a guess whose removability is **unclear** goes on the watchlist in
  `docs/GUESSWORK.md` and is revisited every session.
- Rule 5: if it is knowable by playing, **ask the owner**.
- Rule 6: guesswork is allowed **only** with explicit, specific, recorded owner
  authorisation.
- Rule 7: rule changes are recorded in this changelog.
- Added `docs/GUESSWORK.md` (the register) and `docs/AGENT-PROMPT-ADDENDUM.md`
  (a paste-in note pointing agents here).
