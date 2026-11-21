// Combat model tests: the generator, the melee formula and one round.
//
// The formulas are transcribed from LOF.EXE (see port/combat.h and
// docs/notes-b.md).  Every expected value below is computed by hand from the
// disassembly, not from the implementation.
#include <cassert>

#include "combat.h"

#include <cstdio>
#include <string>

using namespace lofport;

static int failures = 0;

static void check(bool ok, const std::string& what) {
    if (!ok) {
        fprintf(stderr, "FAIL: %s\n", what.c_str());
        failures++;
    }
}

// The engine's generator is the Microsoft C run-time LCG: seed 1 gives the
// well-known 41, 18467, 6334, 26500, 19169 sequence (image 0x30ECC).
static void test_rng() {
    CombatRng rng;
    rng.seed = 1;
    const int want[] = {41, 18467, 6334, 26500, 19169, 15724, 11478, 29358};
    for (int w : want) {
        int got = rng.rand15();
        check(got == w, "rand15 sequence: got " + std::to_string(got) +
                            " want " + std::to_string(w));
    }
    // rand(n) is rand() % n, and rand(0) is 0 (the guard at 0x2D74C).
    CombatRng a, b;
    a.seed = b.seed = 7;
    check(a.below(0) == 0, "below(0) == 0");
    check(a.below(100) == b.rand15() % 100, "below(100) == rand() % 100");
    check(a.below(10) >= 0 && a.below(10) < 10, "below(10) in range");
}

// One attack, bare hands, no bonuses: damage is 1 per hit, and the hit test is
// `rand(100)+1 < hit_score`.
static void test_single_attack() {
    CombatRng rng;
    rng.seed = 1;  // rolls: 41, 18467, 6334, ...
    MeleeInputs in;
    in.hit_score = 100;   // always hits
    in.speed56 = 1;       // (0 + 1) / 1 = one attack
    in.divisor55 = 1;
    in.weapon_damage = 0; // rand(0) + 1 = 1
    in.level17 = 0;
    MeleeResult r = melee_round(rng, in);
    check(r.attacks == 1, "one attack");
    check(r.hits == 1, "one hit");
    check(r.damage == 1, "bare-hand damage is 1");

    // hit_score 0 -> the hit roll (>= 1) never beats it, so no damage.
    CombatRng rng2;
    rng2.seed = 1;
    MeleeInputs in2 = in;
    in2.hit_score = 0;
    MeleeResult r2 = melee_round(rng2, in2);
    check(r2.hits == 0 && r2.damage == 0, "hit_score 0 never hits");
}

// Strength bonus: (str45/55 + mod5e) - 15 added when above 15.
static void test_strength_bonus() {
    CombatRng rng;
    rng.seed = 1;
    MeleeInputs in;
    in.hit_score = 100;
    in.speed56 = 1;
    in.weapon_damage = 0;
    in.str45 = 19;   // 19 - 15 = +4
    MeleeResult r = melee_round(rng, in);
    check(r.damage == 5, "str 19 adds 4 to a bare-hand hit");

    CombatRng rng2;
    rng2.seed = 1;
    in.str45 = 15;   // not above 15: no bonus
    MeleeResult r2 = melee_round(rng2, in);
    check(r2.damage == 1, "str 15 adds nothing");
}

// The skill roll adds (mod5f + level) * (damage/10); with damage < 10 that is
// zero, so use a weapon die large enough to matter.
static void test_skill_bonus() {
    CombatRng rng;
    rng.seed = 1;  // below(50) = 41 % 50 = 41 -> damage 42; below(100) = 18467%100=67
    MeleeInputs in;
    in.hit_score = 100;
    in.speed56 = 1;
    in.weapon_damage = 50;
    in.skill27 = 100;  // 100 >= 67 -> the bonus applies
    in.level17 = 2;
    in.mod5f = 3;
    MeleeResult r = melee_round(rng, in);
    // damage 42 + (3 + 2) * (42/10) = 42 + 20 = 62
    check(r.damage == 62, "skill bonus: got " + std::to_string(r.damage) + " want 62");

    // skill 0 -> 0 >= roll only for roll 0, which rand(100)+1 never yields.
    CombatRng rng2;
    rng2.seed = 1;
    in.skill27 = 0;
    MeleeResult r2 = melee_round(rng2, in);
    check(r2.damage == 42, "no skill bonus without skill");
}

// Attack count is (attacks5d + speed56) / divisor55, capped at ten.
static void test_attack_count() {
    CombatRng rng;
    rng.seed = 1;
    MeleeInputs in;
    in.hit_score = 100;
    in.speed56 = 4;
    in.weapon_damage = 0;
    MeleeResult r = melee_round(rng, in);
    check(r.attacks == 4, "speed 4 -> four attacks");
    check(r.damage == 4, "four bare-hand hits -> 4");

    in.speed56 = 40;  // capped at ten
    MeleeResult r2 = melee_round(rng, in);
    check(r2.attacks == 10, "attack count capped at ten");
}

// The +0x4e percentage: below 100 multiplies the total by 100/pct; the total is
// capped at 99.
static void test_pct_and_cap() {
    CombatRng rng;
    rng.seed = 1;
    MeleeInputs in;
    in.hit_score = 100;
    in.speed56 = 1;
    in.weapon_damage = 0;
    in.str45 = 19;   // 5 damage
    in.pct4e = 50;   // x2
    MeleeResult r = melee_round(rng, in);
    check(r.damage == 10, "pct 50 doubles the damage");

    CombatRng rng2;
    rng2.seed = 1;
    in.pct4e = 100;  // no scaling
    MeleeResult r2 = melee_round(rng2, in);
    check(r2.damage == 5, "pct 100 leaves the damage alone");

    // Cap: ten attacks of 42 on a 100% pct still clamp at 99.
    CombatRng rng3;
    rng3.seed = 1;
    MeleeInputs in3;
    in3.hit_score = 100;
    in3.speed56 = 10;
    in3.weapon_damage = 50;
    MeleeResult r3 = melee_round(rng3, in3);
    check(r3.damage == 99, "damage capped at 99");
}

// One round: the party hits the defenders, the defenders hit the party, and the
// table rows carry the HP deltas.
static void test_round() {
    CombatRng rng;
    rng.seed = 1;

    std::vector<Combatant> party(2);
    party[0].name = "Decker";
    party[0].hp = 10;
    party[0].max_hp = 10;
    party[0].target = 0;
    party[0].melee.hit_score = 100;
    party[0].melee.speed56 = 1;
    party[1].name = "Siegurd";
    party[1].hp = 96;
    party[1].max_hp = 96;
    party[1].target = 0;
    party[1].melee.hit_score = 100;
    party[1].melee.speed56 = 1;
    party[1].action = kActDefend;

    std::vector<Combatant> monsters(1);
    monsters[0].marker = 1;
    monsters[0].name = "Mercenary";
    monsters[0].hp = 8;
    monsters[0].max_hp = 8;
    monsters[0].melee.hit_score = 100;
    monsters[0].melee.speed56 = 1;

    RoundResult res = resolve_round(rng, party, monsters);
    check(res.rows.size() == 2, "one row per party member");
    check(monsters[0].hp == 7, "one bare-hand hit on the mercenary");
    check(res.rows[0].hp_delta == -1, "Decker took the defender's one point");
    check(res.rows[0].attack == 1, "Decker's attack passed");
    check(res.rows[1].hp_delta == 0, "Siegurd was not hit");
    check(res.rows[1].attack == 1, "the defender row is not Failed");
    check(!res.party_wiped && !res.defenders_wiped, "nobody wiped");

    // A member with no live target and no defenders left reports Failed.
    CombatRng rng2;
    rng2.seed = 1;
    party[1].hp = 96;
    monsters[0].hp = 0;
    RoundResult res2 = resolve_round(rng2, party, monsters);
    check(res2.defenders_wiped, "all defenders down");
    check(res2.rows[0].hp_delta == 0, "no damage with no defenders");
}

// The prelude's hit score (image 0x13744..0x13806), hand-computed.
static void test_hit_score() {
    PartyRow atk, tgt;
    atk.hp_pct = 100; atk.body24 = 0; atk.attr_a = 40; atk.attr_b = 100;
    tgt.hp_pct = 100; tgt.body24 = 1; tgt.attr_a = 236; tgt.attr_b = 20;
    // base = 120-20*(0+1) = 100; attack = 100-236 -> 1; power = 100
    check(hit_score(atk, tgt, 0, 1) == (1 + 100 + 100) / 3, "hit score 67");
    check(hit_score(atk, tgt, 2, 1) == 67 + 67 / 4, "hit score +25% for +0x1a==2");
    check(hit_score(atk, tgt, 0, 2) == 67 / 2, "hit score divided by +0x55");

    PartyRow a2, t2;  // every term clamps to 1
    a2.hp_pct = 0; a2.body24 = 4; a2.attr_a = 0; a2.attr_b = 0;
    t2.hp_pct = 0; t2.body24 = 4; t2.attr_a = 0; t2.attr_b = 0;
    check(hit_score(a2, t2, 0, 1) == 1, "hit score clamps to 1");
}

// The hand pick (image 0x13A43): with both hands filled the round draws
// rand(2) once and uses that hand's damage; with one hand it draws nothing.
static void test_hand_pick() {
    MeleeInputs in;
    in.attacks5d = 1; in.speed56 = 0; in.divisor55 = 1;
    in.str45 = 10; in.mod5e = 0; in.mod5f = 0; in.level17 = 0;
    in.skill27 = 0; in.pct4e = 100; in.hit_score = 100;

    // One hand: identical to the fixed-damage override for the same value.
    MeleeInputs one = in;
    one.weapon_damage = -1;
    one.weapon_slot[0] = 3; one.weapon_damage_slot[0] = 6;
    MeleeInputs fixed = in;
    fixed.weapon_damage = 6;
    CombatRng a; a.seed = 1;
    CombatRng b; b.seed = 1;
    assert(melee_round(a, one).damage == melee_round(b, fixed).damage);

    // Both hands: the extra rand(2) shifts the sequence, so the same seed gives
    // a different roll than the single-hand case.
    MeleeInputs two = one;
    two.weapon_slot[1] = 5; two.weapon_damage_slot[1] = 1;
    CombatRng c; c.seed = 1;
    CombatRng d; d.seed = 1;
    int one_dmg = melee_round(c, one).damage;
    int two_dmg = melee_round(d, two).damage;
    assert(one_dmg != two_dmg);

    // Swapping which hand holds which weapon changes the damage taken.
    MeleeInputs swap = two;
    swap.weapon_damage_slot[0] = 1;
    swap.weapon_damage_slot[1] = 6;
    CombatRng e; e.seed = 1;
    assert(melee_round(e, swap).damage != two_dmg);
}

int main() {
    test_rng();
    test_single_attack();
    test_strength_bonus();
    test_skill_bonus();
    test_attack_count();
    test_pct_and_cap();
    test_hit_score();
    test_hand_pick();
    test_round();
    if (failures) {
        fprintf(stderr, "combat_test: %d failure(s)\n", failures);
        return 1;
    }
    printf("combat_test: ok (rng sequence, melee formula, attack count, pct, cap, "
           "hit score, hand pick, round)\n");
    return 0;
}
