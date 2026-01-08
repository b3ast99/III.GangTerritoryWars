// WaveCombat.h
#pragma once
#include "CPed.h"
#include "CVector.h"
#include "ePedType.h"
#include <vector>

class CPlayerPed;

namespace WaveCombat {
    struct EnemyTracker {
        CPed* ped = nullptr;
        int pedHandle = -1;
        int radarBlip = -1;
        CVector lastPos;
        unsigned int stuckSinceMs = 0;
        unsigned int deadSinceMs = 0;
        bool beingCleaned = false;
    };

    // Core management
    void Initialize();
    void Shutdown();
    void Update(unsigned int currentTime);

    // Enemy tracking
    void AddEnemy(CPed* ped, ePedType gangType);
    void RemoveEnemy(CPed* ped);
    void CleanupAllEnemies(bool isShutdown = false);

    // Blip management
    int CreateBlipForPed(CPed* ped, int blipColor);
    void HideBlipImmediately(int blipIndex);
    void RemoveBlipSafely(int& blipIndex);
    void UpdateBlipsForDeadPeds();

    // Combat logic
    void ReassertAggro(CPlayerPed* player);
    void ForceEnemiesToApproachPlayer();

    // Queries
    int GetAliveCount();
    bool IsAlivePed(CPed* ped);
    bool IsValidPed(CPed* ped);
    bool IsDeadPed(CPed* ped);

    // Getter for enemies (for debugging)
    const std::vector<EnemyTracker>& GetEnemies();
}