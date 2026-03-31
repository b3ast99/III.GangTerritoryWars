#pragma once
// Pure functions for act transition logic.
// No game engine dependencies — safe to include in unit test projects.
//
// Act transitions are detected by polling CStats::LastMissionPassedName.
// The three trigger labels (from MAIN.SCM register_mission_passed calls):
//   "JM2"   — "Farewell 'Chunky' Lee Chong" (Mission 25)  → Act 0 → 1
//   "FM4"   — "Last Requests"               (Mission 39)  → Act 1 → 2
//   "LOVE2" — "Waka-Gashira Wipeout!"       (Mission 61)  → Act 2 → 3
//
// Acts only advance, never retreat. The currentAct guard prevents re-triggering.

#include <cstring>

// Given the last-passed mission label and current act, returns the new act.
// Returns currentAct unchanged if the label is not a trigger or the act
// is already at or past the threshold.
inline int GetActForMissionLabel(const char* label, int currentAct) {
    if (!label || label[0] == '\0') return currentAct;
    if (std::strcmp(label, "JM2")   == 0 && currentAct == 0) return 1;
    if (std::strcmp(label, "FM4")   == 0 && currentAct == 1) return 2;
    if (std::strcmp(label, "LOVE2") == 0 && currentAct == 2) return 3;
    return currentAct;
}
