#pragma once

#include "TerritorySystem.h"
#include "CVector.h"
#include "ePedType.h"

#include <vector>

class WarSystem {
public:
    static void Init();
    static void Process();

    // Called by PedDeathTracker when a gang ped death is credited to the player
    static void RecordGangKill(ePedType gangType, const CVector& killPos);

    static bool IsMissionActive();
    static bool CanTriggerWarInTerritory(int territoryOwner);

private:
    struct KillRecord {
        ePedType gangType;
        CVector playerPosition;     // Player position when kill happened
        std::string territoryId;    // Territory player was in when kill happened
        unsigned int timestamp;
    };

    static std::vector<KillRecord> s_recentKills;
    static unsigned int s_triggerWindowMs;
    static constexpr int s_minKillsToTrigger = 3;

    static constexpr unsigned int CHECK_INTERVAL_MS = 500;

    static const Territory* GetTerritoryAtPosition(const CVector& pos);
    static int CountRecentKillsForTerritory(const std::string& territoryId, ePedType gangType);
    static bool CheckAndStartWar(const CVector& pos, ePedType hostileGang);
    static void ClearRecentKills();
};
