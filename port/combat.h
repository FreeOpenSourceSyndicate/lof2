// Combat — the derived part of the engine's round resolution.
//
// Everything here is transcribed from the shipped LOF.EXE (image offsets, i.e.
// file offset minus the 0x54F0 load header).  The routines:
//
//   0x1370C   per-attack routine: prelude, hit score, damage, kill, rewards
//   0x17982   round driver: `arg == 0` normal loop (0x179BB), else quick (0x17BAA)
//   0x14740   weapon melee path; falls through to 0x14D80 when no weapon slot
//   0x14D80   fallback path: up to ten attack rolls, summed
//   0x14C88   HP application (weapon path)   `sub word victim[+0x0a],ax`
//   0x1547E   HP application (fallback path), plus the XP award
//   0x30ECC   the C run-time generator every roll goes through
//
// See docs/notes-b.md ("Melee path — 0x14740", "Damage chain, kill path and
// miss flag") for the byte-level evidence.
#ifndef LOF_PORT_COMBAT_H
#define LOF_PORT_COMBAT_H

#include <cstdint>
#include <string>

#include "char.h"
#include <vector>

namespace lofport {

// The engine's generator (image 0x30ECC): a 32-bit LCG with multiplier
// 0x343FD and increment 0x269EC3 whose output is `(seed >> 16) & 0x7FFF`.
// `rand(n)` (0x2A7D:0x2F6A) is `rand() % n` and returns 0 for n <= 0.
struct CombatRng {
    uint32_t seed = 1;

    int rand15();
    int below(int n);  // rand() % n
};

enum CombatAction : uint8_t {
    kActDefend = 0,
    kActAttack = 1,
    kActStalk = 2,
    kActUseObject = 3,
    kActCastSpell = 4,
};

// One attacker's inputs to the melee routine.  All fields are character-record
// bytes (or item-table bytes) with the offsets the engine reads.
struct MeleeInputs {
    int hit_score = 0;      // prelude: ((atk+power+base)/3 [+25% if +0x1a==2]) / +0x55
    int weapon_damage = -1;  // < 0: use the hand slots below
    // The two hand slots the engine's weapon scan finds (image 0x1398B) and
    // their item damage bytes.  When both are filled `rand(2)` picks the hand
    // that attacks, once per round (0x13A43).
    int weapon_slot[2] = {-1, -1};
    int weapon_damage_slot[2] = {0, 0};  // ItemRecord::damage of the wielded item (0 = bare hands)
    int speed56 = 0;        // +0x56  speed (1..4)
    int attacks5d = 0;      // +0x5d  extra attacks
    int divisor55 = 1;      // +0x55  attack divisor (1 for every shipped character)
    int skill27 = 0;        // +0x27  attack skill
    int str45 = 0;          // +0x45  Strength
    int mod5e = 0;          // +0x5e  attribute modifier
    int mod5f = 0;          // +0x5f  attribute modifier
    int level17 = 0;        // +0x17  level
    int pct4e = 100;        // 32-bit +0x4e; < 100 multiplies damage by 100/pct
    int xp_mult = 0;        // xp_per_damage(victim's combat record +0x2c)
    // Break check (image 0x15852..0x15899): after a hit lands the engine rolls
    // rand(100)+1 and breaks the wielded weapon when the item's +0x10 byte
    // (`f0`) is >= the roll, setting the round's `W` flag and the slot's
    // condition byte to 0.  -1 disables the check (tests / no weapon).
    int weapon_f0[2] = {-1, -1};
};

struct MeleeResult {
    bool weapon_broken = false;  // the W flag (see MeleeInputs::weapon_f0)
    int attacks = 0;  // attack rolls made
    int hits = 0;     // rolls that beat the hit score
    int damage = 0;   // total damage applied (capped at 99 by the engine)
};

// The prelude's hit score (image 0x13744..0x13806): with the attacker's and the
// target's combat rows (the same 12-byte rows the party panel builder at
// 0x27F14 fills, exposed as `PartyRow`),
//
//   base   = 120 - 20 * (atk.body24 + tgt.body24)        ; 0x13756, clamped >= 1
//   attack = atk.attr_b - tgt.attr_a                     ; 0x13777, clamped >= 1
//   power  = atk.hp_pct                                  ; 0x1378e, clamped >= 1
//   hit    = (attack + power + base) / 3                 ; notes-b.md
//
// and `hit += hit / 4` when the attacker's +0x1a is 2, then `/ divisor55`.
int hit_score(const PartyRow& atk, const PartyRow& tgt, int char1a,
              int divisor55);

// XP awarded per point of damage (image 0x15493..0x154C0):
//
//   attacker[+0x6c] += xp_per_damage(victim_combat[+0x2c]) * damage
//
// The multiplier is the byte table at `[0xafc6]:0xf37` (measured `4c92:0f37`:
// `76,1,2,4,8,10,10,12,14,18,20,20,25`), indexed by the victim's combat-record
// byte +0x2c.  That byte is copied verbatim from the `SYST/NSCnn` record: the
// builder at image 0x1195A does `rep movsw` of 0x27 words (the whole 78-byte
// NSC record) into `[0xafc0]:[0x52] + 0x11a*index`, so
// `combat[+0x2c] == nsc.stats[0x2c - 0x26] == nsc.stats[6]`.
int xp_per_damage(int combat_record_2c);

// One melee round for one attacker: the fallback path (0x14D80) with the
// weapon-damage byte of the wielded item.
MeleeResult melee_round(CombatRng& rng, const MeleeInputs& in);

// One combatant row.  The engine keeps these in 12-byte records: party at
// es:[0x176 + 12*i], defenders at es:[0x258 + 72*group + 12*j].
struct Combatant {
    int marker = 2;          // +0x00  2 = party, 1 = defender
    int index = 0;           // +0x01  record index
    std::string name;
    int group = 0;           // party: target group; defenders: own group
    int hp = 0;              // +0x0a  current HP
    int max_hp = 0;
    int action = kActAttack; // +0x0f
    int target = 0;          // +0x0e
    MeleeInputs melee;
    uint32_t exp = 0;        // character +0x6c
};

// One line of the "The result of this round:" table.
struct CombatRoundRow {
    int hp_delta = 0;                 // negative = the member lost HP
    bool w = false, p = false, b = false;  // W/P/B columns (meaning not derived)
    int attack = 1;                   // 0 Failed, 1 Passed, 2 out of action
};

struct RoundResult {
    std::vector<CombatRoundRow> rows;  // one per party member, in order
    int party_down = 0;
    int defenders_down = 0;
    bool party_wiped = false;
    bool defenders_wiped = false;
};

// Resolve one round: every live party member with `action == kActAttack` hits
// the first live defender of its target group, then every live defender
// attacks the first live party member.
RoundResult resolve_round(CombatRng& rng, std::vector<Combatant>& party,
                          std::vector<Combatant>& defenders);

// Type-18 trap damage (image 0x20CD7..0x20D8E).  After sliding the party to the
// record's +0xe4 cell (the same write type 11 does), the handler subtracts
// `rand(+0xe6) + 1` from every present member's current HP (+0x22), clamped at
// 0 (`rand(n)` is 0x2A7D:0x2F6A == rand15() % n).  Mutates the save's raw
// character records so `build_save()` persists the loss.  Returns the total
// damage dealt across the party.  The port does not replay the engine's whole
// RNG stream, so pass a walk-local CombatRng; the formula and the [1, +0xe6]
// per-member range are exact, the absolute HP is a model (no oracle exists).
int apply_trap_damage(SaveGame& g, int damage_scale, CombatRng& rng);

}  // namespace lofport

#endif  // LOF_PORT_COMBAT_H
