# HANDOFF — read this first

You are one of several agents re-implementing the 1990 DOS RPG *Legend of
Faerghail* as native Ubuntu C/C++. This file tells you everything you need to be
productive in the first ten minutes, and how to survive a workspace reset or a
dead-end error. Deep detail lives in `AGENTS.md`; the work split lives in
`docs/WORKSTREAMS.md`.

> **Before anything else: read `docs/RULES.md`** (the owner's standing rules and
> their dated changelog), `docs/GUESSWORK.md` (the guess register), and
> `docs/OWNER-TASKS.md` (work only the owner can do; lane M re-reminds the owner
> every session). The single most important rule is **never guess** — see Rule 1
> there. Rule changes are announced in the `docs/RULES.md` changelog.

> **Incoming maintainer? Read `docs/notes-maintainer-state.md` first** — it
> records the last session's landing of the play-loop work and the current open
> items.

## 0. Sixty-second orientation

```sh
./bootstrap.sh            # restores game data + builds + tests (idempotent)
make                      # build the analysis emulator  -> ./lof-native
make port                 # build the drop-in binaries   -> ./lof ./vor
make test                 # CPU self-test + font test
make check                # health: build, tests, git hygiene
```

### Minimal prompt to start (or replace) an agent

Anyone can spin up a fresh agent with just this:

> You are working in `FreeOpenSourceSyndicate/lof2`. Start by running
> `./bootstrap.sh` and reading `README.md`, then `docs/HANDOFF.md`,
> `docs/RISKS.md`, and `docs/WORKSTREAMS.md` — they tell you everything,
> including how to pick your own work item. Work in long, uninterrupted
> stretches — keep pulling items off the queue in `docs/HANDOFF.md` rather than
> stopping after one step. Do not edit a file another active agent is working on
> (pick a queue item in a disjoint area). Commit and push after every meaningful
> step (the workspace can reset). Review and merge your own PRs. If you get
> blocked, add an entry to
> the area's blocker file under `docs/blockers/` (A = dungeons, B = characters,
> M = shared/engine; the ID convention and fields are defined in the hub
> `docs/BLOCKERS.md`) instead of just stopping.

That prompt plus the repo is all a replacement agent needs — no prior context.
The agent picks its own item from the queue in `docs/HANDOFF.md` §4; you do not
have to point it at one.

### How to work (do this, do not wait to be told)

- **Prefer a queue item in an area you already know.** If you have worked on
  dungeons, characters, or the engine before, your accumulated context is worth
  more than the marginal value of a different area.
- **Work in long, uninterrupted stretches.** Push often for reset-safety, but do
  not stop after one small step to await the next prompt. Keep pulling items off
  the queue in §4 until you are blocked or the work is finished. Idle agents cost
  the project the most time; when the queue item you took is done, take the next
  one, or add a regression test for an entry in `AGENTS.md`'s "do not regress"
  list.
- **You own your PR end to end** — create it, review it, and squash-merge it when
  `make check` is green (see below). Do not wait for the coordinator to merge.

### Merging your PR

**Canonical rule: `docs/RULES.md` Rule 10** — agents create, check, push, and
merge their own PRs; the repository owner never does repository mechanics and
must not be asked to. You may review and merge pull requests yourself — you do
not need the maintainer.

1. Keep the branch green: `make check` must pass.
2. `gh pr create` (or the API) against `main`.
3. Wait for the mergeability check, then merge:
   ```sh
   gh pr merge <n> --repo FreeOpenSourceSyndicate/lof2 --squash
   ```
   (or the async API: `PUT /repos/.../pulls/<n>/merge-async`). Do not merge a
   branch whose `make check` fails.

- `src/`  — an 8086/EGA **emulator**. It runs the *original* `LOF.EXE`/`VOR.EXE`
  so we can observe behaviour and verify our re-implementation. It is a
  **development oracle only** — the shipped drop-in must not use it.
- `port/` — the **deliverable**: native C/C++ that reads the game *data* files
  and reproduces the engine. Never executes the original EXEs.
- `tools/` — Python helpers (`ega.py` decoder, `decompile.py`, capture script).
- `analysis/` — annotated disassembly listings (`lof.asm`, `vor.asm`).
- `game/`, `downloads/` — proprietary game data, **git-ignored** (see risks).

## 1. The hard constraint

1. **Never execute `LOF.EXE` / `VOR.EXE` from the deliverable** (`port/`). Reading
   their bytes for analysis is fine; running them is the emulator's job only.

## 2. You WILL hit these risks — plan for them

### Risk A — your workspace can be reset without warning

Everything you have not committed and pushed is gone. Mitigation:

- **Commit and push after every meaningful step**, not at the end of the task.
  Small commits are cheap; lost work is not.
- Never rely on `/tmp` for anything you need to keep. Put durable notes in the
  repo (e.g. `docs/notes-<yourname>.md`) and push them.
- `game/` and `downloads/` are git-ignored, so a reset loses them. `./bootstrap.sh`
  restores them in one command. Run it first thing after any reset.
- Your *branch* is safe once pushed. Prefer pushing your own branch
  (`work/<yourname>`) over long-lived local-only work.

### Risk B — you can hit an error you cannot recover from

The usual cause is a change to shared code (`src/`, `port/`, `Makefile`) that
breaks the build or the tests, leaving you with no green state to fall back to.
Mitigation:

- **Never leave the tree red.** If you cannot fix a break quickly, `git stash` or
  `git checkout -- <file>` to return to the last green commit, then push a note.
- **Do not edit files another active agent is working on.** There is one lane now
  (`docs/WORKSTREAMS.md`), so you may edit any file — but if other agents are
  running, pick a queue item whose **area** is disjoint from theirs (the queue in
  §4 lists the area per item). Colliding on a shared file is how builds break.
- **Keep `make test` green.** It is the shared contract. If your work needs a new
  test, add it to the test target in `Makefile` (area M; ask the maintainer if you
  are not the maintainer).
- If you are truly stuck, **add an entry to the area's blocker file** —
  `docs/blockers/A.md` (dungeons), `docs/blockers/B.md` (characters/items) or
  `docs/blockers/M.md` (shared/engine) — with the blocked area, responsible
  agent, priority, precise description + what unblocks it, and dependencies (the
  fields and the `BLK-<area><nnn>` ID convention are in the hub
  `docs/BLOCKERS.md`) and push it. Add a `docs/BLOCKED-<yourname>.md` detail file
  when the diagnosis is long, and link it from the registry entry. A fresh agent
  (or the maintainer) can then pick it up with no context loss — that is the
  whole point of the "any agent replaceable" design.

## 3. How to prove your work is correct

The emulator is the oracle. For any screen or behaviour you implement in `port/`:

1. Run the original under the emulator and capture a frame:
   `LOF_CMDLEN=3 ./lof-native --dir game --headless --frames 300 --shot /tmp/oracle.png`
2. Render the same thing with the drop-in:
   `./lof --dir game --headless --shot /tmp/mine.png`
3. Compare (exact or near-exact):
   `python3 tools/compare.py /tmp/oracle.png /tmp/mine.png`

A subsystem is "done" when the drop-in matches the oracle frame for the relevant
screen, and `make test` still passes.

## 4. Work queue (pick the top unclaimed item)

This is the **single queue** for the whole project — there is one lane
(`docs/WORKSTREAMS.md`), so **every** agent pulls from here. The **Area** column
says which domain an item touches; when more than one agent runs, agents pick
items whose areas are **disjoint** so two never edit the same file. Claim an item
by pushing `work/<yourname>` with a one-line claim at the top of your notes file.

| # | Area | Item | Ground truth / next step | Files | Status |
|---|------|------|--------------------------|-------|--------|
| 1 | M | **G-002** — the 16×16 run-time patch sprite | **DONE (2026-11-17)**: it is the engine's **software mouse cursor** (routine `0x2C2F` polls INT 33h fn3 → `0x2758` blits a fixed 16×16 arrow). Recovered byte-exact and baked into `port/lof.cpp` (`kCursorSprite`/`kCursorMask`/`draw_cursor()`), exposed as `--cursor X,Y`; `--cursor 0,0` reproduces the oracle (town/emporium/bank/house 100%, corner exact) | `port/lof.cpp`, `tests/port_*` | **done** |
| 2 | B | **BLK-B005** — ROST per-record checksum | **DONE (2026-11-16)**: `rost_record_checksum()` + `intact` flag in `load_rost()`; tested in `tests/char_test.cpp` (synthetic + real fixtures) | `port/char.{h,cpp}`, `tests/char_test.cpp` | **done** |
| 3 | A | **BLK-A001 / BLK-A005** — first-person viewport composite | **DONE (2026-11-18)**: the view is a *depth-marched single-sprite blit* — depth 1 → `EGA/D<d>/DP<index>` @ (10,11), depth 2 → `EGA/CHAR2/CH40` @ (10,11), depth ≥3 → `EGA/CHAR2/CH25` @ (10,11) (every view cell is a full 120x119 image; the old (58,27)/(-14,28) + x-stride-120 tiling was a byte-11 decoder-truncation workaround, fixed in PR #145), where depth = first cell ahead with low nibble ∉ {0,2,7,8,9}. Pixel-exact vs the DOSBox oracle (spawn `DP001` @ (10,11); moved `CH25` @ (10,11) matching **1.0000**). Wired into `--walk`/`--ui`; `tests/dungeon_test.cpp::test_view_depth` + `tests/port_dungeon_view_test.py` (floor 0.998). **Encounter-id producer (`es:[0x259]`) — RESOLVED, closed as not-portable-by-data (2026-11-18, Round 51):** the producer (routine R, img `0x276CA`) is fully disassembled, but capturing its *input* with the new `LOF_CAP` hook shows it reads a runtime **monster-placement table** built with the game RNG (BIOS-tick seeded) — the id is not a function of the save (measured: seed `0x0B` → `CH25`, seed `0x7777` → `CH40` on the same Temple cell). The port's depth march is the faithful, save-independent model; no port change is warranted | `port/dungeon.{h,cpp}` | **done** |
| 4 | M | **In-game play loop** — movement + combat in `--ui` | **movement DONE (2026-11-17)**, **combat DONE (2026-11-21)**: the `--ui` PLAY state walks on arrows and `C` now runs the whole round — decision prompt → `resolve_round()` → round-result table → back to play. The round model is fully derived (`port/combat.{h,cpp}`): the melee path a real attack takes is the **fallback at `0x14D80`** (`record[+0x16] == 0xff` for all six shipped characters, proven with `LOF_TRACE_IP=24840:24a60`), which rolls `min(10,(+0x5d + +0x56)/+0x55)` attacks, sums the ones beating the prelude's hit score, scales by `100/record32[+0x4e]`, divides by `+0x55`, caps at 99 and applies `victim[+0x0a] -= total` (`0x1547E`) + XP (`0x1549D`); the generator is the MS-C LCG at `0x30ECC`. `lof --fight` exposes it. **Still open** (GUESSWORK G-012): the prelude's hit score needs the round-setup builder `0x27F14`; the hand-slot weapon pick (`rand(2)` at `0x13A43`); the XP multiplier table `[0xafc6]:0xf37+…`; the W/P/B event writes; the monster path `0x159F8`; the quick loop `0x17BAA` | `port/lof.cpp`, `port/combat.*`, `port/char.*`, `tests/combat_test.cpp`, `tests/port_fight_test.py`, `docs/notes-b.md` | **done (model)** |
| 5 | M | **Inn & Temple screens** | **DONE (2026-11-18):** neither is a `D5` first-person interior. The **Temple building screen** is town-style (`EGA/UP/MSCR` + `EGA/TWN/TWN05` viewport scrolled `x0`=40 + `CHR1` text at x=139) — `compose_temple_screen()` + `--temple`, pixel-exact vs the emulator `temple_bldg` frame. Its **(H)eal / (R)evive** options open a character-selection sub-prompt over the *same* viewport (`compose_temple_prompt()` + `--temple-heal`/`--temple-revive`; strings "Whose wounds should / we heal (1-6)?" and "Who should be revived / (1-6)?" + "(C)ontinue"). The **Inn screen is the main menu** (the game starts inside the Inn): the town `I` key renders a frame byte-identical to `ref/oracle/menu.png` (1.0000); its `(R)est` sub-prompt (`compose_inn_rest_screen()` + `--inn-rest`; "Would you like a simple / nights rest …" + `(S)/(W)/(J)` options) is over the same scrolled `TWN03` viewport the menu uses. Guarded by `tests/port_building_test.py` + `tests/port_inn_test.py` | `port/lof.cpp`, `tests/port_building_test.py`, `tests/port_inn_test.py` | **done** |
| 6 | B | **BLK-B004** — combat action dispatcher / round resolution | **RESOLVED (2026-11-18)**: dispatcher + round state grounded; printer at image `0x186A3` (fn `0x1810E`). The `W P B` **meanings are derived from the decompilation** — `W`=weapon damaged, `P`=shield/armour damaged, `B`=an ability improved, `Attack`=Passed/Failed (setters paired with the weapon/shield/armour slot writes; messages `"The weapon of …"`, `"However, the shield/armour of …"`, `"(Defensive ability grows!)"` etc.). **Correction:** an earlier note used the wrong DS string base `0x40000`; the correct base is `0x41D10` (runtime `ds=0x4C92` at the setter). Owner play corroborated `P`; OT-003 (optional `W`/`B` corroboration) **closed as not-needed** — the table shows only in quick combat, the messages only in regular rounds, so they cannot be cross-checked by eye | `port/char.*`, `docs/notes-b.md` | **done** |
| 7 | M | **Regression tests** for `AGENTS.md` "do not regress" | **DONE (2026-11-16)**: `test_keyboard_full_row()` (full unshifted Set-1 row + modifiers) and `test_ega_detect_int10_12()` (INT 10h AH=12h BL=10h → BX=3/CX=0, non-EGA subfn → AL=0) added to `tests/cpu_test.cpp` | `tests/cpu_test.cpp` | **done** |
| 8 | M | **check.sh apparent hang** | **DONE (2026-11-16)**: `check.sh` invoked `tests/golden_test.sh` in both the `if` and the `elif`, so every non-ok result re-ran the whole ~22 min emulator sweep (≈44 min). Capture the output once and branch on it. **Follow-up (2026-11-18): the sweep itself was genuinely slow (~24 min serial).** The 12 scenarios are independent and deterministic, so `golden_test.sh` now runs them on `GOLDEN_JOBS` workers (default `nproc`, longest-first) and emits the hashes in the recorded order — **~615 s at 4-way, golden.txt byte-identical**. The per-run `timeout` also scales with the pool width (`300*jobs`), fixing a latent bug: the `temple` scenario takes ~247 s alone, so the old fixed 300 s ceiling had only 53 s of headroom and a loaded box produced a bogus empty-dump hash. A timed-out run now reports `FAILED(timeout=Ns)` instead of `md5` of nothing | `tests/golden_test.sh` | **done** |
| 9 | M | **`--walk`/`--ui` spawn guess `(0,0)`** | **DONE (2026-11-16)**: PR #117 corrected the wall set to `{6,13}`, which made DNG05's `(0,0)` a wall, so the hard-coded `dungeon_place(...,0,0)` walked outward to `(3,2)` and `port_walk_test.py` went red (hidden by the doubled golden run). Place the party at the DNG entry cell (`dungeon_place_entry()`, Round 22 = the save's `0x9a0`/`0x9a6`); fix the test's stale `(0,0)` expectations | `port/lof.cpp`, `tests/port_walk_test.py` | **done** |
| 10 | M | **Combat decision screen** (`Please make your decision` / `for the next round!`) | **DONE 2026-11-20.** Implemented as `--decision` (`compose_decision_screen()`). The screen is the encounter composition plus the round text block, proven by the two oracles sharing viewport/roster/bottom exactly. Rows are the engine printf sequence at image `0x18734`: index, `"%-10s "` name, rank, action, `"@ %ld"`. **`+0x24` is the combat rank** (KIL/ATK/DEF/RET via the 4-entry far-pointer table at image `0x187F0`), selected by the engine's own `(1)Enter Kill-Rank`..`(4)Enter Retreat-Rank` menu — G-009 resolved. The trailing number is `+0x0d+1` (guard: signed `0..2` and `+0x0f != 0`) or `+0x16+1`, drawn after the 10-cell action field — G-010 resolved. **`+0x49` draw test is `!= 0`** (`cmpb $0,es:[bx+0x49] / je <action word>`, image `0x16b29`), so *any* non-zero value swaps the column for "is out of action." (image `0x18839`). An earlier revision said "`>= 2`" from a mis-read of a separate per-round decrement; that is **retracted** — see G-011. **Colour/advance resolved:** style 1 = 6-px advance, palette index 8 (proven via the shared style-1 decision header in the oracle; `tests/port_decision_test.py::check_style1_colour_and_advance`). Match 0.96 vs `ref/oracle/combat_decision.png` (viewport 1.000). | `port/lof.cpp`, `port/char.*`, `tests/port_decision_test.py`, `tests/char_test.cpp` | **done** |
| 11 | M | **All-out-of-action party message** (decision screen branch) | **Still todo — but the trigger is now decoded (2026-11-20).** The long message `ds:0x6f3f` " The companions have all been put out of action. Earliest revival will be at the beginning of the next round." (draw site image `0x185ce`) is the **decision-screen** branch taken when *no* present member can act: `[bp-0x36]` is set to 1 by image `0x18449` only for a member with `+0x49 == 0 && +0x00 != 0 && +0x1f < 3`, so the branch runs when every member fails one of those. The branch clears the text area, draws the message (newline 1), `0x2e58(0xa,0)`, colour 5, "(A)ttack!" (newline 1), "(Q)uick combat" (newline 0). **Blocked on the message colour** (the branch sets no colour before drawing it) — needs an engine frame; a save patch that sets every member's `+0x49` non-zero (e.g. 5; it is decremented once at round start) does *not* reach an encounter (the patched party cannot leave town), so the oracle needs a mid-encounter state. The round-result table this item was waiting on is now implemented (`--round-result`). | `port/lof.cpp`, `tests/port_decision_test.py` | **todo** |
| 12 | M | **Round-result table** (`The result of this round:`) | **DONE 2026-11-20.** `--round-result` / `compose_round_result_screen()`: encounter chrome + `The result of this round:` (y=92), `Name       HP   W P B Attack` (y=108), rule (y=116), one row per present member at y=124+8n, `(C)ontinue` (y=180). Columns from the engine's printf formats (`%-10s ` name, `%4ld ` HP delta right-aligned in four 6-px cells, `* `/`- ` at x=235/247/259, `Passed`/`Failed`/`-` at x=271). Values are the engine's four per-round arrays; `--round-result delta:w:p:b:a` feeds them per member. Oracle `ref/oracle/round_result.png` (deterministic), text **ink mask 1.0000**. Also settled the drawer's third argument = **newline flag** (GUESSWORK G-011 correction) and added the missing `:` (band 1 cell 37) and `*` (band 1 cell 30) font cells. Guarded by `tests/port_round_result_test.py` + `tests/font_test.cpp`. | `port/lof.cpp`, `port/font.cpp`, `tests/port_round_result_test.py`, `tests/font_test.cpp` | **done** |

Item detail lives in the linked doc (`G-002` → `docs/GUESSWORK.md`; `BLK-*` →
`docs/blockers/*.md`). **Update the Status column** as items land, so the queue
stays the single source of "what is left".

### Current status — done (history)

- [x] Analysis emulator boots LOF to the main menu (EGA mode 0Dh).
- [x] **Emulator renders all four EGA planes correctly** (BLK-A002, fixed
      2026-11-12): the emulator frame is byte-exact against `ref/oracle/menu.png`
      (all planes 100%). The cause was `port_out16` dropping a zero data byte;
      see `AGENTS.md` "Fixed bugs".
- [x] Scripted input drives the menu: `--keys`/`LOF_KEYFRAME` inject keys, and
      letters switch screens (e.g. `L` opens the character screen). The menu is
      keyboard-driven, not mouse-only — an earlier note said otherwise; it was
      wrong (only Enter had been tried).
- [x] Oracle golden-frame regression test — `tests/golden_test.sh` (`make golden`,
      also run by `check.sh`). Guards the emulator output every drop-in screen is
      verified against.
- [x] RNG is decoded and the seed is explicit — `INT 1Ah` now returns the seed
      deliberately (`LOF_TICK=<n>` overrides it); the generator is the classic
      MSVC `rand()` LCG seeded once from the BIOS clock, so combat frames are
      bit-exact. New emulator diagnostics `LOF_PXWATCH=x,y` and a ranged
      `LOF_TRACE_IP=lo:hi` (+`LOF_TRACE_BT=1`) are documented in `AGENTS.md`.
- [x] EGA image format decoded (11-byte header) — `tools/ega.py`, `port/gfx.cpp`.
- [x] Font strip decoded + menu text — `port/font.cpp`.
- [x] Intro sequence (TITL -> CRED -> menu) in the drop-in.
- [x] Data readers: `SYST/SNSC`, `SYST/NSCnn`, `SYST/TXTn` — `port/data.cpp`.
- [x] Emulator: menu key map complete; **the in-game screen is reachable**:
      `O` then Enter loads the save and the engine reads `ROST/ROST`,
      `ROST/SHOP` and the town graphics (`EGA/TWN/TWN00..05`) — the town/play
      screen. Scripted as the `town` scenario in `tests/golden_test.sh`.
- [x] Drop-in exposes area B's reader: `./lof --dir game --dump-spec`
      (decoded `SYST/SPEC0..7`).
- [x] Drop-in: play-screen static composition (`--play`) — HUD background +
      viewport tile + the **full** party panel (name, profession, `+0x1e` stat
      number, `GOOD` status).  The panel is pixel-exact; the screen matches the
      DOSBox capture at ~94.2% (residue = the engine's dynamic viewport sprite).
      Guarded by `tests/port_play_test.py`.
- [x] Drop-in: reproduce the in-game (town) screen — `--town`.  It is
      `EGA/UP/MSCR` + `EGA/TWN/TWN00` at (10,11) + the engine's message and
      building menu drawn in `CHR1` at x=139 (rows 92..188).  Static match
      99.2% vs the emulator frame (viewport + every text row pixel-exact; the
      residue is a 16x16 top-left patch and the dynamic party panel).  The town
      **name** is now selected from the save, not hardcoded: the engine (image
      `0x53E0`) picks "Thyn." / "Cyldane." from the word at save offset `0x9a4`
      (nonzero → Thyn; `--save` chooses).  Guarded by `tests/port_town_test.py`
      (oracles `ref/oracle/town.png` + `ref/oracle/town_cyldane.png`).  G-001
      resolved.
- [x] Building screens.  **Emporium (`--emporium`, `TWN01`), Bank (`--bank`,
      `TWN02`), House of trades (`--house`, `TWN04`) and the Temple (`--temple`,
      `TWN05`) are DONE** and guarded by `tests/port_building_test.py` (viewport
      + every text row pixel-exact vs `ref/oracle/{emporium,bank,house,
      temple_bldg}.png`).  They are the **same screen shape as the town** —
      `MSCR` + the building's `TWN` tile at (10,11) + `CHR1` text at x=139; there
      is **no ornate engine-drawn panel** (the earlier "ornate panel" note was
      wrong — see `docs/notes-maintainer-town.md`, "Correction 2026-11-14").
      The Temple's **(H)eal / (R)evive** options open a character-selection
      sub-prompt over the *same* viewport (`--temple-heal`/`--temple-revive`).
      The **Inn is not a `TWN` tile at all — its screen is the boot main menu**
      (the game starts inside the Inn; the town `I` key renders a frame
      byte-identical to `ref/oracle/menu.png`).  Its `(R)est` sub-prompt is
      `--inn-rest` over the menu's scrolled `TWN03` viewport.  Guarded by
      `tests/port_inn_test.py`.  (An earlier note claimed the Inn/Temple are
      first-person `D5` interiors — that was a misconception; there is no such
      state.)
- [x] Encounter screen (`--encounter`) — the wandering-monster screen.  It
      reuses the play chrome (`MSCR` + `draw_party_and_status`) and blits the
      monster's `EGA/CHAR0/CH<nn>` sprite (the NSC record's `+0x48` art; the
      emulator's own open trace shows `ega\char0\ch24`) into the viewport as a
      tiled first-person cell, then draws the intro + "What will you do ?" +
      one "N <Plural> (Rnk R)" line per enemy group + the four choices in `CHR1`
      at x=139.  Whole-screen **0.96** vs `ref/oracle/combat.png` (viewport
      0.9938; residue = the dynamic party/status values).  Guarded by
      `tests/port_encounter_test.py`.  The sprite's vertical origin is exact for
      the pinned oracle but not yet derived in general — see G-008.
- [x] Combat decision screen (`--decision`) — the per-round prompt
      "Please make your decision / for the next round!".  It is the encounter
      composition with only the right-hand text block replaced (verified: the
      two oracles share their viewport, roster and bottom strip exactly).  Each
      party row is the engine's own printf sequence (image `0x18734`): the
      1-based index, the `"%-10s "` name, the rank code, the action word, then
      `"@ %ld"`.  The rank code is `combat_abbrev_names()[char[+0x24]]` — and
      `+0x24` is the **combat rank** (`KIL`/`ATK`/`DEF`/`RET` via the 4-entry
      far-pointer table at image `0x187F0`).  The engine names the four values
      itself: the rank-entry menu prints `(1)Enter Kill-Rank` ..
      `(4)Enter Retreat-Rank` and writes the pressed digit minus `'1'` at
      `0x197F0`.  The older "body/health state" annotation came from misreading
      those four compares as a dying/dead test; `notes-b.md` is corrected.  The action word is `action_names()[char[+0x0f]]`.
      Whole-screen **0.96** vs `ref/oracle/combat_decision.png` (viewport
      1.000, text 0.993; residue = the dynamic party/status values plus the
      emulator capture's index-12 descender pixels).  Guarded by
      `tests/port_decision_test.py`; the rank mapping, the row guards
      (`+0x1f < 5`) and the trailing-field inputs are pinned by
      `tests/char_test.cpp`.  G-009/G-010/G-011 all resolved — G-011's colour
      and advance are now proven (style 1 = 6 px, palette index 8) via the
      shared style-1 decision header and pinned by
      `check_style1_colour_and_advance`; the `+0x49` branch itself is reachable
      by patching the save and is regression-guarded (`!= 0`).
- [ ] Dungeon subsystem (`DNGS/`) — **area A**; see the work queue (§4).
- [ ] Character/items/combat (`SYST/SPEC*`, `ROST/`) — **area B**; see the work
      queue (§4). **Unblocked:** the SPEC/ROST reads now happen in the oracle
      (`town` scenario), so B can ground the remaining stat/combat semantics
      against real in-game state.
- [x] Front-end UI state machine (`--ui`) — maintainer.  Keyboard-driven intro
      (title -> credits -> menu) with the menu hot-keys wired: `O` loads a save
      and enters the play screen, `Q`/Esc quits.  Input via `Window` (SDL2);
      `--ui-keys` scripts a key sequence for head-less regression testing
      (`tests/port_ui_test.py`).  In-game movement/combat remain to do.
- [x] Native walk loop (`--walk`) — maintainer.  Loads the save's dungeon
      (`DNGS/DNG<d><l>` from the save's location byte), places the party at the
      level's entry cell (`dungeon_place_entry()`, the DNG header's
      `entry_pos`/`entry_facing` = the save's `0x9a0`/`0x9a6`, Round 22) and
      replays a scancode sequence through agent A's walk model
      (0x48 step, 0x50 turn 180, 0x4b/0x4d turn), then renders the play screen
      with the level map in the viewport.  `--walk-keys` scripts the input;
      guarded by `tests/port_walk_test.py`.  The viewport now blits the
      **depth-marched first-person sprite** (`dungeon_view_for`: depth 1 → the
      tileset `DPnnn`, depth 2 → `CHAR2/CH40`, depth ≥3 → `CHAR2/CH25`), matching
      the DOSBox oracle (`tests/port_dungeon_view_test.py`); the encounter-id
      producer and the second element beside `CH25` remain (BLK-A005).
