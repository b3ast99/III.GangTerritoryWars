#pragma once
// Pure functions encoding the act-gated gang affiliation system.
// No game engine dependencies — safe to include in unit test projects.
//
// Acts:
//   0 = NONE      — pre-unlock, no gang wars available
//   1 = ACT1      — Portland: allied with Mafia (GANG1), hostile Triads+Diablos
//   2 = ACT2      — Staunton: allied with Yakuza (GANG4), hostile Mafia+Colombians+Yardies
//   3 = ACT3      — All Islands: independent, ALL gangs hostile
//
// Gang ePedType values:
//   GANG1=7 (Mafia), GANG2=8 (Triads), GANG3=9 (Diablos)
//   GANG4=10 (Yakuza), GANG5=11 (Colombians), GANG6=12 (Yardies)

static constexpr int OWNER_NEUTRAL = -1;
static constexpr int OWNER_CLEARED = -2;

static constexpr int GANG1 = 7;   // Mafia
static constexpr int GANG2 = 8;   // Triads
static constexpr int GANG3 = 9;   // Diablos
static constexpr int GANG4 = 10;  // Yakuza
static constexpr int GANG5 = 11;  // Colombians
static constexpr int GANG6 = 12;  // Yardies

// Returns the player's affiliated gang for the given act, or -1 if independent.
inline int GetAffiliatedGangForAct(int act) {
    switch (act) {
    case 1: return GANG1;   // Act 1: Mafia
    case 2: return GANG4;   // Act 2: Yakuza
    default: return -1;     // Act 3 or NONE: independent
    }
}

// Returns true if the given gang is hostile to the player in this act.
inline bool IsGangHostileInAct(int act, int gangType) {
    // Must be a valid gang type (7-12)
    if (gangType < GANG1 || gangType > GANG6) return false;

    switch (act) {
    case 0: return false;  // NONE: no hostilities
    case 1:
        // Act 1: Triads and Diablos hostile; Mafia is allied
        return gangType == GANG2 || gangType == GANG3;
    case 2:
        // Act 2: Mafia, Colombians, Yardies hostile; Yakuza is allied
        return gangType == GANG1 || gangType == GANG5 || gangType == GANG6;
    case 3:
        // Act 3: ALL gangs hostile (player is independent)
        return true;
    default:
        return false;
    }
}

// Returns true if the given gang is the player's ally in this act.
inline bool IsGangAlliedInAct(int act, int gangType) {
    const int affiliated = GetAffiliatedGangForAct(act);
    return affiliated != -1 && gangType == affiliated;
}

// Can the player attack a territory with this owner in this act?
// Requirements: territory must not be locked, owner must be hostile, and not already cleared/neutral.
inline bool CanAttackTerritoryOwner(int ownerGang, int act, bool isLocked) {
    if (isLocked) return false;
    if (ownerGang == OWNER_CLEARED || ownerGang == OWNER_NEUTRAL) return false;
    return IsGangHostileInAct(act, ownerGang);
}

// Who gets the territory when the player wins a war?
// Acts 1-2: affiliated gang takes ownership (player's gang)
// Act 3: territory is "cleared" (OWNER_CLEARED) — player is independent
inline int GetCaptureOwnerForAct(int act) {
    switch (act) {
    case 1: return GANG1;         // Mafia takes it
    case 2: return GANG4;         // Yakuza takes it
    case 3: return OWNER_CLEARED; // Cleared (player-independent)
    default: return -1;           // NONE: shouldn't happen
    }
}
