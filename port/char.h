// Native readers for the Legend of Faerghail character / item / scenario data
// files (workstream B).  These re-implement the engine's own parsers: they read
// the game *data* files (SYST/SPECn, ROST/ROST, ROST/SHOP) and never run the
// original executables.
//
// Formats (verified against the game data and analysis/lof.asm):
//
//   SYST/SPEC<scenario>  one count byte, then N records of 238 (0xee) bytes.
//                        Each record is one scenario "special": a dungeon
//                        event / item / recipe with up to two text strings and
//                        a behaviour dispatch type.  The stride 0xee is the
//                        `imul [bp-0x44]` in LOF's event dispatcher.
//   ROST/ROST            one count byte, then N records of 414 bytes: the saved
//                        party roster (name + stats + equipment).
//   ROST/SHOP            header byte + 185 per-item stock bytes + 4-byte trailer.
#ifndef LOF_PORT_CHAR_H
#define LOF_PORT_CHAR_H

#include <array>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "data.h"   // SnsRecord (SYST/SNSC, SYST/NSCnn) — read-only reuse

namespace lofport {

// --- SYST/SPECn -----------------------------------------------------------
// Sizes used by the parser (see AGENTS.md / docs/notes-b.md).
constexpr size_t kSpecRecordSize = 238;  // 0xee
constexpr size_t kRostRecordSize = 414;

// The behaviour dispatch type at record offset 0xe2.  The *code* fact is that
// the dispatcher subtracts 7 and indexes a 17-word jump table (image 0x21B9A,
// cs 0x2F6B) when the result is 0..16; values 0..6 never reach the table and
// take the plain-text path at 0x1F5C4.  Most table entries land in one shared
// tail at 0x1F5EC that switches on the type word itself.
//
// The names below are NOT from the code: they are descriptive labels derived by
// correlating each value with the record's text (all 665 records across
// SPEC0..7).  Several (12, 19..22) are placeholders.  Treat them as hypotheses
// until the tail at 0x1F5EC is decoded -- see G-016 in docs/GUESSWORK.md.
enum SpecType : uint16_t {
    SPEC_TYPE_TEXT = 0,        // pure description (scenery, gravestones) — the
                               // common case; the engine just prints text_b
    SPEC_TYPE_RECRUIT = 1,     // asks "may I join you (Yes/No)"
    SPEC_TYPE_SPRING = 2,      // a fountain/spring to drink or fill at
    SPEC_TYPE_SPRING2 = 3,     // a spring breaking through the ground
    SPEC_TYPE_BATTLE = 4,      // battlefield / ambush marker
    SPEC_TYPE_PASSAGE = 5,     // rumbling passageway
    SPEC_TYPE_EMPTY6 = 6,      // (no text in this release)
    SPEC_TYPE_DOOR = 7,        // "would you like to go through?" (uses arg_4c)
    SPEC_TYPE_TRADE = 8,       // offers a deal for gold (Yes/No)
    SPEC_TYPE_SIGN = 9,        // a notice/sign; arg_4c holds the text id
    SPEC_TYPE_SPRING3 = 10,    // deep/clear spring variant
    SPEC_TYPE_TELEPORT = 11,   // transports the party elsewhere
    SPEC_TYPE_BUTTON = 12,     // "press it?" prompt
    SPEC_TYPE_INSCRIPTION = 13,// inscribed stone / cauldron
    SPEC_TYPE_CHALLENGE = 14,  // a figure blocks the way and speaks
    SPEC_TYPE_EMPTY15 = 15,    // (often no text)
    SPEC_TYPE_LAVA = 16,       // hazardous crossing (lava / sarcophagus)
    SPEC_TYPE_PUZZLE = 17,     // item/puzzle interaction
    SPEC_TYPE_TRAP = 18,       // damaging trap ("you lose all your worries")
    SPEC_TYPE_BUTTON19 = 19,
    SPEC_TYPE_BUTTON20 = 20,
    SPEC_TYPE_BUTTON21 = 21,
    SPEC_TYPE_BUTTON22 = 22,
    SPEC_TYPE_DOOR24 = 24,     // massive stone door with a circle
    SPEC_TYPE_ELEMENTAL = 25,  // elemental guardian asks the riddle in text_b;
                               // uses index_c9 (the element/class index)
};

// Short human-readable name for a dispatch type (never null).
const char* spec_type_name(uint16_t type);

// One 238-byte SYST/SPECn record decoded into typed fields.
struct SpecRecord {
    std::array<uint8_t, kSpecRecordSize> raw{};

    // 0x00  NUL-terminated keyword/password (region 0x00..0x0e).  Present in 45
    // of 665 records; the player must type it (or answer the prompt) for the
    // record to fire.  Values are the expected answer: "Y"/"N" for the many
    // yes/no prompts (recruit, door, button, trap), and words for the keyed
    // riddles ("Fire", "Daughter", "Echo", "Eyes" on the four elemental
    // guardians; "Khazad Maran", "Spingo", "Iceflower", "Circle", "Stairs", …).
    std::string keyword;

    std::string text_a;   // NUL-terminated, offset 0x0e (region 0x0e..0x4a)
    std::string text_b;   // NUL-terminated, offset 0x50 (region 0x50..0xc6)

    uint8_t flag_4b = 0;      // 0x4b  boolean (0/1)
    uint16_t arg_4c = 0;      // 0x4c  word argument (used by type 7)
    int16_t arg_4e = 0;       // 0x4e  signed word, `< 0` means "unset"
    uint8_t index_c9 = 0xff;  // 0xc9  class index 0..12, 0xff = none
    // 0xda  four words the effect handlers read.  The dispatcher copies
    // record+0xda+2*i into a local array before jumping through the type table
    // (0x1F520..0x1F552, values 1..185; 263 of 665 records are non-zero), so
    // the handlers see four small integers, not one.  `arg_da` is slot 0.
    std::array<uint16_t, 4> args{};
    uint16_t arg_da = 0;      // 0xda  == args[0]; kept for existing callers
    SpecType type = SPEC_TYPE_TEXT;  // 0xe2  behaviour dispatch type
    uint16_t counter = 0;     // 0xe4  countdown/counter
    uint16_t arg_e6 = 0;      // 0xe6  extra argument (type 7/14)
    uint16_t arg_e8 = 0;      // 0xe8  extra argument
    uint8_t class_eb = 0xff;  // 0xeb  SNSC class index (0xff = none); see notes

    bool has_index() const { return index_c9 != 0xff; }
    bool has_class() const { return class_eb != 0xff; }
};

// How a typed answer interacts with a record's outcome line (`+0x0e`).
//
// Derived from the dispatcher at 0x209C8..0x20A95 and pinned by
// tests/dungeon_test.cpp::test_answer_branches.  The record's keyword (+0x00) is
// compared case-insensitively against the answer; an empty keyword means the
// record never reads one.
//
//   0x209C8: cmp byte es:[bx+si],0        ; keyword empty?
//   0x209CC: jne 0x209D1
//   0x209CE: jmp 0x20AB8                  ; no answer wanted -> the type switch
//   0x20A0C: or dx,ax                     ; compare result, 0 == equal
//   0x20A0E: je  0x20A72                  ; EQUAL   -> the match branch
//   0x20A10: ...                          ; else    -> the mismatch branch
//
// The no-keyword branch (0x209CE) and the match branch both reach the per-type
// effect switch at 0x20AB8; only a wrong answer skips it (0x20A6E jmp 0x1F83C).
// `+0x0e` is printed on both answer branches, with `+0x4b` gating each branch
// separately:
//
//   match    (0x20A89 cmp byte [bx+0x4b],0 ; 0x20A8E je 0x20AB8 -> no print)
//   mismatch (0x20A3D cmp byte [bx+0x4b],0 ; 0x20A42 je 0x20A47 -> print)
//
// so `+0x4b == 0` means "both answers get the line" and `+0x4b == 1` means
// "neither does".  The other gate on each branch is `+0x0e != 0`.
enum class AnswerEffect {
    NoAnswerWanted,   // keyword empty: go straight to the type switch
    Match,            // answer == keyword: effect fires, line per +0x4b
    Mismatch,         // answer != keyword: no effect, line per +0x4b
};

// A type-7 ("would you like to go through?") door's destination, from the
// effect at image 0x20AD7..0x20B13.  The handler writes the record's fields
// straight into the engine's party-position globals:
//   +0xe4 -> [0xb05c]:0x37fa   (location ONES digit)
//   +0xe6 -> [0xb050]:0x460e   (location TENS digit, sign-extended dword)
//   +0xe8 -> [0xb04e]:0x3824   (36x36 cell index)
// and raises [0xb05e]:0x3b15.  The save loader (0x1C93F..0x1C971) proves the
// global roles: save +0x9a2 (ones) -> 0x37fa, +0x9a4 (tens) -> 0x460e,
// +0x9a0 (cell) -> 0x3824, +0x9a6 (facing) -> 0x3d2.  The flag's only reader
// (0x2C1F6 `cmp es:[0x3b15],0`) merely *skips* the normal walk-buffer commit so
// the teleport destination is not overwritten, then clears the flag; it neither
// performs nor undoes the move.  So the door teleports the party to
// (tens*10 + ones, cell).  G-017 resolved 2026-11-23.
struct DoorTarget {
    int location = 0;   // +0xe4 -> [0x37fa]  (location ONES digit)
    int map = 0;        // +0xe6 -> [0x460e]  (location TENS digit)
    int cell = 0;       // +0xe8 -> [0x3824]  (cell index, 0..1295)
    bool pending = false;  // [0x3b15] = 1

    // The party location the door leads to, in the save's encoding
    // (`save_location()` = tens*10 + ones): +0xe6*10 + +0xe4.
    uint16_t destination_location() const {
        return (uint16_t)(map * 10 + location);
    }
};

DoorTarget door_target(const SpecRecord& r);

// Type-16 (SPEC_TYPE_LAVA: lava crossing / sarcophagus) is a two-visit latch
// (image 0x1FA8F and 0x1F639, flag `[0x7d1c]`):
//   entry path 0x1FA8F: type == 0x10 and latch == 0 -> latch = 1 (normal visit)
//   re-entry   0x1F639: type == 0x10 and latch != 0 -> latch = 0, print
//                       ds:0x80d2 "The companions arise out of the coffin."
//                       and jump to 0x21276, which skips the record's own text.
// The string was read out of the live data segment (LOF_SEGDUMP 4C92:80d2).
struct CoffinLatch {
    bool armed = false;
    // True when this visit is the "arise" one (latch was set, now cleared).
    bool visit(uint16_t type);
};

// Classify a typed answer against a record.  `answer` may be empty.
AnswerEffect classify_answer(const SpecRecord& r, const std::string& answer);

// Whether the outcome line (`+0x0e`) is printed for this branch.
bool answer_prints_outcome(const SpecRecord& r, AnswerEffect effect);

// Whether the per-type effect runs for this branch: always, except on a wrong
// answer to a keyword prompt (no keyword -> straight to the type switch).
bool answer_fires_effect(AnswerEffect effect);

// Case-insensitive comparison with the same folding the engine uses
// (0x1BE9F: test byte [bx-0x51fb],2 ; sub ax,0x20).
bool keyword_matches(const std::string& keyword, const std::string& answer);

// A parsed SYST/SPECn file: the count byte plus its records.
struct SpecTable {
    uint8_t declared_count = 0;
    std::vector<SpecRecord> records;
    bool ok = false;
};

// Parse an in-memory SYST/SPECn image.  Returns false (and clears `out`) if the
// buffer is too small or truncated mid-record.
bool parse_spec(const std::vector<uint8_t>& data, SpecTable& out);

// Load and parse SYST/SPECn.  `path` is the full path to the file.
SpecTable load_spec(const std::string& path);

// Which SPEC file a dungeon belongs to.
//
// The level id is the two-digit number in `DNGS/DNG<dd>` (and in the save's
// location byte, `tens*10 + ones`).  The SPEC file is selected by the **second**
// digit -- the level -- not the first.  Ground truth is the emulator's own file
// trace (`LOF_I21=1`) on the documented drive into a dungeon:
//
//   [3D open] 'DNGS\DNG10'  -> game/DNGS/DNG10
//   [3D open] 'SYST\NSC00'  -> game/SYST/NSC00
//   [3D open] 'SYST\SPEC0'  -> game/SYST/SPEC0
//
// i.e. for DNG10 the engine loads SPEC0 = the ones digit.  This also holds as a
// whole-table consistency check: for all 29 shipped levels the largest
// `event_index()` is < the record count of SPEC<ones>, and for several levels
// (DNG06 needs index 43, SPEC0 has 24 records) the other reading would run off
// the end of the table.  `scenario` here is that ones digit; it is the same
// value the status box prints (port/lof.cpp uses `loc % 10`) and the same one
// that indexes SYST/NSC<scenario>.
inline int spec_scenario_for_location(int location) { return location % 10; }

// `SYST/SPEC<n>` for a location.  `dir` is the game data directory.
inline std::string spec_path(const std::string& dir, int location) {
    char p[32];
    snprintf(p, sizeof p, "/SYST/SPEC%d", spec_scenario_for_location(location));
    return dir + p;
}

// --- ROST/ROST ------------------------------------------------------------

// One saved-character record.  The engine stores the character as a 410-byte
// (0x19a) struct plus a 4-byte trailing word (the loader reads 410 then 4 --
// see the `LOF_I21` trace of the town scenario and `imul [bp..],0x19a` in the
// disassembly); in the file the two are contiguous, so the stride is 414.
struct RostRecord {
    std::array<uint8_t, kRostRecordSize> raw{};
    uint8_t present = 0;   // 0x00  non-zero: slot holds a character
    std::string name;      // 0x01  NUL-terminated name (<= 14 bytes)
    uint32_t tail = 0;     // 410..413  trailing 32-bit value (split from the
                           //           410-byte struct by the loader)
    bool intact = false;   // true iff `tail == rost_record_checksum(raw[0..409])`
};

// The engine's per-record integrity check (cheat protection): the 4-byte
// little-endian trailer after each 410-byte record is this alternating
// ADD/XOR fold.  The engine verifies it on load (asm 0x2A1E1) and, on
// mismatch, writes the tamper marker 0xFC into record byte 0x0C (asm 0x2A1F6).
// Fully derived in docs/notes-maintainer-rost.md (asm 0x1BF18); reproduces the
// stored trailer on every record of the shipped saves.
uint32_t rost_record_checksum(const uint8_t* rec410);

// Load ROST/ROST (count byte + N x 414-byte records).
std::vector<RostRecord> load_rost(const std::string& path);

// --- Item table ------------------------------------------------------------
//
// The item table is NOT in a data file: the engine keeps it inside LOF.EXE
// (image offset 0x42cec / data segment 0x2cec, 185 x 32-byte records).  The
// drop-in must run without the original EXE, so the decoded table is
// transcribed into `port/char_items.inc` and exposed here.  Per record: a
// 16-byte NUL-padded name, seven little-endian int16 fields, then a byte item
// class and a byte use-effect index.
constexpr size_t kItemCount = 185;
constexpr size_t kItemNameLen = 16;
constexpr size_t kItemRecordSize = 32;

// Character/creature classes, in SNSC order (SYST/SNSC record index).  The item
// eligibility mask (`f6`, bit n) and SPEC `index_c9` use this ordering.
constexpr size_t kClassCount = 14;
extern const char* const kClassName[kClassCount];

struct ItemRecord {
    std::string name;
    // Seven little-endian int16 fields at record offsets 0x10..0x1c.  `price`
    // and `weight` are grounded; `f0`/`f3`/`f4` are partly named; `f5`/`f6`
    // are grounded.  See docs/notes-b.md ("item table") for the byte-level map.
    int16_t f0 = 0;     // +0x10  weapon/armour class code (weapons: 0..5)
    // +0x12  Price in gold.  The equipment list prints it with `%4d$` (routine
    // at image 0xc474), and the shop buy/sell routines read the same field
    // (0xb745 / 0xb122).  Grounded.
    int16_t price = 0;
    // +0x14  Item weight.  Grounded three ways: the character's carried total
    // (char +0x66) is accumulated as this value over the inventory slots
    // (routine at image 0x21030, the sum ends at 0x210d4); the load check
    // `weight + char[0x66] <= char[0x64]` (image 0x19B54 / 0x2965f); and the
    // equipment list / character sheet, which DISPLAY it divided by 10 as
    // pounds (`%3dlb` at image 0xc477; `div 0xa` before the sheet's `Weight`
    // line at image 0x2c21d).  Raw values run 5..3500.
    int16_t weight = 0;
    int16_t f3 = 0;     // +0x16  armour AC in the LOW byte (the armour routine
                        //        reads `es:[bx+0xfd2]`, image 0x2116b); the HIGH
                        //        byte is the quantity/charges (`charges`)
    // +0x17  Initial quantity / charges: the HIGH byte of `f3`.  A fresh stack
    //        starts with this count (arrows 10, rations 1, tinderbox 200) and
    //        the item-add routine copies it into the inventory slot's count
    //        byte (`+0x84`, image 0xbf5d `mov al, es:[bx+0xfd3]`); for charged
    //        magic items it is the charge count (staves 9..25).  0 for ordinary
    //        non-stackable gear (weapons/armour/shields).  See
    //        ItemRecord::charges.
    uint8_t charges = 0;
    int16_t f4 = 0;     // +0x18  LOW byte = the 1-based *spell index* the item
                        //        invokes when used (exposed as `spell`); HIGH
                        //        byte = weapon damage (exposed as `damage`).
    // +0x18  Spell index (the low byte of `f4`): 1-based into the spell table,
    //        0 = the item casts nothing.  When the party "uses" an equipped
    //        item the engine takes this byte and looks the spell up as
    //        `es:[spell*0x24 + 0x2712]` (image 0x0E59E, 0x0F072, 0x17C6F,
    //        0x1947E, 0x19690); DS:0x2712 is the spell table's record base plus
    //        0x16, so the byte it gates on is `spells()[spell-1].data[0]`.  See
    //        `item_spell()` / `item_use_flags()` / `item_is_usable()`.
    uint8_t spell = 0;
    // +0x19  Weapon damage (the high byte of `f4`): subtracted from a target's
    //        HP in combat (image 0x13ebd `sub es:[bx+0x22],cx`).  0 for non-weapons.
    uint8_t damage = 0;
    int16_t f5 = 0;     // +0x1a  1-based index of the paired item: ammo<->launcher
                        //        (Sling 19<->Bullets 18), Flask of oil<->Lantern,
                        //        and quest pairs (Emerald<->Corona, Ring<->Keystaff).
    // +0x1c  **profession**-eligibility mask (12 bits, bit n => profession n may
    //        use the item).  The equipment list tests it against the character's
    //        profession index `+0x1b` as `test f6, 1<<char[+0x1b]` (image
    //        0xc539..0xc553); max value in the table is 4095 (all 12
    //        professions).  It is *not* the 14-entry SNSC class mask.
    int16_t f6 = 0;
    // Item class: 0 misc/key, 1 melee, 2 armour, 3 shield, 4 projectile,
    // 5 staff, 6 scroll/book/map, 8 ring, 9 potion, 10 treasure, 11 bow/sling.
    uint8_t item_class = 0;
    // +0x1f  Use-effect code (the engine's `es:[bx + 0xfda]` is the *class*
    //        byte below; this is the adjacent byte the use handlers switch on).
    //        0 for ordinary gear; a small code on magic/consumable items that
    //        `item_effect_name()` decodes (Healing potion 10, Poison I 3,
    //        Gunpowder 4, quest jewels 15, book halves 14, …).  Not the spell
    //        index (that is `spell` above).
    uint8_t effect = 0;
};

// The decoded table in engine order (index 0 = "Club").
const std::vector<ItemRecord>& items();

// 1-based item lookup as used by saves/maps; nullptr when out of range.
const ItemRecord* item_by_index(int index);

// Descriptive label for an item's use-effect index (`ItemRecord::effect`), the
// value the engine tests when the party "uses" an item.  The engine compares
// this byte directly; the complete set of values it is *proven* to test, with
// one site each:
//
//   1,2,3,10,19,23,30,31   use-routine effect dispatch   0x0E89D / 0x0EDB0
//   2,8,12,21,24,28,29,33  equip + unequip handlers      0x29FB1 / 0x2995C
//   9,13,15,20,27,40       misc sites  0x14A09 / 0x13AF4 / 0xDF7E /
//                                        0x14976 / 0xE274 / 0xE1BC
//
// The engine's own messages fix some meanings: 1 "growing professionalism."
// (enchant weapon), 2 "feels himself safer." (enchant armour), 3/23 "goes green
// in the face!" (poison), 10 adds HP (0x0EA90, healing), 15/40 "fits together."
// (combine paired items), 30/31 clear the poison state (0xEBE4 / 0xECE2,
// antidotes).  The equip/unequip handler toggles a character-struct byte per
// effect (2 -> +0x5b, 8 -> +0x5a, 12 -> +0x59, 21 -> +0x52, 24 -> +0x53,
// 28/29 -> +0x5a, 33 -> +0x56); see docs/notes-b.md.
//
// The returned strings are item-name-derived descriptions (effect 5 is the
// "Crystal ball" item, effect 8 the "Antimagic ring", …), not engine symbols —
// see G-004 in docs/GUESSWORK.md.  Non-null for every input (unknown/0 =>
// "none").
const char* item_effect_name(uint8_t effect);

// Item indices (into `items()`) whose use-effect index equals `effect`, in
// table order.  `effect == 0` returns the plain (non-magical) gear.
std::vector<size_t> items_with_effect(uint8_t effect);

// True when profession `prof` (0..11, the value of a character's +0x1b field)
// may use item `index`: it tests the item's `f6` profession mask
// (`f6 & (1 << prof)`), exactly as the equipment list does at image 0xc553.
// Out-of-range item or profession returns false.
bool item_usable_by_class(int index, int prof);

struct SpellRecord;  // defined below (spell table)

// --- Item use (spell casting) ----------------------------------------------
//
// The engine has no separate item-use table.  The routine that handles "use an
// item" reads the character's equipped-item record, takes its `+0x18` byte (a
// 1-based *spell* index), and looks that spell up in the spell table:
//
//     mov  ax, 0x24 ; imul [idx]   ; 36-byte records
//     mov  es, [0xafae]            ; DS
//     mov  al, es:[bx + 0x2712]    ; image 0x0E59E (also 0x0F072/0x17C6F/…)
//
// DS:0x2712 is the spell table's record base (DS:0x26FC) + 0x16, i.e. the first
// byte *after* a record's 14-byte parameter block, which is exactly the byte
// `spells()[idx-1].data[0]`.  So the gate the engine evaluates on a spell
// record is `(data[0] & 0x07) != 0` (it tests bits 0/1 with `test al,3` and bit
// 2 with `test al,4`, and takes the "cast" path only when at least one is set;
// otherwise it prints "is not useful in this situation.").
//
// `idx` is `ItemRecord::spell` (+0x18).  `item_use_flags(i)` therefore returns
// `spells()[item[i].spell - 1].data[0]` (0 when the item casts nothing).

// The 1-based spell index an item invokes when used: `ItemRecord::spell`
// (record byte +0x18).  0 = the item casts nothing.
uint8_t item_use_index(int item_index);

// The gate byte the use routine reads for `item_index`: the spell record's
// first parameter byte, `spells()[item[i].spell-1].data[0]`.  Returns 0 when
// the item casts nothing or the index is out of range.
uint8_t item_use_flags(int item_index);

// True when the use routine acts on `item_index`: it is a potion (item class 9,
// which the routine exempts at image 0x0E5C5/0x0E618) or its spell's `data[0]`
// has one of bits 0/1/2 set, i.e. `(data[0] & 0x07) != 0`.  This is the exact
// predicate the handler at image 0x0E59E computes.
bool item_is_usable(int item_index);

// The spell `item_index` casts, or nullptr when it casts nothing / is out of
// range.  (Spells are 1-based; this resolves through `spell_by_index()`.)
const SpellRecord* item_spell(int item_index);

// Print the decoded use mapping (for eyeballing).
void dump_item_use(FILE* out);

// Print the decoded table (for eyeballing).
void dump_items(FILE* out);

// Spell table (also transcribed from LOF.EXE, see port/char_spells.inc).  The
// engine keeps 141 records of 36 bytes: a 22-byte NUL-padded name then 14 data
// bytes.  The records are grouped by caster class; the last 14 carry German
// names (unused/debug).  The data bytes' exact meaning lives in the play loop.
constexpr size_t kSpellCount = 141;
constexpr size_t kSpellNameLen = 22;
constexpr size_t kSpellRecordSize = 36;

struct SpellRecord {
    std::string name;
    uint8_t data[14] = {0};
};

// The decoded table in engine order (index 0 = "Burning hands").
const std::vector<SpellRecord>& spells();

// 1-based spell lookup (spell indices in items/characters are 1-based).
const SpellRecord* spell_by_index(int index);

// Spell effect code: byte 8 of a spell's 14 data bytes (`es:[spell*0x24 +
// 0x271A]`, image `0x0F5D8`).  The cast routine subtracts 1 and dispatches
// through a 63-entry jump table (`cmp ax,0x3e; jbe; add ax,ax; jmp
// *cs:0x2C1E(bx)`, `0x0F5E2`..`0x0F5ED`), so it is the id the play loop
// switches on.  Grouping all 141 spells by it yields clean families (healing,
// light, paralyse, fear, summon, map, …), which is how the labels below were
// assigned — they are descriptive, not engine symbol names.
// (non-null for any input; unknown/0 => "damage"); `spells_with_effect()`
// lists the spells carrying a code.
constexpr size_t kSpellEffectByte = 8;
const char* spell_effect_name(uint8_t code);
std::vector<size_t> spells_with_effect(uint8_t code);

// Print the decoded table (for eyeballing).
void dump_spells(FILE* out);

// Magic-point table (also transcribed from LOF.EXE, see port/char_magic.inc).
// The engine keeps a 12 x 16 byte table (profession x level); the MP fill
// routine at image 0xEC00 indexes it by the character's profession (+0x1b) and
// level (+0x17), then adds +1 when Wisdom > 15 and +2 more when Intelligence >
// 15.  Regenerate with `tools/spec.py --emit-mp`.
constexpr size_t kMagicProfCount = 12;
constexpr size_t kMagicLevels = 16;

// Per-(profession, level) magic-point base, before the Wisdom/Intelligence
// bonuses (returns 0 for an out-of-range profession or level >= 16, matching
// the engine's guards).
uint8_t magic_table_value(uint8_t profession, uint8_t level);

// Character-generation model: the name tables LOF.EXE uses when creating a
// character (transcribed into port/char_names.inc; regenerate with
// `tools/spec.py --emit-names`).  These are the choices the "add a new
// character" flow presents, in engine order.
constexpr size_t kSexCount = 2;
constexpr size_t kRaceCount = 6;
constexpr size_t kProfessionCount = 12;
constexpr size_t kAlignmentCount = 2;
constexpr size_t kSkillCount = 9;
constexpr size_t kLanguageCount = 8;
constexpr size_t kStatusCount = 8;
constexpr size_t kGenderProfessionCount = 12;
constexpr size_t kRaceAdjectiveCount = 6;
constexpr size_t kAlignmentLowerCount = 2;
constexpr size_t kProfessionDisplayCount = 13;
constexpr size_t kActionCount = 5;
constexpr size_t kMoraleCount = 5;
constexpr size_t kCombatAbbrevCount = 4;
constexpr size_t kScenarioCount = 8;

std::vector<std::string> sex_names();
std::vector<std::string> race_names();
std::vector<std::string> profession_names();
std::vector<std::string> alignment_names();
std::vector<std::string> skill_names();
std::vector<std::string> language_names();

// Character condition labels (index 0 = "Good"): the 5-char form the status
// line shows, and the full word (e.g. "Poisn" / "Poisoned").
std::vector<std::string> status_short_names();
std::vector<std::string> status_long_names();

// Character-sheet display tables (party / status screens): the female
// profession forms ("female Warrior".."Priestess"), race adjectives used in
// compound names ("Human-".."Half-Orc-"), lower-case alignments, the padded
// profession column, the per-turn actions and the morale labels.
std::vector<std::string> gender_profession_names();
std::vector<std::string> race_adjective_names();
std::vector<std::string> alignment_lower_names();
std::vector<std::string> profession_padded_names();
std::vector<std::string> action_names();
std::vector<std::string> morale_names();

// Combat-menu action abbreviations (index 0 = "KIL"): KIL/ATK/DEF/RET, the
// terse forms the combat status line shows alongside the full `action_names()`.
std::vector<std::string> combat_abbrev_names();

// Scenario titles, indexed by scenario number: they name the region the
// scenario plays in and parallel `SYST/SPECn` / `SYST/NSC0n`.
std::vector<std::string> scenario_names();

// Status-line tables (LOF.EXE, image 0x422f0/0x42348; 22-byte centred cells).
// `facing_names()` is the compass heading (N/E/S/W) and `time_of_day_names()`
// the clock band (Midnight, Dawn, … Evening) the map screen shows.
constexpr size_t kFacingCount = 4;
constexpr size_t kTimeOfDayCount = 8;
std::vector<std::string> facing_names();
std::vector<std::string> time_of_day_names();

// --- Monster / NPC rosters -------------------------------------------------
//
// SYST/NSCnn is a count byte then that many 78-byte records in the same layout
// as SYST/SNSC (singular + plural name, then a 40-byte stats block).  The
// engine loads SYST/NSC0n for scenario n (format string `SYST\NSC0%ld` at
// image 0x4a363); this release ships scenarios 0..7.
constexpr int kMonsterScenarioCount = 8;

// Singular names of scenario `scenario`'s monsters, in table order (empty when
// the file is missing).  `dir` is the game data directory.
std::vector<std::string> monster_names(const std::string& dir, int scenario);

// The full scenario-`scenario` roster — singular + plural names and the 40-byte
// statistics block — in table order.  The encounter screen picks a record,
// prints its *plural* name, and builds the monster's live character struct from
// the statistics block with `sns_char_seed`.  Empty when the file is missing.
std::vector<SnsRecord> monster_roster(const std::string& dir, int scenario);

// --- Combat / encounter strings -------------------------------------------
//
// The wandering-monster encounter screen and the round-result table draw
// engine strings that live only in LOF.EXE (no data file carries them); the
// drop-in compiles them in via port/char_names.inc.  Grounded by the
// emulator's own read of SYST/NSC00 during the first encounter
// (`LOF_I21=1`, ref/oracle/combat.png); see docs/notes-b.md.

// How the encounter opened, in engine order (image 0x490BD):
//   0 "You unknowingly meet"      2 "You are surprised by:"
//   1 "Without warning you meet"  3 "You still see in front of you:"
constexpr size_t kCombatIntroCount = 4;
// The four menu choices drawn under "What will you do ?" (image 0x49143):
// (A)ttack, (G)reet your opponents, (T)alk to them, (W)ithdraw at once.
constexpr size_t kCombatActionCount = 4;
// The menu prompt "What will you do ?" (image 0x49130).
constexpr size_t kCombatPromptCount = 1;
// Round-result table rows: title, column header, underline rule.
constexpr size_t kCombatRoundHeaderCount = 3;
constexpr size_t kCombatWithdrawHeaderCount = 3;
// Per-character combat config menu (image 0x48E15..): 5 actions (Defend/Attack/
// Sneak/Use object/Perform magic), 4 rank-entry lines, the (C)ontinue key and
// the two-part aim prompt ("Who has " + " aimed at  (1-6) ?").
constexpr size_t kCombatConfigActionCount = 5;
constexpr size_t kCombatConfigRankCount = 4;
constexpr size_t kCombatContinueCount = 1;
constexpr size_t kCombatAimCount = 2;

std::vector<std::string> combat_intro_names();
std::vector<std::string> combat_action_names();
// The encounter menu prompt "What will you do ?" (image 0x49130).
std::vector<std::string> combat_prompt_names();
// Round-result table: {title, column header, underline rule}.  The melee table
// (0x48D2D) lists HP / W / P / B / Attack; the withdraw table (0x49048) lists
// Gld. / Rt / Exp. / HP.
std::vector<std::string> combat_round_header();
std::vector<std::string> combat_withdraw_header();

// The per-character combat *config* menu (shown after choosing (A)ttack at the
// encounter).  The five action choices are in `kActionNames` order, i.e. index
// i sets the character's action byte (struct +0x0f) to i:
//   0 (D)efend, 1 (A)ttack, 2 (S)neak, 3 (U)se object, 4 (P)erform magic.
// The player picks a character with a digit 1..6, then an action letter;
// (C)ontinue commits and runs the round.  Grounded end to end in the emulator:
// (D)efend set char0 +0x0f 1 -> 0 (docs/notes-b.md).
std::vector<std::string> combat_config_action_names();
// The four "Enter <X>-Rank" lines (image 0x48E4C), keyed 1..4.
std::vector<std::string> combat_config_rank_names();
// "(C)ontinue" (image 0x48EA0) — the commit key.
std::vector<std::string> combat_continue_names();
// The aim prompt, split as the engine stores it: "Who has " then
// " aimed at  (1-6) ?" (image 0x48EAB).  Filled into the target byte (+0x0e).
std::vector<std::string> combat_aim_names();

// The count/rank line an encounter prints for one enemy group, e.g.
// "3 Ogres (Rnk 1)": `count`, a space, the plural name, then the rank in
// parentheses (image formats `%ld ` at 0x49120 + ` (Rnk %ld)` at 0x49125).
std::string combat_group_line(uint32_t count, const std::string& plural,
                              uint32_t rank);

// --- SNSC / NSC statistics block (character seed) -------------------------
//
// An SNSC/NSC record is 78 bytes: a singular and a plural NUL-padded name (19
// each), then a 40-byte statistics block at record byte 38 (0x26).  When the
// engine creates a character of a class it copies a handful of those bytes into
// the runtime 410-byte (0x19a) character struct.  The copy sites are grounded in
// `analysis/lof.asm` 1DA8E..1DCBB and verified against the shipped
// `GAMES/GAMEn` / `ROST/ROST` saves (Siegurd == SNSC class 0, byte-for-byte).
//
// Record-relative offsets the init routine reads; the value in the record is
// `stats[off - 38]`.
enum SnsOffset : size_t {
    kSnsIndex2a = 0x2a,  // byte; selects a char +0x7a+n slot (set to 2)
    kSnsBody2d  = 0x2d,  // byte; seeds char +0x1d AND +0x1e = 100 - value; the
                         //   armour routine later replaces +0x1e with the total AC
    kSnsBody2e  = 0x2e,  // byte; char +0x1c (45..75, a base stat)
    kSnsSpeed36 = 0x36,  // byte; char +0x56 (1..4)
    kSnsField3a = 0x3a,  // byte; char +0x14 (word; a per-class value, Siegurd 18)
    // Four (value, level) skill pairs: char +0x142+2n = rec[0x3b+2n] and
    // char +0x143+2n = rec[0x3c+2n], n = 0..3 (the loop at 1DC00/1DC40).
    kSnsSkill3b = 0x3b,  // byte; first of four skill values, stride 2
    kSnsSkill3c = 0x3c,  // byte; first of four skill levels, stride 2
    kSnsHp2c    = 0x2c,  // byte; char +0x20/+0x22 = rand(1..value/2) + value/2
    kSnsClass4c = 0x4c,  // byte; char +0x18 (1 for PCs, 2 for big monsters)
};

// The character-struct fields the SNSC/NSC init routine seeds from a class
// record.  Ground truth is the raw image at file offset 0x22F80..0x231A0 (the
// listing labels it 1DA8E..1DCBB; again, trust the bytes).  Offsets name the
// target byte in the 410-byte character struct.
struct CharSeed {
    uint8_t  c00 = 1;   // +0x00  present marker (1)
    uint8_t  c18 = 0;   // +0x18  rec[0x4c]  class flag (1 PC / 2 monster)
    uint8_t  c1c = 0;   // +0x1c  rec[0x2e]  base stat (45..75)
    uint8_t  c1d = 0;   // +0x1d  100 - rec[0x2d]  (transient: the attribute pass
                        //         overwrites it with 0x0a / 5*dex-0x41)
    uint8_t  c1e = 0;   // +0x1e  seeded 100 - rec[0x2d], then overwritten by the
                        //         armour routine with the total defence (AC)
    uint8_t  c56 = 0;   // +0x56  rec[0x36]  speed 1..4
    uint8_t  hp_seed = 0;  // rec[0x2c]: char +0x20 (max) and +0x22 (current) are
                           //   seeded rand(1..rec[0x2c]/2) + rec[0x2c]/2
    uint8_t  c1b = 0x0c;  // +0x1b  const 12 (later reduced by the char gen)
    uint16_t c14 = 0;   // +0x14  rec[0x3a]  (a per-class value; Siegurd 18)
    uint16_t c64 = 0x1324;  // +0x64  const 4900 (later adjusted per character)
    uint32_t c4e = 100;     // +0x4e  dword const 100 (weight base)
    uint8_t  c7a_index = 0;  // rec[0x2a]; char[0x7a + index] is then set to 2
    // The four seeded skill pairs: c142[n] -> char +0x142 + 2n (rec[0x3b+2n]),
    // c143[n] -> char +0x143 + 2n (rec[0x3c+2n]).  Confirmed byte-for-byte on
    // Siegurd (class 0, all zero) and on the class-2/3 records that carry data.
    uint8_t c142[4] = {0, 0, 0, 0};
    uint8_t c143[4] = {0, 0, 0, 0};
};

// Apply the SNSC/NSC init mapping to a class record (see `SnsOffset`).
CharSeed sns_char_seed(const SnsRecord& rec);

// Offsets into the 410-byte (0x19a) in-memory character struct.  ROST records
// hold this struct at +0x01 (after the count byte) as records of 414 bytes; a
// GAMES save holds it verbatim at +4.  The offsets below are the ones the
// engine's character-init routine writes/reads directly (analysis/lof.asm
// 1B95A.., on the struct array es:[0x30]); the remaining fields are only
// touched through the play-loop view (es:[bx+0x27xx]) and are not mapped here.
enum CharOffset : size_t {
    kCharPresent   = 0x00,  // byte: non-zero -> slot in use
    kCharName      = 0x01,  // 14 bytes, NUL-terminated
    // The erase pass at 1B9C2 acts only when word10 != -1 and word12 > 0, and
    // then clears the slot (present=0, word10=-1, word12=0).  So -1 marks a
    // slot the pass leaves alone; live characters carry -1 here.
    kCharFlags10   = 0x10,  // word
    kCharCounter12 = 0x12,  // word: paired with flags10 in the erase condition
    // +0x17 is the character level / rank.  The level-up routine (0x9144) does
    // `inc byte ptr es:[bx+si+0x17]` and then prints "X enters the %ld rank."
    // (string at DS:0x4ea6), and the character sheet prints +0x17 under "Rnk ".
    kCharLevel17   = 0x17,  // byte: level / rank
    // The init routine at 1BA40 sets stat1d = 0x0a, and when +0x46 (Dexterity)
    // exceeds 0x0f overrides it with 5*dex - 0x41; both are byte quantities.
    kCharStat1d    = 0x1d,  // byte: Dexterity-derived stat (see 1BA40)
    // The five primary attributes, in sheet display order Con, Str, Dex, Int,
    // Wis.  The character-sheet builder (0x2676E) copies the 410-byte struct to
    // its stack frame and prints "Str "(+0x45), "Con "(+0x44), "Dex "(+0x46),
    // "Int "(+0x47), "Wis "(+0x48); the training routine (0x95B0) increments
    // +0x45 with "Strength improves!", +0x46 with "Dexterity improves!", +0x48
    // with "Wisdom improves!", and the Con->HP bonus reads +0x44.
    kCharCon44     = 0x44,  // byte: Constitution
    kCharStr45     = 0x45,  // byte: Strength
    kCharDex46     = 0x46,  // byte: Dexterity
    kCharInt47     = 0x47,  // byte: Intelligence
    kCharWis48     = 0x48,  // byte: Wisdom
    // Fields seeded from an SNSC/NSC class record by the init routine at
    // 1DA8E..1DCBB (see `sns_char_seed`): +0x18 class flag, +0x1c base stat,
    // +0x1e body stat, +0x56 speed.  +0x7a is the base of the seed slots: the
    // routine sets byte +0x7a + rec[0x2a] to 2.
    kCharClass18   = 0x18,  // byte: 1 for PCs, 2 for big monsters
    kCharProf1b    = 0x1b,  // byte: profession index 0..11 (see profession());
                            //   the init routine seeds it to 12 (0x0c), which the
                            //   display routine treats as "no profession"
    kCharBase1c    = 0x1c,  // byte: base stat (45..75)
    kCharStat1e    = 0x1e,  // byte: total defence (armour class); seeded
                            //   100 - rec[0x2d] and replaced by the armour
                            //   routine 0x210E8 (see defense_from_items)
    kCharSpeed56   = 0x56,  // byte: speed (1..4)
    kCharSeed7a    = 0x7a,  // byte: base of the +0x7a+n seed slots
    // Additional fields the same init routine writes (raw image 0x22F80..):
    kCharSeed14    = 0x14,  // word: from rec[0x3a] (per-class; Siegurd 18)
    kCharSeed1b    = 0x1b,  // byte: seeded 0x0c, later reduced by the char gen
    kCharSeed4e    = 0x4e,  // dword: seeded 100 (weight-style base value)
    kCharSeed64    = 0x64,  // word: seeded 0x1324 (4900), later adjusted
    // The character's current / maximum magic points.  `+0x68` is the MAXIMUM
    // and `+0x69` the CURRENT value: the spell routine at image 0x137AD clamps
    // the current down to the maximum (`if +0x68 != 0 && +0x69 < +0x68:
    // +0x69 = +0x68`) and the rest routine at 0x6440 restores it
    // (`+0x69 = +0x68`).  Both are filled together by the routine at image
    // 0xEBF0..0xEC87, which looks up a per-(profession,level) table at
    // `table_seg:[0xa38]` (12 professions x 16 levels, indexed
    // `16 * +0x1b + +0x17`, only while level < 16) and then adds +1 when
    // Wisdom(+0x48) > 15 and +2 more when Intelligence(+0x47) > 15.  The table
    // ships in the EXE (file offset 0x42748) and is transcribed into
    // port/char_magic.inc (see magic_table_value / CharSheet::magic_max).
    // The sheet's "Magic.... " line prints (current, maximum) = (+0x69, +0x68).
    // NOTE: the pair is TRANSIENT in a save (a snapshot can hold a partly
    // depleted current value), so it is projected verbatim, not recomputed.
    kCharMagMax68  = 0x68,  // byte: maximum magic points
    kCharMagCur69  = 0x69,  // byte: current magic points
    // The four seeded (value, level) skill pairs: +0x142+2n / +0x143+2n, n=0..3,
    // copied from rec[0x3b+2n] / rec[0x3c+2n] by the init routine's tail loop.
    kCharSkill142  = 0x142, // byte: first of four seeded skill values (stride 2)
    // ---- inventory array ----------------------------------------------------
    // The character carries 48 inventory slots of 4 bytes, based at +0x82
    // (ends at +0x82 + 48*4 = +0x142, exactly where the skill pairs begin).
    // Proven by the item-add routine (raw image 0xBF00): it scans for a free
    // slot with `cmp byte es:[bx+si+0x82],0` over si = slot*4, and fills
    //   +0x82 slot 0 : item index (0 = empty), a 1-BASED index into the 185-item
    //                  table -- i.e. item_by_index(slot) -- not a 0-based offset
    //   +0x83 slot 1 : auxiliary state byte (several routines store 0 here)
    //   +0x84 slot 2 : per-item byte copied from a runtime table (small)
    //   +0x85 slot 3 : condition/quality byte (100 = full; consumed down by 2)
    // The sheet's "Weight"/"Rations"/"Gold" lines iterate this array.
    kCharInv82     = 0x82,  // byte: base of the 48 x 4-byte inventory array
    kCharInvCount  = 48,
    kCharInvStride = 4,
    // +0x6b is the number of *used* inventory slots.  The add routine
    // (`0xc11a`, `0x2ed8a`) does `inc byte es:[bx+si+0x6b]` after storing a new
    // item and the remove routine (`0x2e36b`) does `dec`; the armour routine
    // (0x210E8) loops the slots only up to this count.  It equals the number of
    // non-empty slots in every shipped ROST/GAMES record.
    kCharInvUsed6b = 0x6b,  // byte: used inventory-slot count (0..48)
    // +0x25 is a load/carry capacity byte (10 for most, 15 for Cassandra).
    kCharLoad25    = 0x25,  // byte: carrying capacity / load
    // The carried-weight pair, grounded in the weight routine (image 0x1BB32)
    // and the load check (image 0x19B54 / 0x2965f):
    //   +0x64  maximum carryable weight.  Seeded 0x1324 (4900) by the init
    //          routine and recomputed from a per-class stat (+0x45): with
    //          stat <= 15 it is 350 * stat, otherwise 100 * stat + 3400
    //          (routine at image 0x9820).  Every shipped save character matches
    //          this (Str 19 -> 5300, 17 -> 5100, 13 -> 4550, 11 -> 3850).
    //   +0x66  currently carried weight.  Accumulated by the weight routine
    //          (image 0x20F7E) as `10 * rations(+0x70) + gold(+0x72 low word)`
    //          plus the sum of every inventory item's weight (item +0x14).
    // The load check refuses an action while `item_weight + +0x66 > +0x64`, and
    // the sheet's "Weight" line prints the pair as `%04d/%04d` (DS:0xafd9).
    kCharWeight64  = 0x64,  // word: maximum carryable weight
    kCharWeight66  = 0x66,  // word: currently carried weight
    // Gold and rations, shown by the character sheet (builder at image 0x2BC5E).
    // The "Rations.. : %02d" line reads the byte +0x70; the "Gold: %05ld$" line
    // reads the 32-bit +0x72 (low word +0x72, high word +0x74).  Both also
    // contribute to the carried weight above: each ration weighs 10 and each
    // gold piece weighs 1 (the routine adds `10*+0x70 + word[+0x72]`).
    kCharGold72    = 0x72,  // dword: gold (low word +0x72, high word +0x74)
    // The bank account is the dword right after gold: the deposit handler prints
    // "Your account:" from +0x76/+0x78 (0xbc97) and gold from +0x72/+0x74
    // (0xbcc2).  Deposit adds to +0x76, withdraw subtracts from it (0xbd87 /
    // 0xbf76).
    kCharBank76    = 0x76,  // dword: bank account (low word +0x76, high word +0x78)
    kCharRations70 = 0x70,  // byte: rations
    // Hit points.  The character-sheet builder at 28054/28067 reads the current
    // value from +0x22 and divides 100*current by the value at +0x20 to get the
    // HP percentage it shows, so +0x20 is the maximum and +0x22 the current HP
    // (both words).  Every live save character satisfies current <= maximum.
    kCharHpMax20   = 0x20,  // word: maximum hit points
    kCharHpCur22   = 0x22,  // word: current hit points
    // 32-bit experience accumulator: +0x6c is the low word, +0x6e the high word.
    // Identified by the pervasive 32-bit accumulator pattern on the pair -- the
    // reward sites add with `add word [bx+si+0x6c], imm16` / `adc word
    // [bx+si+0x6e], imm16` (e.g. +0x86a0:0x0001 at 0x412, +0x61a8 at 0xf28), and
    // the reward printer pushes [bx+si+0x6e] then [bx+si+0x6c] as one dword --
    // together with the character-sheet label block, which carries the label
    // "Experience " followed by the 32-bit format `%011ld` (DS:0xaf83), and the
    // party table's "Exp." column.  The 32-bit value is the only field in the
    // struct that pair describes.
    kCharExp6c     = 0x6c,  // dword (low word at 0x6c, high word at 0x6e)
    // Status code, shown as the white word right of the panel stat.  Image
    // 0x2EF30 reads `es:[bx+si+0x1f]`, scales it by 4 and indexes the
    // far-pointer table at ds:0x0e70 -- Good / Mad / Ill / Poisn / Blnd. /
    // Stun. / Stone / Dead (ds:0x0480..0x04b0) -- then draws that string.  So
    // the word is per character, not a party-wide constant.  Codes 5..7 are the
    // down states (Stun. / Stone / Dead): the decision screen skips any member
    // whose +0x1f >= 5 (image 0x1879E `cmpb $5` / `jae`).  One of the two +0x49
    // decrements (image 0x15C9) is gated on +0x1f == 5; the other (0x18415) is not.
    kCharStatus1f  = 0x1f,  // byte: status code (index into status_short_names())
    // "Out of action" countdown, tested by the combat decision screen (image
    // 0x16b29 `cmpb $0,es:[bx+0x49]` / `je <action word>`): any non-zero value
    // replaces the member's action column with ds:0x6ff2 "is out of action."
    // and suppresses the trailing number.  The value is a cause code / countdown
    // set from the spell effect id at round-end bookkeeping (image 0x1219D):
    // effect 4 (Paralyze) -> 1, effects 5 and 0x27 -> 5, effect 0x28 -> 3,
    // effect 0x1D -> 0xFF (permanent); equipment that "breaks down" (image
    // 0x10389) writes 0x14.  There are TWO decrements, so the countdown does
    // reach drawn rows: image 0x15C9 (gated on `+0x1f == 5`) and image 0x18415
    // (gated only on `> 0`, at round start, no `+0x1f` gate).  The decision
    // screen draws rows only for `+0x1f < 5`, so a member shows as "out of
    // action." after recovering from Stunned/Stoned while its countdown runs.
    kCharOutOfAction49 = 0x49,  // byte: out-of-action countdown / cause code
    // ---- party-status view (grounded in the party-status builder) ----
    // The builder walks the party, reads the fields below from each 410-byte
    // struct, and packs a 12-byte status row per member into the display block
    // at es:[si+0x179..0x184] (segment [0xb0b0]); the party renderer at 0x134A2
    // draws it.  NOTE: the annotated listing `analysis/lof.asm` labels the
    // builder at 27F4E but is *desynced* through its middle (an extra/mis-decoded
    // instruction shifts the later addresses), so its "+0x6a" line is wrong.
    // The ground truth is the raw image: the builder lives at image offset
    // 0x2D436 (the listing's 27F4E is 0x2D4xx there) and the +50 gate really is
    // `cmp byte es:[bx+0x6a],0`.  These reads pin the offsets; the exact game
    // meaning of each is only partly determined, so the names stay descriptive.
    kCharBody24    = 0x24,  // byte: combat rank 0..3, selecting ds:0x0eb0's
                            //   KIL/ATK/DEF/RET entry (4-entry far-pointer
                            //   table at image 0x187F0).  Written at 0x197F0
                            //   from the '(1)Enter Kill-Rank' .. '(4)Enter
                            //   Retreat-Rank' menu (strings ds:0x713C..0x717A,
                            //   compared at 0x1913C/0x1916C/0x1919C/0x191CC),
                            //   read at 0x197C7/0x1D323, and copied verbatim to
                            //   the display byte +0x17b.  An earlier note read
                            //   those four compares as a dying/dead test and
                            //   called the byte 'body/health state'; the menu
                            //   strings show they are the rank selector.
    kCharFlg5b     = 0x5b,  // byte: attribute modifier; builder shows
                            //   100 - (mod5b + stat1e)
    kCharFlg5c     = 0x5c,  // byte: attribute modifier; builder shows
                            //   mod5c + base1c
    kCharFlg5f     = 0x5f,  // byte: attribute modifier; builder shows
                            //   mod5f + level17
    kCharFlg5e     = 0x5e,  // byte: attribute modifier; the encounter code
                            //   compares signed(mod5e) + Strength against 0x11
                            //   (image 0x13E10 / 0x1BD50) and has an
                            //   `inc byte es:[bx+0x5e]` bump (image 0x1BD63)
    // The init routine zeroes all five (1B8C0..1B92E); a fresh save therefore
    // carries 0.  They are bounded signed modifiers added to a base value.
    kCharFlag6a    = 0x6a,  // byte: flag; when non-zero the builder adds 50 to
                            //   the display byte +0x17c (the "hp_mod" row)
    kCharFlg0f     = 0x0f,  // byte: status-line flag; when zero the builder
                            //   subtracts 10 from the attribute byte +0x17d
};

// One 4-byte inventory slot (see kCharInv82).  `item` is the 1-based item-table
// index (0 = empty); the remaining three bytes are the per-slot state the engine
// keeps alongside it.  All three are grounded in the raw image:
//
//   byte 0  `item`      item-table index, 1-based (0 = empty).  The item-add
//                       routine scans for the first slot with this byte == 0
//                       (`cmp byte es:[bx+si+0x82],0`, image 0xbf0c).
//   byte 1  `equipped`  worn / wielded flag: 1 = equipped, 0 = carried only.
//                       Set to 1 by the equip routine (image 0x2f45e) and
//                       cleared to 0 by the unequip / "take off all" routine
//                       (image 0x2f136, 0x2f170, ...).  `defense_from_items`
//                       (image 0x21139 `cmp byte es:[bx+0x83],0`) counts only
//                       slots with this set.
//   byte 2  `charges`   remaining count / charges.  Seeded from item +0x17
//                       when the item is added (image 0xbf5d) and decremented
//                       by the "use" path (image 0x13c53 `dec byte es:[bx]`).
//                       The slot is consumed when it reaches 0 (image 0x13c4d
//                       `cmp byte es:[bx],0`).
//   byte 3  `condition` durability / quality, 0..100 (100 = pristine).  Seeded
//                       to 100 on add (image 0xbf9d); worn armour is slightly
//                       damaged in the shipped saves (96/98).  Combat damage
//                       subtracts 2 (image 0x1abda) and the armour formula
//                       scales AC by `(condition+10) - (condition+10)%20`
//                       (image 0x21141..0x2117c).
struct InvSlot {
    uint8_t item = 0;       // +0x00  item index (0 = empty slot)
    uint8_t equipped = 0;   // +0x01  worn/wielded flag (1 = equipped)
    uint8_t charges = 0;    // +0x02  remaining count / charges
    uint8_t condition = 0;  // +0x03  durability/quality, 0..100 (100 = full)

    bool is_equipped() const { return equipped != 0; }
};

// A decoded view of the fields the engine's character code reads directly from
// the 410-byte (0x19a) struct, with the verified semantics.  `raw` is one such
// struct (e.g. `SaveChar::raw` or a ROST record at +0x01).  This is a read-only
// projection; the struct is not modified.
struct CharSheet {
    uint8_t  present = 0;    // +0x00  non-zero: slot in use
    std::string name;        // +0x01  14 bytes, NUL-terminated
    int16_t  flags10 = 0;    // +0x10  word (erase-condition guard; -1 = leave)
    int16_t  counter12 = 0;  // +0x12  word (paired with flags10)
    uint8_t  level17 = 0;    // +0x17  level / rank
    uint8_t  class18 = 0;    // +0x18  class flag (1 PC / 2 monster)
    uint8_t  prof1b = 0;     // +0x1b  profession index 0..11 (0x0c = none)
    uint8_t  base1c = 0;     // +0x1c  base stat (45..75)
    uint8_t  stat1d = 0;     // +0x1d  Dexterity-derived stat (see 1BA40)
    uint8_t  stat1e = 0;     // +0x1e  total defence / armour class (see
                             //   defense_from_items); seeded 100 - rec[0x2d]
    int16_t  hp_max = 0;     // +0x20  maximum hit points
    int16_t  hp_cur = 0;     // +0x22  current hit points
    uint32_t exp = 0;        // +0x6c  32-bit experience (low +0x6c, high +0x6e)
    uint8_t  con44 = 0;      // +0x44  Constitution
    uint8_t  str45 = 0;      // +0x45  Strength
    uint8_t  dex46 = 0;      // +0x46  Dexterity
    uint8_t  int47 = 0;      // +0x47  Intelligence
    uint8_t  wis48 = 0;      // +0x48  Wisdom
    uint8_t  speed56 = 0;    // +0x56  speed (1..4)
    uint8_t  body24 = 0;     // +0x24  combat rank 0..3 = KIL/ATK/DEF/RET
    uint8_t  mod5b = 0;      // +0x5b  attribute modifier (100 - (mod5b+stat1e))
    uint8_t  mod5c = 0;      // +0x5c  attribute modifier (mod5c + base1c)
    uint8_t  mod5e = 0;      // +0x5e  attribute modifier (mod5e + Strength)
    uint8_t  mod5f = 0;      // +0x5f  attribute modifier (mod5f + level17)
    uint8_t  flag6a = 0;     // +0x6a  flag (adds 50 to display byte +0x17c)
    uint8_t  flag0f = 0;     // +0x0f  status flag (==0 subtracts 10 from +0x17d)
    // The character's current / maximum magic points; the sheet's "Magic.... "
    // line shows this pair (equal at rest).  See kCharMagMax68.
    uint8_t  mag_max68 = 0;  // +0x68  maximum magic points
    uint8_t  mag_cur69 = 0;  // +0x69  current magic points
    uint8_t  load25 = 0;     // +0x25  carrying capacity / load
    uint16_t weight_max = 0; // +0x64  maximum carryable weight (see kCharWeight64)
    uint16_t weight_cur = 0; // +0x66  currently carried weight (see kCharWeight66)
    uint32_t gold = 0;       // +0x72  gold (low word +0x72, high word +0x74)
    uint8_t  rations70 = 0;  // +0x70  rations (see kCharRations70)
    // The 48 four-byte inventory slots at +0x82 (see kCharInv82).
    std::array<InvSlot, kCharInvCount> inv{};
    // Number of used slots (+0x6b; see kCharInvUsed6b).  The engine's armour
    // routine iterates only these; `defense_from_items` does the same.
    uint8_t inv_used = 0;

    // HP as a percentage, computed exactly as the sheet builder does
    // (100 * hp_cur / hp_max, integer division); 0 when hp_max is 0.
    int hp_percent() const;

    // The profession name from +0x1b (see kCharProf1b).  Returns an empty
    // string when the index is out of range (0x0c = "no profession").
    std::string profession() const;

    // The magic-point maximum the engine's fill routine (image 0xEC00) would
    // compute for this character: the per-(profession, level) table value plus
    // +1 when Wisdom > 15 and +2 more when Intelligence > 15, and 0 when the
    // table value is 0 (or the profession/level is out of range).  `mag_max68`
    // projects the saved value; this recomputes it from the stats.
    uint8_t magic_max() const;

    // --- party-status display row -----------------------------------------
    // The party-status builder packs a 12-byte row per member into its display
    // block; these reproduce each byte from the struct.  See the kCharFlg*
    // notes for the raw-image grounding (builder at image offset 0x2D436).
    //   +0x17a  hp_pct       100 * hp_cur / hp_max
    //   +0x17b  body24       combat rank, verbatim from +0x24
    //   +0x17c  hp_mod       (hp_cur >> 3) + (flag6a ? 50 : 0)
    //   +0x17d  attr_a       100 - (mod5b + stat1e), minus 10 if flag0f == 0
    //   +0x17e  attr_b       mod5c + base1c
    //   +0x17f  attr_c       mod5f + level17
    //   +0x180  hp_cur       (word copy)
    uint8_t hp_pct() const;
    uint8_t hp_mod() const;
    uint8_t attr_a() const;
    uint8_t attr_b() const;
    uint8_t attr_c() const;
};

// Decode a 410-byte character struct into a CharSheet.  `raw` must point at a
// 410-byte buffer.
CharSheet decode_char(const uint8_t* raw);

// Sum of the character's inventory items' weights (item +0x14) over its 48
// slots.  The engine's carried total (`CharSheet::weight_cur`, char +0x66) is
// this sum plus a per-character base of `10 * rations + gold` (see
// `weight_base`); since the game stores the total, this only recomputes the
// item portion.  Empty slots (item == 0) and out-of-range indices contribute
// nothing.
uint32_t carried_item_weight(const CharSheet& c);

// The per-character base term the weight routine (image 0x20F7E) adds before
// the item sum: `10 * rations(+0x70) + gold low word (+0x72)` -- i.e. each
// ration counts 10 weight units and each gold piece 1 (the units are tenths of
// a pound, so a ration is 1 lb and a coin 0.1 lb).  `weight_base(c) +
// carried_item_weight(c)` reproduces the stored carried total `weight_cur`.
uint32_t weight_base(const CharSheet& c);

// The character's total defence (armour class), computed exactly as the engine's
// armour routine does (image 0x210E8; the sum lives at 0x2114E..0x211DA).
// Starting from the Dexterity-derived stat (+0x1d), it adds each EQUIPPED
// inventory item's armour AC (item field f3 / record +0x16) scaled by the item's
// condition (slot byte +0x85, the slot's 4th byte):
//     v = cond + 10; v -= v % 20; ac += f3 * v / 100;
// An item only counts when its aux byte (+0x83, the slot's 2nd byte) is non-zero
// (worn / wielded); a merely carried item adds nothing.  The total is clamped to
// 0x5f.  The engine stores the result in char +0x1e, but only recomputes it when
// the slot flag word +0x10 == -1 (see `flags10`); otherwise the saved +0x1e is
// authoritative.  Use this helper when +0x1e may be stale.
uint8_t defense_from_items(const CharSheet& c);

// The two "hand" weapon slots and their damage bytes, as the melee prelude
// selects them (image 0x1398B..0x13A5D, 0x14DC6..0x14DF7):
//
//   slot[0] = first inventory slot holding an item of class 1 (melee),
//             falling back to class 5 (staff) when there is none;
//   slot[1] = the *next* slot after slot[0] holding a class-1 item (the second
//             hand) — -1 when there is only one;
//   damage  = ItemRecord::damage of that slot's item;
//   f0      = ItemRecord::f0, the durability rating the break check uses
//             (image 0x1588E `mov cl, es:[bx+0xfcc]`).
//
// When both hands hold a weapon the engine draws `rand(2)` once per round to
// pick the hand that attacks (`0x13A43`); see `port/combat.cpp`.
struct WeaponSlots {
    int slot[2] = {-1, -1};
    int damage[2] = {0, 0};
    int f0[2] = {-1, -1};   // item +0x10, the break-check rating (see below)
    bool has_weapon() const { return slot[0] >= 0; }
};
WeaponSlots weapon_slots(const CharSheet& c);

// char +0x1d as derived from the Dexterity at +0x46 by the routine at 1BA40: a
// character below Dexterity 16 keeps 10; from 16 up it is 5*dex - 65 (dex 16 ->
// 15, 19 -> 30).  Verified against every live save character.
uint8_t char_stat1d_from_dex(uint8_t dex);

// --- GAMES/GAMEn (saved game / "gamestanding") ----------------------------

// A saved game file is a 4-byte header (0x00 'F' 'O' 'L') followed by the
// party as fixed 410-byte character records (the same 0x19a struct ROST uses),
// then a short trailing block.  The engine opens `GAMES\GAME%d`; records begin
// at offset 4 with stride 410.
constexpr size_t kSaveHeaderSize = 4;
constexpr uint8_t kSaveMagic[4] = {0x00, 'F', 'O', 'L'};

// The party's current location is stored as two separate digit bytes: the
// ones digit at file offset 0x9a2 and the tens digit at 0x9a4 (verified by
// patching them and watching the emulator).  The engine builds the map name as
// `DNGS\DNG%01ld%01ld` (LOF.EXE 0x4bb35) from these two digits: 5 -> DNG05,
// 10 -> DNG10.  The first-person view tiles live in `EGA\D<ones-digit>`
// (5 -> D5, 10 -> D0); only 0..7 are used as view directories.
constexpr size_t kSaveLocOnes = 0x9a2;
constexpr size_t kSaveLocTens = 0x9a4;

// The party's *cell* on the current level: a 16-bit little-endian row-major
// index `y*36 + x` into the DNG tile map, at file offset 0x9a0.  It sits at
// `4 + 6*410`, i.e. immediately after the six party records, and the world
// state (location, facing, clock) follows it.  Verified by patching: changing
// 125 -> 455 in GAMES/GAME  1 moves the emulator's first-person viewport from
// 1.0000 to 0.0380 against `ref/oracle/dungeon_slot1.png`, so this field — not
// the DNG header — is where the engine reads the party from.
//
// The DNG header carries the same encoding as the level's *entry* cell, so a
// save taken at the entry happens to agree with it (GAME1/Decker: both 125).
// Four of the five archived saves do not, which is why the port must prefer
// this field and fall back to the header only when it is absent.
constexpr size_t kSavePartyPos = 0x9a0;
// Sentinel for a save with no position word (a short file).
constexpr uint16_t kSavePartyPosInvalid = 0xffff;

// The party's facing is a separate digit byte at file offset 0x9a6: 0=N, 1=E,
// 2=S, 3=W (verified by diffing the two shipped saves -- GAME1 faces South
// where GAME2 faces North -- and by the DOSBox dungeon oracle, whose status box
// shows "Facing South").  The lower-left status box prints it with the
// `facing_names()` table.  `kSaveFacingInvalid` marks a save with no facing
// byte (a short file).
constexpr size_t kSaveFacing = 0x9a6;
constexpr uint8_t kSaveFacingInvalid = 0xff;

// The lower-left status box's "Time of day" and "Spirits" lines are also save
// fields, not live-only state.  The save loader (image 0x1CA00) copies the
// 32-bit cell at file offset 0x9b6 into the globals es:[0x3b0c]/[0x3b0e] and the
// 32-bit cell at 0x9ba into es:[0x3b10]/[0x3b12] (each as two 16-bit moves).
// The status-box routine (image 0x2181C) then indexes the time table at
// ds:0x0f04 by [0x3b0c] and the morale table at ds:0x0f24 by [0x3b10].
// Verified against the DOSBox oracle: GAME2 has 0x9b6 = 7 and the oracle prints
// "Evening".  The engine's movement gate compares the morale index [0x3b10] to
// 4 ("Bad") and, when it matches, prints "The companions are tired and won't go
// on" (code: `cmpw $0x4,%es:0x3b10` / `jne` / push ds:0x9bc9; image 0x3ab17).
constexpr size_t kSaveTimeOfDay = 0x9b6;  // dword: time-of-day index (0..7)
constexpr size_t kSaveMorale = 0x9ba;     // dword: party morale index (0..4)

struct SaveChar {
    std::array<uint8_t, 410> raw{};  // one 0x19a character struct
    uint8_t present = 0;             // +0x00  non-zero: slot holds a character
    std::string name;                // +0x01  NUL-terminated name (<= 14 bytes)
    uint8_t profession = 0xff;       // +0x1b  index into profession_names()
    uint8_t panel_stat = 0;          // +0x1e  the numeric stat the play panel shows
};

struct SaveGame {
    bool ok = false;
    uint8_t header[4] = {0, 0, 0, 0};
    uint8_t loc_ones = 0;             // +0x9a2  ones digit of the dungeon id
    uint8_t loc_tens = 0;             // +0x9a4  tens digit of the dungeon id
    uint16_t party_pos = kSavePartyPosInvalid;  // +0x9a0  party cell (y*36+x)
    uint8_t facing = kSaveFacingInvalid;  // +0x9a6  party facing (0=N..3=W)
    int time_of_day = -1;             // +0x9b6  index into time_of_day_names()
    int morale = -1;                  // +0x9ba  index into morale_names()
    std::vector<SaveChar> chars;      // exactly kSaveCharCount records
    // World state verbatim (bytes 0x9a0 .. kSaveChecksum) and the per-cell array
    // (kSaveMap .. end).  Kept whole so `build_save()` round-trips byte-exactly
    // without having to decode every field: only the fields the port actually
    // changes (position, facing, clock) are edited in place by the setters.
    std::vector<uint8_t> world;
    std::vector<uint8_t> map;
    // True iff the file's stored checksum matched on load.  A tampered save is
    // still loaded (the player may want to inspect it), but the caller can warn.
    bool sealed = false;
};

// Combined location id (tens*10 + ones) for a save.
uint16_t save_location(const SaveGame& g);

// Map name (`DNGS/DNG05`) and view-tile directory (`D5`) for a location id.  The
// view directory is empty if the ones digit exceeds 7 (no D8/D9).
std::string dungeon_map_name(uint16_t location);
std::string dungeon_view_dir(uint16_t location);

// --- Save file layout -------------------------------------------------------
//
// A saved game is NOT "a header followed by character records to EOF".  It is
// four distinct regions, and the boundary is fixed, not derived from the file
// size:
//
//     0x0000  magic `00 'F' 'O' 'L'`
//     0x0004  6 x 410-byte character records   (0x9a0 = 4 + 6*410)
//     0x09a0  world state (party cell, location, facing, clock, ...)
//     0x2e3a  4-byte checksum over bytes [0, 0x2e3a)
//     0x2e3e  1296-byte per-cell array (36*36)
//     total   13134
//
// The six slots are exactly the six party members; there is no seventh record.
// This matters: the old reader walked `(size-4)/410` = 32 slots to EOF, so slot
// 6 began AT the world state and its `present` byte read 125 (the party cell),
// making it look like a live character.  That inflated the rations total from
// the correct 19 to 38 (the oracle prints 19).  See notes-a Round 51.
constexpr size_t kSaveCharCount = 6;
constexpr size_t kSaveCharsEnd = kSaveHeaderSize + kSaveCharCount * 410;  // 0x9a0
constexpr size_t kSaveChecksum = 0x2e3a;  // 4-byte fold over [0, 0x2e3a)
constexpr size_t kSaveMap = 0x2e3e;       // 36*36 per-cell bytes
constexpr size_t kSaveMapCells = 36 * 36;
constexpr size_t kSaveSize = kSaveMap + kSaveMapCells;  // 13134

// The checksum is the same alternating ADD/XOR fold the ROST records use
// (`rost_record_checksum`), seeded with 1, with NUL counted as 1.  Verified: it
// reproduces the stored word on every intact archived save.  GAME5's stored word
// is wrong (13680 vs 22103), i.e. that archive copy is tampered; the engine
// rejects it, and so must the writer's round-trip check.
uint32_t save_checksum(const uint8_t* data, size_t len);

// Serialise a save.  Produces the full kSaveSize byte image with a correct
// checksum, so `build_save(read_save(f))` is byte-identical to an intact f.
std::vector<uint8_t> build_save(const SaveGame& g);

// True iff the file's stored checksum matches its contents.
bool save_checksum_ok(const std::vector<uint8_t>& bytes);

// Write `bytes` to `path`.  The caller is expected to pass build_save() output,
// which is already sealed; this is the only place the port touches a save file
// on disk, so the "never write an unsealed save" rule has one enforcement point.
bool write_save_file(const std::string& path, const std::vector<uint8_t>& bytes);

// Move the party in the in-memory save: updates the world-state copy at +0x9a0
// and +0x9a6 and the mirrored convenience fields, so build_save() emits the new
// position.  No-op when the save carries no world block.
void save_set_party(SaveGame& g, uint16_t cell, uint8_t facing);

// Set the dungeon location (the two digit bytes at +0x9a2 / +0x9a4).
void save_set_location(SaveGame& g, uint8_t tens, uint8_t ones);

// "The Amulet" item index (1-based) handed out by the town-exit give-artefact
// page; char_items.inc lists it as name #120.
constexpr int kItemAmulet = 120;

// Give a character an item (1-based item-table index), reproducing the engine's
// item-add routine (image 0x69e2..0x6c5f): scan the 48 inventory slots for the
// first empty one (item byte +0x82 == 0), store the item index, seed the slot's
// charges from the item record's +0x13 byte and its condition to 100, increment
// the used-slot count (+0x6b), and refresh the carried weight (+0x66) the way
// the engine's post-add recompute does.  Returns the slot index used, or -1 when
// the inventory is full or the index is invalid.  (The +0x25 "load" tweak at
// 0x6b3d..0x6b6b is shop bookkeeping and is not reproduced here.)
int give_item(SaveChar& c, int item_index);

// --- Inn-rest cost + effect (image 0x820a..0x8680) --------------------------
// The night's-rest price, derived from the counting loop at 0x823a..0x83a9:
//   2 * present_members  [+ 3 * sum_over_members(min(14, (weight_max(+0x64) -
//                          weight_cur(+0x66)) / 10))  when `with_rations`]
// (bp-0x18 = 2 * member_count; the W-rations branch at 0x8397 adds 3 * sum).
// pay_gold deducts `cost` from the payer's gold dword (+0x72) and returns false
// when the payer is short (0xbdec).  rest_party applies the night to every
// present member (0x8480..0x85d8): decrement the out-of-action countdown (+0x49,
// clearing status +0x1f and +0x4b when it hits 0); set rations (+0x70) to 14
// when `with_rations`; for a member whose status (+0x1f) is 0, heal HP (+0x22)
// by 2*level(+0x17) + max(0, Con(+0x44) - 14), clamped to hp_max (+0x20); and
// restore magic points (+0x69 += max(1, +0x68>>3), clamped to +0x68).  The
// spell-memorisation pass (0x841e..0x8487) and the post-"Good night" 32-slot
// reset are not reproduced.
int rest_cost(const SaveGame& g, bool with_rations);
bool pay_gold(SaveChar& c, int cost);
void rest_party(SaveGame& g, bool with_rations);

// --- Temple (H)eal (image 0x9f88..0xab6a) ----------------------------------
// The price for healing a wounded character (type-1 path, 0x9fde..0xa53c):
//   max(1, (hp_max(+0x20) - hp_cur(+0x22)) * level(+0x17) / 2)  when wounded,
//   else 0 (nothing to heal).  The heal (0xaa4a..0xab56) sets hp_cur = hp_max
//   and, if the character carried a status code (+0x1f), clears it and sets
//   +0x55 = 1.  The payer is a separately chosen character (paid through the
//   same pay_gold routine as the inn rest).  The (R)evive option is reproduced
//   separately (temple_revive); the cure-condition-only variant is not.
int temple_heal_cost(const SaveChar& c);
void temple_heal_apply(SaveChar& c);

// --- Temple (R)evive (image 0x9f88..0xab6a, type 3) ------------------------
// Price = level(+0x17) * 300 (0xa5b3..0xa5d2).  The revival roll is
// rand(100)+1 tested against CON(+0x44)*4 + 5 (0xa86d..0xa8ba) and CON must be
// non-zero; on success CON is decremented (if > 1), status (+0x1f) is cleared
// and hp_cur (+0x22) is raised to at least 1 (0xaa06); on failure hp_max and
// hp_cur are zeroed (the body decays).  The +0x6c experience boost the success
// path also adds (a profession-table formula at 0xa983..0xa9d5) is not
// reproduced.  `roll` is the engine's rand(100)+1, in 1..100.
int temple_revive_cost(const SaveChar& c);
bool temple_revive(SaveChar& c, int roll);

// --- Bank deposit / withdraw (image 0xbb7a..0xbf7e) ------------------------
// Each character has a bank account dword (+0x76/+0x78) beside its gold
// (+0x72/+0x74).  bank_deposit moves `amount` from gold to the account (0xbd57
// / 0xbd87) and refuses when gold < amount (0xbdca); bank_withdraw is the
// mirror (account -= amount at 0xbf76, gold += amount in the caller) and
// refuses when account < amount.  `amount` 0 is a no-op.
bool bank_deposit(SaveChar& c, uint32_t amount);
bool bank_withdraw(SaveChar& c, uint32_t amount);

// Read-only accessors for the gold (+0x72) and bank account (+0x76) dwords.
uint32_t char_gold(const SaveChar& c);
uint32_t char_bank(const SaveChar& c);

// --- Joint deposit / Gather gold (shared routine, image 0x28caa) -----------
// Sums every present character's gold (+0x72), then after a weight check on the
// chosen recipient (+0x64 weight_max vs +0x66 weight_cur - target.gold + total)
// moves the whole total into the recipient and zeroes the others
// (0x28dd4..0x28e50).  Returns false when there is no gold or the recipient
// cannot carry the total.  This is the routine behind every "(J)oint deposit"
// and the emporium's "(G)ather gold".
bool joint_deposit(SaveGame& g, int target);

// --- Emporium buy (purchase effect, image 0x63f4) --------------------------
struct ShopData;  // defined below (ROST/SHOP)
// Buy item `item_index` (1-based, item_by_index order) for character
// `char_idx` (0-based).  Refuses if the shop does not carry it (ShopData::
// carries) or the character cannot pay item.price; otherwise deducts the price
// (pay_gold), adds the item (give_item) and decrements the stock slot unless it
// is the 0xFF unlimited sentinel.  The interactive 8-slot shop display (0x6c60)
// and the charisma-haggling price variation (the rare 0x64c2 branch) are not
// reproduced -- this is the normal full-price purchase (the common 0x6458 path).
// `shop` may be null to skip the stock check.  Returns true on a completed buy.
bool buy_item(SaveGame& g, int char_idx, int item_index, ShopData* shop);

// --- Item use effects (use routine 0x0e89d) --------------------------------
// Applies the use-effect of item `item_index` (its +0x1f effect byte) to
// character `c`.  Derived effects: 10 heal HP by item.charges (clamped to
// hp_max), 3 poison (hp -= charges; set status +0x1f=3 / +0x57=4 / +0x58=0x12),
// 1 enchant weapon (+0x5e += 2 when +0x5e + +0x45 < 0x11), 2 enchant armour
// (+0x5b += 2).  Returns a short verb ("heals"/"poisons"/...) or "" if the
// effect is not derived.  Item consumption (removal from the slot) is the
// caller's job in the engine and is not applied here.
std::string use_item_effect(SaveChar& c, int item_index);

// Load GAMES/GAMEn; ok is false only if the file is too short or the header is
// not the expected magic.
SaveGame load_save(const std::string& path);

// --- Party-status projection ----------------------------------------------
// One member's row in the party-status display block the town/play loop builds
// (segment [0xb0b0], at es:[si+0x179..0x184]).  Reproduced byte-for-byte from
// the builder at image offset 0x2D436; see `CharSheet::hp_pct()` etc.
struct SnsRecord;

struct PartyRow {
    uint8_t  index    = 0;  // +0x179  party index (the builder also writes it
                            //   at +0x177; +0x176 is the constant in-use marker 2)
    uint8_t  hp_pct   = 0;  // +0x17a  100 * hp_cur / hp_max
    uint8_t  body24   = 0;  // +0x17b  combat rank (verbatim from +0x24)
    uint8_t  hp_mod   = 0;  // +0x17c  (hp_cur >> 3) + (flag6a ? 50 : 0)
    uint8_t  attr_a   = 0;  // +0x17d  100 - (mod5b + stat1e), -10 if flag0f == 0
    uint8_t  attr_b   = 0;  // +0x17e  mod5c + base1c
    uint8_t  attr_c   = 0;  // +0x17f  mod5f + level17
    uint16_t hp_cur   = 0;  // +0x180  current HP (word copy)
};

// Project a decoded character into its party-status row.
PartyRow party_row(const CharSheet& c, uint8_t index);

// Project one defender into its combat row.  The defender row builder is image
// 0x27A1E (per group `[bp-0x34]`, per member `[bp-0x3a]`, row address
// `0x48*group + 12*member` added to the row base +0x258); it copies the
// monster's *combat record* (the 78-byte `SYST/NSCnn` record, see
// `xp_per_damage`) field by field:
//
//   row +0x00 = 1                        marker ("defender")
//   row +0x01 = the monster index        (the encounter's per-group id list)
//   row +0x02 = combat[+0x4c]
//   row +0x04 = 100 * hp / (8 * combat[+0x2c])      (0x27B27..0x27B35)
//   row +0x05 = group + 1                            (0x27A6F)
//   row +0x06 = 0, +0x32 when combat[+0x39] != 0     (0x27B60/0x27B98; the
//              live capture reads 236 there, so a later write also lands on
//              this byte — it is not read by the hit score)
//   row +0x07 = combat[+0x2d]                        (0x27AA9)
//   row +0x08 = combat[+0x2e]                        (0x27ABF)
//   row +0x09 = combat[+0x30]                        (0x27AD5)
//   row +0x0a = hp (word)                            (0x27B00/0x27B09)
//
// `hp`/`hp_max` are the monster's rolled hit points (the word the builder
// reads from the monster instance), so `hp_pct` here is the builder's own
// `row[+0x04]`, not a 0..100 percentage.
PartyRow defender_row(const SnsRecord& rec, int group, int hp, int hp_max);

// Project a whole save's live party (slots with `present == 1`, in order).
std::vector<PartyRow> party_status(const SaveGame& sv);

// The play-screen party panel draws, under each character's profession line, a
// dark stat number: `+0x1e` in decimal followed by a trailing symbol glyph
// (verified pixel-exact against the DOSBox play-screen capture for all six
// shipped characters -> "50%", "90%", "50%", "20%", "40%", "65%").  The digits
// are CHR2 band-0 cells 27+d and the trailing glyph is CHR2 band-1 cell 37, in
// the panel's 5x5 face.
std::string panel_stat_text(uint8_t stat1e);

// --- ROST/SHOP ------------------------------------------------------------

struct ShopData {
    // ROST/SHOP is 190 bytes: a header byte (0), then 185 stock bytes, one per
    // item in the item-table order, then a 4-byte trailer (word 0x1606 + a
    // zeroed word).  This is the emporium's STOCK table: 0 = the shop does not
    // carry the item, 1..254 = finite units in stock, 0xFF = an "always in
    // stock" sentinel (the buy routine refuses only at 0xFF, image 0xb644).
    // The buy routine reads the slot and does `dec es:0x4616(bx)` (image
    // 0xc130..0xc146); the sell/restock routine does `inc` (image 0xb64f).  The
    // engine copies the 190 bytes to DS:0x4616 at load; if the file cannot be
    // opened it falls back to a default stock of 0xFF for items 1..44 (image
    // 0xa72f..0xa77b), i.e. a fresh game stocks the first 44 items.
    uint8_t stock[kItemCount] = {0};
    uint16_t trailer = 0;
    bool ok = false;

    // Whether the shop carries the item (1-based index, as item_by_index).
    bool carries(int item_index) const {
        return item_index >= 1 && (size_t)item_index <= kItemCount &&
               stock[item_index - 1] != 0;
    }
    // Whether the shop has an unlimited supply (the 0xFF sentinel).
    bool unlimited(int item_index) const {
        return item_index >= 1 && (size_t)item_index <= kItemCount &&
               stock[item_index - 1] == 0xff;
    }
};

ShopData load_shop(const std::string& path);

// --- Debug dump -----------------------------------------------------------
// Human-readable dump used by tests/char_test.cpp --dump-spec and available for
// the maintainer to wire into the `lof` front-end.
void dump_spec(std::FILE* out, const SpecTable& t, const char* label);

}  // namespace lofport

#endif
