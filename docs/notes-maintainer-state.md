# notes-maintainer-state.md — session handover (2026-11-10)

Written by the incoming maintainer at the end of the first session, because the
previous maintainer was interrupted mid-work and their state was unclear. Read
this first, then `docs/HANDOFF.md` (the technical reference) and
`docs/WORKSTREAMS.md` (who owns what).

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

- Do **not** read/clone/fetch the sibling repo `FreeOpenSourceSyndicate/lof`
  (poisoned). `check.sh` guards this.
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
