#pragma once
// Pure functions for deriving territory visual/gameplay state from ownership + affiliation.
// No game engine dependencies — safe to include in unit test projects.
//
// TerritoryState drives blip color, ambient spawning, and war eligibility:
//   LOCKED      — territory on an island the player hasn't reached yet; hidden from radar
//   GANG_HOSTILE — enemy gang territory; can trigger wars here
//   GANG_ALLIED  — owned by the player's affiliated gang; no wars, friendly spawns
//   CLEARED      — player-captured in Act 3 (independent); no hostile spawns, no revert
//   NEUTRAL      — ownerless; will auto-revert to lastOwner after timer

#include "AffiliationRule.h"

enum class TerritoryState {
    LOCKED,
    GANG_HOSTILE,
    GANG_ALLIED,
    CLEARED,
    NEUTRAL
};

// Derive the gameplay state of a territory given its ownership and the current act context.
//   ownerGang:  ePedType gang value, OWNER_NEUTRAL (-1), or OWNER_CLEARED (-2)
//   act:        current story act (0-3)
//   isLocked:   true if territory is on an island the player can't reach yet
inline TerritoryState ComputeTerritoryState(int ownerGang, int act, bool isLocked) {
    if (isLocked)                       return TerritoryState::LOCKED;
    if (ownerGang == OWNER_CLEARED)     return TerritoryState::CLEARED;
    if (ownerGang == OWNER_NEUTRAL)     return TerritoryState::NEUTRAL;
    if (IsGangAlliedInAct(act, ownerGang)) return TerritoryState::GANG_ALLIED;
    if (IsGangHostileInAct(act, ownerGang)) return TerritoryState::GANG_HOSTILE;
    // Non-hostile, non-allied gang in this act (e.g., Yakuza territory in Act 1)
    // Treat as allied (can't attack, friendly behavior)
    return TerritoryState::GANG_ALLIED;
}

// What blip color should this territory use?
// Returns CRadar blip colour constants (matches eBlipColour).
//   0=RED, 1=GREEN, 2=BLUE, 3=WHITE, 4=YELLOW, 5=PURPLE, 6=CYAN
inline int GetTerritoryBlipColor(TerritoryState state) {
    switch (state) {
    case TerritoryState::GANG_HOSTILE: return 0; // RED — enemy territory
    case TerritoryState::GANG_ALLIED:  return 1; // GREEN — friendly territory
    case TerritoryState::CLEARED:      return 3; // WHITE — player-cleared
    case TerritoryState::NEUTRAL:      return 4; // YELLOW — contested/neutral
    case TerritoryState::LOCKED:       return 3; // WHITE (shouldn't render, but fallback)
    default:                           return 3;
    }
}

// Should this territory be visible on the radar?
inline bool IsTerritoryVisible(TerritoryState state) {
    return state != TerritoryState::LOCKED;
}

// Should hostile ambient peds spawn in this territory?
inline bool ShouldSpawnHostilePeds(TerritoryState state) {
    return state == TerritoryState::GANG_HOSTILE;
}

// Can the player start a gang war in this territory?
inline bool CanStartWarInTerritory(TerritoryState state) {
    return state == TerritoryState::GANG_HOSTILE;
}
