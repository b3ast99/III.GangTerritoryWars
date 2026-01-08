#pragma once
#include "plugin.h"
#include "CPed.h"
#include <map>
#include <vector>

class DirectDamageTracker {
public:
    struct DamageRecord {
        CPed* attacker = nullptr;
        float damage = 0.0f;
        unsigned int timestamp = 0;
        bool bPlayerWasAttacker = false;
    };

    static void Initialize();
    static void Shutdown();
    static void Process();

    // Called from DamageHook
    static void RecordDamage(CPed* victim, CPed* attacker, float damage);

    // High-confidence check (based on tracked direct damage)
    static bool DidPlayerKillPed(CPed* ped);

private:
    static std::map<CPed*, std::vector<DamageRecord>> s_damageMap;
    static unsigned int s_lastCleanupTime;

    static void CleanupOldRecords();
};
