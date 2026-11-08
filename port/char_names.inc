// Legend of Faerghail character-generation name tables — transcribed
// from the DOS release (LOF.EXE image offsets 0x41e28/0x41e4a/0x41e98/0x41f28
// for sex/race/profession/alignment, 0x41d52 skills, 0x41dbe languages).
// Regenerate with `tools/spec.py --emit-names`.

static const char* const kSexNames[] = {
    "Female",
    "Male",
};

static const char* const kRaceNames[] = {
    "Human",
    "Half-Elf",
    "Elf",
    "Halfling",
    "Dwarf",
    "Half-Orc",
};

static const char* const kProfessionNames[] = {
    "Warrior",
    "Barbarian",
    "Rogue",
    "Smith",
    "Scout",
    "Priest",
    "Druid",
    "Magician",
    "Illusionist",
    "Paladin",
    "Healer",
    "Monk",
};

static const char* const kAlignmentNames[] = {
    "Lawful",
    "Chaotic",
};

static const char* const kSkillNames[] = {
    "Negotiating",
    "Attack",
    "Defence",
    "Concentration",
    "Pick-pocketing",
    "Stalking",
    "Trap detecting",
    "Trap disarming",
    "Lock picking",
};

static const char* const kLanguageNames[] = {
    "Common tongue",
    "Animal tongue",
    "Orc tongue",
    "Lizard tongue",
    "Dwarven tongue",
    "Elven tongue",
    "Dark tongue",
    "Magic tongue",
};

static const char* const kStatusShortNames[] = {
    "Good",
    "Mad",
    "Ill",
    "Poisn",
    "Blnd.",
    "Stun.",
    "Stone",
    "Dead",
};

static const char* const kStatusLongNames[] = {
    "Good",
    "Mad",
    "Ill",
    "Poisoned",
    "Blind",
    "Stunned",
    "Stoned",
    "Dead",
};

static const char* const kGenderProfessionNames[] = {
    "female Warrior",
    "female Barbarian",
    "female Rogue",
    "",
    "female Scout",
    "Priestess",
    "female Druid",
    "female Magician",
    "female Illusionist",
    "female Paladin",
    "Healer",
    "",
};

static const char* const kRaceAdjectiveNames[] = {
    "Human-",
    "Half-Elven-",
    "Elven-",
    "Halfling-",
    "Dwarven-",
    "Half-Orc-",
};

static const char* const kAlignmentLowerNames[] = {
    "lawful ",
    "chaotic ",
};

static const char* const kProfessionAbbrevNames[] = {
    "Wa",
    "Ba",
    "Ro",
    "Sm",
    "Sc",
    "Pr",
    "Dr",
    "Ma",
    "Il",
    "Pa",
    "He",
    "Mn",
    "??",
};

static const char* const kProfessionPaddedNames[] = {
    "Warrior",
    "Barbarian",
    "Rogue",
    "Smith",
    "Scout",
    "Priest",
    "Druid",
    "Magician",
    "Illusion.",
    "Paladin",
    "Healer",
    "Monk",
    "??",
};

static const char* const kActionNames[] = {
    "Defends",
    "Attacks",
    "Stalks",
    "Use object",
    "Cast Spell",
};

static const char* const kMoraleNames[] = {
    "Good",
    "Satisfactory",
    "Mediocre",
    "Alarming",
    "Bad",
};

static const char* const kCombatAbbrevNames[] = {
    "KIL",
    "ATK",
    "DEF",
    "RET",
};

static const char* const kScenarioNames[] = {
    " Valley of Faerghail",
    "Monastery of Sagacita",
    " Sagacita catakombs",
    "     The Mines",
    "    The Pyramid",
    "     The Temple",
    "     The Castle",
    "    The Mountain",
};

