# notes-maintainer-state.md — session handover (2026-11-10)

Written by the incoming maintainer at the end of the first session, because the
previous maintainer was interrupted mid-work and their state was unclear. Read
this first, then `docs/HANDOFF.md` (the technical reference) and
`docs/WORKSTREAMS.md` (who owns what).

## Session 2026-11-23 (fortieth session) — M-6: type-7 door move + answer-gate fix

Sole active agent (lof2/M-6). Green baseline first: fixed the stale
`tests/answer_path_test.py` effect assertion (it looked for "would join"; the
code prints the engine's own " joins the companionship."), committed and pushed
(`0dfc726`), then ran the full `make check` → **HEALTHY** (emulator golden sweep
and drop-in golden screens both pass).

**G-017 resolved — the type-7 door moves the party.** The `[0x3b15]` reader was
"not found" only because it sits in an asm gap; a raw byte search of the unpacked
image (`LOF.EXE[0x54F0:]` via `tools/mz.py load()`) finds it at image `0x2C1F6`
(`cmp es:[0x3b15],0 ; jne 0x2c22f`). That routine does **not** move the party: it
only *skips* the normal walk-buffer commit when the flag is set, then clears it,
so the handler's own writes (`0x20AE0/0x20AF0/0x20B05`) are the teleport. The save
loader (`0x1C93F..0x1C971`) proves the global roles: `+0x9a2` ones -> `0x37fa`,
`+0x9a4` tens -> `0x460e`, `+0x9a0` cell -> `0x3824`, `+0x9a6` facing -> `0x3d2`.
So a door's `+0xe4`/`+0xe6`/`+0xe8` are location ones/tens and cell; the
destination is `tens*10+ones`. Implemented `DoorTarget::destination_location()`,
`dungeon_follow_door()` and wired it into `fire_cell_event()` (both the `--walk`
loop and the interactive `--ui` loop). SPEC2[0] is the only shipped type-7 record:
ones=1/tens=1/cell 572 -> DNG11 (32,15).

**Latent bug fixed — no-keyword records never reached the type switch.**
`answer_fires_effect()` returned true only for `Match`; the dispatcher shows the
empty-keyword case jumps straight to the type switch (`0x209CE: jmp 0x20AB8`), and
only a wrong answer skips it (`0x20A6E: jmp 0x1F83C`). Fixed to
`effect != Mismatch`; updated `dungeon_test::test_answer_branches` (it had
asserted the old polarity) and the `AnswerEffect` comment.

**Tests.** `tests/dungeon_test.cpp::test_door_follow` (decode + real DNG05->DNG11
follow at cell 572 + bad-destination no-op); `tests/once_only_test.py` extended to
assert the end-to-end `--walk` teleport (`(door -> location 11, cell 572)` and
`walk: x=32 y=15`). `make test` and `make golden-port` both pass.

**Type-11 teleport (P-2, 2026-11-23).** After the door, the dispatcher's next
handler chain (`0x20B16..0x20F60`) was decoded: type 11 (teleport to `+0xe4`),
18 (same cell write + per-member `hp(+0x22) -= f(+0xe6)+1`), 12 (per-(location,
object) latch + clear object), 19/20/21/22 (map-wide tile rewrites). Type 11 is
implemented (`dungeon_teleport_cell()` + a `fire_cell_event()` branch), with a
unit test (`dungeon_test::test_teleport`, 7 records, SPEC3[54] -> cell 1168) and
an end-to-end `--walk` test (`teleport_event_test.py`, DNG17 Air Elemental ->
cell 629 = (17,17)).

**Type-18 trap (P-2, 2026-11-23).** Same cell write, then every present member
loses `rand(+0xe6)+1` HP (clamped at 0). The damage helper `0x2a7d:0x2f6a`
(image `0x2D73A`) is `rand(+0xe6)+1`: it calls the MS-C generator at
`0x2f0e:0x1dec` (image `0x30ECC`, the same LCG as combat) and the 32-bit signed
modulo at `0x2f0e:0x1f32` (image `0x31012`). `apply_trap_damage()` in
`port/combat.{h,cpp}` mutates the save's raw char records so `build_save()`
persists the loss; a walk-local `CombatRng` seeded 0x0B models the roll (formula
exact, absolute HP is a model — no oracle). Unit test `combat_test::test_trap_damage`.

**Reachability finding (2026-11-23).** The type-12/18/19/20/21/22 records are
SPEC indices 81..125, but scanning all 29 DNGS maps, the **largest event index
any map carries is 48**. So those handlers are cut content, unreachable by the
walk's event selector in the shipped data; only type 11 (SPEC7[12],[13]) and the
recruit (SPEC0[1]) are placed. Type 18 is unit-tested only. This means the only
reachable remaining P-2 item is the **recruit stat block (G-018)**.

**Next.** Recruit stat block (G-018, the only reachable P-2 item); P-7 town
transactions / P-4 wilderness are the bigger player-facing gaps.

**G-018 sharpened (2026-11-23).** Traced the recruit accept flow: the dispatcher
prologue (`0x1F31E`) clears the 12 done-flags (`0x1F366`) and picks the **first
empty party slot** (`0x1F38C`, 32-bit `[bp-6]`/`[bp-4]`).  But the copy
(`0x1D99A`) leaves `present` (`slot[0]`) **0**, and the display (`0x2a7d:0x449a`)
reads a *different* segment `[0xb0d2]` (vs the copy's `[0xb036]`) and skips the
detail draw when `present==0`.  So the join path sets only the name and `+0x55`
— it does not create a functional member on its own, and the port's current
text-only behaviour is not contradicted.  The stat/present origin is genuinely
elsewhere; still blocked (do not synthesise).  GAME2 is a full party (6/6
present), so the disassembly's "finds no place" branch (`0x20062`) would fire —
yet the oracle shows "joins", reinforcing that the join targets a separate
`[0xb036]` structure, not the save party.  Also fixed a stale HANDOFF note:
the defender-row `+0x06` was already derived/implemented (`-(10*(4-rec[+0x29]))`).

**P-7 investigation (2026-11-23).** Started on town transactions (the next area).
Found that string-ref navigation is unreliable: code and data live in separate
segments, so the ds<->image offset mapping is not constant (the "finds no place"
string maps at image-0x3C821, but "Thyn." / "welcome to our" do not).  The
building greetings/options are data-driven.  The reliable decode path is runtime:
drive the emulator (`lof-native`; golden `shop`/`temple_bldg` scenarios reach the
buildings) and `LOF_WATCH` the party-record segment's gold (+0x72 dword) and HP
(+0x22 word) while pressing the heal/bank/rest keys — but the add-char golden
party is undamaged, so a *wounded* save is needed to observe the heal cost.  This
is the next concrete step; no transaction was guessed.  Plan recorded in PLAN P-7.

**Plan re-ordered + P-4 first slice (2026-11-23).** Per the owner, the queue was
re-ordered by player-visible priority (progression > survival > combat >
fidelity; P-3 demoted because the shipped monster gate aborts, P-2 blocked on
G-018). The top item is now **P-4 (town exit)**. Its first verifiable slice
landed: the **give-artefact (amulet) page** (`lof --amulet [--save GAMEn]`),
composing the play-screen chrome (MSCR + TWN00 viewport + party panel) + the
engine's narration; the text block (rows 92/100/108/116/132/148/164) is
**ink-mask exact** vs `ref/emu/amulet_newgame.png` (`tests/port_amulet_test.py`,
`make golden-port`). Remaining P-4: the 24-frame ANIM transition and the `--ui`
town->wilderness wiring (the give-item effect and the viewport are done).

**Give-item effect (2026-11-23).** Continued P-4: derived the engine's item-add
routine (image `0x69e2..0x6c5f` — scan 48 slots for the first empty one, store
the item, seed `charges` from item +0x13, set `condition`=100, `inc` the used
count +0x6b at `0x6c2a`, refresh the carried weight) and implemented
`give_item()` in `port/char`. `lof --amulet-give N --save S --amulet-out O`
applies it and writes a sealed save. The amulet viewport is the plain street
tile TWN00 (0.9993 vs TWN00 with the emulator palette; the earlier "78.8%
distinct scene" was a stock-vs-emulator palette-measurement error, corrected
2026-11-23 after the owner confirmed in DOSBox that the page is the town page
with only the text changed). Pinned by
`char_test::test_give_item` + `port_amulet_test.py`.

**Pivot to P-7 — Inn-rest "who pays" prompt (2026-11-23).** P-4's remaining
pieces are blocked (the amulet page itself is done — its viewport is plain
TWN00; the ANIM/PAGE transition pieces are small sprites needing
sequencing decode; the `--ui` town flow needs the add-char chain), so I took
the most tractable high-value slice of P-7.  Driving the emulator to the Inn
(party added, then R,S) produced `ref/emu/inn_rest_pay.png`: the *second*
Inn-rest step, "A nights stay and meal costs 2g, / who pays (1-6)? /
(C)ontinue" over the MSCR+TWN03 chrome (viewport matches TWN03 plain at
0.9994).  Implemented as `lof --inn-rest-pay [COST]`; the price is the
engine's computed value (`"costs "+n+"g,"`), formula undecoded so it is a
parameter.  Pinned by `tests/port_inn_rest_pay_test.py` (lines 1/3/4
pixel-exact, cost line a 25px font-overlap residual).  The pay *effect*
(deduct gold, restore HP/MP, advance the clock) is still open.

**Inn-rest pay effect (2026-11-23).** Continued P-7: found the ds base
(0x3c820, by four-string consistency) and disassembled the inn-rest handler
(image 0x820a..0x8680).  Derived and implemented `rest_cost` / `pay_gold` /
`rest_party` (port/char): cost = 2*members (+3*sum(min(14,spare_lbs)) for the
W option); the payer's gold dword (+0x72) is deducted (refused if short);
each present member has its out-of-action countdown cleared, rations set to
14 (W), HP healed by 2*level+max(0,Con-14) clamped to +0x20, and magic
restored (+0x69 += max(1,+0x68>>3), clamped).  `lof --inn-rest-do N
[--inn-rest-rations] --save S --amulet-out O` applies it and writes a sealed
save.  Unit-tested (`char_test::test_rest_effect`) and e2e on GAME2 (payer 4
pays 12g, wounded char 9→11, resealed).  Still open: the spell-memorisation
pass, the post-rest 32-slot reset, and the clock advance.

**Temple heal effect (2026-11-23).** Located the Temple (H)eal handler by the
print-site of "Whose wounds should" (image 0x9f88..0xab6a) and derived it:
cost = max(1,(hp_max-hp_cur)*level/2) for a wounded character (0 when full),
paid from a separately chosen payer through the same pay_gold routine; the
heal sets hp_cur = hp_max (0xab03) and clears a status code (+0x1f, +0x55=1).
Implemented `temple_heal_cost`/`temple_heal_apply` (port/char) and
`lof --temple-heal-do N [--temple-heal-payer M] --save S --amulet-out O`.
Unit-tested (`char_test::test_temple_heal`) and e2e on GAME4 (c1 level 3, cost
6, 20/24→24/24, resealed).

**Temple revive effect (2026-11-23).** Continued the same handler (type 3):
cost = level*300; success iff CON!=0 && CON*4+5 >= rand(100)+1; success clears
status (+0x1f), decrements CON (if >1) and raises hp_cur to ≥1; failure zeroes
hp_max/hp_cur.  Implemented `temple_revive_cost`/`temple_revive(c,roll)` and
`lof --temple-revive-do N [--temple-revive-payer M] [--temple-revive-roll R]`.
Unit-tested (`char_test::test_temple_revive`) and e2e on the committed fixture
`saves/dng00-Conan-393da5b1.sav` (dead char 1 + 2000g char 2, installed into
slot 3 by bootstrap.sh for manual play-testing): CON 18→17, status→0, hp→1,
payer −900.  Not reproduced: the +0x6c experience boost on success
(profession-table formula) and the cure-condition-only variant.

**Bank deposit/withdraw (2026-11-23).** The bank account is the character dword
at +0x76/+0x78 (the deposit handler prints "Your account:" from +0x76, 0xbc97).
Deposit moves gold→account (0xbd57/0xbd87), refusing when gold < amount;
withdraw is the mirror (account -= amount 0xbf76, gold += amount in the caller).
Empty input = all.  Implemented `bank_deposit`/`bank_withdraw`/`char_gold`/
`char_bank` (port/char) and `lof --bank-deposit N AMT` / `--bank-withdraw N AMT`
(AMT 0 = all).  Unit-tested (`char_test::test_bank`) and e2e on GAME4 (deposit
5 then withdraw 5 round-trips, resealed).

**Joint deposit / Gather gold (2026-11-23).** The shared routine `0x212f:0x79ba`
(image `0x28caa`) behind every "(J)oint deposit" and the emporium "(G)ather
gold": sums every present member's gold, weight-checks the recipient
(`+0x64` vs `+0x66` − recipient.gold + total), then moves the total into the
recipient and zeroes the others.  Implemented `joint_deposit(SaveGame&, target)`
and `lof --joint-deposit N`.  Unit-tested (`char_test::test_joint_deposit`) and
e2e on GAME4 (42g pooled into char 1).  Emporium (S)ell / (R)epair (0x5938)
are interactive item-table UIs with pricing in the runtime segment — still
open, as are House-of-Trades training/magic/language.

**Emporium buy (2026-11-23).** The purchase effect (image `0x63f4`, the common
0x6458 full-price path): `buy_item()` refuses an uncarried item or short gold,
else deducts `item.price` (pay_gold), adds the item (give_item) and decrements
the stock slot unless it is the 0xFF sentinel.  `lof --buy N ITEM`; unit-tested
(`char_test::test_buy_item`) and e2e on the revive fixture (char 2 buys item 3
for 9g, inv_used+1).  The interactive 8-slot shop display (0x6c60) and the
charisma-haggling price variation (0x64c2) are not reproduced.

**Item use effects (2026-11-23).** The use dispatch `0x0e89d` reads item `+0x1f`
and applies: effect 10 heal HP by `item.charges` (f3 high byte), clamped to
hp_max; effect 3 poison (hp −= charges, set `+0x1f`=3 / `+0x57`=4 / `+0x58`=0x12);
effect 1 enchant weapon (`+0x5e` += 2 when `+0x5e + +0x45 < 0x11`); effect 2
enchant armour (`+0x5b` += 2); effect 30 cure poison (state 3 → clear
`+0x1f`/`+0x57`/`+0x58`); effect 31 cure illness (state 4 → clear `+0x1f`).
`use_item_effect()` + `lof --use N ITEM`;
unit-tested (`char_test::test_use_item`) and e2e on GAME4 (healing potion
20→21).  Item consumption is caller-side.  Effects 15/40 (combine) and 34
(lantern) in the >10 branch remain open.

## Session 2026-11-23 (thirty-ninth session) — M-6 takeover: plan refresh

Lane M, sole active agent (lof2/M-6). Read the rules, guess register, owner
tasks, handoff, risks, workstreams and every notes file. No open PRs
(`0` open) and no open blockers (A/B/M all resolved). Working tree clean at
`3a6088d`. `make port` builds; `make test` passes the CPU + font tests and only
fails `dungeon_test` because `game/` is absent (git-ignored) — run
`./bootstrap.sh` first on a fresh box.

**What this session changed (docs only):**

- **Completion figure corrected to ~70 %.** The previous ~87 % weighted only the
  finished screens/models and silently omitted four gameplay subsystems that the
  queue already lists but that are still at 0–25 %: overworld travel (P-4), town
  transactions (P-7), spells (P-5) and the monster's turn (P-3). §1 now carries
  the whole-game table (weights total 100, weighted sum **69.4 %**) and keeps the
  old screens-only figure for continuity. The arithmetic is in the table so it
  can be checked.
- **P-4 progress recorded** (from session 38): leave-town key script validated,
  amulet page = generic give-an-artefact flow, wilderness destination 0.9272.
  Estimate 12–24 → 10–20 h; open items listed in order.
- **P-3 estimate** corrected to its reduced scope (4–7 h, not 8–14 h), matching
  its own row.
- **Two missing queue rows added:** P-14 VOR credits palette (from
  `docs/notes-maintainer-vor.md`) and P-15 the G-014 event-key 70-entry overflow.
- **Remaining effort** recomputed: ~71–128 agent-hours.
- **Single-queue pointer reconciled** (Rule 9/Rule 7): RULES Rule 8 + checklist
  and HANDOFF §4 now point at `docs/PLAN.md` §2 as the authoritative queue;
  HANDOFF §4 is relabelled the per-item status log. Changelog entry added.

**Open guesses (revisited per Rule 4):** watchlist G-005 (wall set — renderer
half decompiled, mover oracle-derived), G-007 (depth-2+ `CHnn` numbers
scenario-5), G-004 (descriptive item-effect labels beyond traced values), G-012
(`P`/`B` flags + no-target state), G-013 (outdoor scenery rule — partly
resolved, needs the D0 draw routine). Queued-normal G-014/15/16/17/18 map to
P-2/P-15. **No owner decision is currently required** (GUESSWORK section A is
empty; OWNER-TASKS "Open" is empty).

**Next queue item:** P-2 (object-code semantics) is the top open item; P-4
(overworld travel) is the largest player-visible gap.

## Session 2026-11-21 (thirty-eighth session) — town exit / P-4 entry

Lane M. Closed **P-2** and **P-3** earlier in the cycle, then worked **P-4**
(the town exit). The tree is green: `make port` builds, `./lof --screen UP/MSCR`
renders, all named tests pass, working tree clean.

**P-4 — what is now proven.**

- **Validated key scripts** (owner-supplied, verified against the emulator):
  new game + leave town = `[any key] - a - 2 - c - c - c - 1`; load a game =
  `[any key] - o - [slot]`. The **third** `c` opens the amulet page; the `1`
  leaves town into the wilderness.
- **Destination**: after the digit, the frame matches the slot-2 wilderness play
  capture at **0.9272** (town: 0.7136). Leaving town is always possible except
  with no living character; a new game starts in the **Inn** (the boot menu *is*
  the Inn), so the first `c` leaves the Inn and the next leaves town.
- **The amulet page is the engine's generic "find an artefact" give-item flow**,
  not a bespoke screen: `You find an artefact:` + kind + item name +
  `Who should receive that (1-6)?` + `(L)eave it`. Engine strings (unpacked image
  offsets): `0x3CF49` "On starting your journey, you receive an amulet from a
  town guard. A gift from the Count.", `0x43A00` "artefact:" + the kind list
  (`a weapon:`/`armour:`/`a shield:`/`some ammunition:`/`a magic staff:`/
  `something to read:`/`a jewel:`), `0x43ADD` "(L)eave it", `0x3E6DC` "Amulet".
  **Implement one `give_artefact()` and reuse it**, not an amulet special case.
- **Reference captures archived**: `ref/emu/amulet_newgame.png` (the page, 12
  colours) and `ref/emu/town_exit_wilderness.png`.

**P-4 — what is open (in order).**

1. The page's **font and layout are unknown**. Do *not* hand-place text. Sweeps
   already ruled out: CHR1 at 6 px and at 8 px, CHR2 at 8 px, CHR2's compact 5x5
   face at 5/6 px, and every `EGA/PAGE/*`, `EGA/IN/*`, `EGA/UP/*` image (no
   match at any offset in index space). It is a code-drawn frame plus text.
2. **Disassemble the give-item routine** — but first *verify* the image→`ds`
   mapping: `LOF.EXE` is LZEXE-packed (`hdr` `0x54F0`), so `d[hdr:]` is **not**
   the image; use `tools/mz.py` `load()`+`flat()` (needs **capstone**) and check
   the known pair `file 0x42BD0` ↔ `ds 0x0EC0` (must read `Defends`) before
   trusting any computed `ds`.
3. Then: the 24-frame `EGA/ANIM/AN00..AN53` transition and `EGA/PAGE/PG00`, and
   the outdoor movement itself (P-4's remaining bulk).

**Environment caveat (important).** This workspace **drops pip installs between
commands**: `numpy`, `pillow` and `capstone` each vanished mid-session. Reinstall
before every analysis run (`pip install numpy pillow capstone`). `capstone` is
needed by `tools/mz.py`, so a "module not found" there is the environment, not a
broken tool.

**Retracted.** My earlier "town exit is two-step / intermediate menu" reading in
`docs/notes-b.md` §3c came from pressing `O`,`2`, which *loads save slot 2* — the
emulator was never in town. The retraction and the corrected notes are in place;
do not resurrect those numbers.

**Still open from earlier sessions**: G-017 (consumer of `[0x3b15]`), G-018
(recruit slot/capacity/stats).

## Session 2026-11-20 (thirty-seventh session) — combat round-result screen

Lane M. The predecessor's round-result work was in `/tmp` only; this session
decoded, implemented, verified and documented it.

- **`--round-result` implemented** (`compose_round_result_screen()`): the
  encounter chrome plus `The result of this round:` (y=92, colour 8), the
  header (y=108) and rule (y=116, colour 0), one row per **present** member at
  y=124+8n, and `(C)ontinue` (y=180, colour 0).  Columns are the engine's own
  printf formats, so they follow from the cell widths: `%-10s ` (11 cells →
  x=205), `%4ld ` right-aligned in four 6-px cells, `* `/`- ` at x=235/247/259,
  `Passed`/`Failed`/`-` at x=271.  Values are the engine's four per-round
  arrays, not save fields; `--round-result delta:w:p:b:a` feeds them per
  present member, the bare flag renders the pinned oracle round.
- **Oracle committed:** `ref/oracle/round_result.png`, captured deterministically
  from the emulator with the documented `LOF_KEYAT` drive (md5 verified twice).
  The port's text block matches its **ink mask 1.0000** (the ~0.5% residual
  index-12 pixels are the emulator's text artefact — the same residue exists in
  `combat.png`/`combat_decision.png` and it is absent from DOSBox).
- **Derivation corrected:** the text drawer's third argument is a **newline**
  flag, not a "style" — the drawer saves CX and calls the newline routine when
  it is non-zero (image `0x2F88`/`0x2FC5`).  The face is `cs:0x1412` (set by
  `lcall 0x18d:0x1787`, call sites `0x1B32E` face 0 / `0x2ED2C` face 1) and the
  colour is `cs:0x1414` (set by `lcall 0x18d:0x175a`).  Written up as a
  correction in GUESSWORK G-011; the measured 6-px/index-8 rendering stands.
- **Font:** `:` (CHR1 band 1 cell 37) and `*` (band 1 cell 30) were missing from
  `port/font.cpp`; both added and pinned in `tests/font_test.cpp`.  Without
  them the title's colon and the W column rendered blank.
- **Tests:** `tests/port_round_result_test.py` (text ink exact; header/rule/
  footer exact in index space; `:` and `*` cells; roster identical to the
  encounter screen; row inputs actually drive the columns and touch nothing
  else) wired into `make golden-port`.  `make test` and the full
  `make golden-port` sweep are green.
- **Queue:** HANDOFF item 12 added (done); item 11 (all-out message) now has its
  trigger decoded (`[bp-0x36]`, image `0x18449`) and remains todo — it is a
  *decision-screen* branch and is blocked on the message colour (no engine frame
  reaches it yet; patching `+0x49` in a save prevents the party from leaving
  town, so the oracle needs a mid-encounter state).
- **Combat (item 4) — inputs verified by live capture.** `LOF_CAP` at the
  resolution entry (`LOF_TRACE_IP=27a82`) dumped the round's input state, which
  pinned the 12-byte combat rows (party `+0x176+12i`, defenders
  `+0x258+72g+12j` in the character segment `[0xafa6]`; marker / record index /
  class / power / rank / defence / offence / **HP at `+0x0a`**, matching `GAME2`
  exactly), plus the attack prelude and the hit test. Recorded in
  `docs/notes-b.md`; `AGENTS.md` now warns that runtime linear = image +
  `0x10100` (segment immediates are relocated `+0x1010`), which cost a detour.

## Session 2026-11-20 (thirty-sixth session) — savegame management (Task 2)

Owner's second task: several savegames were committed at the repo root, and a
slot upload **overwrote** an existing file. What I found and did:

- **The overwrite was real, and I recovered the lost copy.** `GAME4` at the root
  had two distinct contents over time: `d8b1cad7` (Conan/Siegmund/Lara/Laladil/
  Joanna/Gandalf, standing at **Cyldane**) and `b2b765be` (Gonk/Conan/Laladil/
  Bilbo/Helena/Elena, in the **Temple**). The owner's upload replaced the second
  with the first. The committed fixture (HEAD) is `d8b1cad7`, so git was fine;
  the *uncommitted working copy* `b2b765be` was what got clobbered. It still
  survived in the git-ignored `game/GAMES/GAME4`, so both variants are now
  archived and distinct.
- **New layout: `saves/`.** Root `GAME1/GAME4/GAME5` moved (git `R`, history
  kept) and renamed **content-addressed**: `<map>-<party-lead>-<md5[0:8]>.sav`.
  Both GAME4 variants now coexist (`dng00-Conan-d8b1cad7`, `dng05-Gonk-b2b765be`),
  plus the recovered solo `GAME6` (`dng00-Cassandra-6c114403`).
- **No-overwrite scheme.** `tools/ingest-save.sh` files any save dropped in
  `saves/inbox/` or at the root: identical bytes → skipped, different save for
  the same slot → filed under its md5 suffix (old file untouched), non-save →
  rejected. It rebuilds `saves/manifest.tsv` from disk. `tools/saveinfo.py`
  derives the name/metadata from the save bytes (same offsets as `port/char.h`).
- **Tests read the archive, not slots.** `port_town_test.py` now reads
  `saves/dng00-Conan-d8b1cad7.sav`; `bootstrap.sh` installs fixtures into slots
  1/4/5 for manual play only. New `tests/saves_test.py` (in `make test`) guards
  the scheme: content-addressing, fixture hashes, manifest freshness, and the
  never-overwrite behaviour (run against a temp tree). Mutation-checked.
- **Docs:** `saves/README.md` documents the scheme; AGENTS.md, OT-001 updated.

## Session 2026-11-20 (thirty-fifth session) — TL;DR (Maintainer takeover)

Took over the crashed predecessor's lane. Their uncommitted tree and the
`m/out-of-action-answer` branch (PR **#151**, still DRAFT) held a good
"out of action" answer plus the status-box time/morale/rations work. I reviewed
the committed diff, verified its load-bearing claims against the data/oracle,
finished the thread, and **squash-merged #151** (`55239e2`); branch deleted, no
open PRs remain.

What this session changed on top of the predecessor's commit:
- **G-011 fully closed** (colour *and* advance): the "is out of action." row uses
  style 1, which is the *same* style as the decision header that appears in
  every decision frame. Measuring the oracle header gives advance **6 px** and
  colour **palette index 8**; new guard `check_style1_colour_and_advance` pins it.
  No owner input needed — OT-004 moved to Done.
- **Status-box colours corrected in the docs**: time-of-day value is **index 15
  (white)**, morale and rations are **index 7 (light gray)**. The code already
  drew them right; three comments (AGENTS.md, port/lof.cpp,
  notes-maintainer-state.md) wrongly said "all index 7". Verified the note's
  claim that patching GAME2 morale→1 reproduces the oracle's "Satisfactory".
- **Save-slot facts corrected (Rule 1, read from `game/LOF.EXE`)**: format is
  `GAMES\GAME%d` (file off `0x498cd`, no padding) and the prompt at `0x4989a` is
  `   M  1 2 3 4 5 C` = **five** slots. Fixed the `%3d`/six-slot claim and
  dropped the fabricated `GAME6` references.
- **`check.sh` false pass noted**: it reports "no unpushed commits" when the
  branch has no upstream ref (`@{u}` empty), so it can pass while commits are
  unpushed. Push explicitly and verify with `git log FETCH_HEAD..HEAD`.
- **Open items now:** only work-queue item 11 (all-out-of-action party message;
  the round-result screen is not composed yet). No open owner tasks, no open PRs.

## Session 2026-11-20 (thirty-fourth session) — TL;DR

Took over from the crashed predecessor with `docs/HANDOFF.md` and the
session-33 summary; the tree was clean at `84dc550` and `./check.sh` healthy.
Work was confined to `G-011` — the last inferred value on the decision screen.

> **Correction (same session, later).** The first pass of this section recorded
> G-011 as an unresolvable "negative result".  That was **wrong on two counts**
> and both are corrected below.  The branch *is* reachable by patching the save;
> the first A/B simply compared the wrong thing.

> **Second correction (later still — retracts the paragraph below).** The
> `>= 2` / "countdown" conclusion was itself **wrong** and has been reverted;
> the port tests `!= 0` again.  Two errors compounded: (1) the first A/B patched
> save **file byte 78**, which is record `+0x4a`, not `+0x49` (the branch field is
> file byte **77** for member 0), so it never varied the branch field at all;
> (2) the "countdown" story does not survive the decompilation — the single
> `dec` of `+0x49` (image `0x15C9`) is gated on `+0x1f == 5` (image `0x15AA`),
> while the decision row loop only draws members with `+0x1f < 5` (image
> `0x1879E`).  Disjoint sets: the decrement cannot reach a rendered row.  The
> draw test (image `0x18839`) is `cmp ...0 / je <action word>`, i.e. **any
> non-zero** shows the string.  The emulator A/B cannot settle it either: the
> engine renders `+0x49=1` byte-identically to `=0` (0 px), so the value does
> not reach the drawn row in that scenario and the clearing path is untraced.
> Decompilation governs (Rule 1).  See the G-011 register entry; guard is
> `check_out_of_action_any_nonzero`.

- **Owner tasks: 1 open (OT-004).** Reminded per Rule 8.  The owner's question
  this session — *what "out of action" means and what shows it* — is **answered**
  in OT-004; only the **text colour** needs a live-combat confirmation.
- **Watchlist: G-004, G-005, G-007 open; G-011 narrowed to the colour only and
  its branch semantics now fully derived.** See the register entry.
- **`+0x49` IS a save field; the row branch is `!= 0`.**  Save
  record offset `0x49` (file byte `4 + 0x49 = 77` for member 0) loads straight
  into the live record.  The decision row tests
  `cmp byte es:[bx+0x49],0` / `je <action word>` (image `0x18839`), so:
  - `+0x49 = 0` → normal action word;
  - `+0x49 != 0` → the out-of-action string.
  Off-by-one trap: file byte 78 is `+0x4a`.
- **CORRECTION (this session, later): there are TWO `dec` sites, not one.**  The
  earlier text in this note said the countdown "never touches a drawn row"
  because the only `dec` (image `0x15C9`) needs `+0x1f == 5`.  A full image sweep
  finds a **second** decrement at image `0x18415` (`decb es:(bx)` with
  `bx = record+0x49`, computed at `0x18403`), gated only on `> 0`
  (`0x1840F`) and **without** a `+0x1f` gate.  So the counter *does* tick down on
  a `+0x1f < 5` member.  The "disjoint sets" argument is retracted; the draw
  rule (`!= 0`) is unchanged.  G-011 updated.
- **What the `+0x49` values mean (derived, image `0x1219D`).**  Written from the
  spell-effect id: 4 (Paralyze)→1, 5 and 0x27→5, 0x28→3, 0x1D→0xFF (permanent);
  a broken weapon ("... breaks down!", image `0x10389`) writes 0x14.  So it is a
  *cause code / countdown*, not a boolean.
- **"All out of action" party message.**  When no present member can act the
  round prints `The companions have all been put out of action. Earliest revival
  will be at the beginning of the next round.` (ds:0x6F3F, image `0x185CE`),
  gated by the flag at `-0x36(bp)` (init 0 at `0x1811B`, set 1 at `0x18449` when
  a member with `+0x49 == 0`, present, `+0x1f < 3` exists).
- **Port was already correct.**  `compose_decision_screen()` tests `!= 0`.
  Commit `1800710` wrongly changed it to `>= 2`; that is reverted.  The
  regression check is `tests/port_decision_test.py::check_out_of_action_any_nonzero`:
  it asserts `+0x49=1` and `=2` both differ from `=0` in the action column, and
  it *fails* if the threshold is changed to `>= 2` (verified).
- **Status/morale/time save fields (this session).**  Added `kSaveTimeOfDay`
  (`0x9b6`, dword) and `kSaveMorale` (`0x9ba`, word) to `port/char.h`; the parser
  fills `SaveGame::time_of_day`/`morale`; `port/lof.cpp` now draws the
  time-of-day word (y=160) and the morale word (y=176) and the summed rations
  (y=184) in the status box.  Measured from the oracle: the time-of-day value is
  **white (index 15, same as the labels)**; morale and rations are **light gray
  (index 7)**.  (An earlier draft of this note said all three were index 7; the
  time value is index 15.)  The morale region matches the oracle **100%** when
  the save value agrees with the oracle's live moment; GAME2's save says
  Evening(7)/Good(0)
  while `ref/oracle/play.png` shows Midnight(0)/Satisfactory(1) because the
  engine's clock advanced and morale drifted after the save — a live-state
  difference, not a decode error (proven: patching morale=1 renders the oracle's
  `Satisfactory` pixel-exactly).
- **Why the two earlier conclusions were wrong.**  The first "negative result"
  A/B ran without a pinned `LOF_TICK` (combat is tick-seeded), so the two runs
  were not the same experiment.  The second A/B pinned the seed but patched the
  wrong save byte (78 = `+0x4a`), so it never varied `+0x49`.  The "ROST
  checksum" claim was also mistaken: `load_save()` does not verify the trailer at
  all (only `load_rost()` computes `intact`), and the shipped records carry tail
  `0` / `0x06060606` filler — neither matches `rost_record_checksum`, so the
  trailer is not a gate on the save path.  Zeroing HP likewise does not "fail the
  load".
- **Still open (the actual G-011 residue): the colour.**  The drawer is called
  with `0` for the action word and `1` for the out-of-action string, and the
  rank column passes `3`/`4`; that argument is a *style* index, not a raw
  palette index.  No committed oracle exercises the branch, so the port keeps
  index 8.  To clear it: capture the out-of-action frame from DOSBox (the
  ground-truth oracle) rather than the emulator — or have the owner report the
  colour (OT-004).
- **Merged as PR #150** (squash, `fd6d976`), branch deleted.  This correction
  is a follow-up commit.
- **Caution for the next session:** patching `game/GAMES/*` is safe only if you
  restore it — the saves are git-ignored, so a corrupted save is silent in
  `git status` and only shows up as a broken golden test.  Back up to `/tmp`
  and verify with `cmp`; prefer that pattern (or a copied game tree).

## Session 2026-11-20 (thirty-third session) — TL;DR

Environment was **reset** (fresh clone, no `numpy`/`Pillow`, scratch dumps gone);
reinstalled the Python test deps and re-verified everything green.
**The combat decision screen is now implemented** (`--decision`), and the two
guesses the previous session filed for it are **resolved**.

- **Implemented `--decision`** (`compose_decision_screen()`, `port/lof.cpp`).
  It is the encounter composition with only the right-hand text block replaced —
  proven by the two oracles sharing viewport/roster/bottom **pixel-exactly**
  (1.0000 each).  Match vs `ref/oracle/combat_decision.png`: **0.96**
  (viewport 1.000, text 0.993).  New test `tests/port_decision_test.py`, wired
  into `make golden-port`; full sweep EXIT=0.
- **G-009 RESOLVED.** The `KIL`/`ATK`/`DEF`/`RET` column is
  `combat_abbrev_names()[char[+0x24]]` via the 4-entry far-pointer table at image
  `0x187F0`.  `+0x24` **is** the combat rank: the engine's own rank-entry menu
  prints `(1)Enter Kill-Rank` / `(2)Enter Attack-Rank` / `(3)Enter
  Defence-Rank` / `(4)Enter Retreat-Rank` (`ds:0x713C`/`0x714F`/`0x7164`/
  `0x717A`, compared at `0x1913C`/`0x1916C`/`0x1919C`/`0x191CC`) and the write
  at `0x197F0` stores the pressed digit minus `'1'`.
- **G-010 RESOLVED.** The trailing per-row `1` is `+0x0d + 1` when
  `+0x16 == 0xff` (guard at `0x186F3`: signed `+0x0d` in `0..2` and
  `+0x0f != 0`), else `+0x16 + 1`; it is printed by `"@ %ld"` after the 10-cell
  action field, so the digit lands at x=307.  All six GAME2 rows give `1`.
- **Corrected a wrong claim from the previous session.** The notes said the
  `0x1919C`/`0x191CC` compares were "death tests" (rank 2 = dying, 3 = dead) and
  that `+0x24` was "body/health state".  Those compares are the rank-entry menu
  (verified by disassembling `0x190E0..0x191F8` and reading the four strings).
  Fixed in `docs/notes-b.md`, `port/char.h` (`kCharBody24`, `CharSheet::body24`,
  `PartyRow::body24`) and the session docs.
- **New watchlist item G-011:** the `+0x49` "is out of action." override on the
  decision screen (string/position/suppression derived; the **colour** is
  inferred because no oracle frame exercises the branch).  All shipped saves
  have `+0x49 == 0`, so no reachable screen is affected.
- **New C++ assertions** in `tests/char_test.cpp` pin the rank mapping
  (`[0,0,1,2,2,2]` -> `KIL KIL ATK DEF DEF DEF`), the action index, the
  `+0x1f < 5` guard and the trailing-field inputs against the real save.
- Watchlist is now **4 open items**: G-004, G-005, G-007, G-011.
  G-008/G-009/G-010 are resolved.
- **Merged as PR #147** (squash) after `./check.sh` reported HEALTHY.
- **Doc hygiene (PR #148, merged):** every blocker in `docs/blockers/{A,B,M}.md`
  is Resolved, but the hub index still listed BLK-A001/B004/B005 as open and
  omitted A003/A004 from the resolved column.  `OWNER-TASKS.md` OT-003's body
  still said the `W`/`P`/`B` meanings were "still open" while its header records
  it closed and the decompilation has derived all three.  Both fixed.
- **Process deviation (self-reported):** the first attempt at this session-note
  update was committed and pushed **directly to `main`** (`2149700`) instead of
  through a `work/*` branch, contrary to the working agreement in
  `docs/WORKSTREAMS.md`.  The change is docs-only and already public, so it was
  left in place rather than force-pushed; this note is the record.  Everything
  else this session went through a branch + PR (#147, #148, and this note).

## Session 2026-11-18 (thirty-second session) — TL;DR

Environment **HEALTHY**. Merged the G-008 work (PR #146), then found and
documented a **new, unimplemented screen**: the combat *decision* screen.

- **PR #146 merged** (`m/encounter-placement-resolved`): G-008 resolved, CH26
  placement guard added, `ref/oracle/combat_ch26.png` committed.  No CI is
  configured on this repo, so the local suites are the gate: `make test` green,
  all 11 `port_*` tests green, `check.sh` HEALTHY.
- **Stale model text fixed in code:** `compose_encounter_screen()`'s comment
  still described the deleted `(120,-1)` tiling.  Now states the plain
  `(10,11)` blit and the 1.0000 viewport result for both sprites.
- **BLK-B004 body status** still read "Partly resolved" while its summary row
  and `HANDOFF.md` said Resolved.  Body corrected to match (Rule 4 hygiene).
- **NEW FINDING — the combat decision screen (not implemented).**  Replaying
  the documented deterministic path

  ```
  LOF_CMDLEN=3 LOF_NO_TIMER=1 \
    LOF_KEYAT="50:0x18,100:0x03,150:0x50,200:0x48,250:0x2e,650:0x1e" \
    ./lof-native --dir game --headless --frames 820 --shot /tmp/cfg.png
  ```

  (`O`, `2`, `Down`, `Up`, then `A` = `0x1e`) yields a frame that is **not**
  the encounter screen and **not** the config menu `docs/notes-b.md` §3
  describes.  It is the round *decision* screen:

  | y | colour | text | source |
  |---|---|---|---|
  | 92 | 8 | `Please make your decision` | image `0x437cd` (file `0x48cbd`) |
  | 100 | 8 | `for the next round!` | image `0x437e7` (file `0x48cd7`) |
  | 116..156 | 8 | `<n>` + name + action word | party roster |
  | 116..156 | 0 | `KIL`/`ATK`/`DEF` per row | `kCombatAbbrevNames`, image `0x3cd00` |
  | 172 | 0 | `(A)ttack!` | image `0x43824` (file `0x48d14`) |
  | 180 | 0 | `(Q)uick combat` | image `0x4382e` (file `0x48d1e`) |

  All six strings are **grounded** (EXE bytes + CHR1 glyphs, template match
  1.000 for the pinned tokens).  Positions: text x=139 (6-px pitch); per-row
  index digit ~x=139, name x=145, action word x=235, abbreviation x=211.
  Committed as `ref/oracle/combat_decision.png` (md5 `a00f0e0c…`, deterministic
  across repeat runs).

  **Not implemented.**  Two of the three columns are now derived from the save,
  the third is filed as a guess rather than invented:
  * the action word column **is derived**: `Attacks` is `kActionNames[1]`, and
    all six party members carry `+0x0f == 1` in `GAMES/GAME2` (and in the
    runtime dump at the decision screen), so `+0x0f` -> `kActionNames[]`
    reproduces the column.  This confirms `docs/notes-b.md` §2 on the real
    party rather than by inference;
  * the `KIL`/`ATK`/`DEF` column is **not yet derived** — see **G-009**
    (`+0x24` matches the observed pattern exactly but is documented as the
    body/health byte, so it is filed, not adopted);
  * the rightmost column is CHR1 `'1'` at **x=307**, pixel-exact on all six
    rows (1.0000 — it is *not* a compact-face glyph).  What the number *means*
    is underived; filed as **G-010**.

  The screen is queued as `docs/HANDOFF.md` row 10 (Area M), and its two
  underived columns are filed as **G-009** (the `KIL`/`ATK`/`DEF` source) and
  **G-010** (the trailing `1`), so the watchlist is now **5 items**
  (G-004, G-005, G-007, G-009, G-010).

## Session 2026-11-18 (thirty-first session) — TL;DR

Environment **HEALTHY**. Followed up the decoder fix by clearing the guesses it
invalidated (Rule 4) and adding a second-creature guard.

- **G-008 RESOLVED (encounter sprite placement).**  The old note said the
  monster cell was "tiled (120,-1) at origin (56,3), vertical origin varies
  (56,9 for CH26)".  That was the byte-11 decoder truncation again: with the
  fixed decoder the creature cell is a full 120x119 image blitted **plainly at
  (10,11)**.  Verified for two sprites, and (10,11) is the *unique* best offset
  (brute-force search: only offset ≥ 0.9999 for each):
  * `ref/oracle/combat.png` → `CHAR0/CH24` (Mercenary): 1.0000;
  * second encounter → `CHAR0/CH26` (Elven officer): 1.0000.
  The second frame is now committed as `ref/oracle/combat_ch26.png`
  (deterministic: `LOF_TICK=0x7777` + `LOF_NO_TIMER=1`, repeat md5-identical).
- **New guard:** `tests/port_encounter_test.py` renders the CH26 encounter and
  asserts its viewport equals `CHAR0/CH26` at (10,11) (floor 0.9999).  Verified
  it fails when the blit is shifted +3 px.  `MIN_VIEWPORT` raised 0.99 → 0.999.
- **G-006 also moved to resolved** (its `(58,27)`/`(-14,28)` offsets are the
  same decoder artefact); G-007's offset sentence corrected.  Register now: 0
  awaiting owner, **3 on the watchlist** (G-004 item labels, G-005 wall set,
  G-007 `CHnn` labels).
- **Docs corrected** (they had gone stale): `AGENTS.md` (both the encounter and
  the first-person viewport sections described the deleted tiling /
  `DungeonView::tile`), `docs/HANDOFF.md` queue row 3, `docs/blockers/A.md`
  BLK-A001 status header (was "In progress", now Resolved), and the encounter
  test docstring.  `ref/oracle/README.md` documents both encounter captures.
- **Env note:** the container reset had dropped `numpy`/`Pillow`; reinstalled
  per `AGENTS.md` (`pip install numpy pillow`).  All 11 `port_*` tests pass;
  `make test` passes.

## Session 2026-11-18 (thirtieth session) — TL;DR

Environment **HEALTHY**. Fixed the **EGA image decoder payload-offset bug** (the
item the predecessor was mid-experiment on) and removed the viewport workarounds
it had forced.

- **Root cause.** The decoder read the RLE stream from file byte 11.  But the
  engine's loader (image `0x1C70`) leaves `si` on the `0x1234` magic and the
  blitter reads the pattern at `[si+0xa]`, and the header's size field is
  `4*w*h + 10` — so the decoded buffer begins with the ten header bytes and the
  pattern starts at decoded offset 10.  The correct decode is therefore **from
  file byte 1, dropping the first ten decoded bytes** (the predecessor's
  "decode from byte 1, payload at `[10:]`").  Decoding from byte 11 truncates any
  run that straddles the header boundary: `CH25` decoded to 6888 of 7140 bytes,
  `CH24`/`CH40` were similarly short.  This is why the menu matched with the
  byte-1 decode while the "old" viewport code (byte 11) needed scroll/tile
  offsets — those offsets were compensating for the truncation, not reproducing
  engine behaviour.
- **The one exception is the font strips.**  `CHR1`/`CHR2` are 40x24 and are
  addressed at file offset 11 directly: the DOSBox party-panel glyphs match the
  byte-11 decode exactly (the byte-1 decode shifts them down two rows).  So
  `load_ega_image` decodes images from byte 1/skip 10 and fonts from byte 11.
- **Viewport workarounds removed.**  With the complete decode every first-person
  cell is a full **120x119** image, so all viewport tiles blit plainly at the
  origin `(10,11)`:
  * menu / Temple / building tiles: `blit_ega(..., kViewportX, kViewportY)`
    instead of the scrolled `blit_wrapped_viewport` (deleted);
  * dungeon depth-2/>=3 sprites: `CHAR2/CH40` and `CHAR2/CH25` at `(10,11)`
    instead of `(58,27)`/`(-14,28)` + x-stride-120 tiling (`DungeonView::tile`
    deleted).  `CH25` at `(10,11)` matches the oracle **1.0000**;
  * encounter sprite: `CHAR0/CH24` at `(10,11)` instead of the diagonal tile —
    **1.0000** (the tiled rule scored 0.279).
- **Scores improved:** menu **diff 14 -> 0** (pixel-exact), encounter viewport
  **99.4% -> 100.0%**, inn residue **14 -> 0**, dungeon moved window **99.9%**.
  All 11 `port_*` tests pass; `make test` passes (new `ega_decode_test`).
- **New guard:** `tests/ega_decode_test.cpp` (registered in `tests/local.mk`)
  pins the image byte-1/skip-10 path, the font byte-11 path, and a
  header-straddling run — it fails on the byte-11 regression.
- **Docs:** `tools/ega.py` mirrors the same split (used by `port_vor_test` and
  `dngs.py`); `docs/blockers/A.md` BLK-A005/A001 carry a decode-correction log.

## Session 2026-11-18 (twenty-ninth session) — TL;DR

Environment **HEALTHY**. Worked the **encounter screen** (the wandering-monster
screen, first met from the wilderness).

- **Encounter screen (`--encounter`) — landed.**  `port/lof.cpp` gained an
  `EncounterGroup` list and `compose_encounter_screen()`; the shared play chrome
  was factored into `draw_party_and_status()` (the encounter oracle's party
  panel is *byte-identical* to the play oracle's).  The screen blits the
  encountered monster's `EGA/CHAR0/CH<nn>` sprite (the NSC record's `+0x48` art)
  into the viewport as the engine's tiled first-person cell, and draws the intro
  + "What will you do ?" + one "N <Plural> (Rnk R)" line per group + the four
  choices in `CHR1` at x=139.  Whole-screen **0.96** vs `ref/oracle/combat.png`
  (viewport 0.9938; residue = the dynamic party/status values).  Guarded by
  `tests/port_encounter_test.py` (`make golden-port`).
- **Grounded facts.**  The sprite bank is **CHAR0** (emulator open trace
  `ega\char0\ch24` for the pinned oracle; a second forced seed opens
  `ega\char0\ch26`), so `NSC[record].+0x48 -> EGA/CHAR0/CH<nn>` holds across two
  encounters; the `CH` number is RNG-selected monster placement (BLK-A005).
  The combat strings now come from `port/char_names.inc` via the existing
  `combat_*` accessors (added `combat_prompt_names()` for "What will you do ?");
  a `char_test.cpp` assertion pins it.
- **New watchlist item G-008.**  The sprite's horizontal origin (56) and the
  wrap rule are exact, but the **vertical origin** is not a fixed constant: it
  is 3 for the pinned `CH24` and 9 for a forced `CH26` encounter (both 120×119).
  The pinned oracle is exact; the general rule for `dy` is not yet derived
  (needs a decompile read of the encounter-draw routine).  Filed under Rule 4.
- **`make check` is not hanging** — it runs the ~12-scenario emulator sweep
  (`tests/golden_test.sh`, up to 300 s per scenario); give it a long timeout.
  The predecessor's "TIMED OUT" was an under-sized timeout, not a defect.
- **Golden sweep made ~2.3× faster (615 s vs ~1450 s).**  The 12 scenarios are
  independent and deterministic, so `golden_test.sh` now runs them on
  `GOLDEN_JOBS` workers (default `nproc`, dispatched longest-first) and emits
  the hashes in the recorded order — `tests/golden.txt` is byte-identical, and
  the assertions are unchanged in strength (same 12 hashes).  Verified the whole
  set in parallel *and* serially (`GOLDEN_JOBS=1`), plus the timeout path.
  Measured: O3/`-march=native` gives **no** speedup (the interpreter is
  memory-bound), and lowering the frame counts is **not** safe — the `town`
  frame is only stable from ~1300 frames, so trimming would weaken the oracle.
  Also fixed a latent bug: the fixed 300 s per-run ceiling had just 53 s of
  headroom over the 247 s `temple` run, so a loaded box silently produced a
  bogus empty-dump hash; the ceiling now scales with pool width and a timed-out
  run reports `FAILED(timeout=Ns)`.
- **Env note:** `numpy`/`Pillow` vanished from `/usr/local/bin/python3` mid-session
  (they were present at session start), which made `golden-port` fail with
  `ModuleNotFoundError`.  Reinstall with `pip install numpy pillow`; `check.sh`
  correctly reports this as a problem rather than silently passing.

## Session 2026-11-18 (twenty-eighth session) — TL;DR

Environment **HEALTHY** (`./check.sh` → `[check] HEALTHY`, 0 fails). Queue item #3
(BLK-A005) **closed**.

- **BLK-A005 — RESOLVED, closed as *not-portable-by-data* (PR #141).**  The
  `es:[0x259]` encounter-id producer (routine R, `img 0x276CA`) is fully
  disassembled, but capturing its **input** (new one-shot `LOF_CAP` hook, at the
  producer entry `313F:63DA` / linear `0x377CA`) proves the id is **not a function
  of the save**: `es:[0x4612]` is a *trail of NSC record indices* pushed by
  `img 0x23286` from the record whose runtime **cell key** `+0xf8`/`+0xfa` equals
  the party cell; that key is written by the **monster-placement routine**
  (`img 0x23095`), which walks each monster to a random 4-neighbour cell using the
  game RNG (BIOS-tick seeded).  Measured: seed `0x0B` (×2) and `0x4321` put record
  16 (`CH25`) on the party cell; seed `0x7777` put record 35 (`Ogre`, `CH40`)
  there.  The port's depth march (BLK-A001/G-006) is the faithful,
  save-independent model and matches **both** DOSBox oracles at 1.0000, so no port
  change is warranted.  G-007 reframed: depth-keyed, only the `CHnn` *numbers*
  remain as residue.
- **Emulator dev aid added:** `LOF_CAP=<file>` (+ `LOF_CAP_N=<n>`) dumps the full
  1 MB at the Nth `LOF_TRACE_IP` hit — reads a routine's input/output state.

### Revised open-items inventory (agent-hours)

| # | Item | Area | Est. agent-hrs | Status |
|---|------|------|----------------|--------|
| 1 | ~~BLK-A005 producer port~~ | A | ~~6–10~~ | **DONE** — closed as not-portable-by-data (RNG placement) |
| 2 | **Combat** — wire the decoded dispatcher + round model into the play loop | A/B | **12–20** | open (biggest chunk) |
| 3 | **Town services** — Buy/Sell/Repair, Deposit/Withdraw, Training/Learn magic, Inn rest (transactions + save write-back) | B | **10–16** | open |
| 4 | **G-007 residue** — capture a 2nd scenario's depth-2/≥3 view, read its `CHnn` | A | **2–3** | open (labelling only) |
| 5 | **G-004** — `item_effect_name()` labels beyond traced dispatcher values | B | **4–6** | open (labels only; largely already correct) |
| 6 | **G-005 (mover)** — isolate the step-rejection wall test | M | **3–5** | open (hard: indirect call) |
| 7 | **Exploration breadth** — town-exit/overworld travel, unvisited screens, save write-back | M | **15–25** | open-ended |
| 8 | **Party-panel live state** — time of day, morale, rations, internal clock | M | **4–8** | open |

**Remaining *queued* items (2–6): ~31–50 agent-hours.**  With breadth (7) and live
state (8): **~50–83 agent-hours**.

### Overall progress estimate: **~72% complete** (headline)

Weighted by player-facing scope (see the table in the 27th-session entry): the
dungeon viewport dimension rises to ~95% (BLK-A005 closed, both oracles 1.0000),
which lifts the total to ≈**70%**; headline **~72%** given the extra closed item.

## Session 2026-11-18 (twenty-seventh session) — TL;DR

Environment **HEALTHY** (`./check.sh` → `[check] HEALTHY`, 0 fails). One item
advanced and one PR merged.

- **G-005 (dungeon solid-wall set `{6,13}`) — PARTIALLY CLEARED (PR #139).** The
  **renderer** half is now decompiled: the per-cell view routine (`img 0x2BA20`)
  reads the map byte `es:[bx+0x46d8]`, masks it `and al,0x0f` (`img 0x2BA78`)
  and tests `cmp al,6 / je` + `cmp al,0xd / je` (`img 0x2BE2E`).  The port's
  `dungeon_is_solid_tile()` now masks too; verified a **no-op for the wall set on
  all 29 shipped maps** (no code has `solid(c) != solid(c & 0x0f)`); regression
  added to `tests/dungeon_test.cpp`.  The **mover** (step-rejection test) is
  still oracle-derived — the four `mov es:[bx+0x4612],al` sites are all the party
  *trail-shift* (`img 0x10D29`/`0x1A171`/`0x1AD3F`/`0x1AEB1`), and the collision
  routine is reached indirectly (routine R is an `lcall` target with no static
  far-call site).  Recorded in `docs/GUESSWORK.md` (G-005).

### Open-items inventory + agent-hour estimates (owner request, 2026-11-18)

Full status: queue (`docs/HANDOFF.md` §4) is 8/9 done; the only `partial` rows
are **#3** (BLK-A005 producer port) and **#4** (combat).  Watchlist: G-004,
G-005, G-007 (G-006 resolved).  No open blockers, no open owner tasks.

| # | Item | Area | Est. agent-hours | Notes |
|---|------|------|------------------|-------|
| 1 | BLK-A005 — port routine R as `dungeon_encounter_id()`; feed `dungeon_encounter_sprite()`; drop G-007's scenario-5 special case | A | **6–10** | Routine R fully disassembled (Round 50); self-contained port of a 3×6 proximity scan + the `NSC`-bank lookup. |
| 2 | BLK-A004/BLK-B004 — **combat**: wire the (already-grounded) dispatcher + round model into the play loop (encounter start, turn order, damage, messages) | A/B | **12–20** | Semantics decoded; the work is the state machine + rendering. Biggest single chunk. |
| 3 | **Town services** — (B)uy/(S)ell/(R)epair, (D)eposit/(W)ithdraw, (T)raining/(L)earn magic, Inn rest effect | B | **10–16** | Screens + strings done; the *transactions* (item/gold/stat mutation, save write-back) are not. |
| 4 | G-007 — data-drive the depth-2/≥3 view sprite (remove the scenario-5 pin) | A | **2–3** | Subsumed by #1 (the producer supplies the bank index). |
| 5 | G-004 — ground `item_effect_name()` beyond the traced dispatcher values | B | **4–6** | Needs more decompile reads or live-play effects; low risk (labels only). |
| 6 | G-005 (mover) — isolate the step-rejection wall test | M | **3–5** | Hard: indirect call, no static xref; the renderer half is done. |
| 7 | Exploration breadth — town-exit / overworld travel; unvisited screens; save write-back; remaining text pages | M | **15–25** | Open-ended; no single queue item yet. |
| 8 | Party-panel live state (time of day, morale, rations, internal clock) | M | **4–8** | Engine state a save does not carry; labels drawn only today. |

**Estimate for the remaining *queued* items (1–6): ~37–60 agent-hours.**
Adding the open-ended breadth (7) and live state (8): **~55–95 agent-hours** to
a *faithful, feature-complete* drop-in.

### Overall progress estimate: **~70% complete** (headline)

Weighted by the player-facing scope:

| Dimension | Weight | Complete | Contribution |
|-----------|--------|----------|--------------|
| Static screens + rendering (title/credits/menu/town/buildings/play/inventory/spells/character sheets) | 30% | ~95% | 28.5% |
| Data-format decoding (EGA, `SYST`, `ROST`, `DNGS`, saves, font, `DSKTAB`) | 20% | ~90% | 18.0% |
| Dungeon first-person view (depth march + wall set) | 10% | ~85% | 8.5% |
| Movement / play loop | 15% | ~55% | 8.25% |
| Combat | 15% | ~15% | 2.25% |
| Town services (transactions) | 5% | ~10% | 0.5% |
| Save write-back / game progression | 5% | ~5% | 0.25% |
| **Total** | 100% | | **≈66%** |

By **completed queue items** (8 of 9, with 2 partial): **~85%**.  By **test/oracle
coverage of the implemented surface** (golden-port + `make test` green): **~90%**.
The honest headline — weighting the large undelivered gameplay systems
(combat + services + save-write) — is **~70%**.

## Session 2026-11-18 (twenty-sixth session) — TL;DR — READ THIS FIRST

Environment **HEALTHY**. Closed queue item **#5 (Inn & Temple screens)** — the
"D5 first-person interiors" premise was a **misconception**: neither the Inn nor
the Temple has a first-person interior state. What actually exists is a set of
*town-style* screens plus sub-prompts, all now implemented and guarded.

- **Temple (H)eal / (R)evive sub-prompts RESOLVED.** Both open a
  character-selection sub-prompt over the **same** `TWN05` viewport (only the
  greeting/option text changes): heal = "Whose wounds should / we heal (1-6)?",
  revive = "Who should be revived / (1-6)?", each with a lone "(C)ontinue"
  (black) at y=116. Strings are the engine's own (`ds:0x50ea/0x50fe` heal,
  `ds:0x50d8/0x50ee` revive). Implemented as `compose_temple_prompt()` +
  `--temple-heal`/`--temple-revive`; pixel-exact vs fresh emulator frames
  (`ref/oracle/temple_heal.png`, `temple_revive.png`).
- **Inn (R)est sub-prompt RESOLVED.** The Inn screen *is* the boot main menu
  (re-confirmed: a fresh emulator `town`+`I` capture is byte-identical to
  `ref/oracle/menu.png` outside the party panel). Its `(R)est` option opens a
  sub-prompt over the **same** scrolled `TWN03` viewport (x0=16) the menu uses:
  "Would you like a simple / nights rest, or do you also / wish to have all
  rations / replenished?" + (S)imple nights stay / (W)ith replenished rations /
  (J)oint deposit (black) / (C)ontinue (white). Implemented as
  `compose_inn_rest_screen()` + `--inn-rest`; pixel-exact vs the fresh emulator
  `inn_rest` frame (`ref/oracle/inn_rest.png`).
- **Oracle provenance re-verified:** the emulator's `temple_bldg` frame, rendered
  with the standard EGA attribute palette, is **byte-identical** to the committed
  `ref/oracle/temple_bldg.png` (0 diff px); and the fresh `temple_heal` capture is
  byte-identical to the predecessor's `/tmp/temple_h.ega`. So the new oracle PNGs
  use the same recipe the committed ones do.
- **Tests:** `tests/port_inn_test.py` (new; Inn==menu + Inn rest) and the
  `temple_heal`/`temple_revive` rows added to `tests/port_building_test.py`, both
  wired into `make golden-port`.
- **Doc corrections:** `AGENTS.md`, `docs/HANDOFF.md` (§4 item #5 and the
  building-screens status) — removed the "Inn/Temple are first-person `D5`
  interiors" claim everywhere it appeared.

**Open / next:** queue `docs/HANDOFF.md` §4 — #3 partial (encounter-id producer),
#4 partial (combat). **Owner tasks:** none open (OT-003 closed as not-needed).

### Also this session: queue #3 (BLK-A005 producer) advanced

Picked up #3 after closing #5. The blocker's stated unblock action — "drive a
walk move in the emulator (needs BLK-A004)" — is **done**: BLK-A004 is in, so a
forward step is possible and the encounter-id producer **executes**.

- Repro: `LOF_CMDLEN=3 LOF_KEYAUTO=0x18,0x02,0x48,0x48 LOF_KEYGAP=8
  ./lof-native --dir game --headless --frames 1500 --shot /dev/null`.
  `LOF_TRACE_IP=377CA:37d30` (routine R, img `0x276CA`) → **3 hits** (0 at
  boot-to-Temple, matching Round 45); the `es:[0x259]` store `img 0x27A6A` fires
  6×/call.
- Routine R decoded from `LOF_SEGDUMP`: a **3×6 proximity scan** of the cell
  window in front of the party (not a per-cell lookup) — the id depends on party
  cell + view mode `es:[0x4615]` + `NSC<dd>` flags `+0x39`/`+0x2c`/`+0x52`/`+0x54`.
  Full outline + offsets: `docs/blockers/A.md` BLK-A005 Round 50, `docs/notes-a.md`
  Round 50.
- **Left to do:** port the scan to `port/dungeon.{h,cpp}` as
  `dungeon_encounter_id()` and feed `dungeon_encounter_sprite()`, replacing the
  scenario-5 special case (G-007).  Self-contained but not small (routine R is
  ~0x700 bytes / several nested loops); **not attempted this session** — a partial
  port would risk the "never guess" rule.  Merged as PR #137 (docs + findings).

## Session 2026-11-17 (twenty-fifth session) — TL;DR — READ THIS FIRST

Environment **HEALTHY**. Took queue item **#5 (Inn & Temple `D5` interiors)** and
found the premise was **wrong**: the Temple *building screen* is **not** a `D5`
first-person interior — it is town-style, exactly like the Emporium/Bank/House.

- **Temple building screen RESOLVED.** It is `EGA/UP/MSCR` + the temple exterior
  `EGA/TWN/TWN05` in the 120x119 viewport at (10,11) + `CHR1` text at x=139. The
  viewport is the engine's **two-tile scrolled blit** (same mechanism the main
  menu uses): the tile's column 0 lands at viewport x=**40**, and the columns
  scrolled off the right edge wrap back in on the left one scanline lower. With
  that scroll the viewport is pixel-exact vs the emulator's `temple_bldg` frame
  (plain `TWN05` blit only 0.330; the earlier "X0=80" best-fit was the same wrap
  measured from the wrong origin). Strings are the engine's own
  (`ds:0x5086/0x5095/0x50a3/0x50b4/0x50ce/0x50de`): "welcome to our / Great
  Temple!" + (H)eal character, (R)evive a dead character, (J)oint deposit,
  (C)ontinue (rows 92/100/116/124/140/156, white/black).
- **The emulator is a valid building-screen oracle**: the emulator's `shop` frame
  is **byte-identical (1.0000)** to the DOSBox `ref/oracle/emporium.png`, so
  scoring against emulator building frames is sound.
- **Implementation:** `compose_temple_screen()` + `blit_wrapped_viewport()` in
  `port/lof.cpp`, exposed as `--temple`. Guarded by `tests/port_building_test.py`
  (added the Temple row); whole-screen static match **1.0000** with `--cursor 0,0`
  (viewport + text + cursor exact). All `make golden-port` tests pass.
- **Still open (#5):** the Inn's own screen (the intro ends *inside* the Inn with
  no party members) and the Inn/Temple *first-person interiors* entered after the
  building menu are separate, later states.

**Inn screen = the main menu (resolved, no new code).** Driving the boot menu's
`(I)nn` key (`0x17`) in the emulator produces a frame that is **byte-identical to
the main menu** (`ref/oracle/menu.png`, 1.0000) — the game *starts* inside the
Inn, so the boot menu (Pick-pocket / Rest / Look / Erase / Add / Dismiss / Open
file) **is** the Inn screen; there is no separate Inn asset. The only difference
is the dynamic party panel (an empty party at boot vs the save's roster), which
is why the boot-vs-`--menu` diff (450 px, y9..29 x148..197) is confined to the
party panel. So `--menu` already reproduces the Inn; no `--inn` flag is needed.
The Inn/Temple *first-person interiors* reached after `(C)ontinue`/building entry
remain the open part of #5.

**Open / next:** queue `docs/HANDOFF.md` §4 — #3 partial (encounter-id producer),
#4 partial (combat), #5 partial (Inn screen + interiors). **Owner tasks:** none
open (OT-003 closed as not-needed).

## Session 2026-11-17 (twenty-fourth session) — TL;DR — READ THIS FIRST

Environment **HEALTHY**. Took over the crashed predecessor's in-progress queue
item **#3 (BLK-A001 / BLK-A005)** and **closed the viewport-composite half**.

- **BLK-A005 viewport composite RESOLVED.** The moved first-person window is the
  depth≥3 room sprite `CHAR2/CH25` **tiled across the viewport** at **x-stride 120
  (the sprite width) / y-stride -1**: copies at window `(-24,17)`, `(96,16)`,
  `(216,15)`, ... (= screen `(-14,28)`, `(106,27)`, `(226,26)`, ...). Two copies
  reproduce the **emulator framebuffer exactly** (0 mismatching px,
  palette-independent) and the DOSBox oracle window at **0.99930** (the 10
  residual px are the engine's live 1px corner diagonal, absent from the emulator
  too). The predecessor's Round-48 "shade difference" was a **palette-inversion
  bug** (its `attr_rgb()` swapped brown/red); with `port/gfx.cpp::attr_rgb` the
  second copy is pixel-exact with **no colour remap**.
- **Implementation:** `DungeonView::tile` (`port/dungeon.h`) + the tiling loop in
  `render_dungeon_play_view` (`port/dungeon.cpp`). Moved-window score **0.9669 →
  0.9993**; `tests/port_dungeon_view_test.py` floor 0.96 → **0.998**;
  `tests/dungeon_test.cpp::test_view_depth` asserts `v.tile` for depth≥3.
- **Docs:** `docs/notes-a.md` Round 49 (with the palette-inversion lesson);
  `docs/blockers/A.md` (BLK-A005 → *partially resolved*, viewport half closed);
  `AGENTS.md` "First-person viewport"; `docs/HANDOFF.md` queue #3.
- Branch `work/maintainer-viewport`, commit `2c67419`.

**Still open (BLK-A005):** the encounter-id **producer** (`es:[0x259]`, the other
half of BLK-A005) — the consumer is decoded, the producer is not. The depth-2
`CH40` view was **not** re-tested against a clean oracle (the only in-game
evidence is the depth≥3 corridor); tiling is applied to depth≥3 `CH25` only,
where the proof exists (Rule 1 — no guess for `CH40`).

**Open / next:** queue `docs/HANDOFF.md` §4 — #3 partial (encounter-id producer),
#4 partial (combat), #5 Inn/Temple `D5` interiors (M/A). #1/#2/#6/#7/#8/#9 done.
**Owner tasks:** none open (OT-003 closed as not-needed).

## Session 2026-11-17 (twenty-third session) — TL;DR — READ THIS FIRST

Environment **HEALTHY**. Took over the crashed predecessor's in-progress
queue item **#3 (BLK-A001 / BLK-A005, the first-person viewport composite)** and
landed it. Branch `work/maintainer-viewport`, commit `547070b`.

- **BLK-A001 resolved — the dungeon viewport is a depth-marched single-sprite
  blit, not a per-cell composite.** March the column of cells ahead of the party
  to the first **view-opaque** cell (tile-code low nibble NOT in `{0,2,7,8,9}`);
  the depth selects the sprite and its screen offset:
  - depth 1 → `EGA/D<d>/DP<index>` at (10,11) (the tileset sprite,
    `dungeon_base_view_index`);
  - depth 2 → `EGA/CHAR2/CH40` at (58,27);
  - depth ≥3 → `EGA/CHAR2/CH25` at (-14,28), **clipped to the viewport window**.
  Verified **pixel-exact** against the DOSBox oracle: the moved frame
  (`ref/oracle/dungeon_slot1_moved.png`) is `CHAR2/CH25` at (-14,28) matching
  **1.0000 over all 4479 pixels it covers**; the spawn is `DP001` at (10,11).
  The predecessor's "0.9676" was the whole-window score (the residue is a second
  element, below); the clean CH25-only match is 1.0000.
- **Implementation:** `dungeon_view_opaque` + `dungeon_view_for` +
  `render_dungeon_play_view` in `port/dungeon.{h,cpp}`; `render_dungeon_play`
  now uses the depth model. `port/lof.cpp`: `--walk` and `--ui` compose the HUD +
  party panel with no viewport tile, then blit the depth-selected sprite (this
  also wires movement into `--ui`, half of queue #4).
- **Tests:** `tests/dungeon_test.cpp::test_view_depth` (synthetic depth regimes +
  real DNG05 entry column) and `tests/port_dungeon_view_test.py` (spawn/moved
  window vs the oracle; added to `make golden-port`).
- **Docs:** `AGENTS.md` "First-person viewport" section; `docs/blockers/A.md`
  (BLK-A001 → Resolved, BLK-A005 → In progress); `docs/GUESSWORK.md` (G-006
  resolved, new G-007 for the empirical scenario-5 sprite pin); `docs/notes-a.md`
  Round 47 caveat corrected; `docs/HANDOFF.md` queue #3/#4.

**Still open (BLK-A005):** the encounter-id producer (`es:[0x259]`) and the
**second element to the right of CH25** in the moved window (the 472-px residue,
~3.3% of the window). The depth-2/≥3 `CHnn` numbers are the scenario's
encounter bank (`NSC05[id].+0x48`), pinned for D5 only (G-007); other scenarios
keep the depth-1 tileset sprite.

**Open / next:** queue `docs/HANDOFF.md` §4 — #3 partial (BLK-A005 residue),
#4 partial (combat), #5 Inn/Temple `D5` interiors (M/A). #1/#2/#6/#7/#8/#9 done.
**Owner tasks:** none open (OT-003 closed as not-needed).

**Owner interaction (this session).** The owner reported the full combat
round-table layout (`[name] HP W P B Attack`, one row per living member; HP =
the round's HP change; Attack = action Passed/Failed) and confirmed **`P=*` ⟺
armour −2%** and that **`B` is not ammunition**. This corroborates the derived
`W P B` model (the decompilation already fixes `W`=weapon damaged, `P`=shield/
armour damaged, `B`=ability improved). The owner also noted the **table shows
only in quick combat while the messages show only in regular rounds** (so they
can't be cross-checked by eye) and **closed OT-003 as not-needed**. Recorded in
`docs/blockers/B.md`, `docs/notes-b.md` and `docs/OWNER-TASKS.md` (OT-003 →
Done).

## Session 2026-11-17 (twenty-second session) — TL;DR — READ THIS FIRST

Environment **HEALTHY**. `main` green at `05aa0f0` (clean, pushed, **0 open
PRs**, only `main` on the remote). `./bootstrap.sh --check` → game data present
+ all three saves installed; `make test` green; `make golden-port` green after
installing the missing `numpy`/`Pillow` (`bootstrap.sh` warns about these; run
`pip install numpy Pillow` once per reset). No code changed; this session was a
**state-of-affairs check + handover** plus one **lane-B analysis advance**.

- **BLK-B004 advanced — the round-result table printer is FOUND.** The melee
  round table (`The result of this round:` / `Name       HP   W P B Attack`) is
  printed by image **`0x186A3`**, inside the combat round function **`0x1810E`**.
  It reads four **per-character bytes** from the party segment (`0x468E`):
  **W**=`[i*2+0x5608]`, **P**=`[i*2+0x5609]`, **B**=`[i+0x224]`,
  **Attack**=`[i+0x3d48]`; each prints `* `/`- ` (Attack: `Failed`/`Passed`/`-`
  when out of action). The four arrays are **zeroed at round setup** (image
  `0x18198`–`0x181B8`) and set to `1` by shared town/combat setters (W
  `0x156CA`/`0x15955`, P `0x17290`/`0x17658`/`0x1777A`, B `0x12846`/`0x13D7E`/
  `0x1471D`, all inside fn `0x1370C`/`0xF5C0`). A `LOF_WATCH=4bee8:4bef4` run of
  the scripted combat drive confirms a **W bit is set** mid-round
  (`1DAF:7CE0` = image `0x156CA`). **Still open: the combat *meaning* of W/P/B**
  (the arrays are generic event bits reused across result panels — House of
  Trades sets W, the Temple sets P, pick-pocketing sets B — so the combat
  semantics need a populated round frame or a trace of which combat branch sets
  each). Full write-up + reproducible method: `docs/notes-b.md`
  ("BLK-B004 — the round-result printer FOUND"). Branch/PR:
  `work/maintainer-blk-b004` (docs-only).
- **State verified end to end:** all 8 drop-in golden screens pass
  (`port_menu/play/town/building/ui/walk/vor`), `make test` green, tree clean.
  The queue (`docs/HANDOFF.md` §4) is unchanged: #3 BLK-A001/A005 (A, viewport
  composite), #4 play loop (M), #5 Inn/Temple interiors (M/A), #6 BLK-B004
  (B — now **only** the W/P/B *meaning* remains). #1/#2/#7/#8/#9 done.
- **Owner tasks:** none open (OT-001, OT-002 both Done). One **new** owner
  question is filed in `docs/OWNER-TASKS.md` (**OT-003**) because it is a
  live-play observation only the owner can make: *after a combat round, which of
  the W/P/B columns show `*` for a character who attacked / was hit / defended?*
  That settles the last open BLK-B004 item without further guesswork.

## Session 2026-11-17 (twenty-first session) — TL;DR — READ THIS FIRST

Environment **HEALTHY**; `main` green. First session of the new maintainer
(the previous one was interrupted; state verified from the repo, not from any
external hand-off). Two items landed.

- **G-002 resolved (PR #129) — the 16×16 corner patch is the mouse cursor.**
  The patch shared by the menu/town/building/dungeon frames is the engine's
  **software mouse cursor**, not a scene sprite:
  - routine image `0x2C2F` polls `INT 33h` fn3 and, when the pointer moves,
    calls `0x2758`, which blits a fixed 4-plane 16×16 arrow at the polled
    position. The engine never asks the BIOS to draw the cursor (only fn0 reset
    `0x1E85` and fn3 poll `0x1F10`), so the shape is engine data.
  - the emulator never draws it because its mouse never moves; the DOSBox
    captures were taken with the mouse parked at **(0,0)**, which is why the
    patch sits in the top-left corner of the frames that have it.
  - the sprite (already dumped byte-exact in the prior session) is baked into
    `port/lof.cpp` as `kCursorSprite`/`kCursorMask` + `draw_cursor()`, exposed
    as `--cursor X,Y`. `--cursor 0,0` reproduces the oracle: town/emporium/bank/
    house **100.0%** static match with the corner **pixel-exact**; the menu
    residue drops to 14 px (a pre-existing wrapped-strip seam). Tests
    `port_town`/`port_building` now pin the corner (previously masked) and use
    `--cursor 0,0`; `port_menu` tolerance 300 → 20. **No guess remains** — it is
    a fixed overlay recovered from the engine.
- **OT-002 answered (owner live play) — BLK-B004 action enum confirmed.**
  Owner reported: `(S)neak` = success check → "disappears in the shadows" (icon
  removed, not attacked that round), next round auto-Attack + sneak attack
  ("creeps up on … from behind"), may target any rank; `(U)se object` = open
  inventory to use an item (ranged e.g. crossbow); `(P)erform magic` = open the
  spell list; the round-table `Attack` column is just **Passed/Failed**. All
  consistent with the decompiled strings (`0x4309A`, `0x43195`, `0x438B1`,
  `0x438B8`). So `+0x0f` enum 2/3/4 is confirmed by live play. **Still open:**
  the `W P B` column meanings (owner guesses `W`=weapon, `P`=armour, unknown
  `B`) — kept in BLK-B004; settle by decompiling the round-table print routine
  or a follow-up populated-row capture. Recorded in `docs/OWNER-TASKS.md`
  (OT-002 → Done) and `docs/blockers/B.md`.

**Open / next:** the queue (`docs/HANDOFF.md` §4) is the single source of
"what is left": #3 BLK-A001/A005 (A, viewport composite), #4 play loop (M),
#5 Inn/Temple interiors (M/A), #6 BLK-B004 (B, only `W P B` open). #1/#2/#7/#8/
#9 done. **Owner tasks:** none currently open (OT-001, OT-002 both Done).

## Session 2026-11-10 (twentieth session) — TL;DR — READ THIS FIRST

Environment **HEALTHY**; `main` green (HEAD at start `6e3fbcc`, clean, pushed).
Short end-of-day session (owner shutting down). Picked up the interrupted
BLK-B004 combat work and **landed it** (PR #128, squash-merged).

- **BLK-B004 — combat action dispatcher — grounded (PR #128).** Took the
  previous session's combat-dump work and closed the analysis:
  - The **live party struct** is at linear `0x600B4` (= `0x5C92:0x3794`, via
    `es=[ds:0xafa6]=0x468E`, `les bx,es:[0x30]`); 410 B/record, name at `+1`.
    A second copy sits at `0x471E8`; combat writes the `0x600B4` copy
    (watchpoint-verified).
  - The per-character **action byte is `+0x0f`** (default `1` = Attacks);
    `(D)efend` sets it to `0`. `+0x0d` is the selected flag (`0x00`→`0xff`),
    `+0x0e` the target. The enum is the derived `kActionNames`. `+0x70` is a
    *separate* randomised byte, **not** the action (settles the earlier
    `+0xf`-vs-`+0x70` confusion).
  - The config menu is keyboard-driven: digit `1`..`6` picks a character, then
    `D/A/S/U/P` sets the action, `C`=Continue commits. The round-result table
    (`The result of this round:` + `Name       HP   W P B Attack`) renders.
  - Driving it needs `LOF_KEYAT` (the walk eats earlier keys):
    `LOF_KEYAT="50:0x18,100:0x03,150:0x50,200:0x48,250:0x2e,650:0x1e,780:0x02,900:0x20,1020:0x2e"`
    with `LOF_CMDLEN=3 LOF_NO_TIMER=1`; deterministic (md5-stable).
  - Deliverables: `docs/notes-b.md` (full write-up), `docs/blockers/B.md`
    (status → partly resolved), the config-menu strings transcribed into
    `char_names.inc` and exposed from `port/char` (`combat_config_action_names()`,
    `combat_config_rank_names()`, `combat_continue_names()`, `combat_aim_names()`),
    and `tests/char_test.cpp` pins. Also corrected the notes-b combat-string
    offsets: they are **file** offsets, not image offsets (MZ header `0x54F0`).
- **Still open on BLK-B004** (needs a short **owner live-play** check, Rule 5):
  the round table's per-column semantics (`W P B`, `Attack`), and the
  `Sneak`/`Use object`/`Perform magic` enum values (they open a sub-prompt first
  and did not change `+0xf` in scripted runs). Both are filed in
  `docs/blockers/B.md`.

**Open / next:** the queue (`docs/HANDOFF.md` §4) is the single source of "what
is left": #1 G-002 (M), #3 BLK-A001/A005 (A), #4 play loop (M), #5 Inn/Temple
(M/A), #6 BLK-B004 (B, partly done). #2/#7/#8/#9 are done. **Owner tasks:** a
short live-play confirmation of the BLK-B004 round-table columns + the
Sneak/Use/Magic actions would close the last B item.

## Session 2026-11-16 (nineteenth session) — TL;DR — READ THIS FIRST

Environment **HEALTHY**; `main` green. This session found and fixed a **red
tree** that the previous `check.sh` defect had been hiding, and closed the
last Maintainer-owned blocker.

- **check.sh "hang" fixed (queue #8, PR #125).** `check.sh` invoked
  `tests/golden_test.sh` in **both** the `if` and the `elif`, so every non-ok
  result re-ran the whole ~22 min emulator sweep (≈44 min) — that was the
  "hang". Now it runs once and branches on the captured output.
- **Red tree fixed (queue #9, PR #126).** `make golden-port` was failing on
  `main`: `port_walk_test.py` reported `FAIL spawn (got (3,2,0), want (0,0,0))`.
  Cause: PR #117 corrected the wall set to `{6,13}`, making DNG05's `(0,0)` a
  wall, so the walk loop's hard-coded `dungeon_place(...,0,0)` walked out to
  `(3,2)`. Fix: place the party at the level **entry cell**
  (`dungeon_place_entry()` = the DNG header's `entry_pos`/`entry_facing` = the
  save's `0x9a0`/`0x9a6`, Round 22), removing the guessed `(0,0)` spawn; update
  the test's stale expectations to the derived DNG05 entry `(17,3)` facing S.
  `make test` + `make golden-port` + golden emulator (`ok`) all green.
- **BLK-A004 closed (this session).** The Maintainer blocker is now **Resolved**:
  the extended-key delivery makes the party move in the emulator with scripted
  arrows. Verified: the Temple spawn viewport matches `dungeon_slot1.png`
  (62.7 % vs 16.7 %), and after two `Up` steps it matches
  `dungeon_slot1_moved.png` (**80.9 %** vs 16.7 %); both runs md5-deterministic.
  Reproducible recipe published in `ref/oracle/README.md`. `docs/blockers/A.md`
  updated (it had wrongly still said "Open" on `main`).
- **Golden re-verified:** `golden_test: ok (frames=60)` after both merges.
- **Stale-ref cleanup.** The 66 `work/*` / `docs/*` remote-tracking refs left
  over from the eighteenth session were **local cache only** — `git ls-remote
  --heads origin` shows the remote has just `main` (the deletions landed).
  Pruned the local refs (`git branch -r` → `origin/main` only); the audit and
  restore SHAs are in `docs/branch-audit.txt`.

**Open / next:** the queue (`docs/HANDOFF.md` §4) is the single source of "what
is left": #1 G-002 (M), #3 BLK-A001/A005 (A), #4 play loop (M), #5 Inn/Temple
(M/A), #6 BLK-B004 (B, needs M). #2/#7/#8/#9 are done. **Owner tasks:** none open.

## Session 2026-11-16 (eighteenth session) — TL;DR — READ THIS FIRST

Environment **HEALTHY**. HEAD at start `3525171` (main, clean, pushed). Repo
**cleanup** session: the owner deleted the poisoned sibling repo and asked me to
(1) drop every obsolete warning/check about it, (2) triage and clean up the ~70
stale `work/*` branches, then (3) keep advancing the project.

- **Poisoned-sibling-repo guards retired.** The owner deleted
  `FreeOpenSourceSyndicate/lof`, so its warnings are obsolete:
  - `check.sh`: the "poison guard" remote check is gone (repo identity still
    verified); `Makefile` comment updated.
  - `docs/RISKS.md`: **Risk 1 (sibling-repo poisoning) deleted**; the rest
    renumbered (1 workspace reset, 2 the red tree, 3 running the original
    binaries). README says "three risks" now.
  - "never look at the sibling repo" clauses removed from `README.md`,
    `AGENTS.md`, `docs/HANDOFF.md`, `docs/notes-maintainer-state.md`. The
    "two hard constraints" is now "the hard constraint" (only: never run the
    original EXEs).
  - `docs/RULES.md`: changelog entry.
- **Branch cleanup.** All **70 `work/*` remote branches** and 14 stale local
  branches deleted; only `main` remains. Every deleted branch had a **MERGED**
  PR (content already on `main` via squash-merge); the three without a merged PR
  (`work/agent-a-blockers`, `work/agent-b-itemuse-static`,
  `work/merge-maintainer-town`) were **verified content-superseded** by main
  (spot-checked: agent-a-blockers' Round-22 DNG-header code, the D5 sprite
  inventory, `dngs.py composite` and BLK-006/007 notes are all on main;
  itemuse-static's inverted gate was superseded by PR #99; merge-maintainer-town's
  play-loop/0Ch/palette deliverables are on main). Tip SHAs recorded in
  `docs/branch-audit.txt` (restore recipe in the header) for full recoverability.

**Open / next:** the queue (`docs/HANDOFF.md` §4) is unchanged and is the single
source of "what is left": #1 G-002 (area M), #2 BLK-B005 (B), #3 BLK-A001 (A),
#4 play loop (M), #5 Inn/Temple (M/A), #6 BLK-B004 (B, needs M), #7 regression
tests (M). I continued with the top item (G-002) after the cleanup.

## Session 2026-11-16 (seventeenth session) — TL;DR — READ THIS FIRST

Environment **HEALTHY**. HEAD at start `cc2d406` (main, clean, pushed). This was
a **restructure** session requested by the owner; ended at `cfe3ba8`.

- **Backstop (Rule 11):** the stale lane-A DRAFT **PR #117** (encounter-sprite
  consumer, author inactive for several sessions) was adopted: merged `main`
  cleanly, renumbered its two new guesses to avoid a collision with lane B's
  `G-003`/`G-004` (→ `G-005` wall set, `G-006` corridor fallback), and **merged**.
  Green: `make test` + all port golden screens. `BLK-A005` (encounter-id producer)
  stays open and is on the queue. No open PRs remain.
- **Restructure:** the three-lane structure is RETIRED — there is now ONE lane
  (the Maintainer lane). Owner decision: the split mostly produced hand-offs,
  not parallelism (only lane M had unblocked work), and items whose nominal owner
  was idle
  (`BLK-A001`, `BLK-B005`) had nobody feeling responsible. The owner asked for a
  restructure where every active line always has unblocked work — a single lane
  is fine — with the old lane definitions **preserved in the docs**.
- **What changed:**
  - `docs/WORKSTREAMS.md` rewritten: one lane, three **areas** (A/B/M kept as
    domain labels + the file map for fan-out). Explains working-alone vs
    fanning-out (0–2 extra agents pick disjoint-area items off the one queue).
  - `docs/HANDOFF.md` §4 is now the **single prioritized work queue** (7 items,
    each tagged with its area) — the authoritative "what is left". The old
    "done" checklist is kept below it as history.
  - `docs/RULES.md` **Rule 8 adapted** (Rule 9 — not duplicated): "stay in your
    lane" → "do not start an item that touches the same area/file as an item
    another active agent is working on". Changelog entry added (2026-11-16).
    Rules 1/2/4/5/11 and the session checklist had their lane wording neutralised.
  - `docs/BLOCKERS.md` hub relabelled lane→area; the three area files
    (`docs/blockers/{A,B,M}.md`) and the `BLK-<area><nnn>` IDs are **kept
    unchanged** (history/audit trail). Area index refreshed (B now shows
    BLK-B004/BLK-B005 open; A shows BLK-A001 open, BLK-A002 resolved).
  - `README.md`, `AGENTS.md`, `docs/RISKS.md`, `docs/AGENT-PROMPT-ADDENDUM.md`
    updated to the single-lane wording.
- **New canonical prompt** (in `docs/HANDOFF.md` §0 and
  `docs/AGENT-PROMPT-ADDENDUM.md`): an agent now picks an **item from the queue**
  (not a "workstream"); when >1 agent runs, pick a disjoint-area item.
- `make test` green. Docs-only change (no code touched).

**Open / next:**

- The queue (`docs/HANDOFF.md` §4) is the single source of "what is left": #1
  G-002 (area M), #2 BLK-B005 (B), #3 BLK-A001 (A), #4 play loop (M), #5
  Inn/Temple (M/A), #6 BLK-B004 (B, needs M), #7 regression tests (M).
- **Owner can now start 1–3 agents**: each reads the queue and self-assigns the
  top unclaimed item; two agents must pick different areas.

## Session 2026-11-16 (sixteenth session) — TL;DR — READ THIS FIRST

Environment **HEALTHY**. HEAD at start `ec6fda3` (main, clean, pushed). This was
a **continuation** session on the queued lane-M item (G-002) plus a regression
test.

- **G-002 advanced a long way (still cosmetic).** The 16×16 town/menu top-left
  patch is now fully characterized:
  - It is drawn by the **generic EGA-image blitter** (image `0x1C70`; the one
    AGENTS.md calls "the blitter that consumes the decoded buffer"): it does
    `mov ds,dx; cmp [si],0x1234`, so `si` points at a decoded EGA image. At the
    patch blit `si` starts at `0x0A` (past the 10-byte header) and steps `+0x20`
    per plane (`bp`) → a **16×16, 4-plane image** (0x20 bytes/plane) at `ds:0`,
    header `[0]=0x1234 [1]=2 [3]=0x10 [5]=0x8a`.
  - A new `LOF_PXDUMP` snapshot at the blit reproduces the oracle patch exactly
    (ink mask) — the blit is understood.
  - The sprite is **not** in any `EGA/**` asset (exhaustive 676-file scan),
    **not** in `LOF.EXE`/`VOR.EXE`, **not** in any game data file — it is
    composed at run time.  `LOF_WATCH` shows the scratch buffer is filled by the
    **in-memory EGA RLE decoder** (image `0x22F2`), i.e. a compressed image
    decoded from a run-time-built source.
  - It appears identically (ink) in menu/town/buildings/dungeon, so a **single
    fixed overlay** clears G-002 on every explore screen.
  - **Remaining step:** find the routine that stages the 16×16 source image
    (watch the decoder state `cs:[0xa1c]` len / `cs:[0xa06]` dest) and dump its
    bytes. Full write-up: `docs/GUESSWORK.md` G-002.
- **New emulator diagnostics (lane M, `src/machine.cpp`):** `LOF_PXWATCH` now
  also prints `si/di/bx/bp/ds`; new `LOF_PXDUMP=<file>` (`LOF_PXDUMP_AT=<hit>`)
  snapshots the DS segment at a hit — this is what recovered the scratch sprite.
  Documented in `AGENTS.md`. **PR #119 MERGED.**
- **PR #120 MERGED** — added `test_alu_flags()` to `tests/cpu_test.cpp`
  (AGENTS.md "do not regress" ALU items: CMP/SUB don't write their dest;
  SUB/CMP flag order CF/OF/AF/SF/ZF/PF; 0x83 is 16-bit sign-extended).
- `make test` green (cpu incl. new test, font, dungeon, char).

**Open / next (updated):**

- **G-002 (lane M, watchlist, cosmetic):** remaining step = recover the 16×16
  sprite's run-time source (see above). 88 px, not urgent.
- **Lane A:** dungeon viewport composite (per-cell sprite composition) —
  `docs/notes-a.md`.
- **Lane B:** `BLK-B004` (combat round resolution), `BLK-B005` (ROST checksum in
  `port/char.cpp`).
- When lane M runs dry: take the next unchecked `docs/HANDOFF.md` item, or add a
  regression test from AGENTS.md's "do not regress" list (still untested there:
  the INT 10h AH=12h EGA-detection path, the `scancode_ascii` full row, and the
  `reset_cpu_state` transient-state clearing — `scancode_ascii` and
  `reset_cpu_state` already have coverage via `test_keyboard`/the second-program
  boot).

## Session 2026-11-15 (fourteenth session) — TL;DR — READ THIS FIRST

Environment **HEALTHY**. HEAD at start `556fd71` (main, clean, pushed). This was
an **incoming-maintainer orientation** session; I picked up the queued lane-M
item and finished it.

- **G-001 RESOLVED — the town name is decoded and no longer hardcoded.**  The
  predecessor's "computed base + idx*7" hypothesis was wrong.  The town routine
  (image `0x53E0`) is a plain two-way select: it tests the 32-bit cell
  `es:[0x460e]` (read via `[ds:0xaf48]`) and draws `ds:0x5A39` "Thyn. " when it
  is nonzero, else `ds:0x5A40` "Cyldane. ".  The save loader (image `0x1C952`)
  fills that cell from the **16-bit word at save offset `0x9a4`** (sign-extended
  into AX:DX), so the selector is exactly that save word.  The two name
  immediates *are* plain `mov ax,imm` (`0x3D29`/`0x3D30`) — the predecessor
  searched for the ds offsets (`0x5A39`/`0x5A40`) instead of the file offsets
  (`+0x41D10`).  Verified by RAM dumps (Thyn dword=1, Cyldane dword=0; every
  other segment pointer identical) and by a `LOF_WATCH` write watchpoint.
- **Fix:** `compose_town_screen()` now takes the name; `town_name_from_save()`
  reads the word at `0x9a4` (nonzero → "Thyn", else "Cyldane"; unreadable →
  "Thyn", matching the boot initialiser at image `0x179`).  `--save` selects.
  `tests/port_town_test.py` pins both names (Thyn pixel-exact vs
  `ref/oracle/town.png`; Cyldane ink-mask-exact vs `ref/oracle/town_cyldane.png`,
  which uses a faded entry-route palette).  The House-of-Trades greeting
  ("greets you in Thyn.") is a **fixed immediate** in the engine — deliberately
  left un-varying.
- **PR #113 MERGED** (lane M, `work/maintainer-g001`); `make test` green and
  every port golden screen passes (menu/play/town/building/ui/walk/vor/noexe).
- **Doc corrections merged:** #114 (OT-001 note: G-001 no longer "still
  hardcoded"), #115 (HANDOFF building-screens bullet: Emporium/Bank/House are
  DONE; there is no "ornate engine-drawn panel"; only the Inn/Temple remain and
  they are first-person `D5` interiors, i.e. Lane A), #116 (G-002 progress:
  the patch draw site is identified — see below).
- Note: `make check` runs the slow emulator golden suite (~5+ min) — run the port
  tests directly (`python3 tests/port_*_test.py`) for a quick loop.

**Open / next (updated):**

- **G-002 (lane M, watchlist, cosmetic):** the 16×16 patch at the town/menu
  top-left corner is engine-drawn.  **Draw site identified** (this session):
  a masked EGA sprite blitter, store `mov word es:[bx+di],dx` at runtime
  `119D:0463` (image ~`0x1D30`); the same pixel is redrawn by two more stores in
  the same segment (`119D:0B95`, `119D:0FC8`).  The image-`0x2898` copier blits
  a 24×16 sprite from `ds:0xFE0`, but that invocation lands at byte-column 3
  (not the patch's column 0), so the patch must come from the other store(s).
  Next: break at the blit entry (image `0x1CF2`) and capture `si`/`bx`/`cx`, or
  `LOF_PXWATCH` a patch pixel *in the boot-menu* run (single store there) to get
  the exact dest.  88 px, 0.14% of the screen — not urgent.
- **Inn / Temple screens:** first-person `D5` interiors (walkable), not `TWN`
  tiles → **Lane A** (viewport composite).
- **Lane A:** dungeon viewport composite (per-cell sprite composition) — see
  `docs/notes-a.md`.
- **Lane B:** `BLK-B004` (combat round resolution) and `BLK-B005` (implement the
  ROST checksum in `port/char.cpp`).

## Session 2026-11-15 (thirteenth session) — TL;DR — READ THIS FIRST

Environment **HEALTHY**. HEAD at start `dad2fe5` (main, clean, pushed). This was
an **incoming-maintainer orientation** session: I surveyed the state, swept the
PRs, and finished the previous maintainer's interrupted open item.

- **The ROST per-record checksum is DECODED and VERIFIED** (was the previous
  maintainer's unfinished work). The 4-byte trailer after each 410-byte
  `ROST/ROST` record is a 32-bit **alternating ADD/XOR fold with a carry**:

  ```
  acc = 1
  for i, c in enumerate(rec410):
      if c == 0: c = 1
      acc = (acc + c) if i % 2 == 0 else (acc ^ c)
  trailer = uint32(acc)
  ```

  Derived from the decompilation: routine asm `0x1BF18` (far-called
  `0x1BF1:0x0008`; relocated at load). Verified on load at asm `0x2A1E1` —
  mismatch sets the tamper marker `0xFC` in record byte `0x0C` (asm `0x2A1F6`),
  acted on at asm `0xB0CB` (prints the "caught ... trying to fraud" message,
  `ds:0x72CA`). **23/23 records** match across `game/ROST/ROST`, `ROST1`,
  `ROST2`. Full derivation: **`docs/notes-maintainer-rost.md`**.
  - **Lane-B follow-up filed as `BLK-B005`** — `port/char.cpp` (Agent B's file)
    still parses `rec.tail` but does not validate it; the implementation request
    is in `docs/blockers/B.md`.
  - The owner-supplied saves are secured as `ref/rost/ROST1.cheat` and
    `ref/rost/ROST2.banked` (also dropped at the repo root so `bootstrap.sh`
    restores them after a reset).
  - **PR #110 MERGED** (lane M, `work/maintainer-rost-checksum`); `make test`
    green and the port golden screens pass.
- **No other open PRs** at session start (0 open). No blockers, no open owner
  tasks (`docs/OWNER-TASKS.md`: OT-001 Done).
- **PR #111 MERGED** (lane M, `work/maintainer-g001-recon`): static groundwork
  for the queued G-001 (the hardcoded town name). Established that the string
  drawer is image `0x2F78`, that the `0xC22B` stub draws only the message
  fragments, and that the town name (`0x5A39`/`0x5A40`) is drawn by a *computed*
  call elsewhere. The next step is the Thyn-vs-Cyldane live trace; the concrete
  recipe is in `docs/notes-maintainer-town.md` ("G-001 recon for the next
  session"). No port code changed.
- **`check.sh` is fully green** once `numpy`/`Pillow` are present (they are
  installed by `bootstrap.sh`; the only failure this session was their absence
  in the freshly-reset workspace — I installed them and re-ran `make golden-port`
  and every port golden screen passed). **Run `./bootstrap.sh` first, every
  session.**

**Open / next (unchanged priorities):**

- **G-001 (lane M, queued):** the town name `"Thyn."` is hardcoded in
  `compose_town_screen()`; the engine selects between `ds:0x5A39` ("Thyn.") and
  `ds:0x5A40` ("Cyldane."). The static recon is done (PR #111); the remaining
  step is the Thyn-vs-Cyldane live trace — recipe in
  `docs/notes-maintainer-town.md` ("G-001 recon for the next session") and
  `docs/GUESSWORK.md` G-001.
- **G-002 (lane M, watchlist):** the 16×16 patch at the town screen's top-left
  corner is engine-drawn (not any `EGA/**` asset); trace the town draw path.
- **Building screens** (Emporium/Temple/Inn/Bank): the Emporium reads no new
  files and its ornate panel is engine-drawn — decode the panel's draw routine.
  Oracle `ref/oracle/shop.png`.
- **Lane A:** dungeon viewport composite (per-cell sprite composition) — see
  `docs/notes-a.md`.
- **Lane B:** `BLK-B004` (combat round resolution) and the new `BLK-B005`
  (implement the ROST checksum).

## Session 2026-11-15 (twelfth session) — TL;DR

Environment **HEALTHY** (`./check.sh` green). HEAD at start `0adfdc2`; `main` is
now **`d8e35f7`**. This was an **incoming-maintainer orientation** session: I
surveyed the whole state of affairs, swept the PRs, pulled the lane-M items that
were queued, and left a clean handover. No blockers.

- **PR #109 MERGED** (lane M, `work/maintainer-rng-determinism`): the queued
  lane-M work from the previous session's branch.
  - **RNG seed is now explicit** — `INT 1Ah` previously returned whatever
    `CX:DX` the caller held, so the seed was an accidental-but-reproducible
    `0x000B`. It is now returned explicitly and honours `LOF_TICK=<n>`. The
    generator is decoded from the decompilation (classic MSVC `rand()` LCG,
    seeded once at startup from the BIOS clock), so combat is **fully
    deterministic** (6/6 identical runs, with and without `LOF_NO_TIMER`,
    across clock hours). See `docs/GUESSWORK.md` section E.
  - **New emulator diagnostics** (lane-A request): `LOF_PXWATCH=x,y` logs the
    `cs:ip` that writes the viewport byte covering pixel `(x,y)` (with the
    pre-write plane bytes and a hit cap `LOF_PXWATCH_N`); `LOF_TRACE_IP=lo:hi`
    now accepts a half-open range and `LOF_TRACE_BT=1` dumps the raw `ss:sp`
    words + Turbo C frame chain. Both documented in `AGENTS.md`.
- **PR sweep (Rule 11):** closed **#89** (lane A, stale/orphaned — author gone,
  last push 2026-11-12). It was *regressive*: it rewrote `docs/BLOCKERS.md` back
  to the single-file registry, undoing the per-lane split `main` adopted. Its one
  salvageable piece (the DNG-header **entry-cell + facing** decode) is **not** on
  `main`; the branch is left in place so a current lane-A agent can redo it on the
  current base. Reason posted on the PR. **#99 was already merged** (squash, via
  #99's own author path). **Zero open PRs** after this session.
- **BLK-A004 verified resolved** (lane A's "party cannot be moved in the
  emulator" blocker). The extended-key delivery fix (#102, PR `02663f1`) makes
  the arrows visible to LOF: in the Temple (slot 1), the spawn frame and the
  frame after two `Up` presses now **differ** (dumps `2bd6ae7d…` vs `b550897f…`),
  i.e. the party turns/steps. (BLK-A004 itself lives only on the closed #89
  branch; `main`'s `docs/blockers/A.md` carries A001/A002 only — there is nothing
  to mark resolved on `main`.)
- **Owner tasks:** none open (`docs/OWNER-TASKS.md`: OT-001 Done).

**Open / next (unchanged priorities for the next session):**

- **G-001 (lane M, queued):** the town name `"Thyn."` is hardcoded in
  `compose_town_screen()`. Both names are static in RAM; the engine **selects
  between** `ds:0x5A39` ("Thyn.") and `ds:0x5A40` ("Cyldane."). Next step: trace
  the **callers of the town routine `image 0xC22B`** with `GAME4` (Cyldane) loaded
  and find where the name pointer is formed. Full evidence in `docs/GUESSWORK.md`
  G-001 and `docs/notes-maintainer-town.md`.
- **G-002 (lane M, watchlist):** the 16×16 patch at the town screen's top-left
  corner is engine-drawn (not any `EGA/**` asset). Clear it by tracing the
  town-screen draw path.
- **Next lane-M screens:** the Inn (`TWN03`) and Temple (`TWN05`) town buildings;
  then the play-loop viewport foreground sprite.
- **Lane A:** the dungeon viewport composite (base-sprite rule is a low-nibble
  table; the corridor/opening multi-piece composite is still open) — see
  `docs/notes-a.md`.

## Session 2026-11-15 (eleventh session) — TL;DR

Environment HEALTHY. HEAD at start `755fd27`, `main` clean and pushed. Session
goal was to assess state and advance the post-town flow; the owner also supplied
the Cyldane save and asked for an explicit open-PR ownership rule.

- **Rule 11 added** (`docs/RULES.md`, **PR #105**): *the author owns its own PR
  (Rule 10); the **Maintainer (lane M) is the backstop** for any orphaned /
  stale / DRAFT / abandoned PR* — review it, push it green and merge, or close
  with a reason. Added a "sweep open PRs" step to the session checklist and a
  changelog entry.
- **OT-001 RESOLVED** (owner task). The owner supplied `GAME4` — a save standing
  **in front of Cyldane** (not Thyn). `O`, `4`, `Up` enters the town and the
  frame reads *"You enter the proud town of Cyldane."* Captured as
  `ref/oracle/town_cyldane.png` (md5 `bc1fd8201372584ede69ba2d613b08da`).
  Because the archive ships only save slots 1/2/3/5, `GAME4` is **committed at
  the repo root** and `bootstrap.sh` now installs it to slot 4 (else it is lost
  on a workspace reset).
- **G-001 advanced** (town-name selector). 1 MB RAM dumps at both town screens
  show `ds:0x5A39 "Thyn."` **and** `ds:0x5A40 "Cyldane."` present and
  byte-identical in *both* runs: the engine does **not** rewrite the name, it
  **selects between the two addresses**. The selector is a computed pointer (no
  `mov ax,0x5a39/0x5a40` immediate exists; the word `0x5A39` appears zero times
  in the image). Remaining lane-M task: trace the callers of the town routine
  (`image 0xC22B`) with `GAME4` loaded. `compose_town_screen()` still hardcodes
  `"Thyn."`.
- **Rule-11 PR sweep:** #105 merged. Left **#89** (lane A, open) and **#99**
  (lane B, DRAFT) untouched — both are live lanes' own PRs. **#99** resolves
  BLK-B003 and its author (lane B) is now *inactive* (B has nothing unblocked),
  so it is a backstop candidate: the Maintainer may take it over in a later
  session (push green + merge, or close with a reason) if it stays orphaned.
- **No open owner tasks** (`docs/OWNER-TASKS.md`: OT-001 moved to *Done*).
- `check.sh` **HEALTHY**; `main` clean and pushed at `5e0e1f8`.

**Open / next:**

- **G-001 decode** (lane M): trace the town-name selector with `GAME4` loaded
  and de-hardcode `"Thyn."` in `compose_town_screen()`.
- **Next lane-M screens:** the Inn (`TWN03`) and Temple (`TWN05`) town buildings;
  then the play-loop viewport foreground sprite.
- The amulet prompt's first line is still *not* decoded (font ambiguity) — do
  not hard-code it.

## Session 2026-11-15 (tenth session) — TL;DR

Environment HEALTHY. HEAD at start `f70b55f`, `main` clean and pushed. This
session picked up the interrupted "Up-arrow does nothing" investigation and
**closed it**: the root cause was an emulator keyboard bug, not the game.

- **Fixed the DOS extended-key convention** (lane M, `src/machine.cpp` +
  `src/emu.h`): a key whose ASCII is 0 (arrows, PgUp, function keys) must return
  `AL=0` and then the **scan code** on the *next* console read. The emulator only
  ever produced the `AL=0` half, so every arrow was invisible to LOF (which reads
  with `INT 21h AH=07` and keys off the scan code). This is why the party could
  not turn/step on the map and why "Up" never entered the town. **Merged as
  PR #102** (commit `46a5fda`).
- **Town entry now works end to end**: `O`, `2` (load slot 2 = party on the
  wilderness map facing *away* from town), `Down` (turn 180°), `Up` (step into
  town) -> the town main menu. Added a permanent `town_entry` golden scenario
  (`tests/golden_test.sh`, hash in `tests/golden.txt`) so an arrow regression
  fails loudly. Re-ran `make golden` — green.
- **Mapped the post-town flow** (docs + oracle frames, **PR #103**): from the
  town menu a single `(C)ontinue` leaves town (no time gate) -> wilderness ->
  amulet prompt -> idle time -> wandering-monster combat (~frame 700, no input).
  Four new emulator oracle frames in `ref/oracle/` (`wilderness`, `amulet`,
  `combat`, `charsheet`).
- **`check.sh` HEALTHY** at session end; `main` clean and pushed at `e1811b9`.

**Open / next:**

- **OT-001 (owner task, re-reminded):** provide a save standing in a town other
  than Thyn (e.g. Cyldane) so the town-name selector (G-001) can be traced and
  the hardcoded `"Thyn."` removed. No agent can do this (needs playing).
- **Next lane-M work:** the Inn/Temple first-person `D5` interiors (the two
  remaining town buildings), and the play-loop viewport foreground sprite.
- The amulet prompt's first line is *not* decoded (font ambiguity) — do not
  hard-code it (see `docs/notes-maintainer-town.md`).


## Session 2026-11-15 (ninth session) — TL;DR

Environment HEALTHY. HEAD at start `f564a6f`, `main` clean and pushed. This
session: **state assessment + all three town building screens + two rule
clarifications**; ends with `main` at the PR-#101 squash-merge and `check.sh`
HEALTHY.

- **Merged PR #100** (`docs(rules): Rule 9 + Rule 10`) after verifying its
  `check.sh` FAIL was **false**: it was contention from the previous session's
  concurrent emulator runs starving the golden scenario's `timeout 300`. Re-ran
  `tests/golden_test.sh` alone → **ok**. Lesson: never run `golden_test.sh` /
  `check.sh` while other `lof-native` runs are in flight.
- **Implemented all three town building screens** (`lof --emporium/--bank/--house`,
  lane M's top items) and **merged them as PR #101**. They share the town menu's
  shape (MSCR + a `TWN` viewport tile at (10,11) + `CHR1` text at x=139); each is
  pixel-exact (masked) against the emulator frame:
    * Emporium = `TWN01`, Bank = `TWN02`, House of trades = `TWN04`;
    * Inn/Temple open first-person `D5` interiors (no TWN tile) — still to do.
  `compose_town_screen` refactored into a shared `compose_building_screen`; added
  the `/` glyph (`CHR1` band 2 cell 2). `tests/port_building_test.py` (replaces
  the single Emporium test) + `ref/oracle/{emporium,bank,house}.png`, wired into
  `make golden-port`.
- **Two Rule-1 clarifications** (owner re-stated the source-of-truth rule twice;
  reconciled via Rule 9, changelog entries added): the decompilation is the
  **first** source of truth and wins when sources disagree; and **reading the
  decompilation is never out of lane** (Rule 8 cross-referenced).
- **G-001 advanced (not resolved):** a raw word-search of `LOF.EXE` finds the
  town-name addresses `0x5A39`/`0x5A40` stored as a **pointer nowhere** (`0x5A39`
  zero hits; `0x5A40` only as `mov byte es:[bx+si+0x5a],…` immediates) — the
  name address is computed from a data-driven base. Still needs a **Cyldane
  save** to observe the branch: **owner task OT-001, re-reminded below**.
- **G-002 (16×16 corner patch)** unchanged (engine-drawn residue).
- **`ref/oracle/shop.png` (DOSBox) does NOT match the emulator's Emporium**
  (~69%); it is not used by any test. `ref/oracle/town.png` *is* exact vs the
  emulator. Do not treat `shop.png` as an oracle.
- **check.sh timings:** `make test` + `tests/golden_test.sh` are the fast checks;
  `./check.sh` end-to-end is ~15–20 min here (11 golden scenarios) — run it alone
  in the background. Verified this session: `./check.sh` → **HEALTHY**.

## Session 2026-11-14 (eighth session) — TL;DR

Environment HEALTHY. HEAD at start `3fc53c8`, `main` clean and pushed. This
session was **state assessment + documentation corrections** (the owner was
about to stop for the night); no engine/drop-in code changed.

- **Address mapping corrected and pinned.** The rule is **`file = ds_offset +
  0x40000`** (the engine runs with `ds = 0x4AC1`; physical `= 0x4AC10 + off`).
  Verified against the live 1 MB RAM dump (`LOF_DUMP`) **and** the raw EXE:
  `ds:0x5A31 "town of"` -> file `0x45A31` -> physical `0x50641`. The town notes
  previously recorded the wrong name offsets (`ds=0x5A56`/`0x5A5E`, "Cyldane at
  file `0x7C41`"); both are wrong and now fixed. See
  `docs/notes-maintainer-town.md` and `docs/GUESSWORK.md` G-001.
- **G-001 (town name) advanced, not resolved.** The only two town names are
  `ds:0x5A39 "Thyn. "` / `ds:0x5A40 "Cyldane. "` (plus a second "Thyn" at
  `ds:0x687D`). **Neither is loaded by a plain `mov ax, imm`** anywhere in the
  image (searched `B8 39 5A` / `B8 40 5A`), so the selector is computed. The
  town routine at image `0xC22B` loads the message *fragments* `0x5A31`
  ("town of"), `0x5A4C`, `0x5A69` and calls the string drawer
  (`lcall 0x18d:0x16a8`). Next: trace the **caller** of `image 0xC22B` to find
  where the `0x5A39`/`0x5A40` choice is made. Both shipped saves resolve to Thyn;
  a Cyldane save (owner can supply) would expose the other branch (Rule 5).
- **G-002 (16×16 corner patch)** unchanged: still on the watchlist (shared by the
  menu and town frames, no `EGA/**` match, engine-drawn).
- **PR queue (unchanged; not mine to merge):** #99 (Agent B,
  `work/agent-b-itemuse-fix`, draft, **mergeable**) and #89 (Agent A,
  `work/agent-a-blockers`, **mergeable** now — the `docs/BLOCKERS.md` conflict
  was resolved by the lane-split registry). #91 was **closed**. Both open PRs are
  other lanes' work; their authors own merging them.
- **`check.sh` is slow here (~4.5 min per 1600-frame scenario × 11 scenarios ≈
  45–60 min).** `make test` + the `boot` golden hash are the fast checks. Do not
  let a slow `check.sh` block a session; run it once in the background.
  **Verified this session: `./check.sh` → HEALTHY** (full run in the background),
  and the `town` and `shop` oracle golden hashes reproduce exactly.
- **No open owner tasks** at this point (OWNER-TASKS.md empty). Reminder per
  Rule 8: none to re-raise this session. (Rule 5 question raised below.)
- **Added Rule 9 + Rule 10 to `docs/RULES.md`** (owner instruction, 2026-11-14;
  changelog entry added). Rule 9: when the owner states a rule, reconcile it
  against the book — add only if absent, adapt only if the phrasing changes the
  meaning, never duplicate. Rule 10: **agents create, check, push, and merge
  their own PRs; the owner never does repository mechanics.** `docs/HANDOFF.md`
  "Merging your PR" now points at Rule 10 as canonical.

### Open question for the owner (Rule 5)

- **A save that stands the party in `Cyldane`** (or any town other than Thyn)
  would let lane M observe the town-name selection branch and close G-001
  without guessing. If the owner can produce one (play to Cyldane, save, and drop
  the file in `game/GAMES/`), that unblocks it. Until then G-001 stays queued and
  the drop-in keeps the Thyn string (both available saves resolve to Thyn, so the
  current `--town` frame is still correct).

## Session 2026-11-13 (seventh session) — TL;DR

Environment HEALTHY. HEAD `4ee2159`, `main` clean and pushed.

- **Rule 8 added** (`docs/RULES.md`): agents stay in their lane; unowned work must
  be **added to a lane** first; work only the owner can do is an **owner task**
  (`docs/OWNER-TASKS.md`), and lane M re-reminds the owner every session. Wired
  into README/AGENTS/HANDOFF/WORKSTREAMS + `check.sh` + the prompt addendum.
- **Building screens are NOT incomplete.** The old note was wrong: at the golden
  `--frames 1600` the Emporium is fully drawn and its EGA-plane md5 reproduces the
  recorded golden exactly (`shop c9781d20…`). `LOF_DEBUG`'s `mode=13` is
  **decimal** = `0x0D` (EGA 320x200x16); there is **no VGA mode-13h path**.
  `ref/oracle/shop.png` saved. The Emporium reads **no new files** (redraws from
  `ROST/ROST`+`ROST/SHOP`+built-in art).
- **G-001 (town-name "Thyn")**: not a plain string reference — the routine loads
  `0x5a31` / `0x5a4c` / `0x5a69` (plural-form / `(C)ontinue` / etc.) into AX and
  calls the string drawer; the singular "Thyn" is selected further up (not yet
  traced). Kept queued; needs a focused decompilation pass.
- **G-002 (16x16 corner patch)**: shared by the *menu* and *town* frames (so not
  a town-specific sprite), matches no `EGA/**` asset at any offset, and differs
  from `MSCR` in both DOSBox and the emulator → engine-drawn residue. Kept on the
  watchlist.
- No open owner tasks (nothing only the owner can do right now).
- **Next (lane M):** implement the Emporium/Temple screen in the drop-in
  (`--shop` / `--building`), decoding the ornate panel from `ref/oracle/shop.png`;
  and finish G-001 by tracing the singular town-name selection.

## Session 2026-11-12 (sixth session) — TL;DR

Environment HEALTHY. HEAD `97c9167`, `main` clean, 0 ahead / 0 behind.

- **Town screen decoded + shipped** — `./lof --dir game --town`.  Composition:
  `EGA/UP/MSCR` + `EGA/TWN/TWN00` at (10,11) + the engine message/building menu
  in `CHR1` at x=139 (rows 92..188).  Every glyph template-matches a `CHR1`
  cell exactly; recovered two unmapped cells (`.`=band1 cell 34, `?`=band1
  cell 35) now in `port/font.cpp`.  Static match 99.2% vs the emulator frame
  (`ref/oracle/town.png`); residue = a 16x16 top-left patch + the dynamic party
  panel.  Guarded by `tests/port_town_test.py` (wired into `make golden-port`).
  Commits `921665a`, `44f38fb`, `97c9167`.
- **Open PRs (both other lanes' DRAFTS; left to their authors):** #99
  (`work/agent-b-itemuse-fix`, BLK-B003, **merges cleanly**) and #89
  (`work/agent-a-blockers`, **conflicts** on `docs/BLOCKERS.md`).
- **Next session (lane M):** the building screens (Emporium/Temple/…).  The
  `shop`/`temple_bldg` golden scenarios reach them, but a 1700-frame capture was
  **partially drawn** — capture with `--frames` ≥3000 and a larger `LOF_KEYGAP`
  first.  Then the 16x16 town corner patch (source unidentified; not any single
  `EGA/**` asset).

## Session 2026-11-12 (fifth session) — TL;DR

Environment verified HEALTHY (`./check.sh` → HEALTHY at the end of the session).

- **BLK-A002 RESOLVED — the plane-0 bug was a dropped zero-byte port write.**
  Root cause: `Machine::port_out16` only drove the paired EGA data port when
  `AH != 0` (guard `if (v & 0xFF00)`), so a 16-bit `out dx, ax` that set a
  register to **0** was silently dropped and the register kept its old value.
  The game's plane blitter cycles GC *Read Map Select* `0,1,2,3` / SEQ *Map
  Mask* `1,2,4,8` across four passes (`mov al,idx; mov ah,data; out dx,ax`);
  the **first** pass sets Read Map Select **0**, that write was dropped, so the
  pass read the still-selected plane 3 and copied it into plane 0. Hence plane 0
  of *every* frame was wrong while planes 1–3 were perfect.
  - How it was found: scanning each plane for its best base offset in the
    emulator's plane memory — planes 1–3 matched the oracle at base `0x0000`
    (100%), plane 0 matched best at `0x1F40` (88%). The earlier "double buffer"
    probe (`A1F40`) was a *symptom* of this bug, not a renderer/page bug.
  - Fix: always write the data byte (a zero data byte is a legitimate value).
  - Verified against `ref/oracle/menu.png`: **all four planes 100.00%** (plane 0
    80.29% → 100.00%; overall 80.29% → 100.00%). Golden hashes re-recorded
    (`tests/golden.txt`). Recorded in `AGENTS.md` (do-not-regress) and
    `docs/blockers/A.md` (BLK-A002 → Resolved). PR **#98** (merged).
- **Play-screen check:** the play oracle (`ref/oracle/play.png`) still differs
  (overall 0.7787, planes 1–3 ~98% not 100%), but that is a **different
  animation/game-state frame**, not a plane bug — the diff is spread over the
  dynamic viewport + party region, and the menu oracle (same emulator run, one
  screen earlier) is byte-exact. Do not chase it as an emulator bug; if it
  matters, regenerate the play oracle from the same save/animation phase.
- **Open PRs left as-is (not mine to merge):** #89 (Agent A, `work/agent-a-blockers`)
  and #91 (Agent B, `work/agent-b-itemuse-static`). Both are DRAFT and both
  conflict on `docs/BLOCKERS.md`; #91 also carries the disputed item-use offset
  (see `docs/blockers/B.md` BLK-B003 + the maintainer addendum). They need their
  owners to rebase/renumber; do not merge them without that.

## Session 2026-11-12 (fourth session) — TL;DR

Environment verified HEALTHY (`./check.sh`: repo, game data, both builds,
`make test`, oracle goldens, drop-in goldens, clean tree all ok).

- **BLK-A002 reframed — it is an EGA plane-0 (blue) bug, not the palette.**
  Re-derived the emulator's index image from its own plane dump (`LOF_EDUMP`)
  and compared to the oracle's index image (oracle RGB inverted through the
  standard palette; all 64000 oracle pixels invert cleanly): planes **1/2/3 are
  byte-identical** (100.000%), plane 0 matches **80.29%**, 12616 pixels differ
  and **every** difference is plane-0-only. So re-palletting the dump can never
  exceed ~80% (the index image itself is 80.3% right — exactly the figure the
  previous "standard palette" comparison reported). The wrong palette (ramp
  `00..07 10..17`) is a **separate cosmetic** issue. Engine blits one plane at a
  time (`119D:0184` cycles GC Read Map Select + SEQ Map Mask; `119D:0F90` /
  `119D:1250` `rep movsb` plane 0 under mask `0x01`); prime suspect is the
  `gc_latch` handling in `src/ega.cpp` `Ega::write`. **Emulator-only** — the
  drop-in is correct (99.8% vs the oracle); do not change it. PR **#94** (docs
  only) merged. Detail: `docs/blockers/A.md` BLK-A002 and
  `docs/notes-maintainer-town.md` ("BLK-A002 CORRECTED").
- **Next step for BLK-A002:** instrument `gc_latch` around the plane-0
  `rep movsb` (which write mode is in effect for the `119D:0184` loop) and fix
  the plane-0 write path; verify against `ref/oracle/menu.png`.
- No code changed this session (diagnosis + docs only). Two agent drafts remain
  open and unmerged (#89 needs a rebase/renumber; #91 has the conflated-offset
  problem noted above).

## Session 2026-11-12 (third session) — TL;DR

Environment verified HEALTHY (`./bootstrap.sh` + `make test`). Picked up the
in-progress **BLK-B002** (was BLK-005) and closed it.

- **BLK-B002 resolved — the "runtime item-use table" is the static spell table.**
  A live 1 MB dump of the town path (new `LOF_SEGDUMP` hook) is byte-identical to
  the raw `LOF.EXE` image at `0x3EF32`: the `es:[idx*0x24+0x2712]` table is the
  141-record spell table, already transcribed correctly in `port/char_spells.inc`.
  No live-engine dump was ever needed. Full evidence:
  `docs/notes-maintainer-itemuse.md`. PR **#93** (merged).
- **BLK-B003 opened (Agent B).** `port/char_itemuse.inc` / `tools/spec.py
  emit_itemuse` extract from *file* `0x42712` = image `0x3D222` = `ds:0xa02`,
  which **no instruction references**; `item_use_flags()`/`item_is_usable()` are
  therefore built on bytes the engine never reads. Item state comes from the
  32-byte item record (`es:[bx + 0xfda]`, `shl bx,5`).
- New emulator hook `LOF_SEGDUMP=<file>,<seg>:<off>:<len>` (dump any linear
  range) and `ds=`/`ss=` added to the `LOF_DEBUG` line; documented in `AGENTS.md`.
- Note: `check.sh` is slow here (~11 golden scenarios, some 1600 frames) and can
  exceed a 10-minute command timeout; `make test` + the `boot` golden hash are
  the fast checks.
- **Two open PRs reviewed (not merged — they need fixes):**
  - **#91** (Agent B, `work/agent-b-itemuse-static`): also claims BLK-B002 but
    with a **conflated offset** — it treats "image `0x42712`" as "ds `0x2712`".
    The MZ header is `0x54F0`, so image `0x42712` is really `ds:0x0A02`, and the
    `es:[bx*0x24+0x2712]` read lands on the **spell** table. Its relocation
    analysis is real but for image `0x3D222`. Left a review comment asking it to
    retarget; detail in `docs/notes-maintainer-itemuse.md` (addendum).
  - **#89** (Agent A, `work/agent-a-blockers`): adds its own `BLK-006`/`BLK-007`,
    which collided with `main`'s `BLK-006` under the old single-counter scheme.
    **Resolved structurally** this session: the registry is now split one file
    per lane with lane-lettered IDs (`docs/blockers/{A,B,M}.md`), so no two lanes
    can collide. Left a comment on #89 to rebase and renumber its entries to the
    lane scheme (`BLK-A...`).

## Session 2026-11-11 (second session) — TL;DR

Environment verified HEALTHY (`./check.sh`). No open PRs at start. This session:

- Diagnosed BLK-A002 (was BLK-002): the **emulator's whole play-loop palette is the wrong
  ramp** (see item 2 below and `docs/BLOCKERS.md`). The drop-in is already
  correct; do not change it. PR #90 (docs only).
- Corrected the palette item in this handover (it carried the superseded
  "dungeon viewport RGB" framing).
- Slot-2 load remains genuinely slow (a full `--dir game` play run exceeds
  several minutes); `--frames 100` is ~18 s. Use `LOF_NO_TIMER=1` and small
  frame counts for triage.

## TL;DR — what changed this session

The previous maintainer's work was **unlanded**: 48 commits on
`work/maintainer-town` (PR #50, still a draft) on top of a `main` that had moved
on by 28 commits. Nothing was lost, but the tree everyone builds from (`main`)
did **not** have the save-load fix, so it could not reach the in-game screen.

This session landed it and cleared the PR queue. `main` now contains:

| PR | What | State |
|----|------|-------|
| #50 | The maintainer's play-loop work (see below), merged with `main` | **merged** |
| #82 | Workstream A: dungeon-screen oracle check + SPEC-index blocker resolved | **merged** |
| #85 | Workstream B: character-sheet gold/rations fields + weight base | **merged** |
| #86 | `check.sh`/`bootstrap.sh` hardening (this session) | **merged** |

`main` tip: `3cfce11` (then #86's squash). No open PRs remain.

## What PR #50 actually contained (now on `main`)

It was labelled "REP CMPS/SCAS fix" but is the whole play-loop milestone:

- **Emulator (the oracle):**
  - `REP CMPS/SCAS` now leave the remaining count in `CX` (`c.cx--`, not
    `c.cx = 0`). Without it the MS-C `strlen` returns 0xFFFE and a `sprintf`
    overflows a stack local, `retf`-ing to `0000:0000` on save load.
  - DOS 8.3 name resolution matches space-padded names (`GAME  2` -> `GAME2`),
    so the engine can open `GAMES\GAME%3d` saves. **This was the real save-load
    blocker**, not the allocator.
  - Adaptive key feed (`LOF_KEYAUTO`/`LOF_KEYGAP`/`LOF_KEYLOG`), exact-frame key
    scheduling (`LOF_KEYAT`), INT 21h AH=0Ch, clock override (`LOF_HOUR`).
  - New golden frames: `temple` (slot 1) and `play`/wilderness (slot 2).
- **Drop-in (`port/`, the deliverable):**
  - Play screen (`--play SUB/DPNNN`): `EGA/UP/MSCR` + first-person viewport tile
    at (10,11) + the full six-member party panel from the save (panel is
    **pixel-exact**; whole screen ~95% vs DOSBox, residue = dynamic overlays).
  - Interactive front-end (`--ui`, `--ui-keys`): title -> credits -> menu -> `O`
    loads a save -> play screen; `Q`/Esc quits.
  - Walk loop (`--walk`, `--walk-keys`): arrow keys step/turn over the decoded
    dungeon map.
  - VOR preloader (`EGA/IN/DRAG` + `EGA/IN/CRED`), pixel-exact.
  - Correct standard EGA attribute palette; the compact small-caps panel face.

### Merge-conflict resolution (recorded so it is not re-litigated)

Merging current `main` into the branch hit three conflicts:

1. `tests/char_test.cpp` — took `main`'s renamed inventory fields
   (`equipped`/`condition`), and pointed the wilderness-inventory assertions at
   `sv2` (GAME2) because the maintainer renumbered the save slots.
2. `port/char.h` — both sides added a `kCharProf1b` enum; removed the duplicate.
3. `docs/BLOCKED-agent-a.md` — kept the maintainer's "UNBLOCKED" text.

## Save-slot layout (important — differs from the raw archive)

The maintainer renumbered the shipped saves: **slot 1 = the Temple save** (root
`GAME1`, installed by `bootstrap.sh`), **slot 2 = the wilderness save**
(`GAME2`). The archive's own `GAMES/GAME1` is a *different* file (byte
0x9a2/0x9a4: root = 5,0; archive = 0,1). `bootstrap.sh` installs the committed
root `GAME1` as slot 1, so do not "restore" the archive copy over it.

## Verification status

- `make test` green (cpu / font / dungeon / char).
- `make golden` green (boot / look / open / add / dism / erase / temple / play).
- `make golden-port` green (menu / play / ui / walk / vor).
- `./check.sh` → HEALTHY (run again at the start of your session; see below).

## Environment (fresh/reset workspace)

`./bootstrap.sh` now installs the Python deps the frame tests need
(`numpy`, `Pillow`) and warns if SDL2 dev files are absent. If `check.sh` ever
prints `golden-port failed` with no other clue, read
`/tmp/lof_golden_port.log` — a missing dep or a build error lands there.

## What is genuinely open (next session's work)

Pick a workstream from `docs/WORKSTREAMS.md`. The highest-value open items:

1. **First-person viewport composition (workstream A).** The drop-in's `--walk`
   viewport is still the static `DP001` tile; the engine composites per-cell
   sprites (full views + smaller side pieces). Ground truth: `tools/dngs.py
   oracle` scores a composite against `ref/oracle/dungeon_slot1*.png`. The
   spawn view is `DP001` (0.952); the *moved* view matches no single sprite
   (best 0.576), confirming a composite. See `docs/notes-a.md`.
2. **Emulator play-loop plane 0 (workstream A / BLK-A002).** *Reframed
   2026-11-12; the "palette" framing was a red herring.* The emulator's frame at
   the menu, play and temple differs from the DOSBox oracle **only in EGA plane 0
   (blue)**: comparing the emulator's own index image (`LOF_EDUMP`) to the
   oracle's index image (oracle RGB inverted through the standard palette), planes
   **1/2/3 are byte-identical** and plane 0 matches 80.29% — 12616 differing
   pixels, every one plane-0-only. The emulator *also* ends up with the wrong
   attribute palette (ramp `00..07 10..17`), but that is a **separate cosmetic**
   issue: re-palletting the dump cannot exceed ~80%, because the index image
   itself is only 80.3% right (that is exactly the "standard palette" figure the
   old note reported, so the old "only the palette is wrong" conclusion was
   wrong). The engine blits one plane at a time (`119D:0184` cycles GC Read Map
   Select + SEQ Map Mask; `119D:0F90`/`119D:1250` `rep movsb` plane 0 under mask
   `0x01`), so the bug is in the plane-0 write path — prime suspect the
   `gc_latch` handling in `src/ega.cpp` `Ega::write` (write modes 1/2 take the
   value from the latches). **Emulator-only:** the drop-in (`port/gfx.cpp`) is
   correct (99.8% vs the oracle) — do not change it. Full evidence:
   `docs/blockers/A.md` BLK-A002 and `docs/notes-maintainer-town.md`
   ("BLK-A002 CORRECTED").
3. **Font glyphs (resolved — `BLK-M001`).** The engine blits `CHR1`/`CHR2`
   cells directly (no bold/double-struck transform); the port already does, and
   its menu text is pixel-exact. The "bold, double-struck" note was an artefact
   of the wrong-palette era. See `docs/blockers/M.md` and `AGENTS.md`.
4. **Combat / shop / item-use (workstream B).** *Updated 2026-11-12.* The
   "runtime item-use table" is settled — it is the **static spell table**, already
   transcribed (BLK-B002, resolved; PR #93). What remains for B is **BLK-B003**:
   `port/char_itemuse.inc` is mis-sourced (image `0x3D222`, unreferenced) and
   must be re-derived from the item record or dropped. Shop stock/prices still
   want a scripted shop scenario (`--keys`/`LOF_KEYAT`). See `docs/notes-b.md`
   "Blockers" and `docs/notes-maintainer-itemuse.md`.

## Hygiene

- `game/`, `downloads/`, `GAME1` are git-ignored (proprietary); restore with
  `./bootstrap.sh`.
- Commit and push after every meaningful step — the workspace can reset.

### Guesswork audit (2026-11-12, sixth session)

All guesses are now tracked in the canonical register **`docs/GUESSWORK.md`**
(read it every session, per `docs/RULES.md`). The Lane-M findings from this
audit were moved there:

- **G-001** (queued task) — the town name "Thyn." is hardcoded in
  `compose_town_screen()`; the town name is a variable (a string table in
  `LOF.EXE` at file `0x45A1D`, names `"Thyn. "` and `"Cyldane. "`). Derivable →
  normal task.
- **G-002** (watchlist) — the 16×16 town top-left patch; not `MSCR`, not `TWN00`,
  not any single `EGA/**` asset, not a shift/wrap of either. Removability unclear
  → revisit until clear.
- **E (not a guess)** — the RNG is seeded from the BIOS tick counter
  (`analysis/lof.asm` image `0x47A6`, `int 0x1a` then `add cx,dx`), so combat is
  non-reproducible by design; listed so nobody mistakes it for a guess.
- **Party-panel live state** (time of day, morale, rations) is engine state a
  save does not carry; the port deliberately draws the labels only. This is
  correct-by-construction, not guesswork.

### 2026-11-23 — G-019 resolved (potion = spell-cast); oracle + P-4/P-5 scoping

- **G-019 (heal/poison magnitude) RESOLVED as spell-based** (commit `a9059bd`).
  Runtime `LOF_DUMP` (menu + shop) pinned the data layout: DS base `0x4ac10`,
  item table at `DS:0x2cec` (linear `0x4d8fc`), runtime record == file layout.
  The **buy** core (`0x63f4`) reads price via `es=[0xaf5e]` at `[bx+0xfce]`
  (price = record +0x12; Healing potion = 350).  The action-3 "use object" path
  (`0x17c04`) reads `item.f4_low` (`[bx+0xfd4]`) = a **1-based spell index**
  (Healing potion f4=80 = spell 80 "Healing I") and looks it up in the spell
  table (`es=[0xafae]`, `es:[spell*0x24+0x2712]` = block byte 0).  So a potion
  **casts its spell**; the effect-10 `hp += [bx+0xfd5]` (f4 high = 0) is a
  no-op path.  The `charges` (f3 high) stand-in stays until spell-casting
  exists.  `[0xaf5e]`/`[0xafa8]` are transient segments (hold code overlays at
  rest), which is why a static frame dump can't read them.
- **DOSBox is unavailable here** — `apt` has no package index and the shell is
  unprivileged, so `tools/capture_dosbox*.sh` can't run.  That blocks the two
  oracle-dependent polish items: **P-13** (all-out message colour) and **P-14**
  (VOR credits-page palette).  The analysis emulator still works for non-oracle
  experiments (P-12 depth sprites, P-15 synthetic-map overflow).
- **P-4 gap**: `--ui` (`run_ui`) has INTRO -> PLAY -> DECISION -> RESULT only.
  `compose_town_screen` and `compose_amulet_screen` exist but are **not wired**
  into `--ui`; the town-exit -> amulet -> wilderness transition is the missing
  piece.  Wiring it is the concrete next P-4 step (no oracle needed).
- **P-5 (spells)**: the cast path is at image `0x19690` (block-byte-0 flag test:
  bits `&4`, `&1`, `&0x10`) with the effect/target work after; the 14-byte
  spell block is `[flags, ...]` per `char_spells.inc`.  This is the 12-20h task;
  it also fixes potion magnitudes, so it is the highest-value next target.
- **P-11 (`P`/`B` round flags) progress 2026-11-23:** the `P` (shield/armour
  damage) guard is now derived from the big melee routine at image `0x1370C`:
  `roll=rand(100)+1`, threshold `=100-row[+0x07]`; on `roll>threshold` and
  `[bp-0x5a]!=0`, damage the shield (if Dex `char[+0x46]>0xb` and a shield is
  equipped) else the armour, each `slot[+0x85]-=2` + set the `P` flag.
  `[bp-0x5a]` clears when the wielded item's `f11==0x15` (`0x16062`).  Not yet
  ported (the extra `rand(100)` would shift the pinned RNG sequence); `B` still
  unlocated.  Details in GUESSWORK G-012 3c.
- **P-5 (spells) reconnaissance 2026-11-23:** the cast *decision* is at image
  `0x19690` in the combat round: spell record byte 0 (`es:[spell*0x24+0x2712]`)
  is a flag byte — bit 0 or 2 (1|4) -> pick a target (`0x212f:0x789c`) and set
  `char[+0xd]=target, +0xe=spell, +0xf]=4` (action "Cast Spell"); bit 4 (0x10)
  -> auto-target; otherwise the enemy-target picker `0x18cea` runs.  The spell
  record is **0x24 bytes at base 0x2712**; byte 8 (effect code, already the
  port's `kSpellEffectByte=8`) is at `0x271a`.  The *effect dispatch* is image
  `0xf5c0`: read byte 8, `-1`, index a **63-entry jump table at `cs:0x2c1e`**
  (`cmp ax,0x3e; jbe; add ax,ax; jmp word cs:[bx+0x2c1e]`, `0xf5df..0xf5ed`).
  Effect 1's handler (`0xf5f2`) does `sub hp_max(+0x20),5` then clamps
  `hp_cur(+0x22)<=hp_max` — i.e. a drain/curse.  Target selection (`0x11dd6`)
  reads byte 6 (`0x2718`) as a range and draws `rand(range)+1` to pick.  **Next
  step:** the effect dispatch is a **switch**, so its handlers are inline
  case-blocks (no `push bp` prologues) — that is why no table candidate yields
  function starts.  Tried two cs bases statically and neither is right:
  `cs:0x2c1e` as image `0x2c1e` gives data, and as image `0xf5c0+0x2c1e`
  (`0x121de`) gives a table whose entry 15 (effect 16) points *into* the
  dispatch instruction (`0xf5f0`) — invalid.  So the code-segment base must be
  captured at runtime: set `LOF_TRACE_IP=1F6C0:1F6D0` (linear of image `0xf5c0`
  = image+0x10100) while a caster casts in combat, read the real `cs`, then the
  table is `cs:0x2c1e`.  **SOLVED 2026-11-23:** the cs base is image `0xd9f0` (the routine is at
  `cs:0x1bd0`), so the jump table is image `0x1060e` and every one of the 63
  entries lands on a valid inline case-block.  Effect 15 (healing) -> `0xf848`:
  `heal = spell.data[6]` capped at hp_max.  Most codes (3-6,8,10-13,16-18) jump
  to `0xf826` (a bare `sub ax,ax; pop si; mov sp,bp` = no-op return); others:
  1->`0xf5f2`, 2->`0xf6b8`, 7->`0xf762`, 9->`0xf7da`, 14->`0xf7f6`.
- **P-5 effect layer implemented 2026-11-23:** `apply_spell_effect(SaveChar&,
  const SpellRecord&)` dispatches on the spell's effect byte (data[8]) and
  applies the grounded handlers — **15** heal `data[6]` capped (0xf848),
  **1** drain `hp_max`−5 / clamp `hp_cur` (0xf5f2), **14** set `+0x12`=6
  (0xf7f6).  Exposed as `lof --cast N SPELL` (writes the save like `--use`).
  `use_item_effect` case 10 now routes a healing-spell item through it.
  Pinned by `char_test::test_apply_spell_effect`.  Effects 2/9 set visual
  globals (skip); 7 doubles `char+0x4e` (field `kCharSeed4e`, meaning uncertain
  — deferred).  No shipped spell uses effect 1 (only 14: spells 25/75, 15:
  healers); the drain path is exercised synthetically.
- **P-5 cast-flow MP (2026-11-23):** current/max MP are `+0x69`/`+0x68`
  (byte).  A full scan of the image finds only MP *additions* —
  `add es:[bx+si+0x69],al` at `0x8646` (rest), `0x98be`, `0x25d22`, and
  `inc/add es:[bx+si+0x68]` at `0x97fc`/`0x98d6` (level-up) — **no `sub`/`dec`
  of either field**.  So the cast's MP *cost* is not a direct field write
  reachable statically; the "tries to perform magic, but can't." strings
  (`0x43140`, `0x43330`) gate it but the deduction path/mechanism needs a
  runtime trace (or the spell-selection routine).  `--cast` therefore applies
  the effect without an MP cost for now; the cost is the next P-5 sub-task.
- **P-5 cast routine located 2026-11-23 (image `0xf240`..`0xf3ad`):** it reads
  the acting char's memorised-spell slot `+0x142` (spell index; the paired
  `+0x143` holds the spell's level = record byte 9, filled by the rest
  memorisation pass at `0x841e`), calls the **effect dispatch `0xf5c0`** with
  `(spell, ?, ?, ?, char)` to apply the effect, then computes a *practice-XP*
  value and adds it to `char+0x6c` (32-bit XP): roughly
  `((v*100)/(x+1) + 100) * (level/15)`, where `x = 0x212f:0x93d6(char)` and
  `v` comes from a profession table (`[0xafb2]:[prof->+0xbf2]`,
  `[0xafb4]:[->+0xc0e]`) subtracted from `[bp-0x10]`.  **No MP is deducted** —
  `+0x68`/`+0x69` are only ever *added* (rest / level-up).  So MP is not a
  casting cost; casting's functional core is the effect dispatch, which
  `apply_spell_effect()` already implements.  Remaining cast-flow work is the
  UI integration (decision action-4 -> spell selection -> round resolution) and
  the XP award formula.
- **P-5 memorised-spell slots 2026-11-23:** char `+0x142+2n` (spell index) /
  `+0x143+2n` (level) are the character's memorised-spell slots, **not** "skill
  pairs" (the earlier label).  Verified from saves: GAME4 Lara = `(80,10)`
  Healing I, `(81,8)` Light, `(82,8)` Fear evil, `(83,7)` Blessing; Gandalf =
  `(1,8)` Burning hands, `(2,4)` Light, `(3,7)` Magic arrow, `(4,5)` Shield I.
  The stored level equals the spell record's byte 9.  Exposed as
  `spell_slots()` (port/char.{h,cpp}) + `lof --spells N`; pinned by
  `char_test::test_spell_slots`.  The class-seed still fills these from
  rec[0x3b]/[0x3c] (starting spells per class).  This is the caster's spell
  list the cast routine (0xf240) consumes.
- **P-5 round casting 2026-11-23:** `Combatant::spell` (set from
  `spell_slots()[0].index` in `build_party_combatants`) + `cast_combat_spell()`
  in `resolve_round`: a member whose action is `kActCastSpell` casts its slot-0
  spell instead of meleeing — effect 15 heals the most-wounded living ally by
  `data[6]` (capped at max_hp), effect 1 drains a defender's max_hp by 5.
  Pinned by `combat_test::test_cast_round` (Lara casts Healing I on a 5/40 ally
  -> 13; caster does not melee).  `combat_test` now links char.cpp + data.cpp
  (it calls spell_by_index).  Still open: the interactive `--ui` decision ->
  spell-slot/target selection, and the practice-XP award.
- **P-5 --ui cast selection 2026-11-23:** at the `--ui` combat DECISION state a
  digit `1`-`6` toggles that (present) member's action to `kActCastSpell` (its
  slot-0 memorised spell) or back to attack, logging `lof: <name> will cast
  <spell>`; any other key commits the round.  Driven head-lessly via
  `--ui-keys "32,32,79,67,51,13,81"` (space,space,O=play, C=combat, 3=Lara
  casts, Enter=commit, Q=quit) with `--save GAME4`; pinned by `port_ui_test`
  (`cast ok`).  Still open: choosing slot/target beyond slot 0, and the
  practice-XP award.
- **P-5 combat round-trip 2026-11-23:** the `--ui` DECISION handler now writes
  each round's HP (`+0x22`) and practice XP (`+0x6c`) back into `play_sg`, so the
  next round starts from the real (wounded) state instead of the save's full HP.
  Before this, every `C` rebuilt `combat_party` from the unchanged save, so combat
  never accumulated.  `--ui-encounter C:HP` (default `2:8`) overrides the pinned
  encounter for testing.  Logs `lof: combat start: <name> hp <n>` /
  `lof: round end: ...`.  Pinned by `port_ui_test::check_round_trip` (GAME2 vs
  2x200: Decker 10->8 round 1, round 2 starts at 8 then 8->6).
- **P-5 cast-charge consumption 2026-11-23:** disassembling the cast path (image
  `0x12A1E`, `dec es:[bx+0x143]` then `cmp ...,0; mov es:[bx+0x142],0`) shows
  `+0x143` is the **remaining-cast count**, not a spell level: it is decremented
  per cast and the spell is forgotten when it hits 0.  It is initialised to the
  spell record's byte 9 by the rest-memorisation pass (so Lara's Healing I = 10
  casts, not "level 10").  `consume_spell_slot(SaveChar&, slot)` implements it and
  is wired into the `--ui` cast path (the `Combatant` slot-0 cast consumes
  `play_sg.chars[m.index]`'s slot 0).  `SpellSlot.level` renamed to `charges`.
  Pinned by `char_test::test_consume_spell_slot` and
  `port_ui_test::check_cast_charges` (Lara casts Healing I twice: 10->9->8).
- **P-7 level-up / training 2026-11-23:** decoded the level-up routine (image
  `0x9144..0x96E6`).  Entry `0x9144` increments the level (`+0x17`), then prints
  it; `0x9164` adds `rand(10)+1` to HP current (`+0x22`) and max (`+0x20`); four
  capped-100 fields `+0x30/+0x32/+0x34/+0x36` increment; then `+0x45` (Str),
  `+0x46` (Dex), `+0x48` (Wis) increment capped at 19 (`0x13`).  Implemented as
  `level_up(SaveChar&, hp_gain)` (returns the new level + improved attributes) and
  `lof --train N HP [--amulet-out]`.  Pinned by `char_test::test_level_up`.  The
  gold cost and the House-of-Trades `(T)raining` menu trigger are NOT wired yet
  (the routine itself deducts no gold — the cost lives in the caller), so
  `--train` applies the effect directly.  This is the stat-gain body shared by the
  level-up and the paid training.
- **P-5 XP persistence verified 2026-11-23:** the combat-start / round-end logs
  now carry the party member's 32-bit XP (`+0x6c`), so `port_ui_test::check_round_trip`
  verifies BOTH HP and XP persist across rounds and that XP actually accumulates
  (party damage -> `m.exp += damage * xp_mult`, image 0x14D80).  GAME2 vs 2x200:
  Decker hp 10->8 and exp 379->389 in round 1, round 2 starts at 8 / 389.
- **P-7 training gold cost — BLOCKED on emulator capture 2026-11-23:** the
  level-up body (0x9144) deducts no gold; the training cost lives in its caller,
  which the traced decompilation never reached (the training menu was not
  exercised during the `analysis/lof.asm` capture), so the cost formula is not in
  the asm.  Driving the emulator to the House `(T)raining` (keys
  `0x1e,0x03,0x2e,0x2e,0x23,0x14,<digit>`, i.e. add-char -> leave inn -> H -> T
  -> char) timed out at 200s without reaching the routine at `LOF_TRACE_IP=19000:19260`
  — the run needs a longer ceiling / adaptive keys.  Next: capture the gold
  (`+0x72`) delta with `LOF_WATCH`/`LOF_CAP` across the training, then wire the
  cost into `level_up` callers.  Gold is per-character (`+0x72` dword).
- **P-7 building party panel — needs a matching save 2026-11-23:** the DOSBox
  oracles (`ref/oracle/house.png` etc.) show a 3x2 party-slot grid in the
  top-right (`y 0..52, x 140..320`): each slot is numbered; an occupied slot
  shows name (small-caps) / profession / status, an empty slot shows `HT`/`FP`.
  The port's `compose_building_screen` draws only MSCR + tile + text, so this
  region is BLANK — `port_building_test` masks exactly it
  (`mask[0:52,140:320]=False`), so the gap is currently accepted.  Reproducing
  the grid needs a save whose party equals the oracle's (slot 1 = "Stiletto",
  Warrior, GOOD; slots 2..6 empty) to verify the occupied-slot rendering; no
  shipped/archived save matches (the oracle party was a bespoke DOSBox capture
  party), so implementing the layout blind would be a guess.  Next: build such a
  save (or re-capture the oracle with a known save) then draw the grid.
- **House-of-Trades training nav — needs a valid party 2026-11-23:** driving the
  emulator with `LOF_KEYAUTO=0x1e,0x03,0x2e,0x2e,0x23,0x14,0x02` (gap 250 and
  450, 2600/4000 frames) does NOT reach the House: the screenshots land on the
  add-character 6-slot screen ("hit 27" in every box) with no viewport tile or
  House text, i.e. the add-char flow (0x1e,0x03) does not produce a party that
  the town/`H` navigation can use.  `ref/oracle/house.png`'s party (Stiletto) is
  not in any save.  To capture the training gold cost, first get a real party in
  (load a save that starts in town, or finish the add-char flow), then `H` -> `T`
  -> digit, watching `+0x72`.
- **P-7 building party grid DONE 2026-11-23:** the town/emporium/bank/house/temple
  screens now overlay the SAME 3x2 party grid the play screen uses.  Refactored
  `draw_party_grid` out of `draw_party_and_status` (grid = name/prof/+0x1e%/
  status at cols 148/208/268 rows 9/37, plus the fixed 50x3 white HP gauge at
  (x,y+18) that every oracle shows full-width regardless of HP).  `compose_building_screen`
  and the `--ui` town/building paths now pass the loaded save.  A synthetic save
  (GAME2 with slot 1 = Stiletto/Warrior/10%/Good, slots 2-6 empty) renders
  `--house` PIXEL-EXACT (1.0000) vs `ref/oracle/house.png`; the play screen's
  match rises 97.2% -> 98.6%.  New test `port_building_test::check_building_party_panel`;
  `port_ui_town_test` now refs buildings with `--save`.
- **Party grid on the prompt screens 2026-11-23:** extended `draw_party_grid`
  to the temple heal/revive prompt, the Inn rest prompt and the Inn "who pays"
  prompt (they share the MSCR + TWN03/TWN05 chrome and the oracles show the same
  grid).  These oracles use bespoke parties, so the exact grid can't be pinned
  from the shipped saves; the grid structure matches (same 1029-px layout).

## House-of-Trades training — DOSBox captures 2026-11-24

DOSBox is available again (apt `dosbox 0.74-3` installs cleanly on Debian 13;
`tools/capture_dosbox.sh` still reproduces the menu oracle at 1.0000).  Used it
to finally pin the training step that had been blocked.

**Navigation.**  `O`+`2` loads GAME2 (wilderness DNG10); a random encounter
spawns on load.  `W` withdraws, then `Down`,`Up` walks into the town of Thyn
(the walk is RNG-flaky — an encounter can divert it to the Temple dungeon).
Town `(H)ouse of trades` → `(T)raining` → `(N)` selects member N.

**Training cost is PROFESSION-dependent (level-independent).**  Measured by
patching a save's char level/exp/gold and **re-sealing** the save checksum
(the alternating ADD/XOR fold over `[0,0x2e3a)`, seed 1, NUL=1 — patching
without reseal makes the engine print "GAMES\GAMEn is not a valid save game!"):

Full 12-profession table, measured by patching the profession byte (+0x1b) and
re-sealing (the panel shows the new profession, the fee follows it):

| prof | name | cost | 2 stat gains shown |
|---|---|---|---|
| 0  | Warrior     | 2000 | (captured) |
| 1  | Barbarian   | 2000 | |
| 2  | Rogue       | 1500 | Trap detecting, Stalking |
| 3  | Smith       | 1500 | Attack, Attack |
| 4  | Scout       | 2000 | |
| 5  | Priest      | 2250 | |
| 6  | Druid       | 2250 | |
| 7  | Magician    | 2250 | Concentration, Concentration |
| 8  | Illusionist | 2250 | |
| 9  | Paladin     | 2000 | Attack, Defence |
| 10 | Healer      | 1500 | Concentration, Concentration |
| 11 | Monk        | 1500 | |

Grouped by fee: {2,3,10,11}=1500, {0,1,4,9}=2000, {5,6,7,8}=2250.
Decker (Paladin) cost 2000 at level 1→2 **and** level 5→6, so the fee is
level-independent.  Implemented as `training_cost(profession)` in
`port/char.{h,cpp}` (pinned by `tests/char_test.cpp::test_training_cost`).
(An earlier reading of Magician/Healer as 22500/15000 was a 10x misread — the
font's "0" carries a diagonal stroke; the zoomed captures confirm 2250/1500.)

**The level-up gains COMBAT SKILLS, not Str/Dex/Wis.**  The messages are
`The Grand Board of Trades decides that <name> enters the <N> Rank.` then
`<name> profits from training, <skill> improves!` (twice, one per gained
skill).  The engine's level-up routine (image `0x9144..0x96E6`) increments the
level (`+0x17`), both HP words (`+0x20`/`+0x22`) and the four capped-100 bytes
`+0x30`/`+0x32`/`+0x34`/`+0x36` (each `cmp byte,0x64; inc`), and **never writes
the attribute bytes `+0x45`/`+0x46`/`+0x48`** — so Str/Dex/Wis are *not* gained
on level-up.  The port's `level_up()` now matches this (it previously also
incremented Str/Dex/Wis, which was a misreading).  Reproducing the two
`<skill> improves!` messages still needs the per-character source of the two
active skill *names* (the four `+0x30..+0x36` bytes are most likely two skills ×
(value, progress), which is why two — not four — messages appear); tracked as
open.

Captures: `ref/emu/training/` (cost + rank + both "improves!" frames for all
five professions) and `ref/emu/charsheet_rank09.png` (the character sheet with
a patched Rank 09 — confirms the level/exp patches and shows the P-8 layout).

### Training stat gains (observed, level 1->2)

Each training level shows two `<name> profits from training, <skill> improves!`
messages.  Skills seen: Attack, Defence, Concentration, Trap detecting,
Stalking.  Observed set (native chars for 2,3,7,9,10; profession-byte-patched
Decker for 0,1,4,5,8,11 — the patched ones keep Decker's stats, so they may be
stat-influenced and should be treated as a first-level sample only):

| prof | gains (level 1->2) |
|---|---|
| 0 Warrior     | Defence, Attack |
| 1 Barbarian   | Trap detecting, Defence |
| 2 Rogue       | Trap detecting, Stalking |
| 3 Smith       | Attack, Attack |
| 4 Scout       | Attack, Attack |
| 5 Priest      | Concentration, Concentration |
| 6 Druid       | (not captured) |
| 7 Magician    | Concentration, Concentration |
| 8 Illusionist | Defence, Defence |
| 9 Paladin     | Attack, Defence |
| 10 Healer     | Concentration, Concentration |
| 11 Monk       | Defence, Attack |

The exact selection rule (fixed per profession vs rotating per level vs driven
by the char's top stats) is not yet derived.  `level_up()` now matches the
routine (increments HP + the four `+0x30..+0x36` skill bytes, and does **not**
touch Str/Dex/Wis), so the *effect* is faithful; reproducing the two named
`<skill> improves!` messages still needs the per-character skill-index source
(needs the skill offsets + the selection rule), tracked as open.

## EGA/ANIM/AN00..AN53 are combat attack sprites, not a town transition (2026-11-24)

The `docs/notes-b.md` §3c guess that the 24 `EGA/ANIM` frames are a
"leaving-town transition" is **wrong**.  They are six sprite groups of four
frames each (`an{0..5}{0..3}`), each a small EGA sprite (~40-64 px wide x 57
tall): group 0 a knight swinging a sword+shield (four attack frames), group 1 a
staff-bearer, group 2 an axeman, group 3 a robed figure, group 4 a wolf, group 5
a skeleton.  They are **combat attack animations**, preloaded once the party
reaches a place where encounters can occur -- the INT 21h open trace opens them
right after the town exit, ahead of the first wilderness encounter.  The town
exit itself is *instantaneous*: the outdoor map just loads, there is no wipe to
reproduce (the P-4 town exit is DONE without any animation work).  Using these
as the per-attack animation in the port's combat is optional cosmetic polish,
not a transition.

## Character sheet (P-8) findings (2026-11-24)

Investigating the char sheet for P-8 produced four grounded facts:

1. **The palette is NOT custom.**  The char-sheet oracle (`ref/oracle/charsheet.png`)
   has exactly the same 10-colour set as the combat oracle
   (`ref/oracle/combat.png`): light-gray `(170,170,170)`, black, dark-green
   `(0,85,0)`, pale-green `(170,255,170)`, red `(170,0,0)`, yellow-green
   `(170,255,0)`, green `(0,170,0)`, brown `(170,85,0)`, dark-yellow
   `(170,170,0)`, bright-green `(0,255,0)`.  The port already renders the combat
   with these colours (combat match 0.96), so the char sheet can use the same
   standard palette.  The PLAN note "palette lacks the oracle's red/yellow" was
   a misdiagnosis -- it referred to the analysis *emulator's* palette, not DOSBox.
2. **The portrait is a composite of `CHAR1` sprite parts**, not one 120x119
   asset.  Rendering every `EGA/CHAR{0,1,2,3}` image shows the pieces: a bare
   face at `CHAR1/CH67`, a breastplate at `CHAR1/CH68`, plus helmet/arm parts.
   `CHAR3/CH00..CH21` are the full-body class/creature sprites (a knight at
   CH00 etc.), ~119x119 but different art from the close-up portrait.  So the
   portrait is layered, not a single blit.
3. **Layout.**  Left: the portrait (black bg) over the location block (black bg).
   Right: the shared 3x2 party grid (light-gray bg), a decorative divider (the
   dragon-eye strip), then the stat text on the light-gray panel.  Text fields
   are documented in `docs/notes-b.md` section "character sheet"
   (Rnk +0x17, Hit +0x22/+0x20, Magic +0x69/+0x68, State +0x1b profession,
   Str/Con/Dex/Int/Wis +0x45/+0x44/+0x46/+0x47/+0x48, Experience, Weight).
4. **Reach path is still open.**  It is NOT the combat config's `1` (that is
   "Enter Kill-Rank") nor the encounter screen's `1`.  The sheet is a separate
   screen reached by a key not yet pinned; the port should expose it via a
   `--charsheet` direct render (like `--town`) rather than guessing the path.

## P-11 — the `P`/`B` round-result columns stay blank (documented, not guessed)

The round-result melee table has three `*`/`-` columns `W`/`P`/`B`.  `W`
(weapon break) is derived and implemented (`MeleeResult::weapon_broken`,
`weapon_f0`/`weapon_broken`).  `P` and `B` are **effectively unreachable in the
shipped data**, so the port leaves both false and shows the engine's blank
columns — reproducing a value the engine never produces would be a guess.

- **`P` (parry).**  The guard is derived (`0x1756D`..`0x1759D`): defender
  dexterity `+0x46 > 11`, a shield in the armour slot `[bp-0x30]`, then
  `sub slot[+0x85], 2` and `P = 1`, gated on `[0xafc2]:0x3de9 == 0`.  But the
  `P` event runs in the **defender** path, and the only defender with inventory
  is a party member — it needs a *monster attack to land*.  That path is the
  monster-turn gate (P-3), which in every shipped encounter aborts because the
  gate reads the **next row's HP low byte** (`row[+0x16]`), rows are built with
  HP a multiple of 8, so it never equals `0xff` for the shipped 8/16-HP
  Mercenary.  Until P-3 produces a landing monster attack, `P` cannot fire.
- **`B` (untouched).**  Three sites reference it (`0x12846`/`0x13D7D`/`0x1471C`)
  but no derivation ties it to a reachable branch; the flag byte is likely
  `[0xafdc]+0x560A` (beside `W` `+0x5608`, `P` `+0x5609`).

Pinned as a regression by `tests/combat_test.cpp::test_round`
(`P/B columns stay blank`).  Revisit only once P-3 (monster attacks) exists.

## Emporium repair / sell, inn spell-memorisation (2026-11-24)

The **code-segment → flat-image mapping is solved**: a far-call target
`lcall seg:off` is at `img[seg*16+off]` (see AGENTS.md).  With it the runtime
math library decodes (`0x2f0e`): `0x1efe`=32×32 multiply, `0x204a`=unsigned
32÷32, `0x1e62`=signed 32÷32, `0x1dec`=the MS-C RNG (LCG).  This unblocked the
Emporium routines.

**Repair — DONE.**  Handler image `0x6172..0x638a` (the `[bp+6] != 0` branch of
`0x5938`).  `cost = price(+0x12) - (condition(+0x85) * price)/100` (LXMUL then
LDIV by 100).  Magic artefacts (`item.spell` = f4 low byte != 0) are refused
("Oh!, a magic artefact... I can't repair that for you.", `0x4112`); a pristine
item (condition >= 100) needs no repair (`0x419e`); otherwise a payer is chosen
("Who'll pay (1-6)?", `0x421c`), the fee paid via `pay_gold`, and the slot's
condition set to 100 (`0x6378`).  Port: `repair_cost()` / `repair_item()` +
`lof --repair N SLOT [--repair-payer M]`; pinned by `char_test::test_repair`
and the repair check in `port_building_test.py` (GAME2 Small shield 92→100, 1g).

**Sell — still blocked (BLK-B006).**  The `[bp+6] == 0` branch (`0x5C24`):
`base = f(cond, price) - price/25` (floor 1), `bonus = (price/100)*(g(char[+0x25])+1)`,
`final = base + bonus`.  `f` = `0x2f0e:0x1efe`/`0x1e62` (now decoded), but `g` =
`0x2a7d:0x2f6a` calls the RNG `0x1dec` then `0x2f0e:0x1f32` — i.e. the bonus has
a **random haggling** component, and `char[+0x85]`/`char[+0x25]` identities are
unconfirmed.  Do not guess the sell price.

**Inn spell-memorisation — structure decoded, blocked on runtime data.**  Loop
image `0x8416..0x8487`: for each slot `s` in `0..char[+0x6a)`, `school =
0x212f:0x9342(char)`, `spell = [0xaf62]:0xc0e + school*2` (school base) `+ s`,
then the slot's cast count `+0x143` = `spell.data[9]`.  `0x9342` maps the
profession byte `+0x1b` through a 12-entry jump table (image `0x2a6a6`) to a
school id: `{0,1,2,4,9,11} -> -1` (no magic), `3 -> 5`, `5 -> 3`, `6 -> 2`,
`7 -> 0`, `8 -> 1`, `10 -> 4` (elemental schools).  The **school base table
`[0xaf62]:0xc0e` is runtime data** not in the file image, so memorisation stays
unimplemented until a `LOF_SEGDUMP` supplies it.

### Runtime data-table capture (2026-11-24)

`./lof-native --dir game --headless --frames 300 --shot /dev/null` with
`LOF_DUMP=/tmp/lof_mem.bin LOF_DEBUG=1` dumps the full 1 MB linear space and
prints the segment registers.  At the menu the game's `ds = 0x4c92`
(base `0x4c920`), so `ds:X` = `0x4c920 + X`.  The far-pointer variables hold
`[0xaf4a]=0x468e` (the arena) and `[0xaf5e/0xaf62/0xaf64/0xaf68]=0x4c92` (their
tables live in DGROUP itself).  Reading them:
- **profession base table** `[0xaf68]:0xb7a` = 12 words
  `2000,2000,1500,1500,2000,2250,2250,2250,2250,2000,1500,1500` — **identical to
  `training_cost()`** (so the temple XP boost reuses the training-fee table).
- **spell school-base table** `[0xaf62]:0xc0e` = bytes (stride 2)
  `{1,33,59,80,100,118,127}` for schools 0..6.
- **spell table** `[0xaf64]:0x2712`-relative (36-byte records, `data[9]` = casts);
  **item table** `[0xaf5e]:0xfbc`-relative (32-byte records).

The **spell-memorisation pass** is now implemented (`refresh_spell_casts`,
`port/char.cpp`; called by `rest_party`).  It is confirmed for every caster,
e.g. GAME4 Lara prof 5 -> school 3 (base 80) holds slots 80/81/82/83 with casts
10/8/8/7 == their `data[9]`; Laladil prof 6 -> school 2 (base 59) holds 59/60
(8/6); Joanna prof 10 -> school 4 (base 100) holds 100..103; Gandalf prof 7 ->
school 0 (base 1) holds 1..4.  Note +0x6a (the port's `kCharFlag6a`) is, for
casters, the **spell-slot count** the loop bounds on (Lara 4, Laladil 2).

### Level-up derived-stat recompute (2026-11-24)

The level-up function does NOT stop at the skill bytes; it continues (image
`0x96F7..0x9901`, same function, no retf in between) with a derived-stat
recompute, now implemented in `level_up()` (`port/char.cpp`):
- **magic max (+0x68)** = `magicMax[prof][level]` (captured runtime table
  `[0xaf70]:0xa38`, 12 professions x 16 levels) for level<16; then +1 if
  Wisdom(+0x48)>15 and +2 if Intelligence(+0x47)>15 (only when the table value
  is nonzero).  Rows: prof 0/1/2/11 = all 0; prof 3/7/8/10 identical
  `0,1,2,4,5,6,8,10,11,13,14,15,16,18,19,20`; prof 5/6 `0,1,2,3,4,5,6,7,9,10,
  11,12,13,15,16,17`; prof 4 `0,0,0,0,0,0,1,1,2,2,2,2,2,2,2,2`; prof 9
  `0,0,0,1,1,2,2,3,3,4,4,5,5,5,5,5`.
- **magic cur (+0x69)** = the magic max, plus the same Wis/Int bonuses.
- **weight max (+0x64)** = `Str*350` if Str<=15 else `Str*100+3400`.
- **base Con (+0x60)** = `max(+0x60, Con(+0x44))` (this is the divisor in the
  revive XP boost).

Verified to reproduce every shipped character's +0x68 and +0x64 exactly (all six
GAME4 chars).  The routine also nudges speed (+0x56) and the base stat (+0x1c)
under conditions not yet fully decoded (image `0x9920..0x99E0`: a table lookup
`[0xaf72]:0xc00` + `div`, and a `+0x1c -= 0xa` branch) — those are left
untouched (still open); they do not affect magic/weight.

### Level-up speed / base-stat nudges (2026-11-24)

The same derived-stat recompute (image `0x991F..0x99A8`, guarded by the PC flag
`char[+0x10] == 0xFFFF`) also nudges two more fields, now in `level_up()`:
- **speed (+0x56)** += 1 when the new level divides the profession speed table
  (`[0xaf72]:0xc00`, bytes) `{3,3,4,4,3,5,5,5,5,3,5,4}`.
- **base stat (+0x1c)** -= 10 when (level-1) divides the profession base table
  (`[0xaf74]:0xb6c`, bytes) `{2,2,3,3,2,3,4,4,4,2,4,2}` (and level > 1).
Both reproduce every shipped character exactly from a start of speed 1 / base
100 — e.g. Conan (prof 0) and Siegmund (prof 9) reach speed 2 and base 90
precisely at level 3 (speed table 3, base table 2 both divide).  The routine's
final `0x1b83:0x1e8` call sets `+0x1d = Dex<16 ? 10 : Dex*5-65` (verified for all
12 chars) and then recomputes the total defence (+0x1e) from armour; since Dex
does not change on level-up the +0x1d recompute is a no-op here, and +0x1e is
already maintained by the equip logic — both are therefore not re-applied in
`level_up()`.

### House-of-Trades training flow (2026-11-24)

The House dispatch (image `0x8894`) funnels `(T)raining`/`(L)earn magic`/
`(A)nother language` into one action (`0x89DA`) with a mode argument (0/1/2).
Training (mode 0) flow:
1. **XP gate** (image `0x8A20`/`0x8A5B`): the character's accumulated experience
   dword (+0x6c, NOT spent) must be `>= training_cost(prof) * 2^(level-1)`. If
   short, the House prints "The Grand Board of Trades decides that, an
   additional <N> experience points are needed to climb to <next>" (strings
   `0x4c57..0x4cad`) and does not train.
2. **Gold fee** (image `0x8BB2`/`0x905F`): "The costs of training for <name>
   amount to <fee>$, who'll pay that (1-6)?" — `fee == training_cost(prof)` (the
   flat table value), deducted from a chosen payer via `pay_gold` (0xbdec).
3. **Level-up** (image `0x9144`): the effect modelled by `level_up()`.

Experience is a cumulative **threshold**, not a currency — there is no
subtraction of +0x6c anywhere in the flow. Verified against GAME4: every
character is a few hundred XP short of their next level (Conan 7723/8000,
Siegmund 6299/8000, Laladil 4255/4500, Gandalf 4079/4500), exactly as a
threshold model predicts for an adventuring party. Lara/Joanna carry stray
0xFF high bytes at +0x6e/+0x6f (huge XP) — the engine reads the same 4 bytes, so
the port matches. `port/char.{h,cpp}`: `training_xp_needed()`,
`train_character()`; CLI `lof --house-train N PAYER HP`.

### House of Trades — learn magic / language (partially decoded, NOT implemented)

The House dispatch (image `0x8894`) maps `(T)`=mode 0 (training, DONE),
`(L)`=mode 1 (learn magic), `(A)`=mode 2 (learn language); all call the action
routine `0x89DA(char, mode)`. The two "learn" paths are decoded enough to state
their prerequisites but NOT their picker UI or final write, so they are not
implemented (Rule #1 — no guessing the prompt/effect):

- **Learn magic (mode 1, image `0x8C13`).** A character may learn magic only if
  it is profession 3 or 4 **or owns a spellbook** — the routine scans the
  inventory (`+0x6b` used-slot count, slots at `+0x82`) for item index `0x28`
  (a spellbook); if absent it prints "In order to learn new spells, <name>
  needs at least a spellbook!" (strings `0x4d15/0x4d2c/0x4d35`). The number of
  *new* spells it can still learn is `0x212f:0x93d6(char) - char[+0x6a]` (the
  spell cap function `0x93d6` minus the already-known count `+0x6a`). Past
  `0x8DB8` is the cost/learn step (not fully decoded); the spell chooser and the
  slot write are undecoded.
- **Learn language (mode 2, image `0x8E9E`).** Scans the 8 seed slots
  `char[+0x7a + n]` (n = 0..7, `0x63` = a "full/end" sentinel set at
  `+0x89E7..0x8EB6`) for a free one; if none, "has great command of all
  languages that we teach, send another scholar" (`0x4de6/0x4dfa/0x4e20`).
  Otherwise it would store a chosen language — picker + store undecoded.

**Missing to finish either:** the spell/language picker prompt strings, the gold
cost expression, and the effect write (the `0x8DB8+` magic step and the
language store). Also needs a save fixture holding a spellbook (item `#0x28`) to
verify — file as part of OT-005. `0x212f:0x93d6` (the spell-cap function) is a
useful future decode.

### Emporium SELL price — decoded spine (2026-11-24)

Emporium dispatch (image `0x57D4`): `B`->buy `0x6392`, `S`->`0x5938(0)` (sell),
`R`->`0x5938(1)` (repair), `G`->joint deposit `0x212f:0x79ba`, `C`->leave.
`0x5938` is a shared sell/repair menu:
1. Prompt (`0x3f13/0x3f2c` sell, `0x3f34/0x3f4b` repair), pick character
   (`0x212f:0x789c`), skip if inventory empty (`+0x6b`).
2. Show the item list / pick an item (`0x212f:0x547e`); arrows/space to move,
   `C` to cancel, confirm -> `0x5818`.
3. **Sell price** (mode 0, image `0x5C24..0x5D4A`), 32-byte item records
   (`idx<<5`), item price at `+0xfce`:
   - **condition term** = `(price * condition) / 100 - price/25`, where
     `condition` = the inventory slot's byte `+0x85` (slot base `+0x82`, so
     item byte +3). Helpers: `0x1efe` = unsigned 32x32 mul (image `0x30FDE`);
     `0x1e62` = signed 32/32 divide (image `0x30F42`, div by 100); the trailing
     `price/25` is `div 0x19`.
   - **haggle base** = `(price/100) * (haggle+1)` where
     `haggle = rand15 / charisma` (`char[+0x25]`), via signed divide `0x1f32`
     (image `0x31012`), and `rand15` is `0x1dec` (image `0x30ECC`) — the **same
     MS-C LCG as `CombatRng`** (`seed*0x343FD+0x269EC3`, returns
     `(seed>>16)&0x7FFF`), global seed at `[0xaf06]`.
   - **final** = haggle base + condition term, then decremented until it is
     `< price - 10` (the sell can never reach list price), with a floor.
4. Confirm (`0x5818`, "who gets the gold") -> remove the item, `add` gold to the
   character.

So the sell price is **inherently random** (charisma-seeded LCG) — there is no
single oracle value. A faithful port is possible by feeding `rand15` from a
`CombatRng` seeded by charisma, but the exact cap floor / signedness and the
rand->price end-to-end still need confirmation against a *scripted sell* with a
known seed (OT-006). **Not implemented / not guessed** (Rule #1). Buy-side
`[0xaf52]:0x4616==1` is a buy-only "fixed price" flag (absent from the sell path).

### Clock primitive (P-4 recon, 2026-11-24)

The game clock phase is the dword at save offset `0x9b6` (low `+0x9b6`, high
`+0x9b8`), mirrored to global `[0x3b0c:0x3b0e]` (es segment `[0xb072]`). The time
routine (image `0x25BE2..0x25C24`) **advances it by 1 phase per step** and wraps
it to 0 once it exceeds 7:
```
add word [0x3b0c],1 ; adc word [0x3b0e],0
if high!=0 || low>7 -> { low=high=0 }     ; so the phase is 0..7 (8 phases,
                                           ; Midnight..Evening, table ds:0x0f04)
```
Right after the increment the same routine iterates the six party slots
(`0x19a*i`) doing per-step updates (`char[+0x57]=4`, `char[+0x22] += <per-member
amount>[i]`) — i.e. per-time-step regeneration. The **rest** advance count and
this per-step regen are the P-4 clock model, still to be pinned (how many steps a
rest/wait adds). This is the hard P-4 item (AGENTS.md flags it) — delegated to
M-6 as a research decode (see the parallel note in the reply). The port does NOT
yet advance the clock on rest (`rest_party` handles cost/HP/rations/magic only).

### Effect 19 (scrolls/maps) — resolved as map-file I/O (not a spell), 2026-11-24

The use-dispatch handler for effect 19 (image `0xEB32`) pushes the party-location
dword (`[0xafa2]:0x37fe/0x3800`) and `2`, then `lcall 0x1cfe:0x7e2`. That routine
(image `0x1D7C2`, i.e. `0x1cfe*16+0x7e2`) formats a path from the location
(`0x1bde` sprintf, strings at `ds:0x7d83/0x7d8e`), then `x<<2` and file I/O
(`0x18d:0x2d74` open, `0x18d:0x2cd0` read/write). In other words effect 19 is the
**exploration-map persistence** routine (save/load the explored-map bitmap for the
party's location), NOT a castable spell. The port does not persist exploration
bitmaps the same way, so effect 19 cannot be ported as a spell effect and stays a
documented no-op (Rule #1 — not guessed). This closes the P-6 "effect 19" question:
it is map I/O, not unimplemented spell logic.

### Learn magic / language — decoded gate + capacity formula (P-7 recon, 2026-11-24)

House shared action `0x89DA` (called for modes: `(T)`=0 handled by its own gate,
`(L)` magic = mode 1, `(A)` language = mode 2). The **learn-magic gate** (mode 1,
`0x8C01`): a character may learn magic iff **profession `+0x1b` == 3 or 4**
(`0x8C33/0x8C37`) **OR carries the spellbook, item `0x28` (=40),** in any inventory
slot (`0x8C92`; several GAME4 members carry item 40). With neither, it prints the
"cannot learn" line (`0x4d15/0x4d2c` + name + `0x4d35`).

The learnable count is `spell_capacity(char) - char[+0x6a]` (learned spells). The
info screen (`0x8DB8`) shows that remainder; the per-spell cost is
`remainder * [0xaf56]:0x381e` (`0x8E48`).

**`spell_capacity`** = `0x212f:0x93d6` (image `0x2A6C6`): a 12-entry profession
jump-table (`cs:[bx-0x6b78]`, prof `+0x1b`) sets a school factor `S`
(`-1` = cannot cast); then at the common end `0x2A790`:
```
level = char[+0x17]; if level > 15 level = 15;
cap = SCHOOL_BASE[S] * level / 15        (signed); if cap == 0 cap = 1;
return (S == -1) ? 0 : cap;
```
`SCHOOL_BASE` is a signed-byte table at `[0xb09e]:0xc0f` (per school factor; needs
a runtime capture — like the `[0xaf62]:0xc0e` school-base table already captured
for memorisation). The 12 per-profession branches live at the jump-table targets
(`0x30a16,0x2fd9f,0x29e2d,0x2a53e,0x2f4c1,0x2f4c1,0x2d3f3,0x2197e,0x2c2b0,
0x21616,0x264f6,0x238f0`) and must each be read to map prof -> school factor.

**Not implemented** (Rule #1): the full learn flow is an interactive picker
(choose which of the school's spells to add, consume cost, fill slot
`+0x142+2*[+0x6a]`, bump `+0x6a`), and `SCHOOL_BASE` + the 12 branch factors need
decode/runtime capture. The gate + capacity formula above are the derived spine.

## GAME3 high-level fixture validates the derived-stat recompute (2026-11-25)

Owner upload GAME3 -> `saves/dng01-Joanna-f969c0ed.sav` ("Monastery of Sagacita"
entrance, DNG01; GAME1 `dng11` is one level deeper).  Party lead Joanna; **two
high-level casters**: Joanna (prof 10, lvl 12) and Skol (prof 5, lvl 11).  Their
XP is large-negative (owner's negative-XP levelling quirk) — harmless for the
stat fields.

Validation of `recompute_derived_stats()` (image `0x96F7..0x9901`) against GAME3:
* **Joanna lvl12 magic max 18**, **Skol lvl11 magic max 14**, **Elzndra lvl2 5**,
  weights 4900/5100/3850 — all reproduce exactly (levels 11–12 were previously
  untested; the old fixtures were ≤ lvl 4).  `char_test::test_recompute_high_level`.
* The disassembly re-confirms the exact formula, including the **deliberate
  double application** of the Wisdom/Intelligence bonuses to the magic MAX:
  `+0x68 = table[prof][lvl]` (`0x9733`), `+1 if Wis>15` (`0x9763`),
  `+2 if Int>15` (`0x978B`), then `+1 if Wis>15` again (`0x97D9/0x97FC`) and
  `+{1|2} if Int>15` (2 when Int>18) again (`0x9886`).  So the port was already
  correct; do not "simplify" the double bonus.
* **Caveat — level-1 creation values differ from this routine.**  A freshly
  *created* lvl-1 character has a different magic max than the level-up recompute
  would give (GAME3 Merlin prof7 lvl1 Int19: stored `+0x68`=3 = creation value,
  while the level-up recompute yields 5).  The recompute models the LEVEL-UP path
  only; do not assert it against lvl-1 creation saves.  (Char creation's own
  magic logic is not yet decoded — if it ever matters, file it.)
* Factored `recompute_derived_stats(SaveChar&)` out of `level_up()` (behaviour
  unchanged) so it is testable directly.

## GAME3 spell slots — confirms memorisation school-base, +0x6a scaling (2026-11-25)

Dumping GAME3's casters confirms the **memorisation school-base table**
(`[0xaf62]:0xc0e` = {1,33,59,80,100,118,127}): each caster's spell indices are
**sequential from its school base** (`spell index = school_base[school] + s`):

| member | prof | lvl | +0x6a | spell indices |
|--------|------|-----|-------|----------------|
| Joanna | 10 | 12 | 14 | 100..111 (+ 12,13) |
| Skol   | 5  | 11 | 14 | 80..91 (+ 12,13) |
| Elzndra| 8  | 2  | 3  | 33,34,35 |
| Merlin | 7  | 1  | 2  | 1,2 |
| Claudia| 6  | 1  | 1  | 59 |
| Dorgir | 3  | 1  | 1  | 118 |

School base by profession confirmed here: **prof7->1, prof8->33, prof6->59,
prof5->80, prof10->100, prof3->118** (six of the seven table entries).  This
validates the spell-memorisation machinery at high level too.

`+0x6a` (learned spell count) scales with level: lvl 1 -> 1-2, lvl 2 -> 3,
lvl 11-12 -> 14.  Whether `+0x6a` equals `spell_capacity` (fully learned) or is
just "learned so far" is not proven, so it does **not** by itself validate the
`spell_capacity` formula.

**spell_capacity still blocked.** `spell_capacity` = `0x212f:0x93d6` (image
`0x2A6C6`).  Its `cap = SCHOOL_BASE[S]*min(15,level)/15` needs the signed-byte
`SCHOOL_BASE` at `[0xb09e]:0xc0f` (uncaptured).  The per-profession branch-factor
targets listed in the earlier recon
(`0x30a16,0x2fd9f,0x29e2d,0x2a53e,0x2f4c1,...`) do **not** decode to clean
"set factor S; jmp 0x2A790" stubs at those image offsets — re-derive them (they
may be a relocated/relative table) before trusting them.  Next step to unblock:
capture `SCHOOL_BASE` at runtime (drive the House learn screen, dump
`[0xb09e]:0xc0f` like the memorisation table was captured).  Do **not** synthesise
the capacity (Rule #1).

## spell_capacity — callers mapped; capture needs a genuine in-town save (2026-11-25)

`spell_capacity` (`0x212f:0x93d6`, image `0x2A6C6`) has **8 call sites** (image
offsets): `0x8D19`, `0x8DBB` (learn-magic flow), `0x9BAC`, `0xD4B0`,
`0xF301`, `0xF31A` (spell-cast path), `0x12B09`, `0x12B22` (combat-cast).  The
learn **info screen** (`0x8DB8`) *displays* `remaining = capacity - char[+0x6a]`,
so reading that on-screen number would give `capacity` directly (then
`SCHOOL_BASE[S] = capacity*15/min(15,level)`).

**`SCHOOL_BASE` (`[0xb09e]:0xc0f`) is built at runtime** — the known memorisation
table `{1,33,59,80,100,118,127}` does **not** appear as contiguous bytes in a
`LOF_DUMP` of either the menu or a loaded save, so it can't be recovered by a raw
memory search; a live capture (trace the routine's `es` load, or read the learn
screen) is required.

**Town entry confirmed (good news):** loading a save whose location has no DNGS
map (patched `ones=8`, `DNG08` absent) in the analysis emulator renders the
**town street viewport + the party panel** — so the engine really does "stay in
town" on a missing map, matching the port's town heuristic.  (No port bug there.)

**Blocker:** driving the patched in-town save through town-menu -> House -> (L)earn
-> character -> info screen in the slow analysis emulator was not reliable
(key-timing vs the long save load; captures landed on the play screen).  Cleanest
unblock: a **genuine in-town save** — a save made while the party is standing in a
town with the town menu on screen — so navigation starts from the menu directly.
Requested from the owner.  Do **not** synthesise `SCHOOL_BASE` (Rule #1).

## Shipped "fresh install" saves are a WILDERNESS start, not in-town (2026-11-25)

`downloads/ext_msdos/legfaer/GAMES/` ships `GAME1/2/3/5` (the "fresh install"
saves).  All four are the **same start**: location `ones=0, tens=1` -> **`DNG10`**
("Valley of Faerghail", `town_sel=1` = Thyn region), party Decker/Siegurd/Handy/
Merlin/Cassandra/Assanla.  `DNG10` is a real outdoor map file, so this is the
**wilderness**, not a town (towns have no `DNGS` map).  Loading the valid one
(`GAME2`) in the emulator renders "Valley of Faerghail / Facing North / Dawn".

Checksum status: **GAME2 and GAME3 are valid**; **GAME1 and GAME5 are corrupt**
(stored trailer != fold over `[0,0x2e3a)`), so the engine rejects them with
"The file GAMES\GAMEn is not a valid game:" + "(C)ontinue".  (`GAME1.SAV` at the
release root is a 0-byte stub.)

Conclusion: there is **no shipped in-town save** — an in-town save must be
synthesised (town entry is a runtime state, not a save field: the town hub is
`image 0x53E0`, key dispatch `B`->Bank`0x739a`, `C`->leave`0x557a`,
`E`->Emporium`0x5680`, `H`->House`0x873c`, `I`->Inn`0x7646`, `T`->Temple`0x9e5e`,
but `0x53E0` is not referenced by any `E8`/`9A`/word table in the image — it is
reached through a computed dispatch still to be located).

## Emporium SELL price — refined static decode + the inconsistency (2026-11-25)

Re-traced the sell-price routine (`0x5C24..0x5E54`) and the gold credit
(`0x611A`) directly from the assembly.  **Corrects** the earlier "inherently
random price" note (the credited gold does *not* read the haggle term where I
can see it — but see the inconsistency below, so this is not yet trusted).

- **Condition term** (`[bp-0x16]`/`[bp-0x14]`, 32-bit signed):
  `cond = price*condition/100 - price/25`, floored to **1** (`0x5C8D..0x5CA3`).
  `price` = item `+0xfce` (idx<<5); `condition` = inv-slot byte `+0x85`.
- **Haggle base** (`[bp-0x12]`/`[bp-0x10]`): `(price/100)*(haggle+1)`,
  `haggle = rand15/charisma(+0x25)`; `rand15` = MS-C LCG `0x30ECC`, seed
  `[0xaf06]`.
- **Cap loop** (`0x5CFB..0x5D2E`): while `(haggle_base + cond) > price-10`,
  decrement `[bp-0x12]` (haggle_base).  So the *sum* is capped at `price-10`.
- **Branch** (`0x5D30`): if `cond>=10 && haggle_base>=10` -> normal path
  `0x5DA8->0x5E54` (displays `[bp-0x16]+[bp-8]`); else `0x5D4C` (displays
  `[bp-0x16]`).
- **Gold credit** (`0x611A`): `char+0x72 += [bp-0x16]/[bp-0x14]` (i.e. the
  **condition term**), after a weight check (`0x5D60`: `+0x66 + [bp-0x16] <=
  +0x64`, else "too heavy").

**INCONSISTENCY (why this is NOT implemented):** under this reading the cap loop
adjusts only `haggle_base`, which is then never credited — i.e. the loop would be
dead code, which is implausible.  So the true credited price is almost certainly
the **capped sum** `min(haggle_base + cond, price-10)` and `[bp-0x16]` must hold
that sum at `0x611A` via a write I have not located (or the gold-add reads a
different local on the real path).  Rule #1: **do not guess**.  Needs a
**scripted sell** with a known seed to settle (OT-006).

**Empirical verification blocked by timing:** the golden `shop` scenario reaches
the Emporium (`LOF_NO_TIMER=1 LOF_CMDLEN=3 LOF_KEYAUTO=0x1e,0x03,0x2e,0x2e,0x12
LOF_KEYGAP=250`, frames 1600), but reproducing it ad-hoc to add sell keys landed
on the play screen (empty party grid) both times — the exact key frames matter.
A DOSBox capture of a real sell (price shown + gold delta) would settle the
formula fastest.


## Emporium SELL price — IMPLEMENTED + emulator-verified (2026-11-25)

Resolved the earlier cap-loop inconsistency with an **actual scripted sell in the
analysis emulator** (town -> Emporium -> S -> char 1 -> item 1).  The shop
offered **"For the object Leather armour I'll offer 15$, do you agree?"** for
Leather armour (list price **15**, condition **100%**).  That equals
`price*condition/100 - price/25` = 15 - 15/25 = **15**, the **condition term** —
NOT the capped haggle sum.  So the credited gold is the condition term; the
`haggle` (rand15/charisma) only drives a *display* variant on the 0x5DA8/0x5E54
path for items priced >= ~100 (it does not change the gold credited at 0x611A).
The earlier "random price" note was wrong.

**Implemented** (port/char.{h,cpp}): `int sell_price(int item_index, int
condition)` = `max(1, price*condition/100 - price/25)`; and `lof --sell-auto N
SLOT [--amulet-out FILE]` computes it, does the engine weight check (`+0x66 +
price <= +0x64`, else "too heavy"), credits gold (+0x72) and removes+compacts.
(`--sell N SLOT PRICE` raw effect unchanged.)

**Verified:** `char_test::test_sell_price` pins Leather armour 100%->15
(emulator), Studded 100%->24, Scale mail 100%->53, Scale 50%->25, Scale 0%->1,
invalid->0.  `port_building_test::check_sell_auto` runs `--sell-auto`
end-to-end (GAME4 Gandalf slot 0 -> price 8, gold +8, item removed).


## Learn-magic: reachable in town; needs a SPELLBOOK; second gate found (2026-11-25)

Following the owner's tip (the Emporium-sell key path is *in town*, so swap
`E`->`H`, `S`->`L`), I reached the House-of-Trades **learn-magic** screen in the
analysis emulator:

- Keys that reach the House learn prompt (from the boot/add-character town hub):
  `LOF_NO_TIMER=1 LOF_CMDLEN=3 LOF_KEYAUTO=0x1e,0x03,0x2e,0x2e,0x23,0x26
  LOF_KEYGAP=250` -> **"Who would like to learn magic spells (1-6)?"**.
- Picking the added warrior (STILETTO) -> **"In order to learn new spells,
  Stiletto needs at least a spellbook!"**.  So learning requires the
  **Spellbook item (item 15, price 30)** in the character's inventory.  No
  shipped save carries one.
- **Owner OT-001 idea CONFIRMED:** loading the "in front of Cyldane" save
  (`dng00-Conan-d8b1cad7`, DNG00, facing West) and pressing **Up (0x48) enters
  the town** (the party then shows in the town hub / House of Trades viewport).
- With a Spellbook added to Joanna (prof10 HEALER, +1000g, re-sealed), picking
  her in town gives the *second* gate: **"The Grand Board of Trades announces
  that Joanna cannot, at the moment learn new spells."**  Clearing her
  memorised slots (`+0x142`, four stride-2 slots = spells 100..103) did **not**
  lift it, so the gate is **not** the `+0x142` slot count.  Candidate causes
  (unproven): a separate permanent learned-spell array vs `+0x6a` capacity
  (level-based), or a level-up gate.  Needs a runtime `LOF_PXWATCH`/`LOF_TRACE`
  of the learn routine to pin — **not guessed** (Rule #1).

Recipe to reproduce the in-town learn screen: copy `dng00-Conan` to a slot,
add item 15 (condition 100) to a caster (e.g. Joanna, slot 4) + re-seal (fold:
seed 1, add even / xor odd, NUL->1 over `[0,0x2e3a)`), load with `O`+slot, press
`Up` to enter town, `H` then `L`, then the character digit.

STILL TO DO: decode the "cannot at the moment" gate + the spell-list picker +
the slot write + the cost, then implement `learn_magic()` + a port test (the
in-town learn screen is now a reachable oracle).


## Learn-magic — FULLY decoded via the emulator (2026-11-25)

Reached the House-of-Trades learn screen (see the earlier note) and traced the
whole routine.  **No per-spell picker** — the guild teaches *all* available
spells for one fee.

Flow (image offsets):
- House `0x873c` dispatches: `T`(0x54)->mode 0 (training), `L`(0x4c)->mode 1
  (learn magic), `A`(0x41)->mode 2 (language) -> shared `0x8894` (pick char;
  `+0x10`==-1 gate, which all normal chars already satisfy) -> learn routine
  `0x89da(mode, charidx)`.
- **Gate 1** (`0x8C7D`): passes if `prof(+0x1b)==3||4` **or** the inventory
  holds item `0x28` (40).  Else prints the first "cannot" message (0x4d15..).
- **Gate 2** (`0x8D3A`): `char[+0x6a] >= spell_capacity(char)` -> the *observed*
  **"cannot, at the moment learn new spells"** (strings 0x4d52/0x4d6d/0x4d7d).
  So **`+0x6a` = learned-spell count**, capacity = `spell_capacity()` (level-based).
- `N = capacity - +0x6a`; **cost = N * [0xaf56:0x381e]** (the per-spell price,
  measured **15**).  Prompt: **"The Wise demand for N new spells C$, who'll pay
  (1-6)?"**.  Paying with a member short of gold prints **"You haven't enough
  money on you to pay our demanded sum."**
- On payment: **"The Wise of The Grand Board of Trades teach <name> the extra
  spells that she makes note of ..."** then **"Concentration improves!"**.  It
  learns all `N` at once (no picker).

Emulator evidence: Joanna (HEALER prof10, item 40 in bag, `+0x6a` set to 0,
1000g): "The Wise demand for 4 new spells 60$. Who'll pay (1-6)?" (4*15=60)
-> pay -> "teach Joanna the extra spells ... Concentration improves!".

STILL TO IMPLEMENT (`learn_magic()`): the write of the N spells into the
learned-spellbook array + `+0x6a += N`, and the `spell_capacity()` formula.
NB the 4-slot array at `+0x142` is the *memorised/castable* set, NOT the learned
count (`+0x6a`); clearing `+0x142` did not lift gate 2, clearing `+0x6a` did.


### learn_spells() implemented (2026-11-25)
`port/char.{h,cpp}`: `learn_spells(SaveChar&, capacity)` reproduces the decoded
write (0x9B7C..0x9CFD) - sets +0x6a = capacity, fills +0x142+2s = kBase[school]+s
and +0x143+2s = spell data[9] for s = old..capacity-1, bumps +0x2b (clamp 100);
returns false for a non-caster.  `learn_cost(capacity, learned) = (cap-learned)*15`.
Pinned by `char_test::test_learn_spells` (HEALER, capacity 4 -> spells 100..103,
60 gold).  NOT yet wired to a CLI or the town UI; the capacity tables
(spell_capacity jump table + the 0xc0f table) and the spellbook/gate-1 item check
are still undecoded, so the guild flow is only half-implemented (the *effect*,
not the prompt/gate).

### --learn-magic CLI wired (2026-11-25)
`lof --learn-magic N --learn-capacity C [--learn-payer M] --save S --amulet-out O`
loads the save, pays `learn_cost(C, learned)=(C-learned)*15` from the payer,
applies `learn_spells(chars[N-1], C)`, and writes a sealed save, printing
"taught character N <n> spell(s) for <c> gold ...; Concentration improves!".
`--learn-capacity` is a parameter because the engine's `spell_capacity()`
(0x2A6C6: `max(1, kCap[base]*min(level,15)/15)`, per-prof base jump table + a
0xc0f table) is not yet fully extracted.  Verified end-to-end: HEALER (prof10)
learned 0, capacity 4 -> 4 spells (100..103), 60 gold, charges 10/6/6/7 (matches
the emulator).  Guarded by `port_building_test::check_learn_magic`.
STILL OPEN: the "who'll pay"/spellbook/item-40 gate prompts + `spell_capacity`.

### CORRECTION: the spell tables ARE in the EXE; spell_capacity implemented (2026-11-25)
Earlier notes called `[0xaf62]:0xc0e` / `[0xb09e]:0xc0f` "runtime data not in the
file image".  **That was wrong** - they are in the EXE's DGROUP (ds=0x4c92), at
image **0x3D42E**, stored stride-2 and interleaved:
  even bytes 0x3D42E = school-base {1,33,59,80,100,118,127}  (`kBase`)
  odd  bytes 0x3D42F = capacity factors {32,26,21,20,18,10,14} (`kCap`)
(`[0xaf62]` and `[0xb09e]` both = 0x4C92 = DGROUP, confirmed from a memory dump.)
The prof->capacity-class jump table is the `cs:0x9488` table = image **0x2A778**
(cs=0x313F = 0x212f relocated +0x1010; ip = image - 0x212F0):
  prof 0,1,2,11 -> -1 (no magic); 3->5, 4->2(level-5,cap10), 5->3, 6->2,
  7->0, 8->1, 9->3(level-3,cap10), 10->4.
So `spell_capacity()` (image 0x2A6C6) = `max(1, kCap[class]*min(15,level+adj)/15)`,
level = `+0x17` (NOT +0x1a).  Verified: Joanna prof10 level 4 -> 18*4/15 = 4,
exactly the emulator's "4 new spells".  `port/char.cpp::spell_capacity()` now
implements it; `--learn-magic N` uses it automatically (no --learn-capacity
needed); pinned by `char_test::test_spell_capacity`.  Everything here is EXE
content (code + DGROUP), transcribed like the spell/magic tables - no data file.

### learn-magic gate DONE (2026-11-25)
Disassembled the gate in the learn routine (0x8C33..0x8D3C):
- **gate-1** (0x8C33): pass if profession 3/4 (Paladin/Cleric) OR the bag holds
  item **40** (scanned as `es:[bx+si+0x82]==0x28` over the used slots +0x6b).
  Failing it draws "In order to learn new " (0x4d15) + "spells, " (0x4d2c) +
  <name> + " needs at least a spellbook!" (0x4d35) - so **item 40 is the engine's
  "spellbook"** (Joanna's bag has {26,7,40,47}; removing 40 makes the CLI report
  "needs at least a spellbook").
- **gate-2** (0x8D3A): `char[+0x6a] < spell_capacity()` else "The Grand Board of
  Trades announces that <name> cannot, at the moment, learn new spells."
`learn_magic_gate()` returns Ok / NoSpellbook / NoCapacity; `--learn-magic`
enforces it (prints the engine's reason).  Pinned by
`char_test::test_learn_magic_gate` + the building-test learn-magic check.
**learn-magic is now functionally complete**: gate + cost (15/spell) + pay +
teach-all (learn_spells) + capacity (spell_capacity) + sealed save.  Remaining:
the interactive `--ui` House (L) wiring (currently the House screen only draws;
the prompt-driven pick/pay flow is the last layer).
