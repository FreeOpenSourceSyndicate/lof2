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

*(none currently)*

## Done

### OT-002 — Confirm the BLK-B004 combat round-table columns and the Sneak/Use/Magic actions

- **Delivered:** the owner played a combat round and reported the action
  semantics (2026-11-17).
- **Outcome — `(S)neak`:** "only opens a sub-prompt if there are multiple ranks
  of enemies. In the combat round, it runs some sort of check and if successful,
  the character 'disappears in the shadows'. This removes their icon from the
  positioning panel in the lower left and seems to be doing nothing else in that
  round. In the next round, the character autoselects 'attack'. Starting the next
  combat round, the message reads '[character] creeps up on the [enemy] and now
  him from behind'. Some typos in there, but it seems to trigger a sneak attack
  in the following round. It looks like a character in the shadows will not be
  attacked. Sneaking allows to attack any rank, not just the first."
  - This is **consistent with the decompilation**: the sneak messages are
    `" disappears in the shadows..."` (image `0x4309A`), `" is seen creeping
    up!"` (`0x430D5`) and `" creeps up on "` + `" and now "` + `"from behind!"`
    (image `0x43195`/`0x431A4`/`0x431AE`) — matching the reported wording exactly
    (including the engine's own typo "and now him from behind").
- **Outcome — `(U)se object`:** "basically opens the inventory to select an item
  to use. This allows e.g. for ranged combat, by using a crossbow." Consistent
  with the derived `kActionNames` index 3 and the item-use path
  (`docs/blockers/B.md` BLK-B002/B003).
- **Outcome — `(P)erform magic`:** "opens the list of spells and allows for spell
  use in combat." Consistent with `kActionNames` index 4 and the spell table
  (BLK-B002).
- **Outcome — the round table:** "In the quick combat sum up table, the 'attack'
  column simply seems to state if the character successfully performed their
  action … it says either 'Failed' or 'Passed'." The engine strings confirm this:
  `"Failed"` (image `0x438B1`) and `"Passed"` (image `0x438B8`) are the only two
  literals, so the `Attack` column is a boolean Passed/Failed. The owner was
  **unsure** about `W P B`: "I think 'W' stands for weapon and 'P' for armour
  ((P)anzerung in German). But I have no idea what the 'B' may stand for."
- **Still open (narrowed):** the exact meaning of the `W P B` columns is *not*
  yet confirmed (owner could not resolve `B`; the header text alone does not
  name the values). It stays open in **BLK-B004** (`docs/blockers/B.md`) — to be
  settled by decompiling the round-table print routine, or by a follow-up owner
  observation of a populated `W P B` row.
- **Unblocked:** BLK-B004's action enum (`+0x0f`) is now confirmed by live play
  for Sneak/Use/Magic (2/3/4); only the `W P B` column semantics remain.

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
