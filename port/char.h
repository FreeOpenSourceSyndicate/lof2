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
//   ROST/SHOP            a header byte, one signed adjustment per item, a word.
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

// The behaviour dispatch type at record offset 0xe2.  LOF reads this word in
// its event handlers (`cmp es:[bx+si+0xe2], TYPE`); the observed values are
// 0..22, 24 and 25.  The names below are derived by correlating each value with
// the record's text (all 665 records across SPEC0..7) and the handler set the
// disassembly keys off (1, 7, 9, 0x0c, 0x0e, 0x11, 0x19).  They are descriptive
// labels, not engine symbol names.
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
    uint16_t arg_da = 0;      // 0xda  word argument
    SpecType type = SPEC_TYPE_TEXT;  // 0xe2  behaviour dispatch type
    uint16_t counter = 0;     // 0xe4  countdown/counter
    uint16_t arg_e6 = 0;      // 0xe6  extra argument (type 7/14)
    uint16_t arg_e8 = 0;      // 0xe8  extra argument
    uint8_t class_eb = 0xff;  // 0xeb  SNSC class index (0xff = none); see notes

    bool has_index() const { return index_c9 != 0xff; }
    bool has_class() const { return class_eb != 0xff; }
};

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
};

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
    // Seven little-endian int16 fields.  f1..f4 are only partially named (their
    // exact combat semantics live in the play-loop code); f5/f6 are grounded.
    int16_t f0 = 0;  // weapon/armour class code (1..4)
    int16_t f1 = 0;  // weapon: damage; armour: cost
    int16_t f2 = 0;  // weapon: cost; armour: AC bonus
    int16_t f3 = 0;  // armour: further AC; projectiles: ammo count
    int16_t f4 = 0;  // weapon: cost / damage multiplier
    int16_t f5 = 0;  // projectile -> launcher item index (weapons: 0)
    int16_t f6 = 0;  // class-eligibility mask (bit n => class n may use)
    // Item class: 0 misc/key, 1 melee, 2 armour, 3 shield, 4 projectile,
    // 5 staff, 6 scroll/book/map, 8 ring, 9 potion, 10 treasure, 11 bow/sling.
    uint8_t item_class = 0;
    // Use/effect index: 0 for ordinary gear; 1..40 on magic & consumable items
    // (potions, rings, wands, special weapons).  Not an equip slot.
    uint8_t effect = 0;
};

// The decoded table in engine order (index 0 = "Club").
const std::vector<ItemRecord>& items();

// 1-based item lookup as used by saves/maps; nullptr when out of range.
const ItemRecord* item_by_index(int index);

// Descriptive label for an item's use-effect index (`ItemRecord::effect`), the
// value the engine switches on when the party "uses" an item (potions, rings,
// wands, scrolls, quest items).  Derived by grouping the item table by effect
// value, so the labels are descriptive, not engine symbol names.  Returns a
// non-null string for every input (unknown/0 => "none").
const char* item_effect_name(uint8_t effect);

// Item indices (into `items()`) whose use-effect index equals `effect`, in
// table order.  `effect == 0` returns the plain (non-magical) gear.
std::vector<size_t> items_with_effect(uint8_t effect);

// True if item `index` (1-based) is usable by SNSC class `cls` (0..13) per its
// eligibility mask (f6); out-of-range item or class returns false.
bool item_usable_by_class(int index, int cls);

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

// Spell effect code: byte 8 of a spell's 14 data bytes.  Grouping all 141
// spells by it yields clean families (healing, light, paralyse, fear, summon,
// map, …), so it is the id the play loop switches on.  Descriptive label
// (non-null for any input; unknown/0 => "damage"); `spells_with_effect()`
// lists the spells carrying a code.
constexpr size_t kSpellEffectByte = 8;
const char* spell_effect_name(uint8_t code);
std::vector<size_t> spells_with_effect(uint8_t code);

// Print the decoded table (for eyeballing).
void dump_spells(FILE* out);

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
    kSnsBody2d  = 0x2d,  // byte; char +0x1e = 100 - value, char +0x1d = same
    kSnsBody2e  = 0x2e,  // byte; char +0x1c (45..75, a base stat)
    kSnsSpeed36 = 0x36,  // byte; char +0x56 (1..4)
    kSnsField3a = 0x3a,  // byte; char +0x14 (word; a per-class value, Siegurd 18)
    // Four (value, level) skill pairs: char +0x142+2n = rec[0x3b+2n] and
    // char +0x143+2n = rec[0x3c+2n], n = 0..3 (the loop at 1DC00/1DC40).
    kSnsSkill3b = 0x3b,  // byte; first of four skill values, stride 2
    kSnsSkill3c = 0x3c,  // byte; first of four skill levels, stride 2
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
    uint8_t  c1e = 0;   // +0x1e  100 - rec[0x2d]  body stat
    uint8_t  c56 = 0;   // +0x56  rec[0x36]  speed 1..4
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
    kCharBase1c    = 0x1c,  // byte: base stat (45..75)
    kCharStat1e    = 0x1e,  // byte: body stat (100 - rec[0x2d])
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
    // 0xEC00..0xEC2B, which looks up a per-(class,level) table in the loaded
    // data segment `[0xaf70]:0xa38` (16 bytes per class, indexed
    // `16 * class_field(+0x1b) + level(+0x17)`, only while level < 16) and then
    // adds +1 when Wisdom(+0x48) > 15 and +2 more when Intelligence(+0x47) > 15.
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
    // +0x25 is a load/carry capacity byte (10 for most, 15 for Cassandra).
    kCharLoad25    = 0x25,  // byte: carrying capacity / load
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
    kCharBody24    = 0x24,  // byte: body/health state; the builder copies it to
                            //   the display byte +0x17b, and the death routine
                            //   (0x1919C/0x191CC) tests it for == 2 / == 3
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
    // The play-screen party panel reads the profession byte +0x1b (index into
    // profession_names()) and the numeric stat byte +0x1e (printed "%d%%").
    kCharProf1b    = 0x1b,  // byte: index into profession_names()
};

// One 4-byte inventory slot (see kCharInv82).  `item` is the 1-based item-table
// index (0 = empty); the remaining three bytes are auxiliary state the engine
// keeps alongside it (class/condition/flag).
struct InvSlot {
    uint8_t item = 0;   // +0x00  item index (0 = empty slot)
    uint8_t aux1 = 0;   // +0x01  auxiliary state byte
    uint8_t aux2 = 0;   // +0x02  auxiliary state byte
    uint8_t cond = 0;   // +0x03  condition/quality byte (100 = full)
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
    uint8_t  base1c = 0;     // +0x1c  base stat (45..75)
    uint8_t  stat1d = 0;     // +0x1d  Dexterity-derived stat (see 1BA40)
    uint8_t  stat1e = 0;     // +0x1e  body stat (100 - rec[0x2d])
    int16_t  hp_max = 0;     // +0x20  maximum hit points
    int16_t  hp_cur = 0;     // +0x22  current hit points
    uint32_t exp = 0;        // +0x6c  32-bit experience (low +0x6c, high +0x6e)
    uint8_t  con44 = 0;      // +0x44  Constitution
    uint8_t  str45 = 0;      // +0x45  Strength
    uint8_t  dex46 = 0;      // +0x46  Dexterity
    uint8_t  int47 = 0;      // +0x47  Intelligence
    uint8_t  wis48 = 0;      // +0x48  Wisdom
    uint8_t  speed56 = 0;    // +0x56  speed (1..4)
    uint8_t  body24 = 0;     // +0x24  body/health state (2 dying, 3 dead)
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
    // The 48 four-byte inventory slots at +0x82 (see kCharInv82).
    std::array<InvSlot, kCharInvCount> inv{};

    // HP as a percentage, computed exactly as the sheet builder does
    // (100 * hp_cur / hp_max, integer division); 0 when hp_max is 0.
    int hp_percent() const;

    // --- party-status display row -----------------------------------------
    // The party-status builder packs a 12-byte row per member into its display
    // block; these reproduce each byte from the struct.  See the kCharFlg*
    // notes for the raw-image grounding (builder at image offset 0x2D436).
    //   +0x17a  hp_pct       100 * hp_cur / hp_max
    //   +0x17b  body24       (verbatim)
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

// The party's facing is a separate digit byte at file offset 0x9a6: 0=N, 1=E,
// 2=S, 3=W (verified by diffing the two shipped saves -- GAME1 faces South
// where GAME2 faces North -- and by the DOSBox dungeon oracle, whose status box
// shows "Facing South").  The lower-left status box prints it with the
// `facing_names()` table.  `kSaveFacingInvalid` marks a save with no facing
// byte (a short file).
constexpr size_t kSaveFacing = 0x9a6;
constexpr uint8_t kSaveFacingInvalid = 0xff;

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
    uint8_t facing = kSaveFacingInvalid;  // +0x9a6  party facing (0=N..3=W)
    std::vector<SaveChar> chars;      // floor((size - 4) / 410) records
    std::vector<uint8_t> trailer;     // any bytes past the last whole record
};

// Combined location id (tens*10 + ones) for a save.
uint16_t save_location(const SaveGame& g);

// Map name (`DNGS/DNG05`) and view-tile directory (`D5`) for a location id.  The
// view directory is empty if the ones digit exceeds 7 (no D8/D9).
std::string dungeon_map_name(uint16_t location);
std::string dungeon_view_dir(uint16_t location);

// Load GAMES/GAMEn; ok is false only if the file is too short or the header is
// not the expected magic.
SaveGame load_save(const std::string& path);

// --- Party-status projection ----------------------------------------------
// One member's row in the party-status display block the town/play loop builds
// (segment [0xb0b0], at es:[si+0x179..0x184]).  Reproduced byte-for-byte from
// the builder at image offset 0x2D436; see `CharSheet::hp_pct()` etc.
struct PartyRow {
    uint8_t  index    = 0;  // +0x179  party index (the builder also writes it
                            //   at +0x177; +0x176 is the constant in-use marker 2)
    uint8_t  hp_pct   = 0;  // +0x17a  100 * hp_cur / hp_max
    uint8_t  body24   = 0;  // +0x17b  body/health state (verbatim)
    uint8_t  hp_mod   = 0;  // +0x17c  (hp_cur >> 3) + (flag6a ? 50 : 0)
    uint8_t  attr_a   = 0;  // +0x17d  100 - (mod5b + stat1e), -10 if flag0f == 0
    uint8_t  attr_b   = 0;  // +0x17e  mod5c + base1c
    uint8_t  attr_c   = 0;  // +0x17f  mod5f + level17
    uint16_t hp_cur   = 0;  // +0x180  current HP (word copy)
};

// Project a decoded character into its party-status row.
PartyRow party_row(const CharSheet& c, uint8_t index);

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
    // ROST/SHOP is 190 bytes: a header byte (observed 0), then one signed byte
    // per item (185 of them, parallel to the item table order), then a 4-byte
    // trailer word.  Observed values are 0..-10, a per-item shop adjustment.
    int8_t adjustments[kItemCount] = {0};
    uint16_t trailer = 0;
    bool ok = false;
};

ShopData load_shop(const std::string& path);

// --- Debug dump -----------------------------------------------------------
// Human-readable dump used by tests/char_test.cpp --dump-spec and available for
// the maintainer to wire into the `lof` front-end.
void dump_spec(std::FILE* out, const SpecTable& t, const char* label);

}  // namespace lofport

#endif
