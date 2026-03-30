#pragma once
// Pure functions encoding the neutral-territory auto-revert rule.
// No game engine dependencies — safe to include in unit test projects.
//
// Rule (Option A):
//   A territory that goes neutral (-1) records its last real owner and a timestamp.
//   After NeutralRevertMs have elapsed, it reverts to lastOwnerGang (or
//   defaultOwnerGang as fallback if lastOwnerGang is unknown).
//   Setting revertMs to 0 disables auto-revert entirely.

inline bool NeutralRevertDue(unsigned int neutralSinceMs, unsigned int nowMs, unsigned int revertMs) {
    if (revertMs == 0) return false;
    if (neutralSinceMs == 0) return false; // not stamped → not tracking
    return (nowMs - neutralSinceMs) >= revertMs;
}

// Returns which gang should take ownership on revert.
// Prefers lastOwnerGang; falls back to defaultOwnerGang.
// Returns -1 if neither is known (revert should not happen).
inline int NeutralRevertTarget(int lastOwnerGang, int defaultOwnerGang) {
    if (lastOwnerGang != -1) return lastOwnerGang;
    if (defaultOwnerGang != -1) return defaultOwnerGang;
    return -1;
}
