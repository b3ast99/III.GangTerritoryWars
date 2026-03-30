#pragma once
// Pure kill-credit decision logic.
// No game engine dependencies — safe to include in unit test projects.
//
// Two-part rule (both checked, either grants credit):
//   1. Ratio rule:  player dealt >= 60% of total recent damage to the ped.
//   2. Flat rule:   player dealt >= 25 damage regardless of total (handles cases where
//                   NPCs topped up health or the ped was low when targeted).

inline bool EvaluateKillCredit(float playerDamage, float totalDamage) {
    if (totalDamage > 0.f && (playerDamage / totalDamage) >= 0.60f) return true;
    if (playerDamage >= 25.f) return true;
    return false;
}
