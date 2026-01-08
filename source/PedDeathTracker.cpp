#include "PedDeathTracker.h"
#include "DirectDamageTracker.h"
#include "DebugLog.h"

#include <algorithm>

std::vector<PedDeathTracker::DeathRecord> PedDeathTracker::s_recentlyProcessed;
unsigned int PedDeathTracker::s_lastCleanupTime = 0;

void PedDeathTracker::Initialize()
{
    s_recentlyProcessed.clear();
    s_lastCleanupTime = 0;
    DebugLog::Write("PedDeathTracker initialized");
}

void PedDeathTracker::Shutdown()
{
    s_recentlyProcessed.clear();
    DebugLog::Write("PedDeathTracker shutdown");
}

bool PedDeathTracker::WasRecentlyProcessed(CPed* ped)
{
    if (!ped) return false;

    // Check if this ped is already in our recently processed list
    for (const auto& r : s_recentlyProcessed) {
        if (r.ped == ped) {
            // Additional safety: if ped has respawned or isn't dead anymore, we can remove it
            if (ped->m_fHealth > 0.0f &&
                ped->m_ePedState != PEDSTATE_DEAD &&
                ped->m_ePedState != PEDSTATE_DIE &&
                !ped->bIsPedDieAnimPlaying) {
                // Ped is alive again - remove from list
                return false;
            }
            return true;
        }
    }
    return false;
}

bool PedDeathTracker::PlayerOnFootAndControllable(CPlayerPed* player)
{
    if (!player) return false;
    if (player->m_bInVehicle) return false;

    if (player->m_ePedState == PEDSTATE_DIE || player->m_ePedState == PEDSTATE_DEAD)
        return false;

    return true;
}

bool PedDeathTracker::IsPlayerRecentlyAttacking(CPlayerPed* player)
{
    if (!player) return false;

    // Strong signals
    if (player->bIsShooting) return true;
    if (player->m_nShootTimer > 0) return true;
    if (player->m_ePedState == PEDSTATE_ATTACK) return true;

    // Medium signals
    if (player->bIsAimingGun) return true;
    if (player->m_ePedState == PEDSTATE_AIMGUN) return true;

    // Weak-ish but still useful
    if (player->m_nAttackTimer > 0) return true;

    return false;
}

void PedDeathTracker::Process()
{
    const unsigned int now = CTimer::m_snTimeInMilliseconds;

    // Periodic cleanup - remove records older than 30 seconds
    if (now - s_lastCleanupTime > 5000) { // Check every 5 seconds
        s_recentlyProcessed.erase(
            std::remove_if(s_recentlyProcessed.begin(), s_recentlyProcessed.end(),
                [now](const DeathRecord& r) {
                    // Keep records for 30 seconds to prevent duplicate counting
                    return (now - r.timestamp) > 30000;
                }),
            s_recentlyProcessed.end()
        );
        s_lastCleanupTime = now;
    }

    CPlayerPed* player = CWorld::Players[0].m_pPed;
    if (!PlayerOnFootAndControllable(player)) {
        return;
    }

    const CVector playerPos = player->GetPosition();

    // NOTE: CPools iteration differs by SDK forks; keep your existing pool iteration
    for (int i = 0; i < CPools::ms_pPedPool->m_nSize; i++) {
        CPed* ped = CPools::ms_pPedPool->GetAt(i);
        if (!ped) continue;

        // Check if we've already processed this ped's death
        if (WasRecentlyProcessed(ped)) {
            continue;
        }

        // Check if ped is actually dead/deadish
        if (!IsPedJustDied(ped)) {
            continue;
        }

        // Check if ped is a gang ped
        if (!IsGangPed(ped)) {
            continue;
        }

        const CVector deathPos = ped->GetPosition();
        const float dist = (playerPos - deathPos).Magnitude();

        // First: high-confidence direct damage attribution
        bool creditedToPlayer = DirectDamageTracker::DidPlayerKillPed(ped);

        // Fallback: proximity + "recent combat" (conservative)
        if (!creditedToPlayer) {
            if (dist <= 35.0f && IsPlayerRecentlyAttacking(player)) {
                creditedToPlayer = true;
            }
        }

        if (creditedToPlayer) {
            ePedType gangType = GetPedGangType(ped);

            DebugLog::Write("KillCredit: player -> gang %d ped %p dist=%.1f",
                (int)gangType, ped, dist);

            // Add to recently processed BEFORE calling WarSystem::RecordGangKill
            // This prevents duplicate processing if RecordGangKill triggers another loop
            s_recentlyProcessed.push_back({ ped, now });

            WarSystem::RecordGangKill(gangType, deathPos);
        }
    }
}

bool PedDeathTracker::IsPedJustDied(CPed* ped)
{
    if (!ped) return false;

    // Check if ped is actually dead (not just dying)
    if (ped->m_fHealth <= 0.0f) return true;

    // Check ped state
    if (ped->m_ePedState == PEDSTATE_DEAD || ped->m_ePedState == PEDSTATE_DIE) {
        return true;
    }

    // Check if death animation is playing
    if (ped->bIsPedDieAnimPlaying) return true;

    return false;
}

bool PedDeathTracker::IsGangPed(CPed* ped)
{
    if (!ped) return false;
    return (ped->m_ePedType >= PEDTYPE_GANG1 && ped->m_ePedType <= PEDTYPE_GANG3);
}

ePedType PedDeathTracker::GetPedGangType(CPed* ped)
{
    if (!ped) return PEDTYPE_GANG1;
    return ped->m_ePedType;
}