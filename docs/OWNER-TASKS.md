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

### OT-002 — Confirm the BLK-B004 combat round-table columns and the Sneak/Use/Magic actions

- **What is needed:** play a round of combat and report (a) what the melee
  round-result columns `W P B` and `Attack` mean, and (b) whether choosing
  `(S)neak`, `(U)se object` and `(P)erform magic` in the per-character config
  menu behaves as those labels say (each opens a sub-prompt first).
- **Why only the owner:** this is a Rule-5 judgement/live-play call — the
  emulator reaches the round deterministically (`LOF_KEYAT` script in
  `docs/notes-b.md`) but the *meaning* of the table columns is only visible by
  playing; the disassembly gives the header strings, not their semantics.
- **Unblocks:** the last open part of BLK-B004 (`docs/blockers/B.md`). The
  dispatcher, the action byte (`+0x0f`) and the config-menu strings are already
  grounded and merged (PR #128).

## Done

### OT-001 — Provide a save that stands the party in a town other than Thyn (e.g. Cyldane)

- **Delivered:** the owner supplied `GAME4` (party standing in front of **Cyldane**).
- **Outcome (2026-11-15):** loaded via the Open dialog (slot `4`), one `Up` steps
  into town and the message reads **"You enter the proud town of Cyldane."**
  Reproduced with
  `LOF_CMDLEN=3 LOF_NO_TIMER=1 LOF_KEYAUTO=0x18,0x05,0x48 LOF_KEYGAP=50`
  (O, digit `4`, Up) — captured as `ref/oracle/town_cyldane.png`.
- **Unblocked G-001** (town-name selector).  **Resolved 2026-11-15** (PR #113):
  the selector is the 16-bit word at save offset `0x9a4` (`es:[0x460e]`;
  nonzero → "Thyn.", zero → "Cyldane."), and `compose_town_screen()` now takes
  the name from the save instead of hardcoding it.  See `docs/GUESSWORK.md` G-001.
- **Kept:** the save is preserved at `game/GAMES/GAME4` (and `game/GAMES-owner/`).
