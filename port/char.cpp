#include "char.h"

#include <cstdio>
#include <cstring>

#include "data.h"

namespace lofport {

#include "char_items.inc"
#include "char_spells.inc"
#include "char_names.inc"

namespace {

std::vector<uint8_t> read_file(const std::string& path) {
    std::vector<uint8_t> v;
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return v;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n > 0) {
        v.resize(n);
        if (fread(v.data(), 1, n, f) != (size_t)n) v.clear();
    }
    fclose(f);
    return v;
}

uint16_t rd16(const uint8_t* p) { return (uint16_t)(p[0] | (p[1] << 8)); }

// Read a NUL-terminated string from `p`, at most `limit` bytes.
std::string cstr(const uint8_t* p, size_t limit) {
    size_t l = 0;
    while (l < limit && p[l]) l++;
    return std::string((const char*)p, l);
}

}  // namespace

bool parse_spec(const std::vector<uint8_t>& data, SpecTable& out) {
    out = SpecTable{};
    if (data.empty()) return false;
    const size_t count = data[0];
    if (data.size() < 1 + count * kSpecRecordSize) return false;

    out.declared_count = (uint8_t)count;
    out.records.reserve(count);
    for (size_t i = 0; i < count; i++) {
        const uint8_t* r = data.data() + 1 + i * kSpecRecordSize;
        SpecRecord rec;
        std::memcpy(rec.raw.data(), r, kSpecRecordSize);

        rec.text_a = cstr(r + 0x0e, 0x4a - 0x0e);
        rec.text_b = cstr(r + 0x50, 0xc6 - 0x50);

        rec.flag_4b = r[0x4b];
        rec.arg_4c = rd16(r + 0x4c);
        rec.arg_4e = (int16_t)rd16(r + 0x4e);
        rec.index_c9 = r[0xc9];
        rec.arg_da = rd16(r + 0xda);
        rec.type = (SpecType)rd16(r + 0xe2);
        rec.counter = rd16(r + 0xe4);
        rec.arg_e6 = rd16(r + 0xe6);
        rec.arg_e8 = rd16(r + 0xe8);

        out.records.push_back(std::move(rec));
    }
    out.ok = true;
    return true;
}

SpecTable load_spec(const std::string& path) {
    SpecTable t;
    parse_spec(read_file(path), t);
    return t;
}

std::vector<RostRecord> load_rost(const std::string& path) {
    std::vector<RostRecord> out;
    std::vector<uint8_t> d = read_file(path);
    if (d.empty()) return out;
    const size_t count = d[0];
    if (d.size() < 1 + count * kRostRecordSize) return out;
    out.reserve(count);
    for (size_t i = 0; i < count; i++) {
        const uint8_t* r = d.data() + 1 + i * kRostRecordSize;
        RostRecord rec;
        std::memcpy(rec.raw.data(), r, kRostRecordSize);
        rec.present = r[0x00];
        rec.name = cstr(r + 0x01, 14);
        rec.tail = (uint32_t)r[410] | ((uint32_t)r[411] << 8) |
                   ((uint32_t)r[412] << 16) | ((uint32_t)r[413] << 24);
        out.push_back(std::move(rec));
    }
    return out;
}

SaveGame load_save(const std::string& path) {
    SaveGame g;
    std::vector<uint8_t> d = read_file(path);
    if (d.size() < kSaveHeaderSize) return g;
    std::memcpy(g.header, d.data(), kSaveHeaderSize);
    g.ok = std::memcmp(g.header, kSaveMagic, 4) == 0;
    if (!g.ok) return g;
    size_t n = (d.size() - kSaveHeaderSize) / 410;
    for (size_t i = 0; i < n; i++) {
        const uint8_t* r = d.data() + kSaveHeaderSize + i * 410;
        SaveChar c;
        std::memcpy(c.raw.data(), r, 410);
        c.present = r[0];
        std::string nm;
        for (int k = 1; k < 15 && r[k]; k++) nm.push_back((char)r[k]);
        c.name = std::move(nm);
        g.chars.push_back(std::move(c));
    }
    size_t end = kSaveHeaderSize + n * 410;
    g.trailer.assign(d.begin() + end, d.end());
    return g;
}

ShopData load_shop(const std::string& path) {
    ShopData s;
    std::vector<uint8_t> d = read_file(path);
    if (d.empty()) return s;
    // Byte 0 is a header (zero in this release); the next 185 bytes are one
    // signed adjustment per item, parallel to the item table order (the engine
    // reads 186 then 4).  The file ends with a 16-bit trailer word.
    if (d.size() < 1 + kItemCount) return s;
    for (size_t i = 0; i < kItemCount; i++) s.adjustments[i] = (int8_t)d[1 + i];
    if (d.size() >= 4) s.trailer = rd16(d.data() + d.size() - 4);
    s.ok = true;
    return s;
}

const char* spec_type_name(uint16_t type) {
    switch (type) {
        case 0: return "text";
        case 1: return "recruit";
        case 2: return "spring";
        case 3: return "spring2";
        case 4: return "battle";
        case 5: return "passage";
        case 6: return "empty6";
        case 7: return "door";
        case 8: return "trade";
        case 9: return "sign";
        case 10: return "spring3";
        case 11: return "teleport";
        case 12: return "button";
        case 13: return "inscription";
        case 14: return "challenge";
        case 15: return "empty15";
        case 16: return "lava";
        case 17: return "puzzle";
        case 18: return "trap";
        case 19: return "button19";
        case 20: return "button20";
        case 21: return "button21";
        case 22: return "button22";
        case 24: return "door24";
        case 25: return "elemental";
        default: return "unknown";
    }
}

void dump_spec(std::FILE* out, const SpecTable& t, const char* label) {
    std::fprintf(out, "%s: %u records\n", label, (unsigned)t.declared_count);
    for (size_t i = 0; i < t.records.size(); i++) {
        const SpecRecord& r = t.records[i];
        std::fprintf(out,
                     "  [%3zu] type=%-3u (%-11s) idx=%02x arg4c=%u arg4e=%d cnt=%u\n",
                     i, (unsigned)r.type, spec_type_name(r.type), r.index_c9,
                     r.arg_4c, r.arg_4e, r.counter);
        if (!r.text_a.empty()) std::fprintf(out, "        a: %s\n", r.text_a.c_str());
        if (!r.text_b.empty()) std::fprintf(out, "        b: %s\n", r.text_b.c_str());
    }
}

const std::vector<ItemRecord>& items() {
    static const std::vector<ItemRecord> table = [] {
        std::vector<ItemRecord> v;
        v.reserve(kItemCount);
        for (size_t i = 0; i < kItemCount; i++) {
            ItemRecord r;
            r.name = kItemNames[i];
            r.f0 = kItemFields[i][0];
            r.f1 = kItemFields[i][1];
            r.f2 = kItemFields[i][2];
            r.f3 = kItemFields[i][3];
            r.f4 = kItemFields[i][4];
            r.f5 = kItemFields[i][5];
            r.f6 = kItemFields[i][6];
            r.item_class = kItemClassEffect[i][0];
            r.effect = kItemClassEffect[i][1];
            v.push_back(std::move(r));
        }
        return v;
    }();
    return table;
}

const ItemRecord* item_by_index(int index) {
    if (index < 1 || (size_t)index > kItemCount) return nullptr;
    return &items()[index - 1];
}

const char* const kClassName[kClassCount] = {
    "Siegurd", "Eljot", "Ihl Kazar", "Mask Bearer", "Dragon", "Earth", "Fire",
    "Water", "Air", "Angel", "Woodkeeper", "Phantomfighter", "Warrior",
    "Werewolf",
};

bool item_usable_by_class(int index, int cls) {
    const ItemRecord* it = item_by_index(index);
    if (!it || cls < 0 || (size_t)cls >= kClassCount) return false;
    return (it->f6 & (1 << cls)) != 0;
}

void dump_items(std::FILE* out) {
    const auto& v = items();
    for (size_t i = 0; i < v.size(); i++) {
        const ItemRecord& r = v[i];
        std::fprintf(out,
                     "[%3zu] %-18s class=%u eff=%u f0..6=%d %d %d %d %d %d %d\n",
                     i, r.name.c_str(), r.item_class, r.effect, r.f0, r.f1, r.f2,
                     r.f3, r.f4, r.f5, r.f6);
    }
}

const std::vector<SpellRecord>& spells() {
    static const std::vector<SpellRecord> table = [] {
        std::vector<SpellRecord> v;
        v.reserve(kSpellCount);
        for (size_t i = 0; i < kSpellCount; i++) {
            SpellRecord r;
            r.name = kSpellNames[i];
            std::memcpy(r.data, kSpellData[i], sizeof(r.data));
            v.push_back(std::move(r));
        }
        return v;
    }();
    return table;
}

const SpellRecord* spell_by_index(int index) {
    if (index < 1 || (size_t)index > kSpellCount) return nullptr;
    return &spells()[index - 1];
}

void dump_spells(std::FILE* out) {
    const auto& v = spells();
    for (size_t i = 0; i < v.size(); i++) {
        std::fprintf(out, "[%3zu] %-22s", i, v[i].name.c_str());
        for (uint8_t b : v[i].data) std::fprintf(out, " %02x", b);
        std::fputc('\n', out);
    }
}

namespace {
std::vector<std::string> to_vec(const char* const* a, size_t n) {
    return std::vector<std::string>(a, a + n);
}
}  // namespace

std::vector<std::string> sex_names() { return to_vec(kSexNames, kSexCount); }
std::vector<std::string> race_names() { return to_vec(kRaceNames, kRaceCount); }
std::vector<std::string> profession_names() {
    return to_vec(kProfessionNames, kProfessionCount);
}
std::vector<std::string> alignment_names() {
    return to_vec(kAlignmentNames, kAlignmentCount);
}
std::vector<std::string> skill_names() {
    return to_vec(kSkillNames, kSkillCount);
}
std::vector<std::string> language_names() {
    return to_vec(kLanguageNames, kLanguageCount);
}
std::vector<std::string> status_short_names() {
    return to_vec(kStatusShortNames, kStatusCount);
}
std::vector<std::string> status_long_names() {
    return to_vec(kStatusLongNames, kStatusCount);
}
std::vector<std::string> gender_profession_names() {
    return to_vec(kGenderProfessionNames, kGenderProfessionCount);
}
std::vector<std::string> race_adjective_names() {
    return to_vec(kRaceAdjectiveNames, kRaceAdjectiveCount);
}
std::vector<std::string> alignment_lower_names() {
    return to_vec(kAlignmentLowerNames, kAlignmentLowerCount);
}
std::vector<std::string> profession_padded_names() {
    return to_vec(kProfessionPaddedNames, kProfessionDisplayCount);
}
std::vector<std::string> action_names() {
    return to_vec(kActionNames, kActionCount);
}
std::vector<std::string> morale_names() {
    return to_vec(kMoraleNames, kMoraleCount);
}
std::vector<std::string> combat_abbrev_names() {
    return to_vec(kCombatAbbrevNames, kCombatAbbrevCount);
}
std::vector<std::string> scenario_names() {
    return to_vec(kScenarioNames, kScenarioCount);
}

std::vector<std::string> monster_names(const std::string& dir, int scenario) {
    std::vector<std::string> out;
    if (scenario < 0 || scenario >= kMonsterScenarioCount) return out;
    char path[64];
    std::snprintf(path, sizeof(path), "%s/SYST/NSC0%d", dir.c_str(), scenario);
    for (const SnsRecord& r : load_nsc(path)) out.push_back(r.singular);
    return out;
}

}  // namespace lofport
