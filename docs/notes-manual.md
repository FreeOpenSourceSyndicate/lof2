# Original manual — research notes (facts only)

The game's printed manual was located (by the owner) and read for cross-checking
the decompilation. **The PDF is proprietary and is NOT stored in the repo**; it is
kept git-ignored under `downloads/faerghail-manual.pdf`. Source URL:

    https://www.mocagh.org/miscgame/faerghail-manual.pdf

(66 pages, ~24 MB, scanned with an OCR text layer — `pdftotext -layout` extracts
it.) Only *facts* derived from it are recorded here; do not paste its text.

## Screen layout (VII. Gameplay) — confirms the port's composition

The play screen has four regions:

1. **Character list** — numeric list of the party members (the left roster).
2. **Graphic window** — the first-person "visual link to the world" (the 3-D
   viewport the port composes; BLK-A005).
3. **Text window** — narration / prompts.
4. **General-information screen** (the status box) — place name, movement
   direction, time of day, group morale, rations left; in combat it becomes a
   top-down battle overview.

The party panel fields are documented as: name, member number, profession,
constitution, armour protection (%), hit points (%), available spells (%).

## Character display (character sheet) — resolves P-8's "reach key"

**How it opens:** LEFT-CLICK a party member's button in the character list; a
graphic + overview of that character is shown. **Exit = RIGHT mouse button.**
So the sheet is reached by a *mouse click on the party panel*, not a keyboard
hot-key — the earlier "unknown reach key" was a mouse action. (For the headless
port this is a synthetic click on the party row.)

The sheet is paged; each page ends with `(E)xchange gold/rations`,
`(N)ext page`, `(C)ontinue`:

1. **Attributes** — name/alignment/sex/profession, then Rank (XP level), Str,
   Con (Health: healthy/poisoned/…), Dex, Int, Wis, Experience (XP number),
   Hit points actual/max, Magic points actual/max, Weight (load) actual/max.
2. **Objects** — load actual/max, rations, gold; an 8-row item list with each
   item's in-use/worn marker (`E`), an unusable marker (`+`), condition (%),
   and weight (lb). Arrows scroll a longer list.
3. **Spells** (magic-capable professions only — page 3 is omitted otherwise) —
   magic points remaining/max, then a spell list with per-day uses.
4. **Special abilities** — each with a percentage: Parley/Negotiate, Attack,
   Defend, Concentrate, Pick-pocket, Sneak/Creep, Trap finding, Trap disposal,
   Open doors. *(This is the skill list for the P-7 level-up skill-gain work.)*
5. **Languages** spoken (only those known are listed).

## Play / dungeon menu shortcuts

Documented single-key actions in the dungeon/wilderness: `U` use item,
`C` cast spell, `F` forge (blacksmith only), `R` rest, `D` dismiss member,
`L` lure (bait), `O` options/files (save/load), `P` pause. The same menu is
also on the **right mouse button** over the text window.

## Non-facts to avoid

- The "screens turning in 36 different angled views in real-time" line belongs
  to the *Powerslide* "Coming soon" preview in the same booklet, **not** to
  Legend of Faerghail — do not cite it as evidence about the dungeon renderer.
