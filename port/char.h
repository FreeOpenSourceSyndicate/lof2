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
    kSnsClass4c = 0x4c,  // byte; char +0x18 (1 for PCs, 2 for big monsters)
};

// The character-struct fields the SNSC/NSC init routine seeds from a class
// record (analysis/lof.asm 1DA8E..1DCBB).  Offsets name the target byte in the
// 410-byte character struct.
struct CharSeed {
    uint8_t c18 = 0;  // rec[0x4c]
    uint8_t c1c = 0;  // rec[0x2e]
    uint8_t c1d = 0;  // 100 - rec[0x2d]
    uint8_t c1e = 0;  // 100 - rec[0x2d]
    uint8_t c56 = 0;  // rec[0x36]
    uint8_t c7a_index = 0;  // rec[0x2a]; char[0x7a + index] is then set to 2
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
    // Hit points.  The character-sheet builder at 28054/28067 reads the current
    // value from +0x22 and divides 100*current by the value at +0x20 to get the
    // HP percentage it shows, so +0x20 is the maximum and +0x22 the current HP
    // (both words).  Every live save character satisfies current <= maximum.
    kCharHpMax20   = 0x20,  // word: maximum hit points
    kCharHpCur22   = 0x22,  // word: current hit points
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
    uint8_t  con44 = 0;      // +0x44  Constitution
    uint8_t  str45 = 0;      // +0x45  Strength
    uint8_t  dex46 = 0;      // +0x46  Dexterity
    uint8_t  int47 = 0;      // +0x47  Intelligence
    uint8_t  wis48 = 0;      // +0x48  Wisdom
    uint8_t  speed56 = 0;    // +0x56  speed (1..4)

    // HP as a percentage, computed exactly as the sheet builder does
    // (100 * hp_cur / hp_max, integer division); 0 when hp_max is 0.
    int hp_percent() const;
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

struct SaveChar {
    std::array<uint8_t, 410> raw{};  // one 0x19a character struct
    uint8_t present = 0;             // +0x00  non-zero: slot holds a character
    std::string name;                // +0x01  NUL-terminated name (<= 14 bytes)
};

struct SaveGame {
    bool ok = false;
    uint8_t header[4] = {0, 0, 0, 0};
    std::vector<SaveChar> chars;      // floor((size - 4) / 410) records
    std::vector<uint8_t> trailer;     // any bytes past the last whole record
};

// Load GAMES/GAMEn; ok is false only if the file is too short or the header is
// not the expected magic.
SaveGame load_save(const std::string& path);

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
