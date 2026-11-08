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
//   ROST/SHOP            a small table of signed per-item price adjustments.
#ifndef LOF_PORT_CHAR_H
#define LOF_PORT_CHAR_H

#include <array>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

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

    bool has_index() const { return index_c9 != 0xff; }
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
// class and a byte equipment slot.
constexpr size_t kItemCount = 185;
constexpr size_t kItemNameLen = 16;
constexpr size_t kItemRecordSize = 32;

struct ItemRecord {
    std::string name;
    int16_t f0 = 0;  // melee weapon: hands (1/2); else 0
    int16_t f1 = 0;  // melee weapon: damage; armour/others: a cost
    int16_t f2 = 0;  // weapon: cost; armour: raw AC bonus
    int16_t f3 = 0;  // projectiles: ammo count; armour: equipped AC
    int16_t f4 = 0;  // weapon: class/penalty mask
    int16_t f5 = 0;  // projectile pairing / misc
    int16_t f6 = 0;  // class-eligibility mask (bit n => class n may use)
    uint8_t item_class = 0;  // 1 melee, 2 armour, 4 projectile, 5 helmet, ...
    uint8_t slot = 0;        // equipment slot
};

// The decoded table in engine order (index 0 = "Club").
const std::vector<ItemRecord>& items();

// 1-based item lookup as used by saves/maps; nullptr when out of range.
const ItemRecord* item_by_index(int index);

// Print the decoded table (for eyeballing).
void dump_items(FILE* out);

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
    // The init routine at 1BA40 sets stat1d = 0x0a, and when level46 > 0x0f
    // overrides it with 5*level46 - 0x41 + 1; both are byte quantities.
    kCharStat1d    = 0x1d,  // byte
    kCharLevel46   = 0x46,  // byte
};

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
    std::vector<int8_t> adjustments;  // bytes 1..45, signed
    uint16_t trailing = 0;            // last word in the file
    bool ok = false;
};

ShopData load_shop(const std::string& path);

// --- Debug dump -----------------------------------------------------------
// Human-readable dump used by tests/char_test.cpp --dump-spec and available for
// the maintainer to wire into the `lof` front-end.
void dump_spec(std::FILE* out, const SpecTable& t, const char* label);

}  // namespace lofport

#endif
