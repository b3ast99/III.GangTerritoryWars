#pragma once
// Tracks the current narrative act. Acts drive gang affiliation, territory unlock,
// and spawn/war eligibility via AffiliationRule.h / TerritoryStateRule.h.
//
// Acts: 0=NONE (pre-unlock), 1=ACT1_PORTLAND, 2=ACT2_STAUNTON, 3=ACT3_ALL_ISLANDS
//
// Act transition triggers (detected via CStats::LastMissionPassedName):
//   'JM2'   — "Farewell 'Chunky' Lee Chong" (Mission 25)  → Act 0→1
//   'FM4'   — "Last Requests"               (Mission 39)  → Act 1→2
//   'LOVE2' — "Waka-Gashira Wipeout!"       (Mission 61)  → Act 2→3
//
// PollMissionProgress() is called every game loop tick.
// InferActOnFirstLoad() is called by TerritoryPersistence when no ACTL chunk exists.

class ActManager {
public:
    static void Init();
    static int  GetCurrentAct() { return s_currentAct; }
    static void SetAct(int act);

    // Called every game loop tick to detect mission completions in real time.
    static void PollMissionProgress();

    // Called by TerritoryPersistence when loading a save with no ACTL chunk (mod
    // installed mid-game). Best-effort: infers act from available CStats data.
    static void InferActOnFirstLoad();

    // Public for TerritoryPersistence save/load.
    inline static int  s_currentAct     = 0;
    inline static char s_lastMissionSeen[8] = {};
};
