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

### OT-004 — Confirm the "out of action" indicator colour on a live combat screen

- **Answered this session (2026-11-20):** *What "out of action" means and what
  shows the state is reached* — see the answer below.  The **only** remaining
  owner action is to confirm the **text colour**.
- **The answer (derived from the decompilation, image `0x18839` / `0x18720`):**
  A party member is "out of action" when its per-character field `+0x49` is
  non-zero.  The **combat decision screen** ("Please make your decision for the
  next round!") then replaces that member's **action column** — normally the
  action word like `Attack` at x=235 — with the literal string
  **`is out of action.`** and drops the trailing number.  This is the *only*
  on-screen indicator; there is no separate icon or panel change.
  * When **every** present member is out of action, the round instead prints the
    party message
    **`The companions have all been put out of action. Earliest revival will be
    at the beginning of the next round.`**
  * The state is set by combat effects: Paralyze, Stun, Petrify and similar
    (spell-effect ids 4/5/0x27/0x28/0x1D), and by a weapon breaking down
    (`... breaks down!`).  The counter ticks down one step per round.
- **What only the owner can do:** reach a live combat screen where a member is
  out of action and report **what colour the `is out of action.` text is** —
  light gray (like the stat numbers) or dark gray (like the action words).
  The port currently draws it in index 8 (dark gray) by inference; the
  decompilation pins the string, position and the suppression of the number, but
  not the palette index.  A screenshot is not required: a short note
  ("lighter/darker than the names") is enough.  (Uploads, if any, land in the
  repo root — but a written answer is fine.)
- **Why it is owner-only:** the state cannot be reached from a shipped save (all
  six members of every shipped save have `+0x49 == 0`), so it requires playing
  combat until the condition occurs.
- **Unblocks:** closing the last inferred value in G-011 (`docs/GUESSWORK.md`).

## Done

### OT-003 — Corroborate the `W` and `B` columns of the round-result table

- **Closed (2026-11-17): the owner is skipping it** — the `W`/`B` meanings are
  already derived from the decompilation, so the live-play confirmation is not
  needed (Rule 1: the decompilation is the primary source).
- **What the table looks like (owner report, 2026-11-17, confirmed correct):** one
  row per living party member,

  ```
  [name]                -7 - * - Passed
  [name]                -5 - - - Failed
  ```

  Columns after the name are: **HP** (the change in that member's HP this round),
  then the four flag columns **W P B** (`*` = the event happened, `-` = it did
  not) and **Attack** (`Passed`/`Failed` = whether the member's chosen action
  succeeded).  The owner confirmed **`P = *` ⟺ armour −2%** (the derived `P`
  rule) and that **`B` is not used ammunition**.
- **New engine-behaviour fact (owner, 2026-11-17):** the round-result **table** is
  shown only for **quick combat**; the **per-action messages** (the weapon-damage
  / ability lines) are shown only in a **regular** (non-quick) round.  So the two
  are never on screen together — which is why the owner could not cross-check a
  `*` against its message.  Recorded in `docs/blockers/B.md` and
  `docs/notes-b.md`.
- **Outcome:** BLK-B004's columns are fully grounded by the decompilation
  (`W`=weapon damaged, `P`=shield/armour damaged, `B`=ability improved,
  `Attack`=action Passed/Failed); no further live-play confirmation is required.

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
- **Settled (2026-11-18) by the decompilation:** `W` = the character's weapon was
  damaged this round, `P` = a protective item (shield/armour) was damaged,
  `B` = an ability improved. Each `W`/`P` setter is followed by a per-slot
  condition write on the same character, and the rust dispatch names the slot
  (`"The weapon rusts!"` / `"The shield rusts!"` / `"The armour rusts!"`); the
  three `B` setters print `(Defensive ability grows!)` / `(Sneaking ability
  improves!)` / `(Concentration improves!)`. See BLK-B004 in `docs/blockers/B.md`
  and `docs/notes-b.md`. This task (OT-003) is closed; the owner was not asked
  again.

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
