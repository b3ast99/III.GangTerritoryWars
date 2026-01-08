#include "WarSystem.h"

#include "CWorld.h"
#include "CPlayerPed.h"
#include "CTimer.h"

#include "TerritorySystem.h"
#include "WaveManager.h"

#include "CMessages.h"
#include "DebugLog.h"
#include "CTheScripts.h"

#include <map>

// Configuration constants (could be moved to config file)
static constexpr int MAX_RECENT_KILLS = 100; // Prevent unbounded growth
static constexpr unsigned int KILL_MESSAGE_DISPLAY_MS = 1500;
static constexpr unsigned int FRIENDLY_WARNING_MESSAGE_MS = 2000;
static constexpr unsigned int WAR_PROVOKED_MESSAGE_MS = 3000;

// Static members
std::vector<WarSystem::KillRecord> WarSystem::s_recentKills;
unsigned int WarSystem::s_triggerWindowMs = 15000; // 15 seconds, like SA

void WarSystem::Init() {
    s_recentKills.clear();
    s_recentKills.reserve(MAX_RECENT_KILLS);
    s_triggerWindowMs = 15000;
}

void WarSystem::Process() {
    unsigned int now = CTimer::m_snTimeInMilliseconds;

    // Clean old kills
    if (!s_recentKills.empty()) {
        auto it = s_recentKills.begin();
        while (it != s_recentKills.end()) {
            if (now - it->timestamp > s_triggerWindowMs) {
                DebugLog::Write("Cleaning old kill: gang=%d, territory=%s, age=%u",
                    (int)it->gangType, it->territoryId.c_str(), now - it->timestamp);
                it = s_recentKills.erase(it);
            }
            else {
                ++it;
            }
        }
    }

    // Check for war trigger every 500ms
    static unsigned int lastCheck = 0;
    if (now - lastCheck > CHECK_INTERVAL_MS) {
        CPlayerPed* player = CWorld::Players[0].m_pPed;
        if (player && !s_recentKills.empty()) {
            const CVector playerPos = player->GetPosition();
            const Territory* currentTerr = GetTerritoryAtPosition(playerPos);

            if (currentTerr) {
                // Check if we have enough kills for this territory
                for (int gangType = PEDTYPE_GANG1; gangType <= PEDTYPE_GANG3; gangType++) {
                    if (currentTerr->ownerGang == gangType) {
                        int killCount = CountRecentKillsForTerritory(currentTerr->id, (ePedType)gangType);

                        if (killCount >= s_minKillsToTrigger) {
                            DebugLog::Write("Attempting to trigger war: %d kills in %s",
                                killCount, currentTerr->id.c_str());

                            if (CheckAndStartWar(playerPos, (ePedType)gangType)) {
                                ClearRecentKills();
                                break;
                            }
                        }
                    }
                }
            }
        }
        lastCheck = now;
    }
}

void WarSystem::RecordGangKill(ePedType gangType, const CVector& /*killPos*/) {
    // Never provoke wars during missions
    if (IsMissionActive()) {
        return;
    }

    // Don't track provocation once a war is running
    if (WaveManager::IsWarActive()) {
        return;
    }

    CPlayerPed* player = CWorld::Players[0].m_pPed;
    if (!player) return;

    // Player must be on foot (use reliable state flags)
    if (player->m_bInVehicle ||
        player->m_ePedState == PEDSTATE_DRIVING ||
        player->m_ePedState == PEDSTATE_PASSENGER) {
        return;
    }

    const CVector playerPos = player->GetPosition();
    const Territory* terr = GetTerritoryAtPosition(playerPos);
    if (!terr) {
        DebugLog::Write("RecordGangKill: Player not in any territory at (%.1f, %.1f)",
            playerPos.x, playerPos.y);
        return;
    }

    // Territory must be owned by the gang being provoked
    if (terr->ownerGang != gangType) {
        DebugLog::Write("RecordGangKill: Territory %s owned by gang %d, but killed gang %d",
            terr->id.c_str(), (int)terr->ownerGang, (int)gangType);
        return;
    }

    // Must be hostile (can't trigger in friendly territory)
    if (!CanTriggerWarInTerritory(terr->ownerGang)) {
        // keep silent to avoid spam when killing friendlies
        return;
    }

    // Don't provoke if already under attack
    if (terr->underAttack) {
        return;
    }

    // Prevent unbounded growth
    if (s_recentKills.size() >= MAX_RECENT_KILLS) {
        s_recentKills.erase(s_recentKills.begin());
    }

    KillRecord record;
    record.gangType = gangType;
    record.playerPosition = playerPos;
    record.territoryId = terr->id;
    record.timestamp = CTimer::m_snTimeInMilliseconds;
    s_recentKills.push_back(record);

    // Debug: count how many kills for this territory/gang combo
    int count = CountRecentKillsForTerritory(terr->id, gangType);
    DebugLog::Write("Recorded kill: gang=%d, territory=%s, total recent=%d/3",
        (int)gangType, terr->id.c_str(), count);

    if (count >= s_minKillsToTrigger) {
        DebugLog::Write("READY TO TRIGGER WAR: %d kills in territory %s",
            count, terr->id.c_str());
    }
}

bool WarSystem::IsMissionActive() {
    return CTheScripts::IsPlayerOnAMission();
}

bool WarSystem::CanTriggerWarInTerritory(int territoryOwner) {
    // GTA III doesn’t have “player gang affiliation” in the same way SA does.
    // If you have a notion of player-affiliated gang, enforce it here.
    // For now: allow provoking any non-neutral owner.
    return territoryOwner != 0;
}

const Territory* WarSystem::GetTerritoryAtPosition(const CVector& pos) {
    return TerritorySystem::GetTerritoryAtPoint(pos);
}

int WarSystem::CountRecentKillsForTerritory(const std::string& territoryId, ePedType gangType) {
    unsigned int now = CTimer::m_snTimeInMilliseconds;
    int count = 0;

    for (const auto& kill : s_recentKills) {
        // Only count recent kills (within trigger window)
        // For the SAME territory and SAME gang type
        if ((now - kill.timestamp) <= s_triggerWindowMs &&
            kill.territoryId == territoryId &&
            kill.gangType == gangType) {
            count++;
        }
    }
    return count;
}

bool WarSystem::CheckAndStartWar(const CVector& pos, ePedType hostileGang) {
    // Don't start during missions
    if (IsMissionActive()) {
        return false;
    }

    // Player must be on foot
    CPlayerPed* player = CWorld::Players[0].m_pPed;
    if (!player) return false;

    if (player->m_bInVehicle ||
        player->m_ePedState == PEDSTATE_DRIVING ||
        player->m_ePedState == PEDSTATE_PASSENGER) {
        return false;
    }

    const Territory* terr = GetTerritoryAtPosition(pos);
    if (!terr) return false;

    // Must be owned by this gang
    if (terr->ownerGang != hostileGang) return false;

    // Must be hostile
    if (!CanTriggerWarInTerritory(terr->ownerGang)) {
        CMessages::AddMessageJumpQ("You can't attack your own gang!", FRIENDLY_WARNING_MESSAGE_MS, 0);
        return false;
    }

    // Already active / already under attack
    if (WaveManager::IsWarActive()) return false;
    if (terr->underAttack) return false;

    DebugLog::Write("Starting gang war in territory %s (gang %d)",
        terr->id.c_str(), (int)hostileGang);

    CMessages::AddMessageJumpQ("You have provoked a gang war!", WAR_PROVOKED_MESSAGE_MS, 0);
    WaveManager::StartWar(hostileGang, terr);

    return true;
}

void WarSystem::ClearRecentKills() {
    s_recentKills.clear();
}
