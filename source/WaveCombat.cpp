// WaveCombat.cpp
#include "WaveCombat.h"
#include "GangInfo.h"
#include "DebugLog.h"
#include "CWorld.h"
#include "CPools.h"
#include "CRadar.h"
#include "CTimer.h"
#include <algorithm>

namespace WaveCombat {
    static std::vector<EnemyTracker> s_enemies;
    static unsigned int s_nextBlipUpdateTime = 0;

    // Utility functions
    namespace {
        int GetHandle(CPed* ped) {
            return ped ? CPools::GetPedRef(ped) : -1;
        }

        float Distance2D(const CVector& a, const CVector& b) {
            float dx = a.x - b.x;
            float dy = a.y - b.y;
            return sqrtf(dx * dx + dy * dy);
        }
    }

    void Initialize() {
        s_enemies.clear();
        s_nextBlipUpdateTime = 0;
    }

    void Shutdown() {
        CleanupAllEnemies(true);
    }

    void Update(unsigned int currentTime) {
        if (currentTime >= s_nextBlipUpdateTime) {
            UpdateBlipsForDeadPeds();
            s_nextBlipUpdateTime = currentTime + 100;
        }

        // Force enemies to move toward player if they're too far
        static unsigned int lastMoveCheck = 0;
        if (currentTime - lastMoveCheck > 2000) {  // Every 2 seconds
            ForceEnemiesToApproachPlayer();
            lastMoveCheck = currentTime;
        }
    }

    void AddEnemy(CPed* ped, ePedType gangType) {
        if (!ped) return;

        const GangInfo* info = GangManager::GetGangInfo(gangType);
        int blipColor = info ? info->blipColor : BLIP_COLOUR_RED;

        EnemyTracker tracker;
        tracker.ped = ped;
        tracker.pedHandle = GetHandle(ped);
        tracker.radarBlip = CreateBlipForPed(ped, blipColor);
        tracker.lastPos = ped->GetPosition();

        s_enemies.push_back(tracker);
        DebugLog::Write("Added enemy to tracker: %p, handle %d", ped, tracker.pedHandle);
    }

    void RemoveEnemy(CPed* ped) {
        auto it = std::find_if(s_enemies.begin(), s_enemies.end(),
            [ped](const EnemyTracker& e) { return e.ped == ped; });

        if (it != s_enemies.end()) {
            if (it->radarBlip != -1) {
                RemoveBlipSafely(it->radarBlip);
            }
            s_enemies.erase(it);
        }
    }

    void CleanupAllEnemies(bool isShutdown) {
        DebugLog::Write("CleanupAllEnemies: %d enemies (shutdown=%d)",
            (int)s_enemies.size(), isShutdown);

        for (auto& e : s_enemies) {
            if (e.beingCleaned) continue;
            e.beingCleaned = true;

            if (e.radarBlip != -1) {
                RemoveBlipSafely(e.radarBlip);
            }

            if (e.ped) {
                if (isShutdown) {
                    e.ped = nullptr;
                }
                else {
                    if (CPools::GetPedRef(e.ped) != -1) {
                        //CWorld::Remove(e.ped);
                        //delete e.ped;

                        // Mark as not mission ped
                        e.ped->m_nCharCreatedBy = 0;
                        // Kill it properly
                        e.ped->m_fHealth = 0.0f;
                        e.ped->m_ePedState = PEDSTATE_DEAD;
                        // Game engine will handle cleanup
                    }
                    e.ped = nullptr;
                }
            }
            e.pedHandle = -1;
        }

        s_enemies.clear();
    }

    // Blip management (copied from your original)
    int CreateBlipForPed(CPed* ped, int blipColor) {
        int pedHandle = GetHandle(ped);
        if (pedHandle < 0) return -1;

        CRadar::SetEntityBlip(BLIP_CHAR, pedHandle, blipColor, BLIP_DISPLAY_BLIP_ONLY);
        for (int i = 0; i < 175; ++i) {
            if (CRadar::ms_RadarTrace[i].m_nBlipType == BLIP_CHAR &&
                CRadar::ms_RadarTrace[i].m_nEntityHandle == pedHandle) {
                return i;
            }
        }
        return -1;
    }

    void HideBlipImmediately(int blipIndex) {
        if (blipIndex < 0 || blipIndex >= 175) return;

        auto& trace = CRadar::ms_RadarTrace[blipIndex];
        if (trace.m_nBlipType != BLIP_NONE) {
            trace.m_nBlipDisplay = BLIP_DISPLAY_NEITHER;
        }
    }

    void RemoveBlipSafely(int& blipIndex) {
        if (blipIndex < 0 || blipIndex >= 175) {
            blipIndex = -1;
            return;
        }

        if (CRadar::ms_RadarTrace[blipIndex].m_nBlipType != BLIP_NONE) {
            CRadar::ClearBlip(blipIndex);
        }

        blipIndex = -1;
    }

    void UpdateBlipsForDeadPeds() {
        for (auto& e : s_enemies) {
            if (e.radarBlip == -1) continue;
            if (!IsValidPed(e.ped)) continue;

            if (IsDeadPed(e.ped)) {
                if (e.radarBlip >= 0 && e.radarBlip < 175) {
                    auto& trace = CRadar::ms_RadarTrace[e.radarBlip];
                    if (trace.m_nBlipDisplay != BLIP_DISPLAY_NEITHER) {
                        HideBlipImmediately(e.radarBlip);
                    }
                }
            }
        }
    }

    // Combat logic (copied from your original)
    void ReassertAggro(CPlayerPed* player) {
        if (!player) return;

        for (auto& e : s_enemies) {
            if (IsAlivePed(e.ped)) {
                // More aggressive: Always re-target player if not already attacking
                if (e.ped->m_ePedState == PEDSTATE_IDLE ||
                    e.ped->m_ePedState == PEDSTATE_NONE ||
                    e.ped->m_ePedState == PEDSTATE_WANDER_RANGE ||
                    e.ped->m_ePedState == PEDSTATE_WANDER_PATH) {

                    e.ped->SetObjective(OBJECTIVE_KILL_CHAR_ON_FOOT, player);

                    // Make them move toward player more aggressively
                    float distToPlayer = Distance2D(e.ped->GetPosition(), player->GetPosition());

                    if (distToPlayer > 30.0f) {  // If far away, run
                        e.ped->SetMoveState(PEDMOVE_RUN);
                    }
                    else if (distToPlayer > 15.0f) {  // Medium distance, walk
                        e.ped->SetMoveState(PEDMOVE_WALK);
                    }
                }
            }
        }
    }

    void ForceEnemiesToApproachPlayer() {
        CPlayerPed* player = CWorld::Players[0].m_pPed;
        if (!player) return;

        CVector playerPos = player->GetPosition();

        for (auto& e : s_enemies) {
            if (IsAlivePed(e.ped)) {
                float dist = Distance2D(e.ped->GetPosition(), playerPos);

                // If enemy is too far away (> 50 units), force them to run toward player
                if (dist > 50.0f) {
                    e.ped->SetMoveState(PEDMOVE_RUN);
                    e.ped->SetObjective(OBJECTIVE_KILL_CHAR_ON_FOOT, player);
                }
                // If moderately far (30-50 units), walk (since there's no JOG)
                else if (dist > 30.0f) {
                    e.ped->SetMoveState(PEDMOVE_WALK);
                }
            }
        }
    }

    // Queries
    int GetAliveCount() {
        int count = 0;
        for (const auto& e : s_enemies) {
            if (IsAlivePed(e.ped)) ++count;
        }
        return count;
    }

    bool IsValidPed(CPed* ped) {
        if (!ped) return false;
        return CPools::GetPedRef(ped) != -1;
    }

    bool IsDeadPed(CPed* ped) {
        if (!ped) return true;
        if (ped->m_fHealth <= 0.0f) return true;

        // Use correct ped state from CPed.h - it's m_ePedState
        if (ped->m_ePedState == PEDSTATE_DEAD ||
            ped->m_ePedState == PEDSTATE_DIE) {
            return true;
        }

        return false;
    }

    bool IsAlivePed(CPed* ped) {
        return ped && IsValidPed(ped) && !IsDeadPed(ped);
    }

    const std::vector<EnemyTracker>& GetEnemies() {
        return s_enemies;
    }
}