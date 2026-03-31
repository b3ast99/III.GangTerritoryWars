#include "ActManager.h"
#include "ActTransitionRule.h"
#include "DebugLog.h"

#include "CStats.h"

#include <cstring>

void ActManager::Init()
{
    s_currentAct = 0;
    std::memset(s_lastMissionSeen, 0, sizeof(s_lastMissionSeen));
}

void ActManager::SetAct(int act)
{
    if (act == s_currentAct) return;
    DebugLog::Write("ActManager: act %d -> %d", s_currentAct, act);
    s_currentAct = act;
}

// Called every game loop tick. Detects when CStats::LastMissionPassedName changes
// and advances the act if the label matches one of our three trigger missions.
// Acts are never decremented — the guard (s_currentAct < N) also prevents
// re-triggering if the player somehow sees the same label twice.
void ActManager::PollMissionProgress()
{
    const char* last = CStats::LastMissionPassedName;

    // Periodic raw dump so we can verify the field is being read correctly
    // even when it hasn't changed. Fires every ~10 seconds. Remove once confirmed.
    static unsigned int s_nextDiagMs = 0;
    {
        // Use a simple counter since we don't have CTimer here without including it
        static unsigned int s_diagTick = 0;
        ++s_diagTick;
        if (s_diagTick >= 600) { // ~600 ticks ≈ 10s at 60fps
            s_diagTick = 0;
            DebugLog::Write("ActManager [DIAG poll] LastMissionPassedName=\"%s\" IndustrialPassed=%d currentAct=%d",
                last, CStats::IndustrialPassed, s_currentAct);
        }
    }

    if (std::strncmp(last, s_lastMissionSeen, 8) == 0) return;

    std::strncpy(s_lastMissionSeen, last, 8);
    s_lastMissionSeen[7] = '\0';

    DebugLog::Write("ActManager: mission passed '%s' (currentAct=%d)", s_lastMissionSeen, s_currentAct);

    const int newAct = GetActForMissionLabel(s_lastMissionSeen, s_currentAct);
    if (newAct != s_currentAct) SetAct(newAct);
}

// Called by TerritoryPersistence when a save has no ACTL chunk (mod installed
// mid-game). Infers the most accurate act possible from available CStats data.
//
// "Last Requests" (FM4) is the only trigger mission that calls an opcode
// (industrial_passed) which increments CStats::IndustrialPassed — so that's
// our most reliable mid-game indicator.
void ActManager::InferActOnFirstLoad()
{
    // Seed the polling baseline so PollMissionProgress doesn't re-fire on
    // the next tick from a stale LastMissionPassedName in the loaded save.
    std::strncpy(s_lastMissionSeen, CStats::LastMissionPassedName, 8);
    s_lastMissionSeen[7] = '\0';

    // Dump raw CStats values so we can verify the flags are correct at runtime.
    // Check each byte so we catch null-padding, unexpected encoding, etc.
    const char* raw = CStats::LastMissionPassedName;
    DebugLog::Write("ActManager [DIAG] LastMissionPassedName raw bytes: "
        "%02X %02X %02X %02X %02X %02X %02X %02X  string=\"%s\"",
        (unsigned char)raw[0], (unsigned char)raw[1],
        (unsigned char)raw[2], (unsigned char)raw[3],
        (unsigned char)raw[4], (unsigned char)raw[5],
        (unsigned char)raw[6], (unsigned char)raw[7],
        s_lastMissionSeen);
    DebugLog::Write("ActManager [DIAG] IndustrialPassed=%d", CStats::IndustrialPassed);

    // LastMissionPassedName check: catches the case where the player saved
    // immediately after one of our trigger missions.
    if (std::strcmp(s_lastMissionSeen, "LOVE2") == 0) { SetAct(3); return; }
    if (std::strcmp(s_lastMissionSeen, "FM4")   == 0) { SetAct(2); return; }
    if (std::strcmp(s_lastMissionSeen, "JM2")   == 0) { SetAct(1); return; }

    // CStats::IndustrialPassed is incremented by the 'industrial_passed' opcode
    // in "Last Requests" (FM4). If non-zero, the Salvatore arc is complete.
    if (CStats::IndustrialPassed > 0 && s_currentAct < 2) {
        DebugLog::Write("ActManager: inferred Act 2 (IndustrialPassed=%d)", CStats::IndustrialPassed);
        SetAct(2);
        return;
    }

    // No reliable indicator for Act 1 vs Act 0 when LastMissionPassedName isn't JM2.
    // Default to Act 0 — the player will advance naturally through PollMissionProgress.
    DebugLog::Write("ActManager: no act indicator found, defaulting to Act %d", s_currentAct);
}
