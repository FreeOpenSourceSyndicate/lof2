#include "combat.h"

namespace lofport {

int CombatRng::rand15() {
    seed = seed * 0x343FDu + 0x269EC3u;
    return (int)((seed >> 16) & 0x7FFFu);
}

int CombatRng::below(int n) {
    if (n <= 0) return 0;
    return rand15() % n;
}

int hit_score(const PartyRow& atk, const PartyRow& tgt, int char1a,
              int divisor55) {
    int base = 120 - 20 * (int)(atk.body24 + tgt.body24);
    int attack = (int)atk.attr_b - (int)tgt.attr_a;
    int power = atk.hp_pct;
    if (base < 1) base = 1;
    if (attack < 1) attack = 1;
    if (power < 1) power = 1;
    int hit = (attack + power + base) / 3;
    if (char1a == 2) hit += hit / 4;
    if (divisor55 > 0) hit /= divisor55;
    return hit;
}

// Fallback melee path, image 0x14D80..0x15086.  The engine rolls up to ten
// attacks (`(record[+0x5d] + record[+0x56]) / record[+0x55]`, hard-capped at
// ten), sums the ones that beat the prelude's hit score, applies the +0x4e
// percentage multiplier, caps the total at 99 and zeroes it when the acting
// side flag is clear.  Verified instruction by instruction; see notes-b.md.
MeleeResult melee_round(CombatRng& rng, const MeleeInputs& in) {
    MeleeResult out;
    int divisor = in.divisor55 > 0 ? in.divisor55 : 1;

    int attacks = (in.attacks5d + in.speed56) / divisor;
    if (attacks > 10) attacks = 10;
    if (attacks < 0) attacks = 0;
    out.attacks = attacks;

    // The hand pick is made once per round, before the attack loop (0x13A43),
    // and only when both hands hold a weapon.
    int weapon = in.weapon_damage;
    if (weapon < 0) {
        weapon = 0;
        if (in.weapon_slot[0] >= 0)
            weapon = (in.weapon_slot[1] >= 0 && rng.below(2) == 1)
                         ? in.weapon_damage_slot[1]
                         : in.weapon_damage_slot[0];
    }

    int total = 0;
    for (int i = 0; i < attacks; i++) {
        int dmg = rng.below(weapon) + 1;                        // 0x14DFA
        int str = in.str45 / divisor + in.mod5e;               // 0x14E29
        if (str > 15) dmg += str - 15;                         // 0x14E3E
        int roll = rng.below(100) + 1;                         // 0x14E63
        if (in.skill27 / divisor >= roll)                      // 0x14E83
            dmg += (in.mod5f + in.level17) * (dmg / 10);       // 0x14EBE
        int hit_roll = rng.below(100) + 1;                     // 0x14EFE
        if (hit_roll < in.hit_score) {                         // 0x14F0D
            total += dmg;
            out.hits++;
        }
    }

    if (in.pct4e > 0 && in.pct4e < 100) {                      // 0x14F84
        int scale = 100 / in.pct4e;
        total *= scale;
        out.hits *= scale;
        if (total == 0) total = 1;
        if (out.hits == 0) out.hits = 1;
    }
    out.hits /= divisor;                                       // 0x15060
    if (total > 99) total = 99;                                // 0x15070
    out.damage = total;
    return out;
}

namespace {

bool alive(const Combatant& c) { return c.hp > 0; }

Combatant* first_live(std::vector<Combatant>& v, int group) {
    for (auto& c : v)
        if (alive(c) && c.group == group) return &c;
    return nullptr;
}

Combatant* first_live_any(std::vector<Combatant>& v) {
    for (auto& c : v)
        if (alive(c)) return &c;
    return nullptr;
}

}  // namespace

RoundResult resolve_round(CombatRng& rng, std::vector<Combatant>& party,
                          std::vector<Combatant>& defenders) {
    RoundResult res;
    std::vector<int> before;
    before.reserve(party.size());
    for (const auto& m : party) before.push_back(m.hp);
    std::vector<int> connected(party.size(), 0);  // damage dealt per member
    std::vector<int> engaged(party.size(), 0);    // had a live target

    // Party phase: every live member whose action is Attack.
    for (size_t i = 0; i < party.size(); i++) {
        Combatant& m = party[i];
        if (!alive(m) || m.action != kActAttack) continue;
        Combatant* tgt = first_live(defenders, m.target);
        if (!tgt) tgt = first_live_any(defenders);
        if (!tgt) break;
        engaged[i] = 1;
        MeleeInputs in = m.melee;
        in.xp_mult = tgt->melee.xp_mult;
        MeleeResult r = melee_round(rng, in);
        tgt->hp -= r.damage;
        m.exp += (uint32_t)(r.damage * in.xp_mult);
        connected[i] = r.damage;
        if (tgt->hp <= 0) res.defenders_down++;
    }

    // Defender phase: every live defender hits the first live party member.
    for (auto& d : defenders) {
        if (!alive(d)) continue;
        Combatant* tgt = first_live_any(party);
        if (!tgt) break;
        MeleeResult r = melee_round(rng, d.melee);
        tgt->hp -= r.damage;
        d.exp += (uint32_t)(r.damage * tgt->melee.xp_mult);
        if (tgt->hp <= 0) res.party_down++;
    }

    res.rows.resize(party.size());
    for (size_t i = 0; i < party.size(); i++) {
        CombatRoundRow& row = res.rows[i];
        row.hp_delta = party[i].hp - before[i];
        if (before[i] <= 0)
            row.attack = 2;                       // out of action
        else if (party[i].action != kActAttack)
            row.attack = 1;                       // defended / acted elsewhere
        else if (!engaged[i])
            row.attack = 1;                          // no target: not "Failed"
        else
            row.attack = connected[i] > 0 ? 1 : 0;   // Passed / Failed (miss)
    }

    res.party_wiped = first_live_any(party) == nullptr;
    res.defenders_wiped = first_live_any(defenders) == nullptr;
    return res;
}

}  // namespace lofport
