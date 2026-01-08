#pragma once

#include "plugin.h"
#include "CPed.h"
#include "CWorld.h"
#include "CPools.h"
#include "CTimer.h"
#include "WarSystem.h"
#include <vector>

class PedDeathTracker {
public:
    static void Initialize();
    static void Shutdown();
    static void Process();

private:
    static bool IsPedJustDied(CPed* ped);
    static bool IsGangPed(CPed* ped);
    static ePedType GetPedGangType(CPed* ped);

    static bool PlayerOnFootAndControllable(CPlayerPed* player);

    // Conservative “recent combat” signal
    static bool IsPlayerRecentlyAttacking(CPlayerPed* player);

    struct DeathRecord {
        CPed* ped = nullptr;
        unsigned int timestamp = 0;
    };

    static bool WasRecentlyProcessed(CPed* ped);

    static std::vector<DeathRecord> s_recentlyProcessed;
    static unsigned int s_lastCleanupTime;

    static constexpr unsigned int s_cleanupIntervalMs = 2000;
};
