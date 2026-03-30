#include "TerritoryAmbientSpawner.h"

#include "DebugLog.h"
#include "IniConfig.h"
#include "TerritorySystem.h"
#include "GangInfo.h"
#include "WaveManager.h"

#include "CWorld.h"
#include "CPlayerPed.h"
#include "CPools.h"
#include "CPed.h"
#include "CTimer.h"
#include "CStreaming.h"
#include "CModelInfo.h"
#include "CPopulation.h"

#include <vector>
#include <cstdlib>
#include <algorithm>
#include <cmath>

namespace {

// --------------------------
// Tunables (feel free to tweak)
// --------------------------
static bool  s_enabled = true;

// Density check radius around player/territory focus
static float s_checkRadius = 75.0f;

// How many owner-gang peds we want ambiently around (soft target)
static int   s_targetGangPeds = 1;

// Hard cap (never exceed this many owner-gang peds within radius)
static int   s_hardCapGangPeds = 2;

// Spawn distance band from player (avoid popping on top of player)
static float s_spawnMinDist = 25.0f;
static float s_spawnMaxDist = 55.0f;

// Attempts per tick to find a valid spawn spot
static int   s_spawnAttempts = 10;

// Rate limiting
static unsigned int s_globalCooldownMs = 3500;     // at most 1 spawn per ~3.5s overall
static unsigned int s_perTerritoryCooldownMs = 7000; // at most 1 spawn per territory per ~7.0s

// Extra throttle to avoid runaway in weird scenarios
static unsigned int s_noSpawnBackoffMs = 1800; // if we decide not to spawn, wait a bit

// State
static unsigned int s_nextGlobalActionMs = 0;
static unsigned int s_nextTickMs = 0;

// We track per-territory cooldown by current territory vector index.
// (Territory list is stable-ish; if it changes, we resize.)
static std::vector<unsigned int> s_nextTerritoryActionMs;

static inline float Dist2(const CVector& a, const CVector& b) {
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    const float dz = a.z - b.z;
    return dx*dx + dy*dy + dz*dz;
}

static inline float Rand01() {
    return (float)std::rand() / (float)RAND_MAX;
}
static inline float RandRange(float a, float b) {
    return a + (b - a) * Rand01();
}

static bool IsOwnerGangValid(int ownerGang) {
    // Your mod uses 3 gangs; ownerGang corresponds to PEDTYPE_GANG1..3
    return ownerGang >= (int)PEDTYPE_GANG1 && ownerGang <= (int)PEDTYPE_GANG3;
}

static int CountNearbyOwnerGangPeds(const CVector& center, float radius, int ownerGang) {
    const float r2 = radius * radius;
    int count = 0;

    for (int i = 0; i < CPools::ms_pPedPool->m_nSize; ++i) {
        CPed* p = CPools::ms_pPedPool->GetAt(i);
        if (!p) continue;
        if (p->m_ePedState == PEDSTATE_DEAD) continue;
        if (!p->m_bInVehicle && p->m_fHealth <= 0.0f) continue;

        // Exclude player
        if (p == CWorld::Players[0].m_pPed) continue;

        // Count only owner gang type
        if ((int)p->m_ePedType != ownerGang) continue;

        const CVector pos = p->GetPosition();
        if (Dist2(pos, center) <= r2) {
            ++count;
        }
    }
    return count;
}

static bool ClampIntoTerritory(const Territory* t, float& x, float& y) {
    if (!t) return false;
    x = std::max(t->minX, std::min(x, t->maxX));
    y = std::max(t->minY, std::min(y, t->maxY));
    return true;
}

// Very lightweight “find a ground-ish spot” without overengineering.
// GTA III is forgiving; we just keep it within territory + distance band.
// If you later want *path node* correctness, we can reuse your WaveManager logic.
static bool FindSpawnPos(const Territory* t, const CVector& playerPos, CVector& out) {
    if (!t) return false;

    const float minDist2 = s_spawnMinDist * s_spawnMinDist;

    for (int attempt = 0; attempt < s_spawnAttempts; ++attempt) {
        const float ang = RandRange(0.0f, 6.2831853f);
        const float dist = RandRange(s_spawnMinDist, s_spawnMaxDist);

        float x = playerPos.x + std::cos(ang) * dist;
        float y = playerPos.y + std::sin(ang) * dist;

        // Keep inside territory to avoid “leaking”
        ClampIntoTerritory(t, x, y);

        // Reject if clamping pushed the point too close to the player
        const float dx = x - playerPos.x, dy = y - playerPos.y;
        if (dx * dx + dy * dy < minDist2) continue;

        // Basic Z: start near player Z; GTA often corrects this on placement anyway
        out = CVector(x, y, playerPos.z + 1.0f);
        return true;
    }

    return false;
}

static inline bool EnsureModelLoaded(int modelId) {
    if (modelId < 0) return false;

    // Must exist in model info table
    if (!CModelInfo::GetModelInfo(modelId)) return false;

    // Ask streaming to ensure it’s loaded; this is compatible with GTA III plugin-sdk
    // across variants (no HasModelLoaded / no RW pointer assumptions).
    CStreaming::RequestModel(modelId, GAME_REQUIRED | KEEP_IN_MEMORY);
    CStreaming::LoadAllRequestedModels(false);

    // We can’t reliably query “loaded” across SDK variants; assume the load call did its job.
    return true;
}


} // namespace

void TerritoryAmbientSpawner::Init() {
    s_enabled = true;
    s_nextGlobalActionMs = 0;
    s_nextTickMs = 0;
    s_nextTerritoryActionMs.clear();

    auto& ini = IniConfig::Instance();
    ini.Load("III.GangTerritoryWars.ini");
    s_checkRadius          = ini.GetFloat("AmbientSpawning", "CheckRadius",            s_checkRadius);
    s_targetGangPeds       = ini.GetInt  ("AmbientSpawning", "TargetGangPeds",          s_targetGangPeds);
    s_hardCapGangPeds      = ini.GetInt  ("AmbientSpawning", "HardCapGangPeds",         s_hardCapGangPeds);
    s_spawnMinDist         = ini.GetFloat("AmbientSpawning", "SpawnMinDist",            s_spawnMinDist);
    s_spawnMaxDist         = ini.GetFloat("AmbientSpawning", "SpawnMaxDist",            s_spawnMaxDist);
    s_globalCooldownMs     = (unsigned int)ini.GetInt("AmbientSpawning", "GlobalCooldownMs",      (int)s_globalCooldownMs);
    s_perTerritoryCooldownMs = (unsigned int)ini.GetInt("AmbientSpawning", "PerTerritoryCooldownMs", (int)s_perTerritoryCooldownMs);

    DebugLog::Write("TerritoryAmbientSpawner initialized (checkR=%.1f cap=%d spawnDist=%.1f-%.1f cooldown=%ums)",
        s_checkRadius, s_hardCapGangPeds, s_spawnMinDist, s_spawnMaxDist, s_globalCooldownMs);
}

void TerritoryAmbientSpawner::Shutdown() {
    s_nextTerritoryActionMs.clear();
    DebugLog::Write("TerritoryAmbientSpawner shutdown");
}

void TerritoryAmbientSpawner::SetEnabled(bool enabled) { s_enabled = enabled; }
bool TerritoryAmbientSpawner::IsEnabled() { return s_enabled; }

void TerritoryAmbientSpawner::Update() {
    if (!s_enabled) return;

    const unsigned int now = CTimer::m_snTimeInMilliseconds;
    if (now < s_nextTickMs) return;
    s_nextTickMs = now + 250; // light polling, cheap

    // Don’t seed while war active — war system controls density & pacing
    if (WaveManager::IsWarActive()) return;

    if (!TerritorySystem::HasRealTerritories()) return;

    CPlayerPed* player = CWorld::Players[0].m_pPed;
    if (!player) return;

    // Don't seed ambient peds while player is driving — gang members appear on foot
    if (player->m_bInVehicle) return;

    const CVector playerPos = player->GetPosition();

    // Only seed when player is inside a territory
    const Territory* t = TerritorySystem::GetTerritoryAtPoint(playerPos);
    if (!t) return;

    const int ownerGang = t->ownerGang;
    if (!IsOwnerGangValid(ownerGang)) return;

    // Sync cooldown array to territories size
    const auto& terrs = TerritorySystem::GetTerritories();
    if (s_nextTerritoryActionMs.size() != terrs.size()) {
        s_nextTerritoryActionMs.assign(terrs.size(), 0);
    }

    // Find territory index (small N; fine)
    int terrIndex = -1;
    for (int i = 0; i < (int)terrs.size(); ++i) {
        if (&terrs[i] == t) { terrIndex = i; break; }
    }
    if (terrIndex < 0) return;

    // Rate limits
    if (now < s_nextGlobalActionMs) return;
    if (now < s_nextTerritoryActionMs[terrIndex]) return;

    // Density check
    const int nearby = CountNearbyOwnerGangPeds(playerPos, s_checkRadius, ownerGang);

    if (nearby >= s_hardCapGangPeds) {
        s_nextGlobalActionMs = now + s_noSpawnBackoffMs;
        s_nextTerritoryActionMs[terrIndex] = now + s_noSpawnBackoffMs;
        return;
    }

    if (nearby >= s_targetGangPeds) {
        // We’re “good enough”
        s_nextGlobalActionMs = now + s_noSpawnBackoffMs;
        s_nextTerritoryActionMs[terrIndex] = now + s_noSpawnBackoffMs;
        return;
    }

    // Spawn 1 ped (we intentionally keep it slow/stable)
    const ePedType ownerType = (ePedType)ownerGang;
    const int modelId = GangManager::GetRandomModelId(ownerType);
    if (modelId < 0) {
        s_nextGlobalActionMs = now + s_noSpawnBackoffMs;
        s_nextTerritoryActionMs[terrIndex] = now + s_noSpawnBackoffMs;
        return;
    }

    if (!EnsureModelLoaded(modelId)) {
        // Don’t block the frame; just try next time
        s_nextGlobalActionMs = now + 600;
        s_nextTerritoryActionMs[terrIndex] = now + 600;
        return;
    }

    CVector spawnPos;
    if (!FindSpawnPos(t, playerPos, spawnPos)) {
        s_nextGlobalActionMs = now + 600;
        s_nextTerritoryActionMs[terrIndex] = now + 600;
        return;
    }

    CPed* p = CPopulation::AddPed(ownerType, (unsigned)modelId, spawnPos);
    if (p) {
        // Mild nudge: make sure they don’t immediately despawn as “mission”
        // Leave createdBy as default so the engine can cull naturally.

        static unsigned int s_nextLogMs = 0;
        if (now >= s_nextLogMs) {
            s_nextLogMs = now + 2000;
            DebugLog::Write(
                "AmbientSpawn: terr=%s owner=%d nearby=%d -> spawned model=%d at (%.1f,%.1f,%.1f)",
                t->id.c_str(), ownerGang, nearby, modelId, spawnPos.x, spawnPos.y, spawnPos.z
            );
        }

        // Advance cooldowns
        s_nextGlobalActionMs = now + s_globalCooldownMs;
        s_nextTerritoryActionMs[terrIndex] = now + s_perTerritoryCooldownMs;
    } else {
        // If spawn failed, back off slightly
        s_nextGlobalActionMs = now + 700;
        s_nextTerritoryActionMs[terrIndex] = now + 700;
    }
}
