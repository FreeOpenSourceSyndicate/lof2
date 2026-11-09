// Self-test for the character / item / scenario data readers (workstream B).
//
// The synthetic part always runs (no game data required) and checks the SPEC
// record layout and field decoding.  When the game data is present it also
// checks the real SYST/SPEC0 and ROST/ROST files against known values.
#include "char.h"

#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

using namespace lofport;

static void put16(std::vector<uint8_t>& v, size_t off, uint16_t val) {
    v[off] = (uint8_t)(val & 0xff);
    v[off + 1] = (uint8_t)(val >> 8);
}

static void putstr(std::vector<uint8_t>& v, size_t off, const char* s, size_t limit) {
    size_t n = std::strlen(s);
    assert(n < limit);
    std::memcpy(&v[off], s, n);
    v[off + n] = 0;
}

// Build a one-record SPEC image with recognisable field values and confirm the
// parser recovers every field.
static void synthetic_spec() {
    std::vector<uint8_t> img(1 + kSpecRecordSize, 0);
    img[0] = 1;
    uint8_t* r = &img[1];

    // keyword / text_a / text_b
    putstr(img, 1 + 0x00, "Swordfish", 0x0e);
    putstr(img, 1 + 0x0e, "Hello world", 0x4a - 0x0e);
    putstr(img, 1 + 0x50, "A big pile of bones.", 0xc6 - 0x50);

    r[0x4b] = 1;
    put16(img, 1 + 0x4c, 0x1234);
    put16(img, 1 + 0x4e, (uint16_t)(int16_t)-4);
    r[0xc9] = 7;
    put16(img, 1 + 0xda, 321);
    put16(img, 1 + 0xe2, 25);   // SPEC_TYPE_ELEMENTAL
    put16(img, 1 + 0xe4, 9);
    put16(img, 1 + 0xe6, 111);
    put16(img, 1 + 0xe8, 222);

    SpecTable t;
    assert(parse_spec(img, t));
    assert(t.ok);
    assert(t.declared_count == 1);
    assert(t.records.size() == 1);

    const SpecRecord& rec = t.records[0];
    assert(rec.keyword == "Swordfish");
    assert(rec.text_a == "Hello world");
    assert(rec.text_b == "A big pile of bones.");
    assert(rec.flag_4b == 1);
    assert(rec.arg_4c == 0x1234);
    assert(rec.arg_4e == -4);
    assert(rec.index_c9 == 7);
    assert(rec.has_index());
    assert(rec.arg_da == 321);
    assert(rec.type == SPEC_TYPE_ELEMENTAL);
    assert(rec.counter == 9);
    assert(rec.arg_e6 == 111);
    assert(rec.arg_e8 == 222);

    // A record with the 0xff "no index" sentinel.
    img[1 + 0xc9] = 0xff;
    assert(parse_spec(img, t));
    assert(t.records[0].index_c9 == 0xff);
    assert(!t.records[0].has_index());

    // Truncated buffers must be rejected, not read out of bounds.
    SpecTable bad;
    std::vector<uint8_t> trunc(1 + kSpecRecordSize - 1, 0);
    trunc[0] = 1;
    assert(!parse_spec(trunc, bad));
    assert(!bad.ok && bad.records.empty());

    std::vector<uint8_t> empty;
    assert(!parse_spec(empty, bad));   // no count byte at all

    // A count byte of 0 is valid and yields no records.
    std::vector<uint8_t> none(1, 0);
    assert(parse_spec(none, bad));
    assert(bad.ok && bad.records.empty());

    // Dispatch-type names (oracle-derived): the handler set the disassembly
    // keys off must all have stable labels, and unknown values must not crash.
    assert(std::string(spec_type_name(0)) == "text");
    assert(std::string(spec_type_name(1)) == "recruit");
    assert(std::string(spec_type_name(7)) == "door");
    assert(std::string(spec_type_name(9)) == "sign");
    assert(std::string(spec_type_name(0x0c)) == "button");
    assert(std::string(spec_type_name(0x0e)) == "challenge");
    assert(std::string(spec_type_name(0x11)) == "puzzle");
    assert(std::string(spec_type_name(0x19)) == "elemental");
    assert(std::string(spec_type_name(9999)) == "unknown");

    // SNSC -> character seed mapping (analysis/lof.asm 1DA8E..1DCBB): a synthetic
    // class record with distinct bytes at the mapped offsets must decode to the
    // documented character-struct fields, including the 100-n fold on 0x1d/0x1e.
    SnsRecord sns;
    sns.singular = "Tester";
    sns.plural = "Testers";
    sns.stats[kSnsIndex2a - 38] = 1;
    sns.stats[kSnsBody2d - 38] = 10;
    sns.stats[kSnsBody2e - 38] = 65;
    sns.stats[kSnsSpeed36 - 38] = 4;
    sns.stats[kSnsClass4c - 38] = 2;
    // Four (value, level) skill pairs, distinct so the stride-2 mapping is checked.
    for (int n = 0; n < 4; ++n) {
        sns.stats[kSnsSkill3b + 2 * n - 38] = (uint8_t)(10 + n);
        sns.stats[kSnsSkill3c + 2 * n - 38] = (uint8_t)(1 + n);
    }
    CharSeed s = sns_char_seed(sns);
    assert(s.c18 == 2);
    assert(s.c1c == 65);
    assert(s.c1d == 90 && s.c1e == 90);   // 100 - 10
    assert(s.c56 == 4);
    assert(s.c7a_index == 1);
    for (int n = 0; n < 4; ++n) {
        assert(s.c142[n] == (uint8_t)(10 + n));
        assert(s.c143[n] == (uint8_t)(1 + n));
    }

    // decode_char(): the 410-byte struct projection.  Build one with distinct
    // values at every mapped offset and confirm each lands in the right field.
    // The offsets are grounded in analysis/lof.asm: the sheet builder (0x2676E)
    // prints +0x45/+0x44/+0x46/+0x47/+0x48 as Str/Con/Dex/Int/Wis, the level-up
    // routine (0x9144) increments +0x17 ("enters the %ld rank"), the init
    // routine (1BA40) writes +0x1d from the Dexterity at +0x46, and the SNSC
    // seed writes +0x18/+0x1c/+0x1e/+0x56.
    std::vector<uint8_t> chr(410, 0);
    chr[kCharPresent] = 1;
    putstr(chr, kCharName, "Tester", 14);
    put16(chr, kCharFlags10, (uint16_t)(int16_t)-1);
    put16(chr, kCharCounter12, 7);
    chr[kCharLevel17] = 3;
    chr[kCharClass18] = 1;
    chr[kCharBase1c] = 65;
    chr[kCharStat1d] = 15;
    chr[kCharStat1e] = 90;
    put16(chr, kCharHpMax20, 40);
    put16(chr, kCharHpCur22, 10);
    // 32-bit experience: low word +0x6c, high word +0x6e.
    put16(chr, kCharExp6c, 0x1234);
    put16(chr, kCharExp6c + 2, 0x0001);
    chr[kCharCon44] = 14;
    chr[kCharStr45] = 19;
    chr[kCharDex46] = 16;
    chr[kCharInt47] = 12;
    chr[kCharWis48] = 15;
    chr[kCharSpeed56] = 2;
    CharSheet ch = decode_char(chr.data());
    assert(ch.present == 1 && ch.name == "Tester");
    assert(ch.flags10 == -1 && ch.counter12 == 7);
    assert(ch.level17 == 3);
    assert(ch.class18 == 1 && ch.base1c == 65 && ch.stat1d == 15 &&
           ch.stat1e == 90);
    assert(ch.hp_max == 40 && ch.hp_cur == 10 && ch.hp_percent() == 25);
    assert(ch.exp == 0x00011234u);   // low +0x6c, high +0x6e
    assert(ch.con44 == 14 && ch.str45 == 19 && ch.dex46 == 16 &&
           ch.int47 == 12 && ch.wis48 == 15);
    assert(ch.speed56 == 2);
    // Party-status fields (builder at image offset 0x2D436): body state +0x24,
    // the attribute modifiers +0x5b/+0x5c/+0x5e/+0x5f, the hp-mod gate +0x6a
    // and the status flag +0x0f.
    chr[kCharBody24] = 2;
    chr[kCharFlg5b] = 3;
    chr[kCharFlg5c] = 4;
    chr[kCharFlg5e] = 5;
    chr[kCharFlg5f] = 6;
    chr[kCharFlag6a] = 1;
    chr[kCharFlg0f] = 0;
    ch = decode_char(chr.data());
    assert(ch.body24 == 2);
    assert(ch.mod5b == 3 && ch.mod5c == 4 && ch.mod5e == 5 && ch.mod5f == 6);
    assert(ch.flag6a == 1 && ch.flag0f == 0);
    // Display row: hp_pct = 100*10/40 = 25; hp_mod = (10>>3) + 50 = 51;
    // attr_a = 100 - (3 + 90) = 7, then -10 (flag0f == 0) = 253 (byte wrap);
    // attr_b = 4 + 65 = 69; attr_c = 6 + 3 = 9.
    assert(ch.hp_pct() == 25 && ch.hp_mod() == 51);
    assert(ch.attr_a() == (uint8_t)(7 - 10) && ch.attr_b() == 69 &&
           ch.attr_c() == 9);
    // With flag0f non-zero the -10 is skipped: attr_a == 7.
    chr[kCharFlg0f] = 1;
    ch = decode_char(chr.data());
    assert(ch.attr_a() == 7);
    // +0x1d is derived from Dexterity: 5*16 - 65 == 15, matching stat1d above.
    assert(ch.stat1d == char_stat1d_from_dex(ch.dex46));

    printf("char_test: synthetic SPEC ok\n");
}

// Verify the real data, when present.  Verifies record counts and a known
// string from SPEC0 and a known roster name.
static int real_data() {
    const char* dir = "game";
    std::string p0 = std::string(dir) + "/SYST/SPEC0";
    FILE* f = fopen(p0.c_str(), "rb");
    if (!f) {
        printf("char_test: game data absent, skipping real-data checks\n");
        return 0;
    }
    fclose(f);

    SpecTable t = load_spec(p0);
    assert(t.ok);
    assert(t.declared_count == 24);
    assert(t.records.size() == 24);
    // SPEC0[0] is the Woodkeeper recruit (verified against the game file and
    // the oracle's own event dispatcher, which prints text_b at +0x50).
    assert(t.records[0].text_a == "He shakes his head.");
    assert(t.records[0].text_b.rfind("A Woodkeeper plants himself", 0) == 0);
    assert(t.records[0].type == SPEC_TYPE_RECRUIT);
    // +0xeb is a rarely-set SNSC class index (0xff = none); SPEC0[8] sets it to
    // 2 (Ihl Kazar) and the engine reads SNSC stats[0x0a] for that class.
    assert(t.records[8].class_eb == 2 && t.records[8].has_class());
    assert(t.records[0].class_eb == 0xff && !t.records[0].has_class());
    // +0x00 is a keyword/password (region 0x00..0x0e); the four elemental
    // guardians answer "Fire"/"Daughter"/"Echo"/"Eyes", the recruit slots "Y".
    assert(t.records[1].keyword == "Y" && t.records[8].keyword == "Y");
    assert(t.records[15].keyword == "Fire");
    assert(t.records[16].keyword == "Daughter");
    assert(t.records[17].keyword == "Echo");
    assert(t.records[18].keyword == "Eyes");
    assert(t.records[0].keyword.empty());

    // SPEC0[15..18] are the four elemental guardians (type 25); the engine's
    // 0x19 handler reads index_c9 (+0xc9) to pick which one.  The observed
    // indices 6,7,8,9 correspond to Water, Earth, Air, Fire in text order.
    assert(t.records[15].type == SPEC_TYPE_ELEMENTAL);
    assert(t.records[15].index_c9 == 6);
    assert(t.records[15].text_b.rfind("A Water Elemental appears", 0) == 0);
    assert(t.records[16].index_c9 == 7);
    assert(t.records[17].index_c9 == 8);
    assert(t.records[18].type == SPEC_TYPE_ELEMENTAL);
    assert(t.records[18].index_c9 == 9);
    assert(t.records[18].text_b.rfind("A Fire Elemental appears", 0) == 0);

    // Every record's type lies in the observed set 0..25 (no gaps above 22
    // except 23; 23 never appears in this release).
    for (const auto& r : t.records)
        assert(r.type <= 25 && r.type != (SpecType)23);

    // 665 records across the eight scenario files (stable for this release).
    size_t total = 0, unset_4e = 0;
    for (int n = 0; n <= 7; n++) {
        char p[64];
        snprintf(p, sizeof p, "%s/SYST/SPEC%d", dir, n);
        SpecTable tt = load_spec(p);
        assert(tt.ok);
        total += tt.records.size();
        for (const auto& r : tt.records)
            if (r.arg_4e < 0) unset_4e++;  // -1 == "unset" (engine handler
                                           // gates the text window on it)
    }
    assert(total == 665);
    // arg_4e (offset 0x4e, read signed by the event dispatcher) is -1 in all
    // but six records in this release.
    assert(unset_4e == 659);

    // SPEC6 has 182 records; SPEC7 has 41 (the counts are stable per file).
    SpecTable t6 = load_spec(std::string(dir) + "/SYST/SPEC6");
    assert(t6.ok && t6.records.size() == 182);
    SpecTable t7 = load_spec(std::string(dir) + "/SYST/SPEC7");
    assert(t7.ok && t7.records.size() == 41);

    auto roster = load_rost(std::string(dir) + "/ROST/ROST");
    assert(roster.size() == 11);
    assert(roster[0].name == "Connar");
    assert(roster[10].name == "Siegurd");
    assert(roster[0].present == 1);
    // The 4-byte trailing value the loader reads separately (410..413).
    assert(roster[0].tail == 0x68a);
    assert(roster[10].tail == 0x592);
    for (size_t i = 0; i < roster.size(); i++)
        assert(roster[i].present == 1);  // all 11 slots hold a character

    ShopData shop = load_shop(std::string(dir) + "/ROST/SHOP");
    assert(shop.ok);
    // One signed adjustment per item, parallel to the item table order.
    assert(shop.adjustments[0] == -10);   // Club
    assert(shop.adjustments[1] == -2);    // War scythe
    assert(shop.adjustments[12] == -1);   // Short sword
    assert(shop.adjustments[32] == -8);   // Full plate
    assert(shop.trailer == 0x1606);

    // Saved games ("gamestandings"): header 0x00 'F' 'O' 'L' + 410-byte
    // character records.  The engine opens GAMES\GAME%d (string in LOF.EXE).
    SaveGame sv = load_save(std::string(dir) + "/GAMES/GAME1");
    assert(sv.ok);
    assert(sv.header[0] == 0x00 && sv.header[1] == 'F' &&
           sv.header[2] == 'O' && sv.header[3] == 'L');
    assert(sv.chars.size() == 32);
    assert(sv.chars[0].present == 1 && sv.chars[0].name == "Decker");
    assert(sv.chars[1].name == "Siegurd");
    assert(sv.chars[5].name == "Assanla");
    assert(sv.trailer.size() == 10);

    // Character-struct offsets the engine's init/erase routines use directly
    // (analysis/lof.asm 1B9C2..1BA7E).  Live slots carry word10 == -1.
    assert((int16_t)(sv.chars[0].raw[kCharFlags10] |
                     (sv.chars[0].raw[kCharFlags10 + 1] << 8)) == -1);
    assert(sv.chars[0].raw[kCharStat1d] == 0x0a);
    // Decker: level/rank 1, Dexterity 14 (the stat +0x1d derives from).
    assert(sv.chars[0].raw[kCharLevel17] == 0x01);
    assert(sv.chars[0].raw[kCharDex46] == 0x0e);

    // Item table (transcribed from LOF.EXE; no data file carries it).
    const auto& it = items();
    assert(it.size() == kItemCount);
    assert(kItemCount == 185);
    assert(it[0].name == "Club" && it[0].item_class == 1);
    assert(it[12].name == "Short sword" && it[12].f0 == 1 &&
           it[12].f1 == 10 && it[12].f2 == 50);
    assert(it[0x1a].name == "Leather armour" && it[0x1a].item_class == 2);
    assert(it[184].name == "The Staff");
    // The trailing byte is a use-effect index (0 for plain gear), not a slot.
    assert(it[0x21].name == "Small shield" && it[0x21].item_class == 3 &&
           it[0x21].effect == 0);
    assert(it[0x2e].name == "Healing potion" && it[0x2e].item_class == 9 &&
           it[0x2e].effect == 10);
    assert(it[0x48].name == "Lightbringer" && it[0x48].item_class == 1 &&
           it[0x48].effect == 33);
    // 1-based lookup: index 13 is "Short sword"; out-of-range is null.
    assert(item_by_index(13) && item_by_index(13)->name == "Short sword");
    assert(item_by_index(185) && item_by_index(185)->name == "The Staff");
    assert(item_by_index(0) == nullptr && item_by_index(186) == nullptr);
    // Every class-eligibility mask fits the 14 SNSC classes.
    for (const auto& r : it) assert(r.f6 >= 0 && r.f6 <= 0x3fff);
    // Class table is in SNSC order and the mask lookup uses it.
    assert(kClassCount == 14 && std::string(kClassName[0]) == "Siegurd" &&
           std::string(kClassName[13]) == "Werewolf");
    // Full plate (idx 33) is 0x0201: only class 0 (Siegurd) and class 9 (Angel).
    assert(item_usable_by_class(33, 0) && item_usable_by_class(33, 9));
    assert(!item_usable_by_class(33, 4));  // not the Dragon
    assert(!item_usable_by_class(0, 0) && !item_usable_by_class(186, 0));
    // Use-effect names + reverse lookup by effect value.
    assert(std::string(item_effect_name(0)) == "none");
    assert(std::string(item_effect_name(10)) == "healing");
    assert(std::string(item_effect_name(255)) == "none");
    auto heals = items_with_effect(10);
    assert(heals.size() == 1 && it[heals[0]].name == "Healing potion");
    assert(items_with_effect(10)[0] == 0x2e);
    assert(items_with_effect(200).empty());

    // Spell table (141 x 36 bytes, also transcribed from LOF.EXE).
    const auto& sp = spells();
    assert(sp.size() == kSpellCount && kSpellCount == 141);
    assert(sp[0].name == "Burning hands");
    assert(sp[0].data[0] == 0x20 && sp[0].data[10] == 0x03 &&
           sp[0].data[13] == 0x00);
    assert(sp[130].name == "Drachenatem I");
    assert(spell_by_index(1) && spell_by_index(1)->name == "Burning hands");
    assert(spell_by_index(141) && spell_by_index(141)->name == "Drachenatem II");
    assert(spell_by_index(0) == nullptr && spell_by_index(142) == nullptr);
    // Spell effect code = data byte 8 (0x0f = healing family).
    assert(kSpellEffectByte == 8);
    assert(std::string(spell_effect_name(0x0f)) == "healing");
    assert(std::string(spell_effect_name(0x37)) == "map");
    assert(std::string(spell_effect_name(0xff)) == "special");
    auto heal = spells_with_effect(0x0f);
    assert(heal.size() == 7 && sp[heal[0]].name == "Healing I" &&
           sp[heal[6]].name == "Healing III");
    assert(spells_with_effect(0x01).empty());

    // Character-generation name tables.
    auto sexes = sex_names();
    auto races = race_names();
    auto profs = profession_names();
    auto aligns = alignment_names();
    auto skills = skill_names();
    auto langs = language_names();
    assert(sexes.size() == kSexCount && sexes[0] == "Female" && sexes[1] == "Male");
    assert(races.size() == kRaceCount && races[0] == "Human" &&
           races[5] == "Half-Orc");
    assert(profs.size() == kProfessionCount && profs[0] == "Warrior" &&
           profs[11] == "Monk");
    assert(aligns.size() == kAlignmentCount && aligns[0] == "Lawful" &&
           aligns[1] == "Chaotic");
    assert(skills.size() == kSkillCount && skills[0] == "Negotiating" &&
           skills[8] == "Lock picking");
    assert(langs.size() == kLanguageCount && langs[0] == "Common tongue" &&
           langs[7] == "Magic tongue");
    auto stat_short = status_short_names();
    auto stat_long = status_long_names();
    assert(stat_short.size() == kStatusCount && stat_short[0] == "Good" &&
           stat_short[3] == "Poisn" && stat_short[7] == "Dead");
    assert(stat_long.size() == kStatusCount && stat_long[3] == "Poisoned" &&
           stat_long[6] == "Stoned");
    auto gp = gender_profession_names();
    auto ra = race_adjective_names();
    auto al = alignment_lower_names();
    auto pp = profession_padded_names();
    auto ac = action_names();
    auto mo = morale_names();
    assert(gp.size() == kGenderProfessionCount && gp[0] == "female Warrior" &&
           gp[5] == "Priestess");
    assert(ra.size() == kRaceAdjectiveCount && ra[0] == "Human-" &&
           ra[5] == "Half-Orc-");
    assert(al.size() == kAlignmentLowerCount && al[0] == "lawful " &&
           al[1] == "chaotic ");
    assert(pp.size() == kProfessionDisplayCount && pp[0] == "Warrior" &&
           pp[12] == "??");
    assert(ac.size() == kActionCount && ac[0] == "Defends" &&
           ac[4] == "Cast Spell");
    assert(mo.size() == kMoraleCount && mo[0] == "Good" && mo[4] == "Bad");
    auto ca = combat_abbrev_names();
    auto sc = scenario_names();
    assert(ca.size() == kCombatAbbrevCount && ca[0] == "KIL" && ca[3] == "RET");
    assert(sc.size() == kScenarioCount && sc[0] == " Valley of Faerghail" &&
           sc[7] == "    The Mountain");
    auto fa = facing_names();
    auto td = time_of_day_names();
    assert(fa.size() == kFacingCount && fa[0] == "    Facing North" &&
           fa[3] == "    Facing West");
    assert(td.size() == kTimeOfDayCount && td[0] == "      Midnight" &&
           td[7] == "       Evening");

    // Monster rosters (SYST/NSC0n, same 78-byte layout as SNSC).  NSC00 holds
    // 25 scenario-0 monsters; the first is the Ape.
    auto m0 = monster_names(dir, 0);
    assert(m0.size() == 25 && m0[0] == "Ape");
    assert(monster_names(dir, kMonsterScenarioCount).empty());
    assert(monster_names(dir, -1).empty());

    // SNSC statistics block -> character-struct seed (analysis/lof.asm
    // 1DA8E..1DCBB).  Verified byte-for-byte against the shipped save: SNSC
    // class 0 is "Siegurd" and GAME1's Siegurd was built from it.
    auto snsc = load_snsc(std::string(dir) + "/SYST/SNSC");
    assert(snsc.size() == kClassCount);
    assert(snsc[0].singular == "Siegurd" && snsc[0].plural == "Siegurd");
    CharSeed seed = sns_char_seed(snsc[0]);
    // The record's own bytes the mapping reads (record offsets 0x2a..0x4c).
    const uint8_t* st = snsc[0].stats;
    assert(seed.c18 == st[kSnsClass4c - 38]);   // char +0x18
    assert(seed.c1c == st[kSnsBody2e - 38]);    // char +0x1c
    assert(seed.c1d == (uint8_t)(100 - st[kSnsBody2d - 38]));  // char +0x1d
    assert(seed.c1e == seed.c1d);               // char +0x1e mirrors +0x1d
    assert(seed.c56 == st[kSnsSpeed36 - 38]);   // char +0x56
    assert(seed.c7a_index == st[kSnsIndex2a - 38]);
    // And they equal the live character's fields (Siegurd == SNSC class 0).
    // Note: char +0x1d is written from the same 100-n value here, but the
    // Dexterity rule at 1BA40 then overrides it (Siegurd's Dexterity is 0, so
    // it keeps 0x0a; Decker's Dexterity is 0x0e, still below 16 -> 0x0a).
    const uint8_t* ch = sv.chars[1].raw.data();  // GAME1 slot 1 = Siegurd
    assert(ch[kCharClass18] == seed.c18);
    assert(ch[kCharBase1c] == seed.c1c);
    assert(ch[kCharStat1e] == seed.c1e);
    assert(ch[kCharSpeed56] == seed.c56);
    assert(ch[kCharStat1d] == 0x0a);   // overridden by the 1BA40 Dexterity rule
    assert(ch[kCharSeed7a + seed.c7a_index] == 2);
    assert(seed.c18 == 1 && seed.c1c == 65 && seed.c1d == 90 && seed.c56 == 4);
    // Monsters carry class 2 (e.g. the Dragon) and a distinct body value.
    CharSeed dseed = sns_char_seed(snsc[4]);
    assert(dseed.c18 == 2 && dseed.c1c == 45 && dseed.c56 == 4);

    // Skill pairs: the classes that carry skill data (2 = Ihl Kazar, 3 = Mask
    // Bearer) seed char +0x142/+0x143 with stride 2 straight from rec[0x3b..].
    CharSeed kseed = sns_char_seed(snsc[2]);
    const uint8_t* kst = snsc[2].stats;
    for (int n = 0; n < 4; ++n) {
        assert(kseed.c142[n] == kst[kSnsSkill3b + 2 * n - 38]);
        assert(kseed.c143[n] == kst[kSnsSkill3c + 2 * n - 38]);
    }
    assert(kseed.c142[0] == 28 && kseed.c143[0] == 2);   // Ihl Kazar

    // decode_char() projects the fields the engine reads directly.  The HP pair
    // (+0x20 max / +0x22 current) is proven by the sheet builder at 28054/28067
    // (100 * cur / max as a percentage); every live character satisfies
    // cur <= max.  Siegurd's max HP (96) is exactly the value 1BA40 writes.
    CharSheet cs = decode_char(sv.chars[1].raw.data());
    assert(cs.present == 1);
    assert(cs.name == "Siegurd");
    assert(cs.class18 == 1 && cs.base1c == 65 && cs.stat1e == 90);
    assert(cs.hp_max == 96 && cs.hp_cur == 96);
    assert(cs.hp_percent() == 100);
    assert(cs.level17 == 0 && cs.speed56 == 4);   // Siegurd: rank 0
    // +0x1d is the Dexterity-derived stat (1BA40): <16 -> 10, else 5*dex-65.
    assert(cs.stat1d == char_stat1d_from_dex(cs.dex46));
    assert(char_stat1d_from_dex(0) == 10 && char_stat1d_from_dex(15) == 10);
    assert(char_stat1d_from_dex(16) == 15 && char_stat1d_from_dex(19) == 30);
    // The five primary attributes sit at +0x44..+0x48 (Con, Str, Dex, Int, Wis),
    // verified against the sheet builder (0x2676E) and the training routine
    // (0x95B0): Siegurd is the un-rolled template (all zero); Decker is a real
    // character (Con 14, Str 19, Dex 14, Int 12, Wis 15).
    assert(cs.con44 == 0 && cs.str45 == 0 && cs.dex46 == 0 &&
           cs.int47 == 0 && cs.wis48 == 0);
    CharSheet dkr = decode_char(sv.chars[0].raw.data());
    assert(dkr.con44 == 14 && dkr.str45 == 19 && dkr.dex46 == 14 &&
           dkr.int47 == 12 && dkr.wis48 == 15);
    // The 32-bit experience accumulator (+0x6c low / +0x6e high).  GAME1 holds
    // non-zero, distinct values per character (Decker 379, Siegurd 699, Handy
    // 231); they are small so the high word stays 0, and the accumulator must
    // not borrow from a neighbour.  Siegurd is the un-rolled template, but its
    // experience is still set.
    assert(dkr.exp == 379);
    assert(cs.exp == 699);
    assert(decode_char(sv.chars[2].raw.data()).exp == 231);
    // Every in-use character: current HP never exceeds the maximum, the HP
    // percentage matches the integer-division formula, and +0x1d is consistent
    // with the Dexterity rule.  The save carries spare/garbage slots too (the
    // engine sets +0x00 == 1 for a used slot), so only check those.
    for (const auto& sc : sv.chars) {
        if (sc.present != 1) continue;
        CharSheet c = decode_char(sc.raw.data());
        assert(!c.name.empty());
        assert(c.hp_cur <= c.hp_max);
        assert(c.hp_percent() == (c.hp_max ? 100 * c.hp_cur / c.hp_max : 0));
        assert(c.stat1d == char_stat1d_from_dex(c.dex46));
    }
    // A damaged character still projects consistently (Handy: 9 of 11 HP).
    CharSheet dk = decode_char(sv.chars[2].raw.data());
    assert(dk.name == "Handy" && dk.hp_cur == 9 && dk.hp_max == 11);
    assert(dk.hp_percent() == 81);   // 100*9/11, integer division

    // Party-status fields on the real save: GAME1's fresh characters carry
    // zeroed attribute modifiers (+0x5b/+0x5c/+0x5e/+0x5f); the body state
    // (+0x24) and flag (+0x6a) hold the values the character generator wrote
    // (Handy body 1 flag 1, Merlin body 2 flag 2).
    assert(dk.body24 == 1 && dk.mod5b == 0 && dk.mod5c == 0 &&
           dk.mod5e == 0 && dk.mod5f == 0 && dk.flag6a == 1);
    CharSheet merlin = decode_char(sv.chars[3].raw.data());
    assert(merlin.name == "Merlin" && merlin.body24 == 2 && merlin.flag6a == 2);

    // Party-status display row for Handy (hp 9/11, +0x1c=100, +0x1e=50,
    // +0x17=1, all modifiers 0, +0x6a=1, +0x0f=1): hp_pct 81, hp_mod 51,
    // attr_a 50, attr_b 100, attr_c 1.
    assert(dk.flag0f == 1);
    assert(dk.hp_pct() == 81 && dk.hp_mod() == 51);
    assert(dk.attr_a() == 50 && dk.attr_b() == 100 && dk.attr_c() == 1);

    // The whole-party projection: only the six live slots, in order, each row
    // carrying the same bytes the builder would pack.
    std::vector<PartyRow> party = party_status(sv);
    assert(party.size() == 6);
    for (size_t i = 0; i < party.size(); i++) assert(party[i].index == i);
    assert(party[0].hp_pct == 100 && party[0].body24 == 0);
    assert(party[2].hp_pct == 81 && party[2].body24 == 1 &&
           party[2].hp_mod == 51 && party[2].hp_cur == 9);

    printf("char_test: real SPEC/ROST/SHOP/GAMES/items/spells/names ok\n");
    return 0;
}

static void print_char(FILE* out, const CharSheet& c, size_t i) {
    fprintf(out, "[%2zu] %-12s present=%d class=%d base=%d stat1d=%d "
                 "stat1e=%d hp=%d/%d (%d%%) rnk=%d con=%d str=%d dex=%d "
                 "int=%d wis=%d spd=%d exp=%u body=%d mods=%d/%d/%d/%d "
                 "mood=%d flag0f=%d row=[%d %d %d %d %d]\n",
            i, c.name.empty() ? "<none>" : c.name.c_str(), c.present, c.class18,
            c.base1c, c.stat1d, c.stat1e, c.hp_cur, c.hp_max, c.hp_percent(),
            c.level17, c.con44, c.str45, c.dex46, c.int47, c.wis48, c.speed56,
            c.exp, c.body24, c.mod5b, c.mod5c, c.mod5e, c.mod5f, c.flag6a,
            c.flag0f, c.hp_pct(), c.body24, c.hp_mod(), c.attr_a(), c.attr_b());
}

int main(int argc, char** argv) {
    synthetic_spec();

    // --dump-spec prints the decoded tables (for eyeballing / the front-end).
    if (argc > 1 && std::string(argv[1]) == "--dump-spec") {
        for (int n = 0; n <= 7; n++) {
            char p[64];
            snprintf(p, sizeof p, "game/SYST/SPEC%d", n);
            SpecTable t = load_spec(p);
            if (t.ok) {
                char label[64];
                snprintf(label, sizeof label, "SYST/SPEC%d", n);
                dump_spec(stdout, t, label);
            }
        }
        return 0;
    }

    if (argc > 1 && std::string(argv[1]) == "--dump-items") {
        dump_items(stdout);
        return 0;
    }

    // --dump-char [FILE] prints decoded character sheets from a save (default
    // game/GAMES/GAME1) or a ROST file.
    if (argc > 1 && std::string(argv[1]) == "--dump-char") {
        const char* path = argc > 2 ? argv[2] : "game/GAMES/GAME1";
        std::string p(path);
        if (p.find("ROST") != std::string::npos) {
            auto ro = load_rost(p);
            for (size_t i = 0; i < ro.size(); i++)
                print_char(stdout, decode_char(ro[i].raw.data()), i);
        } else {
            SaveGame sv = load_save(p);
            for (size_t i = 0; i < sv.chars.size(); i++)
                print_char(stdout, decode_char(sv.chars[i].raw.data()), i);
        }
        return 0;
    }

    int rc = real_data();
    if (rc == 0) printf("char_test: ok\n");
    return rc;
}
