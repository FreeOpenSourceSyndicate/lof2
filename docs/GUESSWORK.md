# GUESSWORK — the guess register

Every **guess** in this project is tracked here, per the rules in
[`docs/RULES.md`](RULES.md). A guess is any value/behaviour/layout/name/algorithm
that is not traceable to a source of truth (decompilation, data files, oracle
frames, or live play).

**Read this file at the start of every session** and act on each open item:
resolve it, advance it, or re-remind the owner. Nothing is ever deleted; a
resolved item moves to the "Resolved" section with how it was removed.

How to use each section:

| Section | Meaning | Required action |
|---------|---------|-----------------|
| **A. Cannot be removed — owner decision required** | Proven impossible to derive. | **Re-remind the owner every session** until an explicit order is given (Rule 3). |
| **B. Unclear — revisit (watchlist)** | Not yet known whether it can be derived. | Revisit every session; promote to A or to a task/blocker once clear (Rule 4). |
| **C. Owner-authorised guesses** | The owner explicitly allowed this specific guess. | Keep the exact authorisation wording + date; cite it in code (Rule 6). |
| **D. Can be removed — queued (normal task)** | Derivable; not yet done. | Do it, or file a blocker if another lane is needed (Rule 2). |
| **E. Known non-determinism (not a guess)** | Irreducible randomness by design. | Not a guess; listed so nobody mistakes it for one. |
| **F. Resolved / removed** | Guess has been removed. | Record how, keep for history. |

---

## A. Cannot be removed — owner decision required

*(none currently)*

---

## B. Unclear — revisit (watchlist)

### G-005 — dungeon solid-wall set `{6,13}` is oracle-derived, not decompiled (Lane A)

- **What:** `dungeon_is_solid_tile()` in `port/dungeon.cpp` treats tiles 6 and 13
  as the only walls (everything else, including the void tile 0, is walkable).
- **Source of truth:** the emulator **opacity sweep** (Round 27,
  `docs/notes-a.md`): patching the ahead cell to code `X` and varying the cell
  *behind* it leaves the view byte-identical only for `X in {6,13}`; the render
  for those is the full-screen wall face `DP036` (== `DP060`, 100% ink).  This is
  source-of-truth #3 (oracle frames), so it is not a *chosen* value.
- **Why it is still on the watchlist:** the sweep covered the ahead cell of one
  DNG05 corridor.  It is direct evidence for 6/13 and for the tested see-through
  codes (0/1/2/3/7/8/12), but codes that never occur directly ahead of a party
  were not individually swept, and the *generality* (all 29 levels) is inferred
  from consistency, not traced in the engine.
- **To clear it:** confirm the wall test in the decompilation (`analysis/lof.asm`,
  the move/collision path near image `0xD83`), which is a **Lane M** routine — if
  it is not removable by Lane A it becomes a blocker (`docs/blockers/A.md`).
  Cross-checking is also possible with more oracle sweeps (patch the *party
  cell*, not the ahead cell) at no decompile cost.
- **Progress (2026-11-18, maintainer).** The **renderer** half is now
  decompiled: the per-cell view routine (`img 0x2BA20`, capstone from
  `LOF_SEGDUMP`) reads the map byte `es:[bx+0x46d8]`, **masks it `& 0x0f`**
  (`img 0x2BA78`, `and al,0xf`) into a scratch grid, and its feature test
  (`img 0x2BE2E`) then `cmp al,6 / je` + `cmp al,0xd / je` (plus 4/5/0xa/7/8/0xc
  for other features).  So the engine keys the wall decision on the **masked**
  low nibble, exactly as the port's `dungeon_base_view_index` does; the port's
  `dungeon_is_solid_tile` does *not* mask, but this is a **no-op for the wall
  set on every shipped map** (verified: for all 29 `DNGS/DNG*`, no tile code has
  `solid(c) != solid(c & 0x0f)`), so the oracle-derived `{6,13}` stands under the
  mask.  The **mover** (the step-rejection test) is still not isolated — the four
  `mov es:[bx+0x4612],al` sites in the image are all the party *trail-shift*
  (`img 0x10D29`/`0x1A171`/`0x1AD3F`/`0x1AEB1`, each preceded by
  `dec es:[0x4615]`), not the collision test, and the collision routine is
  reached indirectly (routine R is an `lcall` target with no static far-call
  site).  **Status: partially cleared** — renderer-level `{6,13}` decompiled +
  mask shown harmless; the mover remains oracle-derived.

### G-007 — depth-2/≥3 dungeon view sprite: depth-keyed, `CHnn` residue (Lane A)

- **What:** `dungeon_view_for()` blits `EGA/CHAR2/CH40` (depth 2) / `CHAR2/CH25`
  (depth ≥3) only when the scenario is 5; other scenarios keep the depth-1
  tileset sprite.
- **Source of truth:** the emulator depth sweep + the DOSBox oracle for the
  D5/Temple save (slot 1).  The sprites are exact for that scenario and both blit
  at the viewport origin (10,11).
- **Resolved (2026-11-18, Round 51):** BLK-A005 is closed as *not-portable-by-data*
  — the engine's `es:[0x259]` encounter id is RNG-seeded monster placement, not a
  function of the save.  The **observable, save-independent rule is the depth
  march** (the port's model): depth 1 -> the tileset sprite, depth 2 -> one bank
  sprite, depth ≥3 -> another.  That is what the port implements.
- **Residue:** the exact `CHnn` *numbers* for depth 2/≥3 are still scenario-5
  values; they are that scenario's encounter bank, and whether the numbers are
  fixed or scenario-dependent is unproven.  This is now a *labelling* residue,
  not a behavioural gap (the structure — which depth blits a bank sprite, and at
  which offset — is decompiled).
- **To clear it:** capture a second scenario's depth-2/≥3 view and read its
  `CHnn`; if it differs, table the numbers per scenario.

### G-004 — `item_effect_name()` labels beyond the traced dispatcher values (Lane B)

- **What:** `item_effect_name()` gives 41 labels for the item effect byte
  (`+0x1f`).  The engine tests the byte at several sites; the returned strings
  are item-name-derived descriptions, not engine symbol names.
- **Source of truth:** every read of `es:0xfdb` (`+0x1f`) in the image.  The
  complete set of values the engine is **proven** to compare against, with a
  site: `1,2,3,10,19,23,30,31` (use-routine effect dispatch `0x0E89D` /
  `0x0EDB0`); `2,8,12,21,24,28,29,33` (equip/unequip handler `0x2995C` /
  `0x29FB1`); `9,13,15,20,27,40` (misc sites `0x14A09`, `0x13AF4`, `0xDF7E`,
  `0x14976`, `0xE274`, `0xE1BC`).  The engine's own messages fix the meaning of
  several: 1 "growing professionalism." (enchant weapon), 2 "feels himself
  safer." (enchant armour), 3/23 "goes green in the face!" (poison), 10 adds HP
  (`0x0EA90`), 15/40 "fits together." (combine paired items), 30/31 clear the
  poison state (`0xEBE4`/`0xECE2`).  Live play grounds one more: **34 (lantern)**
  — using a lantern with a flask of oil and a tinderbox consumed the oil and
  printed "… make light…" (the lantern's `+0x18` spell index 7 is "Permanent
  light").  Effects the engine never compares (5, 7, 35, 37, …) keep purely
  descriptive labels.
- **Why unclear:** for the values the engine **never** compares, the label is a
  grouping of the item table by effect (effect 5 = "Crystal ball", effect 8 =
  "Antimagic ring", …) — the engine stores no effect-name table, so those strings
  can only ever be descriptive naming choices, not engine symbols.  The
  equip/unequip stat deltas (effects `2,8,12,21,24,28,29,33`) **are** now
  derived statically (see `docs/notes-b.md`); the residual is the effect values
  the engine never tests.
- **To clear it:** for values the engine never tests, the descriptive name is
  already the correct answer (a naming choice, not a claim about the engine); no
  further derivation is possible or needed for them.

---

### G-012 — combat round model: four values the port must take as parameters (Lane M)

- **What:** `port/combat.{h,cpp}` implements the derived melee round (see
  `docs/notes-b.md`, "Fallback melee path 0x14D80"), but four inputs are *not*
  derived from the disassembly and are therefore explicit parameters, defaulted
  for the CLI only:
  1. **`hit_score`** — **DERIVED 2026-11-21 for the party side** (see below);
     the entry below records the history.  The prelude's
     `((attack + power + base)/3 [+25% when +0x1a==2]) / +0x55`.  The combat
     rows' `+0x04` (power), `+0x05` (rank), `+0x07` (defence), `+0x08` (offence)
     are *read* by the prelude (image `0x13778`..`0x13820`), but the builder
     that fills them from the character record (the round-setup routine around
     `0x27F14`) is not decoded.  `--fight` passes `--hit-score` (default 100,
     "always hits"), which is a *test* value, not a game value.
  2. **The wielded item** — **DERIVED 2026-11-21.**  The prelude's item scan
     (`0x1398B`..`0x13A5D`) walks the inventory with the engine's helper
     `0x21D2(char, class, from)`: first a **class 1 (melee)** item, then the
     next class-1 item (the second hand), falling back to **class 5 (staff)**
     when there is no melee weapon.  With both hands filled it draws `rand(2)`
     once per round to pick the hand (`0x13A43`); the damage is that slot's
     item `damage` byte (`ItemRecord::damage`, `[0xafa8]:0xfd5 + 32*item`) and
     the roll is `rand(damage)+1` (`0x14DF0`/`0x14DFA`).  Implemented as
     `weapon_slots()` (`port/char.cpp`) + the hand pick in `melee_round`
     (`port/combat.cpp`).
  3. **`xp_mult`** — the XP award multiplies the damage by the **byte table at
     `[0xafc6]:0xf37`** indexed by `victim_combat[+0x2c]` (image `0x15493`
     `mov bl, es:[bx+si+0x2c]` / `0x1549D` `mov al, es:[bx+0xf37]` / `0x154A4`
     `mul word [bp-0x48]`), accumulated into the attacker's 32-bit `+0x6c`
     (`0x154C0`).  Measured contents (`4c92:0f37`): `76,1,2,4,8,10,10,12,14,18,
     20,20,25,...`.  The per-monster `+0x2c` byte belongs to the **0x11a-byte
     combat record**, which is built from the `NSC` record by a routine that is
     not yet decoded, so the port still uses 1.
     **CLOSED 2026-11-21:** the builder is image **`0x1195A`** — it copies the
     whole 78-byte `SYST/NSCnn` record into the combat record with
     `rep movsw` of `0x27` words (`es=[0xafc0]`, `les di, es:[0x52]`,
     `lea di,[bx+di]` with `bx = 0x11a*index`), so the first 78 bytes of the
     combat record ARE the NSC record and `+0x2c == nsc.stats[6]`.  Shipped
     NSC records use `+0x2c` values 1..8.  `port/combat.cpp::xp_per_damage()`
     carries the measured table and `--fight` takes the index from
     `--monster SCEN:REC` (default the pinned Mercenary, NSC00 record 12).
  3b. **The `W` column — DERIVED 2026-11-21.**  Image `0x15849`..`0x15975`
     (the landed-hit path): if the attacker holds a weapon (`[bp-0x38] >= 0`)
     the engine rolls `rand(100)+1` (`0x15852`), reads the wielded item's `+0x10`
     byte (`mov cl, es:[bx+0xfcc]`, `0x1588E`) and, when that byte is `>=` the
     roll, prints "Unfortunately, the weapon … is not durable enough, and
     breaks with a loud bang!", sets the round's `W` flag at
     `[0xafdc]+0x5608+2*index` and zeroes the slot's condition byte
     (`char[+0x85 + 4*slot] = 0`, `0x15975`).  Implemented as
     `MeleeInputs::weapon_f0` / `MeleeResult::weapon_broken` /
     `CombatRoundRow::w`; the roll is taken on every landed hit while a weapon
     is held (so the RNG sequence matches), and `-1` disables it.
  3c. **`P` (shield/armour damaged) and `B` (ability improved) — still open.**
     The **round-flag array is at `[0xafdc] + 0x5608`, one byte per flag per
     member with a 2-byte stride**: `+0x5608` = `W` (`0x15955`), `+0x5609` =
     `P` (`0x17658`), so `B` is presumably `+0x560A`.  `P` is set together with
     a `sub …+0x85, 2` on the shield (`[bp-0x30]`, `0x17658`) or armour
     (`[bp-0x3e]`, `0x1777A`) slot, plus the rust variant that zeroes the slot
     (`0x17290`); `B` is the three ability messages (`0x12846`, `0x13D7D`,
     `0x1471C`).  **Their guards are not decoded**, so the port leaves both
     false — the next step is to read back from `0x1764F`/`0x1777A` to the
     branch that reaches them.
  4. **The defender rows** (`+0x258 + 72*group + 12*member`) — **DERIVED
     2026-11-21.**  Builder at image `0x27A1E`: `row[+0x01]` = monster index,
     `+0x02` = `combat[+0x4c]`, `+0x04` = `100*hp/(8*combat[+0x2c])`,
     `+0x05` = group+1, `+0x07` = `combat[+0x2d]`, `+0x08` = `combat[+0x2e]`,
     `+0x09` = `combat[+0x30]`, `+0x0a` = hp.  Implemented as
     `defender_row()` (`port/char.cpp`); for the Mercenary that is
     `+0x07` = 20, `+0x08` = 100, `+0x09` = 1, `hp_pct` = 50 at 8 HP — which
     is exactly what the live capture holds, and it *corrects* the earlier
     measured constants (the capture's `+0x06` = 236 is written by code that
     is not decoded yet; the hit score does not read it).
  4. **The W/P/B columns** of the round-result table — the printer is derived
     (which column takes `*` vs `-` is not), so the round model leaves them
     false.
- **Progress (2026-11-21):** item 1 is now derived.  The "combat rows" ARE the
  party-status rows the builder at image `0x27F14` fills (already ported as
  `party_row()`): `+0x04` = `hp_pct`, `+0x05` = `+0x24` (combat rank),
  `+0x07` = `attr_a`, `+0x08` = `attr_b` — confirmed against a live capture
  (party rows and the two Mercenary defender rows).  The prelude at
  `0x13744`..`0x13806` then gives
  `base = 120 - 20*(atk_rank + tgt_rank)`, `attack = atk_attr_b - tgt_attr_a`,
  `power = atk_hp_pct` (each clamped >= 1), `hit = (attack+power+base)/3`,
  `hit += hit/4` when `+0x1a == 2`, `hit /= +0x55`.
  `port/combat.cpp::hit_score()` implements it and `--fight`/`--ui` use it.
  **Still unverified:** the exact rounding of the `+0x1a` bonus (`hit/4` vs
  `hit*5/4`) and the sum's final division site (the combination itself is
  recorded from the previous session's derivation, not re-read instruction by
  instruction).  Items 2-4 remain open.
- **Status:** OPEN (items 2-4).  Each is isolated behind a named field
  (`MeleeInputs::hit_score`, `::weapon_damage`, `::xp_mult`,
  `CombatRoundRow::w/p/b`), so clearing any one is a local change.
- **To clear it:** decode the round-setup builder at `0x27F14` (fills the combat
  rows), the hand-slot pair, and the `[0xafc6]` table; then the printer's W/P/B
  sources (the three `*`/`-` draws in `0x1867D`..`0x189F6`).
- **Also not derived:** the row state for a party member with *no live target*
  (all defenders down).  The engine's loop simply finds no target; the port
  prints "Passed" rather than "Failed", because "Failed" is the miss flag
  (`[0xafd6]:[record+0x3d48] == 1`, image `0x14295`), which an attack that never
  ran cannot set.

## C. Owner-authorised guesses

*(none)*

---

## D. Can be removed — queued (normal task)

### G-014 — the event-key table's 70-entry cap overflow is unobserved

- **Guess:** the engine's per-level event-key table (`[0xb048]+0x56`, count at
  `[0x7d00]`) holds at most **0x46 = 70** entries, and the code at `0x1F189`
  simply stops inserting past that. What happens to a cell whose word would have
  been the 71st is unobserved: the build loop's cap check is in the *lookup*
  path (`0x1F189` guards the current cell's word against the limit), and no
  shipped level comes close.
- **Why it is a guess:** no level exercises it. The largest table across the
  eight Temple levels is 44 keys (DNG06), so the overflow branch never runs.
- **Ground truth needed:** a synthetic DNGS with >70 distinct tile-3 object words
  fed to the emulator, watching `[0x7d00]` and the resulting SPEC index.
- **Impact today:** none — the port caps at `kEventKeyMax` and never reaches it.
  Queued so it is not mistaken for proven behaviour.

### G-015 — the move-path flag that gates the event resolver is inferred

- **Guess:** the resolver at `0x2E0BE` (which calls the event dispatcher for
  tile-3 cells) is reached from `0x2C8CF` as `push cs; call 0x2e0be`, and the
  preceding `cmp byte ptr [bp+8],0 / je` means "only if something changed", i.e.
  the party actually moved. Read literally the argument is just a stack byte at
  `[bp+8]`; its *meaning* is not proven, only that it is a caller-supplied flag.
- **Why it is a guess:** no trace confirms `[bp+8]` is a moved/blocked status.
  The surrounding code in `0x2C6FE` does not obviously set it, and the emulator
  cannot currently drive a walk (BLK-A004), so the flag's value on a real step is
  unobserved.
- **Ground truth needed:** a `LOF_TRACE_IP` hit count at flat `0x3E1CC` (image
  `0x2E0BE`) while stepping onto (a) a tile-3 cell and (b) a wall; plus reading
  the pushed argument at the call site. Movement in the emulator is the
  prerequisite.
- **Impact today:** none on the selector (proven) and none on the port (it has
  no move loop yet). It only affects *when* the port should call
  `event_index()` after a step, which is the remaining P-2 work.

### G-016 — the `SpecType` labels come from record text, not from code

- **Guess:** the names in `port/char.h`'s `SpecType` enum (`SPEC_TYPE_DOOR = 7`,
  `SPEC_TYPE_TELEPORT = 11`, `SPEC_TYPE_TRAP = 18`, ...) are descriptive labels
  inferred by correlating each type value with the record's text. The *code* now
  says something weaker: values 7..23 index a 17-word jump table at image
  `0x21B9A`, but most entries land in one shared tail at `0x1F5EC` that switches
  on the type word itself, and types 0..6 never reach the table at all.
- **Why it is a guess:** no handler body has been decoded end to end. The two
  checks done so far are consistent with the labels (type 11 writes the latch at
  `[0x7d1c]` that the tail tests, which fits "teleport"; the `+0xe4 == 1`
  once-only gate fits "fire once") but consistency is not proof, and several
  labels (12, 19, 20, 21, 22) are placeholders named after their own numbers.
- **Ground truth needed:** decode the shared tail at `0x1F5EC` and the four
  branches the type word selects inside it, then rename the enum from code.
- **Impact today:** none on behaviour -- nothing dispatches these types yet. It
  matters as soon as effects are implemented, because a wrong label would put
  the wrong behaviour behind the right number.

---

## E. Known non-determinism (not a guess)

### RNG seeded once from `INT 1Ah` — fully decoded, and the oracle is deterministic

- **Fact (decoded from the decompilation, 2026-11-16):** the engine's generator
  is the **classic MSVC `rand()` LCG**: `state = state*0x343FD + 0x269EC3` and
  `rand()` returns `(state >> 16) & 0x7FFF` (`analysis/lof.asm` image `0x30ECC`
  is `rand`, `0x30EBA` is `srand` = store the seed in `state`, `0x2D73A` is a
  `rand_range(lo,hi)` wrapper). It is seeded **exactly once**, at startup:
  image `0x3E` (`lcall 0x18d,0x2ed1`) reads the BIOS clock (`0x47A6`: `int 0x1a`
  then `add cx,dx`) and passes the sum to `srand` (`lcall 0x2f0e,0x1dda`). There
  is **one** `srand` call site and **one** `rand()` call site in the image.
- **Consequence:** enemy placement/movement is a pure function of that single
  seed — it is *not* irreducible per-frame randomness. With the seed pinned, a
  combat frame is bit-exact and reproducible.
- **Emulator status:** the old `INT 1Ah` handler left `CX:DX` as whatever the
  caller held, so the seed was an accidental (but reproducible) `0x000B`. The
  handler now returns that value **explicitly** and honours `LOF_TICK=<n>` to
  choose a different seed. Verified: 6/6 runs identical, with and without
  `LOF_NO_TIMER`, across clock hours; `tests/golden_test.sh` stays green.
- This is **not** a guess — the algorithm is in the decompilation — it is listed
  so no agent mistakes the randomness for underived behaviour. (An earlier
  version of this entry claimed combat "cannot be pinned to a bit-exact golden
  frame"; that was wrong — it was a measurement artefact of running emulators
  concurrently, not the game.)

---

## F. Resolved / removed

### G-011 — "is out of action." row: style, colour and advance (Lane M) — RESOLVED

- **Resolution (2026-11-20):** the row's drawer style (`1`) is the *same* style
  the decision header uses, and the header is present in every decision frame.
  Measuring the oracle header settles both unknowns: advance = **6 px** and
  colour = **index 8**.  The port already draws the row that way, so no code
  change was needed; a guard (`check_style1_colour_and_advance`) now pins it.
  Details under "RESOLVED" below.
- **CORRECTION (session 37, 2026-11-20) — the third drawer argument is a
  NEWLINE flag, not a "style".**  The measured rendering above (6 px, index 8)
  stands, but the *explanation* was wrong.  Reading the drawer end to end:
  `lcall 0x18d:0x16a8` (image `0x2F78`) takes **three** pushed words —
  `(offset, segment, newline)`.  The argument helper `0x5008` returns them in
  AX/BX/CX (`[bp+8]`, `[bp+0xa]`, `[bp+0xc]`); the drawer saves CX at `0x2F88`
  and at `0x2FC5` does `pop bx; cmp bx,0; je skip; call 0x18d:0x13be` — i.e. a
  non-zero third argument advances to the next text row *after* the string.
  That is why the round-result table's rows are 8 px apart: each row's last
  draw ("Passed"/"Failed"/"-") passes newline=1.  The *face* is the separate
  global `cs:0x1412`, set by `lcall 0x18d:0x1787` (image `0x3057`, verified at
  the call sites `0x1B32E` face 0 and `0x2ED2C` face 1); `cs:0x1408` /
  `cs:0x140e` are indexed by **that face** (face 0 → advance 6, face 1 → 5,
  matching `draw_text`/`draw_small`).  The *colour* is a third global,
  `cs:0x1414`, set by `lcall 0x18d:0x175a` (image `0x302A`) from the byte table
  at `cs:0x174c`.  The port needs no change (its round-result frame matches the
  engine's ink exactly), but do not cite "style 1" as the colour/advance source.
- **What:** on the combat decision screen a party member with a non-zero `+0x49`
  gets the action column replaced by the fixed string `ds:0x6FF2`
  "is out of action." (image `0x18839` → `0x18720`).  The **string**, its
  **position** (x=235, the action column) and the **suppression of the trailing
  number** are all derived from the decompilation.
- **What is not derived (superseded — now derived, see "RESOLVED" below):** what
  the drawer's style argument changes.  The two paths push *different* values:
  the action word pushes `0` (`0x18843` `sub ax,ax; push ax`), the `+0x49` path
  pushes `1` (`0x18720` `mov ax,1; push ax`).  The drawer (`0x18d:0x16a8`, image
  `0x2f78`) stores that argument in `cs:0x1412` (`0x2f8e`) and indexes two `cs`
  tables with it: `cs:0x1408` = `[6, 5, 12, 12, 10, 10]` (`0x2f95`) and
  `cs:0x140e` = words `{0x491b, 0x4921, …}` (`0x2fa1`).  The earlier reading
  that `cs:0x1408` is a per-character *advance* (6 px style 0, 5 px style 1) is
  **wrong** — the oracle header (also style 1) advances 6 px, so that table is
  not the advance.
- **Colour lead (obsolete):** `cs:0x140e` (style→descriptor word) was the
  suspected colour source; it is not needed now that the shared style-1 header
  pins the colour to index 8 directly.
- **To clear it (done):** the oracle header rows already use style 1, so no
  special mid-combat capture was required — measuring the header settled both
  advance and colour.
- **Reachable by patching the save (verified 2026-11-20).**  `+0x49` *is* a save
  field: save record offset `0x49` (i.e. file byte `4 + 0x49 = 77` for member 0,
  past the 4-byte save header) loads straight into the live record's `+0x49`
  (confirmed with a marker byte: patching file byte 77 with `0xAB` leaves
  `0xAB` at the live record's `+0x49`).  **Note the off-by-one trap:** file byte
  78 is `+0x4a`, *not* `+0x49`.
- **The draw branch is `!= 0`.**  The draw test at image `0x18839` is
  `cmp byte es:[bx+0x49],0` / `je <action word>`: **any non-zero value** shows
  "is out of action.".
- **CORRECTED (session 34, 2026-11-20): there are TWO `dec` sites of `+0x49`, not
  one.**  The previous revision claimed a single decrement at image `0x15C9`
  gated on `+0x1f == 5`, and used that to argue the countdown can never reach a
  rendered row.  A full sweep of the image finds exactly two decrements of the
  field:
  * image `0x15C9` `decb es:0x49(bx+si)` — the per-round tick, gated on
    `+0x1f == 5` (image `0x15AA`).
  * image `0x18415` `decb es:(bx)` where `bx` was computed as `record+0x49`
    (image `0x18403` `add ax,0x49`) — inside the round-start loop at `0x18145`.
    This one is gated only on the value being `> 0` (image `0x1840F`
    `cmpb $0 / jle`) and **has no `+0x1f` gate**, so it *can* tick a
    `+0x1f < 5` member's counter down.
  So the countdown *does* reach rendered rows.  The "disjoint sets" argument in
  the earlier revision is **retracted** — it was based on an incomplete sweep.
  The draw test is still `!= 0`, so the port rule is unchanged; what changes is
  that the counter is genuinely a countdown that expires over rounds.
- **RESOLVED (session 34, 2026-11-20): advance and colour are both derived now.**
  The drawer's style argument does **not** change the advance or the colour; it
  selects a **font/face**, and both styles render with a **6-px advance** and
  **palette index 8**.  Evidence:
  * *Advance = 6 px for style 1.*  The decision prompt `Please make your
    decision` (25 chars, style 1, image `0x16fb4`) inks x=139..287 in
    `ref/oracle/combat_decision.png` — 24 advances of 6 px + a 5-px glyph =
    149 px.  `for the next round!` (19 chars) inks 140..250 (18·6+6).  A 5-px
    advance would end far short, so the `cs:0x1408` table entry for style 1 is
    **not** the advance (the earlier reading of that table as an advance was
    wrong).
  * *Colour = index 8 for style 1.*  The three header strings (`The result of
    this round:`, `Name … Gld. Rt Exp. HP …`, the `----` rule — image
    `0x1875a`/`0x1877c`/`0x1878d`) all push style `1` and are inked at index 8
    on the index-7 background in the oracle.  `is out of action.` pushes the
    same style `1`, so it is index 8 too — the port's draw is **measured
    correct**, not inferred.
  * The earlier "emulator A/B is INCONCLUSIVE for the colour" reasoning was
    wrong: it looked for the colour on the *row* path (unreachable) instead of
    on the *header* path (present in every decision frame, same style).
- **What the values mean (derived, image `0x1219D` round-end bookkeeping).**  The
  field is written from the spell-effect id: effect `4` (Paralyze) → `1`,
  effects `5` and `0x27` → `5`, effect `0x28` → `3`, effect `0x1D` → `0xFF`
  (permanent).  A **broken weapon** ("... breaks down!", image `0x10389`) writes
  `0x14`.  So `+0x49` is a *cause code / countdown*, not a boolean.
- **Owner answer (see `docs/OWNER-TASKS.md` OT-004):** "out of action" is shown
  on the **combat decision screen** as the literal words `is out of action.` in
  the member's **action column** (x=235, where `(A)ttack!`-style action words
  normally sit), replacing both the action word and its trailing number.  It is
  reached when the member is paralysed/stunned/petrified (`+0x49 != 0`); the row
  only draws for `+0x1f < 5`, so it appears on a member who is Stunned/Stoned
  (`+0x1f == 5/6` are themselves skipped) once recovered enough to be listed but
  still carrying a running counter.  When *every* present member is out of
  action the round prints the party message `The companions have all been put
  out of action. Earliest revival will be at the beginning of the next round.`
  (ds:0x6F3F, image `0x185CE`).
- **Emulator A/B is INCONCLUSIVE for the *row* path — superseded for the
  colour.**  The rendered decision frame is byte-identical for `+0x49=0` and
  `=1` (0 differing pixels) because the decision screen is only reached with
  everyone able to act; the row branch needs a mid-combat state to be seen.
  The **colour is nevertheless settled** (index 8) via the shared style-1 header
  path — see the RESOLVED bullet above.  Only the row *branch* remains
  oracle-unreachable, and it is settled by the decompilation.
- **Port rule = the decompilation:** `!= 0`.  Since the branch is not
  oracle-reachable, Rule 1 (decompilation is the source of truth) governs.
- **Guard:** `tests/port_decision_test.py::check_out_of_action_any_nonzero` pins
  `!= 0` (values 1 and 2 must both differ from 0 in the action column).  It
  fails if the threshold is ever changed to `>= 2` again (verified).
- **Corrected claim.**  An earlier revision of this entry said patching the save
  "does not reach the branch" and that the record checksum blocks the load.
  Both were wrong: the first A/B ran without a pinned `LOF_TICK` (combat is
  tick-seeded, so the two runs were not comparable), and `load_save()` does not
  verify the record trailer at all — the shipped tails are `0` / `0x06060606`
  filler and do not match `rost_record_checksum`.
- **Owner impact:** none for the shipped saves (`+0x49 == 0` for all six
  members in both `GAME1` and `GAME2`), so the reachable screens are unaffected.
  The remaining residue is only the **colour**, which needs a DOSBox capture of
  the branch (the ground-truth oracle) rather than the emulator.

### G-009 — combat decision screen: source of the `KIL`/`ATK`/`DEF` column (Lane M)

**Resolved 2026-11-20 (thirty-third session) from the decompilation.**  The column is
`kCombatAbbrevNames[char[+0x24]]`, and `+0x24` **is** the combat rank — the
previous "body/health state" annotation was the wrong reading of the same byte.

- The row builder at image `0x18734` emits, per party slot: the 1-based index
  (`"%ld"`, colour 8, x=139), `"%-10s "` name (x=145), then the rank through a
  **4-entry far-pointer table at image `0x187F0`** indexed by `+0x24`, which
  selects the strings at `ds:0x0EB0` = `KIL `/`ATK `/`DEF `/`RET `
  (file `0x42BC0`, four 4-byte slots).
- `+0x24` read/write sites: read at `0x197C7` and `0x1D323`, written at
  `0x197F0`, and compared at `0x1913C`/`0x1916C`/`0x1919C`/`0x191CC`.
  **The engine names the four values itself:** those compares gate the strings
  `ds:0x713C` `(1)Enter Kill-Rank`, `ds:0x714F` `(2)Enter Attack-Rank`,
  `ds:0x7164` `(3)Enter Defence-Rank`, `ds:0x717A` `(4)Enter Retreat-Rank`, and
  the write at `0x197F0` stores the digit that was pressed minus `'1'`.  So
  `+0x24` is a 0-based rank selecting KIL/ATK/DEF/RET, in the engine's own
  words — no inference left.
- The older annotation called `+0x24` "body/health state" and read those four
  compares as a dying/dead test.  That was wrong: the compares are the
  rank-entry menu.  `notes-b.md` and `port/char.h` are corrected to match.
- The save confirms it: GAME2's six members carry `+0x24` =
  `[0,0,1,2,2,2]`, which is exactly the oracle column `KIL KIL ATK DEF DEF DEF`.
  The predecessor's inconclusive save-patch experiment is now unnecessary: the
  table lookup is read straight out of the image, and the shipped save already
  exercises three of the four entries.

**Fix (this session):** `compose_decision_screen()` (`port/lof.cpp`) indexes
`combat_abbrev_names()` with `+0x24`; `tests/char_test.cpp` pins the mapping and
the row guards (`+0x1f < 5`, `+0x49 == 0`) against the real save;
`tests/port_decision_test.py` pins the rendered frame.  The `kCharBody24`
annotation in `port/char.h` now reads "combat rank … also the health state".

### G-010 — combat decision screen: meaning of the trailing per-row `1` (Lane M)

**Resolved 2026-11-20 (thirty-third session) from the decompilation.**  The trailing
number is `+0x0d + 1` (or `+0x16 + 1`), printed by the `"@ %ld"` format
(`ds:0x6FE6`/`ds:0x6FEC`) that follows the action field.

- Image `0x18880`: `mov al,es:[bx+0x16]; cmp al,0xff; jne .other` — when
  `+0x16 == 0xff` the engine takes the `ds:0x6FE6` path with `+0x0d + 1`
  (`mov al,+0x16; cbw; inc ax` at `0x18892` is the `.other` branch, `+0x16+1`).
- The `+0x0d + 1` branch is guarded (image `0x186F3`) on signed `+0x0d` being in
  `0..2` **and** `+0x0f != 0`; outside that range nothing is printed.  A
  non-zero `+0x49` takes a different path (image `0x18839`): the action column
  is replaced by the fixed string `ds:0x6FF2` "is out of action." and the
  number is not printed at all.
- The action word is emitted as a `"%-10s "` field, so the `"@ %ld"` cursor
  starts 10 cells (60 px) past x=235, i.e. x=295.  `@` and the space advance
  6 px each but have no glyph, so only the digit inks — at x=307 (measured ink
  x=308..311), colour 8.  All six GAME2 rows have `+0x0d = 0`, `+0x0f = 1`,
  `+0x16 = 255`, hence `1` on every row.

**Fix (this session):** the composer reproduces the format arithmetic (cursor at
`295 + 2*6`, the two uninked cells) and both branches, including the guard and
the `+0x49` "is out of action." override.
`tests/port_decision_test.py` pins the digit placement; `tests/char_test.cpp`
pins the `+0x16 == 0xff`, `+0x0d + 1 == 1` inputs for all six rows.

### G-001 — town name "Thyn." hardcoded (Lane M)

**Resolved 2026-11-15 (fourteenth session) from the decompilation.**  The town
name is selected by the engine, not hardcoded, and the selector is fully decoded:

- The town routine (image `0x53E0`) prints `"You enter the proud town of"`
  (`ds:0x5A1D`), then tests the 32-bit cell `es:[0x460e]` (loaded via
  `[ds:0xaf48]`): `mov ax,es:[0x460e]; or ax,es:[0x4610]; je .cyldane;
  mov ax,0x3D29 ("Thyn. "); … .cyldane: mov ax,0x3D30 ("Cyldane. ")`, then
  `lcall 0x18d:0x16a8` (the string drawer).  So **nonzero → "Thyn.", zero →
  "Cyldane."** — the only two names in the EXE (`ds:0x5A39`/`ds:0x5A40`).
- The cell is filled by the save loader (image `0x1C952`):
  `mov ax,es:[bx+0x9a4]; cdq; mov es,[0xb00a]; mov es:[0x460e],ax;
  mov es:[0x4610],dx`.  So the selector is exactly **the 16-bit word at save
  offset `0x9a4`** (sign-extended into a dword).  Confirmed by watchpoints: the
  boot initialiser (image `0x179`) writes `1` (→ Thyn, no save), and the loader
  overwrites it with the save word (Thyn save → `1`, Cyldane save → `0`).
- The second `"Thyn"` (`ds:0x687D`, "The Grand Board of Trades greets you in
  Thyn.") is a **fixed immediate** (`mov ax,0x4B5F`, image `0x876D`), not a
  selector: the House of Trades greeting always says "Thyn" in the engine.

**Fix (this session):** `compose_town_screen()` now takes the name as a
parameter, chosen by `town_name_from_save()` from the word at `0x9a4`
(`port/lof.cpp`).  `port_town_test` pins both names: the Thyn frame must stay
pixel-exact against `ref/oracle/town.png`, and GAME4 (Cyldane) must render
"Cyldane." and match `ref/oracle/town_cyldane.png` in the message block (ink
mask, because that oracle uses a faded entry-route palette).  The predecessor's
"computed base + idx*7" hypothesis is superseded: the name is a plain
address-select on one save word, no table indexing.

### G-003 — `item_is_usable()` default for `spell == 0` non-potions (Lane B)

**Resolved 2026-11-14 by owner live play.**  Using an ordinary item in the
item-use menu prints "<item> is not useful in this situation." (+ a "(C)ontinue"
prompt), so the engine treats a `spell == 0` non-potion as **not usable**.
`item_is_usable()` returning `false` matches it, so the value is derived (live
play is a Rule-1 source of truth) rather than guessed.  The decompilation agrees:
the gate's verdict word `[bp-4]:[bp-2]` (image `0x0E562`) is left unwritten on
this path and the "is not useful in this situation." string (`ds:0x5C52`, loaded
at image `0x0E1A9`) is what the engine prints.

**Bonus from the same live-play report:** a **lantern** used with a flask of oil
and a tinderbox consumes the oil and prints "… make light…", grounding item
effect value 34 (lantern) as a light source; recorded under G-004.

*(The original watchlist entry moved here from section B.)*

### G-002 — 16×16 patch at the town screen's top-left corner (Lane M)
- **What:** the town screen (`--town`, `compose_town_screen()` in `port/lof.cpp`)
  matches the emulator frame at 99.2%; the residue is an 88-pixel 16×16 patch at
  rows 0..15 / cols 0..15 that the engine draws over `EGA/UP/MSCR`.
- **Why it looks like a guess:** the port currently just leaves `MSCR` there
  (i.e. it renders *something* not derived from the engine's actual draw), so the
  patch is an underived value.
- **Evidence:** the patch is not `MSCR`, not `TWN00`, not any single `EGA/**`
  asset, and not a shifted/wrapped copy of either (best match ≈0% on that patch;
  the oracle patch is a diagonal arrow shape). See
  `docs/notes-maintainer-town.md`.
- **New evidence (2026-11-13):** the *same* patch appears in both the town frame
  and the main-menu frame (the two frames are identical in rows 0..10 of the
  corner), so it is **not** a town-specific sprite. It also differs from `MSCR`
  in **both** DOSBox and the emulator, so it is not a decode artefact — it is
  something the engine draws over `MSCR`. Template-matching every `EGA/**` asset
  at every 4-px offset found **no** match (<25 mean-abs threshold). This points
  firmly at an engine-drawn sprite/algorithm (→ trace the routine).
- **Why unclear:** it may be removable by tracing the routine that draws the town
  screen (a small sprite blit / arrow) in `analysis/lof.asm`; not yet attempted.
- **Progress (2026-11-15, fourteenth session) — the draw site is found.**  A
  `LOF_PXWATCH=5,5 LOF_PXWATCH_N=40` run of the `town` scenario (and of the boot
  menu) shows the patch pixel is written by a **masked EGA sprite blitter** whose
  store is `mov word es:[bx+di],dx` at runtime `119D:0463` (image ~`0x1D30`; the
  surrounding code at image `0x1CF2..0x1D38` does the per-byte read-modify-write:
  it builds a run mask in AX from `ds:[bp]`, a shift `cl`, and the sprite byte
  `[si+0xa]`, then `and dx,ax; or dx,cs:[0x37e]; mov es:[bx+di],dx`).  The
  **same pixel is redrawn by two more stores** in the same segment
  (`119D:0B95`, `119D:0FC8`) — i.e. the patch is blitted more than once (layers).
  Next step: break at the blit entry (image `0x1CF2`) and capture the caller-set
  `si` (sprite source), `bx` (plane base), `cx` (width/height) to recover the
  sprite bytes, then reproduce the blit in `port/lof.cpp`.  The patch is
  **cosmetic** (88 px, 0.14% of the screen); it is the *only* residue besides the
  dynamic party panel, so it is not urgent.
- **To clear it:** capture the sprite + dest at the blit entry and reproduce it.
  If the source turns out to be a data-table sprite → move to **D**. If it is
  computed with no recoverable source → move to **A**.
- **Progress (2026-11-16, fifteenth session) — the sprite is recovered; the
  remaining gap is the *selector* that chooses its source.**
  - The `119D:0463` blitter is the **generic EGA-image blitter** (image
    `0x1C70`, the one AGENTS.md calls "the blitter that consumes the decoded
    buffer"): it does `mov ds,dx; cmp [si],0x1234` and reads a 4-word header at
    `si` (magic, width-bytes, height, stride), so `si` points at a **decoded,
    11-byte-header-less EGA image** in memory.  At the patch blit `si` starts at
    `0x0A` (right past the 10-byte header) and steps `+0x20` per plane
    (`bp` = plane, 4 passes) → the sprite is a **16×16, 4-plane, 0x20 bytes/plane
    image at `ds:0`** with header `[0]=0x1234 [1]=2 [3]=0x10 [5]=0x8a`.
  - A `LOF_PXDUMP` snapshot at the blit (`ds` scratch buffer) reproduces the
    oracle patch exactly (ink-mask), so the blit is understood.  The sprite is
    **not** in any `EGA/**` asset, **not** in `LOF.EXE`/`VOR.EXE`, and **not** in
    any game data file — it is produced at runtime.
  - `LOF_WATCH=<linear of ds:0>` shows the scratch buffer is filled by the
    **general EGA-image RLE decoder** at image `0x22F2` (it reads
    `ds = cs:[0x35a2]` and writes to `es = cs:[0xa06]`).  So the patch is a
    **compressed EGA image decoded at run time** into the scratch buffer, then
    blitted — not a `EGA/**` file (a different, in-memory loader feeds it).
  - **Remaining task:** the decoder's source pointer `cs:[0x35a2]` is set once
    early to segment `0x4130`, which at run time holds the 120×119 view image
    (`[esc]34 12 [w=15][h=119][size=0x1bee]`), so the patch's own source is
    selected elsewhere; find the routine that stages the 16×16 image (watch the
    decoder's other state, `cs:[0xa1c]` length and `cs:[0xa06]` dest) and dump
    its source bytes.  That yields the exact sprite to bake into `port/lof.cpp`.
    The image is drawn identically (ink) in menu/town/buildings/dungeon, so a
    single fixed overlay clears G-002 for every explore screen.  Cosmetic: 88 px.
  - New emulator diagnostics added this session: `LOF_PXWATCH` now also prints
    `si/di/bx/bp/ds`, and `LOF_PXDUMP=<file>` (`LOF_PXDUMP_AT=<hit>`) snapshots
    the DS segment at a watch hit.
- **RESOLVED (2026-11-17, twentieth session).** The patch is the game's **software
  mouse cursor**, not a scene sprite — so it was never a guess, only an
  un-identified draw. The engine draws its own arrow: routine image `0x2C2F`
  polls `INT 33h` fn3 and, when the pointer moves, calls `0x2758`, which blits a
  fixed 4-plane 16×16 sprite at the polled mouse position. The engine never asks
  the BIOS to draw the cursor (only fn0 reset at image `0x1E85` and fn3 poll at
  `0x1F10` are used), so the shape is engine data.
  - **Why the emulator lacked it:** the emulator's mouse never moves, so the
    "pointer moved" branch is never taken and no cursor is drawn. DOSBox
    rasterises it, and every reference capture was taken with the mouse parked at
    **(0,0)** — hence the patch sits in the top-left corner of the affected
    frames (bank/emporium/house/menu/menu2/town/town2/dungeon_slot1[_moved]) and
    is absent from the ones where the capture had no cursor (play/shop/wilderness/
    charsheet/combat/amulet/town_cyldane).
  - **Fix:** the sprite was recovered byte-exact (the scratch-buffer dump the
    prior session produced) and baked into `port/lof.cpp` as `kCursorSprite` /
    `kCursorMask` + `draw_cursor()`, exposed via a new `--cursor X,Y` flag.
    `--cursor 0,0` reproduces the reference captures exactly: town / emporium /
    bank / house are now **100.0%** static match with the corner **pixel-exact**;
    the menu residue drops to 14 px (the pre-existing wrapped-strip seam). Tests
    `port_town`/`port_building` now pin the corner (previously masked) and use
    `--cursor 0,0`; `port_menu`'s tolerance tightened 300 → 20. Merged as PR #129.
  - The `LOF_PXDUMP` note above (decoder `0x22F2` staging the sprite) is the
    same mechanism seen from the decode side: `0x2758`'s staging path runs the
    general EGA-image decoder to expand the 16×16 cursor image into the scratch
    buffer, then the generic blitter (`0x1C70`) draws it. No data-table sprite is
    involved, so this resolves to **D** (data-file/asset) — the fixed overlay is
    the correct reproduction.

### G-008 — encounter viewport sprite placement (Lane M) — RESOLVED

- **Status: RESOLVED (2026-11-18).**  The encounter sprite is blitted **plainly
  at the viewport origin (10,11)** — no tiling, no (56,3)/(56,9) offset.  Every
  `EGA/CHAR0/CH<nn>` creature cell is a full 120x119 image.  Both captured
  encounters are exact at (10,11), and (10,11) is the *unique* best offset in a
  brute-force search:
  * `ref/oracle/combat.png` → `CHAR0/CH24` (Mercenary): **1.0000 at (0,0)** window
    offset (= screen (10,11)), the only offset ≥ 0.9999;
  * second encounter (`LOF_TICK=0x7777`) → `CHAR0/CH26`: **1.0000 at (0,0)**,
    the only offset ≥ 0.9999.
  The earlier "(56,3) tiled / (56,9) varies" reading was a symptom of the
  byte-11 decoder truncation: `CH24`/`CH26` decoded short, so the missing
  bottom-right was being "filled" by wrapped copies at a shifted origin.  With
  the decoder fixed (PR #145) the plain origin is exact for both.  `port/lof.cpp`
  now does a single `blit_ega(..., kViewportX, kViewportY)`.
- **Bank confirmed (not part of the guess):** both seeds open the **CHAR0** bank
  (`ega\char0\ch24`, `ega\char0\ch26`), so `NSC[record].+0x48 -> EGA/CHAR0/CH<nn>`
  is grounded across two encounters; the `CH` number is RNG-selected by monster
  placement (see BLK-A005 / the RNG entry in section E), not by the save.
- **To clear it:** read the encounter-draw routine in `analysis/lof.asm` to find
  how it computes the sprite's `dy` (the writes go through the shared plane
  blitter `119D:129F`, so the position is computed earlier); or capture a third
  encounter and fit the rule.  Until then the port is exact for the pinned
  oracle and *approximate* for other monsters.

### G-006 — open-corridor dungeon view falls back to `DP001` (Lane A)

- **Status: RESOLVED (2026-11-17).**  The corridor view is a *depth-marched
  single-sprite blit*: depth 1 → the tileset `DPnnn`, depth 2 → `CHAR2/CH40`,
  depth ≥3 → `CHAR2/CH25`, **all blitted at the viewport origin (10,11)** (every
  view cell is a full 120x119 image).  The DOSBox oracle
  (`ref/oracle/dungeon_slot1_moved.png`) matches `CHAR2/CH25` at (10,11) at
  **1.0000**.  The earlier `(58,27)`/`(-14,28)` offsets and the x-stride-120
  tiling were a workaround for the byte-11 decoder truncation, now fixed (see
  `docs/blockers/A.md` BLK-A005 decode-correction log).  `dungeon_base_view_index()`
  is unchanged (it is the depth-1 case); `dungeon_view_for()` implements the
  march.  See `AGENTS.md` "First-person viewport" and `docs/notes-a.md` Round 47.
- **What it was:** `dungeon_base_view_index()` returned `DP001` for the open-cell
  ahead codes `0x0/0x2/0x7/0x8/0x9`, because no single `DPnnn` reproduces the
  corridor case.  It is now handled by the depth march instead of a fallback.

### G-013 — the outdoor horizon-band piece set is measured on one cell only

- **Status: UNCLEAR — revisit (owner decision likely needed).**  `kOutdoorPieces`
  in `port/dungeon.cpp` (DP012 at (0,49), DP004 at (15,46), DP005 at (90,50)) is
  applied whenever the outdoor (D0) base resolves to DP001.  It reproduces the
  one oracle frame (`ref/oracle/play.png`, cell 745) at 1.0000, but a re-sealed
  save sweep (notes-a Round 50) shows it does **not** hold for other cells:

  | cell | DNG10 tile | depth | vs DP001 | vs shipped composite |
  |------|-----------|-------|----------|----------------------|
  | 745 (oracle) | 0x00 | 2 | 0.9076 | **0.9926** |
  | 636 | 0x0c | 3 | 0.8931 | 0.9463 |
  | 673 | 0x03 | 9 | **0.9168** | 0.9265 |
  | 492 | 0x0c | 2 | 0.7975 | 0.7842 |

  (composite scores use the port's transparent-overlay rule; the 745 column also
  shows the opaque-paste score, which is what the naive comparison used.)

- **What is established:** the base view for an outdoor cell is DP001; the
  horizon band carries per-cell scenery (trees/buildings) drawn from the same
  D0 bank; cells 636 and 673 are *closer to bare DP001* than to the 745 piece
  set, so the pieces are not a fixed overlay.
- **What is not established:** which map property selects the scenery.  The
  obvious candidate (the ahead cell's tile code) does not separate them: the
  depth march gives 2, 3 and 9 for 745, 636 and 673 respectively, yet all three
  draw the base view.  The remaining possibilities are the cell's object/feature
  record, the surrounding tile pattern, or a scenario table.
- **Why it is not a normal task yet:** the piece rectangles are identified by
  template match against one frame; there is no decoded rule (Rule 1) and no
  second cell where the *same* piece set is confirmed, so any generalisation
  would be a guess.  The port keeps the measured set and stays pixel-exact for
  the pinned oracle.
- **Refined by Round 50b (partial answer, still not fully derived).**  The
  ahead-nibble rule *does* apply outdoors, but D0 has no `DP060`, so the wall
  nibble needs a D0-specific target and there are two of them:

  | cell (DNG10) | facing | ahead | behind the wall | best match |
  |--------------|--------|-------|-----------------|------------|
  | 37 | N | `0x06` | off-map (y=0) | `D0/DP036` **0.9648** |
  | 38 | N | `0x06` | off-map (y=0) | `D0/DP036` **0.9648** |
  | 39 | N | `0x06` | off-map (y=0) | `D0/DP036` **0.9648** |
  | 10 | W | `0x06` | more wall (`0x06`x3) | `D0/DP001` **0.9499** |

  So "wall that ends the map -> `DP036`, wall with depth behind it -> `DP001`"
  is measured on four cells and is now implemented (and pinned by
  `tests/dungeon_test.cpp`).

- **PARTLY RESOLVED (Round 51).**  The "overfit" residual on the oracle cell was
  diagnosed: the composite was 106 px wrong, and those 106 px are a **map object**
  (`EGA/UP/UP22` at window (70,53)), which the port was not drawing.  With it the
  oracle window is **0 px wrong**, and the trigger is the cell's object record
  (cells with `obj=(0,0)` get *worse* if it is drawn).  So the horizon band was
  never fitted noise — all three pieces are necessary under ablation.

  Still a guess: (a) whether the band is the same for every outdoor cell —
  cells 636/673 score 0.9463/0.9265 with no object, so something else varies;
  (b) the object-code -> `UPnn` mapping (only code `1` seen, on one cell).

  What is **not** settled is the rest of the D0 table: a doorway (`0x0a`, cell 95)
  matches `D0/DP002` only 0.5569, i.e. it is a composite the port does not build,
  and the open-cell cases are still `DP001` fallbacks.
- **To settle it:** locate the outdoor draw routine in `analysis/lof.asm` and
  read which table supplies the base sprite and the scenery.  Note `0x3B72` is
  **not** it — that is a data offset passed to the string printer
  (`0x18d:0x16a8`).
