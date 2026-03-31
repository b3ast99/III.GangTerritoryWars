#pragma once
// Pure functions for island identification and act-based island unlocking.
// No game engine dependencies — safe to include in unit test projects.

enum class Island { PORTLAND = 0, STAUNTON = 1, SHORESIDE = 2, UNKNOWN = -1 };

// GTA III world coordinate boundaries (approximate, well-known values):
//   Portland:  x > 616
//   Staunton:  -378 < x <= 616
//   Shoreside: x <= -378
inline Island GetIslandForPosition(float x, float /*y*/) {
    if (x > 616.0f)   return Island::PORTLAND;
    if (x > -378.0f)  return Island::STAUNTON;
    return Island::SHORESIDE;
}

// Act values: 0=NONE, 1=ACT1_PORTLAND, 2=ACT2_STAUNTON, 3=ACT3_ALL_ISLANDS
inline bool IsIslandUnlocked(Island island, int currentAct) {
    switch (island) {
    case Island::PORTLAND:  return currentAct >= 1;
    case Island::STAUNTON:  return currentAct >= 2;
    case Island::SHORESIDE: return currentAct >= 3;
    default:                return false;
    }
}
