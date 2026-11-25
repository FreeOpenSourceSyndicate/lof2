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

> Re-audited under the single-lane definition (a "blocker" = needs outside help;
> see `docs/BLOCKERS.md`). What genuinely needs you is **real save fixtures** and
> **real-play verification of random/timed mechanics** — everything else is
> maintainer research. The Maintainer re-checks this each session (Rule 8).

### OT-005 — Provide real late-game / varied-state save fixtures (verification data)

- **Status: open.** Every archived save is **level ≤ 4** and located in `DNG00`
  (wilderness) or `DNG05` (the Temple). To *verify* — not merely derive — several
  behaviours I need real saves the repo does not have. **Why only you:** I can
  patch a save to *fake* a state, but that does not prove the engine actually
  reaches it, so it cannot validate the port.
- **What I need (any one or more of these, via `saves/inbox/`):**
  1. a **high-level caster** (level ≥ 10, ideally two different professions) — to
     confirm the level-5..16 magic-max table rows beyond what level ≤ 4 exercises;
  2. a save **inside `DNG13`** (the deep dungeon, header `level=4`) or any
     non-Temple dungeon at depth ≥ 2 — to confirm the D≥2 view sprites
     (`CHAR2/CH40`, `CHAR2/CH25`) are keyed by depth and not by scenario 5;
  3. a save **mid-shop or mid-combat** (shop open with items listed / a combat
     round in progress) — to confirm shop stock/price and round state.
- **Unblocks:** BLK-A005 remainder, the level-5..16 magic verification, and
  shop/combat state checks. **Not urgent** (derivation continues without them).

### OT-006 — Real-play confirmation of randomised / timed mechanics (when implemented)

- **Status: open (activation deferred until the feature exists).** Encounter
  spawns (P-4), the Emporium sell/buy haggling price (BLK-B006) and the internal
  clock's pacing are **randomised/time-driven**. DOSBox and the analysis emulator
  can only *sample* them, never prove a formula.
- **Why only you:** confirming the port matches **real play** here needs you to
  play the original (or capture several DOSBox frames of the same action) so I
  can compare distributions, not a single sample.
- **Unblocks:** closing BLK-B006 and P-4 with confidence rather than only a
  derived-but-unverified implementation.
- **Action now:** none — I will ping when a feature is ready to sample.

### OT-007 — Capture the VOR credits (CRED) page under DOSBox (settles the P-14 palette)

- **Status: open (needs DOSBox, which this agent's sandbox cannot run — no root).**
- **What:** VOR fades its own 5-table palette and leaves the bright table active
  (confirmed from the image: tables at `0x55c6..0x55f9`, routine `0xF83`), but
  AGENTS.md's "menu renders as STOCK in DOSBox" caveat makes it ambiguous whether
  DOSBox honours that palette literally for the credits. Only a real capture
  settles it.
- **Action:** with DOSBox available, run `tools/capture_dosbox.sh` on the VOR
  credits (press a key after the dragon) → save `ref/oracle/vor_cred.png`. I then
  compare RGB per index and implement the VOR palette exactly (no guess).
- **Unblocks:** P-14 (VOR credits/DRAG colours) and the test upgrade to compare RGB.

## Done

### OT-004 — What "out of action" means, and what shows the state (ANSWERED, closed)
- **Status: closed — answered from the decompilation; no owner action needed.**
- **What it means (derived, image `0x18839` / `0x18720`):**
  A party member is "out of action" when its per-character field `+0x49` is
  non-zero.  The **combat decision screen** ("Please make your decision for the
  next round!") then replaces that member's **action column** — normally the
  action word like `Attack` at x=235 — with the literal string
  **`is out of action.`** (`ds:0x6ff2`) and drops the trailing number.  This is
  the *only* on-screen indicator; there is no separate icon or panel change.
  * When **every** present member is out of action, the round instead prints the
    party message (`ds:0x6f3f`)
    **`The companions have all been put out of action. Earliest revival will be
    at the beginning of the next round.`**
  * The state is set by combat effects.  The writers of `+0x49` (image
    `0x104aa`, `0x104ee`, `0x10510`, `0x10532`, `0xe679`) store the effect id
    into it (`1`, `5`, `0x14`, `3`, `0xff`); `0x16685` and the round boundary
    clear it to `0` when the member is revived.  The group message says revival
    is "at the beginning of the next round".
  * A member that is out of action is also skipped by the action code (the
    decision routine jumps over its action dispatch when `+0x49 != 0`).
- **Colour — derived, not guessed:** the `is out of action.` string is drawn
  with drawer style argument `1` (`push 1`, image `0x16a15`).  The decision
  screen's own header rows (`The result of this round:`, `Name … Gld. Rt Exp.
  HP …`, image `0x1875a`/`0x1877c`/`0x1878d`) use the **same** style `1`, and the
  DOSBox oracle (`ref/oracle/combat_decision.png`) inks them at palette **index
  8** (dark gray) on the index-7 background.  So style `1` = index 8, and the
  port's index-8 draw of `is out of action.` is correct.  (An earlier note
  claimed this colour was unverifiable without the owner; it is verifiable
  because the string shares the drawer style with the already-verified header.)
- **Port coverage:** `compose_decision_screen()` renders the per-member row
  exactly (`tests/port_decision_test.py` pins the out-of-action row).  The
  all-out-of-action party message is not yet composed (it belongs to the
  round-result screen, which the port does not draw); filed as follow-up work.

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
- **Kept:** the save is committed as `saves/dng00-Conan-d8b1cad7.sav` (the
  only copy not in the archive; `bootstrap.sh` installs it into
  `game/GAMES/GAME4`).  Saves are stored under content-addressed names in
  `saves/` so a new upload can never overwrite them (see `saves/README.md`).
  `tests/port_town_test.py` reads the archived file, not the live slot, so a
  different save dropped into slot 4 cannot break the test.
