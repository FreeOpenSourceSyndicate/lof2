# OWNER-TASKS — work only the repository owner can do

Per `docs/RULES.md` Rule 8, work that **only the owner can do** is not an agent
lane; it lives here. The **Maintainer (lane M) MUST re-remind the owner of every
open task in this file every session** (in the reply/handover) until the owner
confirms it is done. An owner task is never allowed to go quiet.

Rules for this file:

- Add an entry for anything that genuinely requires the owner (playing the game,
  a judgement call, an authorisation, access only the owner has).
- Do **not** put agent work here — that belongs in a lane (Rule 8).
- Keep the entry actionable: what is needed, why only the owner can do it, and
  what it unblocks.
- When the owner acts, move the entry to **Done** with the date and outcome; do
  not delete it.

## Open

### OT-001 — Provide a save that stands the party in a town other than Thyn (e.g. Cyldane)

- **What is needed:** a `GAMES/GAMEnn` save file in which the party is inside a
  town other than Thyn (Cyldane is the other name the engine knows).
- **Why only the owner can do it:** it requires **playing the game** (Rule 5) to
  reach the other town and save there. The two saves shipped in the repo
  (`GAME1` Temple, `GAME2` wilderness) both resolve to Thyn, so the town-name
  selection branch cannot be observed from them.
- **What it unblocks:** **G-001** — the town name `"Thyn."` is currently
  hardcoded in the drop-in (`compose_town_screen()`, `port/lof.cpp`). With a
  Cyldane save, lane M can trace the selector that chooses `ds:0x5A39 "Thyn. "`
  vs `ds:0x5A40 "Cyldane. "` and remove the hardcode without guessing.
- **How to deliver:** play to Cyldane, save to a slot, and drop the resulting
  `GAME   n` file into `game/GAMES/` (or hand the bytes to the maintainer).
- **Status:** open (raised 2026-11-14). Lane M re-reminds every session.

## Done

*(none yet)*
