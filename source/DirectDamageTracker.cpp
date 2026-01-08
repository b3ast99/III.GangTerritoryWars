#include "DirectDamageTracker.h"
#include "CWorld.h"
#include "CTimer.h"
#include "DebugLog.h"

std::map<CPed*, std::vector<DirectDamageTracker::DamageRecord>> DirectDamageTracker::s_damageMap;
unsigned int DirectDamageTracker::s_lastCleanupTime = 0;

void DirectDamageTracker::Initialize()
{
    s_damageMap.clear();
    s_lastCleanupTime = 0;
    DebugLog::Write("DirectDamageTracker initialized");
}

void DirectDamageTracker::Shutdown()
{
    s_damageMap.clear();
    DebugLog::Write("DirectDamageTracker shutdown");
}

void DirectDamageTracker::RecordDamage(CPed* victim, CPed* attacker, float damage)
{
    if (!victim || !attacker || damage <= 0.0f) return;

    DamageRecord rec;
    rec.attacker = attacker;
    rec.damage = damage;
    rec.timestamp = CTimer::m_snTimeInMilliseconds;
    rec.bPlayerWasAttacker = (attacker == CWorld::Players[0].m_pPed);

    s_damageMap[victim].push_back(rec);

    // Optional debug for gang peds
    if (rec.bPlayerWasAttacker) {
        if (victim->m_ePedType >= PEDTYPE_GANG1 && victim->m_ePedType <= PEDTYPE_GANG3) {
            DebugLog::Write("DamageTrack: player -> gang %d ped %p dmg=%.1f",
                (int)victim->m_ePedType, victim, damage);
        }
    }
}

bool DirectDamageTracker::DidPlayerKillPed(CPed* ped)
{
    if (!ped) return false;

    auto it = s_damageMap.find(ped);
    if (it == s_damageMap.end()) return false;

    // Only consider recent damage (avoid old noise)
    const unsigned int now = CTimer::m_snTimeInMilliseconds;
    const auto& records = it->second;

    float total = 0.0f;
    float player = 0.0f;

    for (const auto& r : records) {
        if (now - r.timestamp <= 4000) { // last 4 seconds
            total += r.damage;
            if (r.bPlayerWasAttacker) player += r.damage;
        }
    }

    // Only check "kill" if ped is actually dead/dying
    const bool isDeadish =
        (ped->m_fHealth <= 0.0f) ||
        (ped->m_ePedState == PEDSTATE_DEAD) ||
        (ped->m_ePedState == PEDSTATE_DIE) ||
        ped->bIsPedDieAnimPlaying;

    if (!isDeadish) return false;

    // High-confidence rule: majority of recent damage was player
    if (total > 0.0f && (player / total) >= 0.60f) {
        return true;
    }

    // Secondary rule: player did a meaningful chunk (tune this)
    if (player >= 25.0f) {
        return true;
    }

    return false;
}

void DirectDamageTracker::CleanupOldRecords()
{
    const unsigned int now = CTimer::m_snTimeInMilliseconds;

    for (auto it = s_damageMap.begin(); it != s_damageMap.end(); ) {
        CPed* ped = it->first;
        auto& recs = it->second;

        // Cull old records
        recs.erase(
            std::remove_if(recs.begin(), recs.end(),
                [now](const DamageRecord& r) { return (now - r.timestamp) > 12000; }),
            recs.end()
        );

        // Remove map entry if empty or ped is gone/dead
        if (!ped || recs.empty() || ped->m_fHealth <= 0.0f) {
            it = s_damageMap.erase(it);
        }
        else {
            ++it;
        }
    }

    s_lastCleanupTime = now;
}

void DirectDamageTracker::Process()
{
    const unsigned int now = CTimer::m_snTimeInMilliseconds;
    if (now - s_lastCleanupTime > 3000) {
        CleanupOldRecords();
    }
}
