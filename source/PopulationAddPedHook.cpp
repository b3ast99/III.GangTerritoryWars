#include "PopulationAddPedHook.h"

#include "DebugLog.h"
#include "HookUtil.h"

#include "CTimer.h"
#include "TerritorySystem.h"
#include "GangInfo.h"
#include "CWorld.h"           // NEW: for FindObjectsInRange
#include "CPed.h"             // for CPed and m_ePedType
#include "CStreaming.h"
#include "CPopulation.h"

#include <Windows.h>
#include <cstdint>
#include <cmath>

// ------------------------------------------------------------
// Diagnostic globals
// ------------------------------------------------------------
extern "C" {
    volatile uint32_t g_PopAddPed_HitCount = 0;
    volatile uint32_t g_PopAddPed_GangHitCount = 0;
    volatile uint32_t g_PopAddPed_RewriteCount = 0;
    volatile uint32_t g_PopAddPed_CivRewriteCount = 0;     // NEW
    volatile uint32_t g_PopAddPed_SkippedDueToRate = 0;    // NEW
    volatile uint32_t g_PopAddPed_SkippedDueToDensity = 0; // NEW
    volatile uint32_t g_PopAddPed_LastPedType = 0;
    volatile uint32_t g_PopAddPed_LastModelArg = 0;
    volatile uint32_t g_PopAddPed_LastOwnerGang = 0;
}

// ------------------------------------------------------------
// Static state
// ------------------------------------------------------------
bool PopulationAddPedHook::s_installed = false;
bool PopulationAddPedHook::s_enabled = true;
uint32_t PopulationAddPedHook::s_hookedAddr = 0;
PopulationAddPedHook::AddPed_t PopulationAddPedHook::s_original = nullptr;

// ------------------------------------------------------------
// Configuration constants (tune these!)
// ------------------------------------------------------------
static constexpr float REWRITE_PROB_GANG = 0.70f;  // 70% chance to replace existing gang spawn
static constexpr float REWRITE_PROB_CIV = 0.25f;  // 25% chance to convert civilian
static constexpr float DENSITY_CHECK_RADIUS = 50.0f;  // meters
static constexpr int   MAX_GANG_IN_AREA = 5;     // don't spawn more if already this many owner gang nearby

static constexpr unsigned int AMBIENT_INJECT_INTERVAL_MS = 5000;
static constexpr float AMBIENT_INJECT_RADIUS_MIN = 20.0f;
static constexpr float AMBIENT_INJECT_RADIUS_MAX = 40.0f;
static constexpr float AMBIENT_INJECT_MAX_PLAYER_DIST = 120.0f;

static bool s_bypassRewriteForAmbientInject = false;

// Shared civilian model IDs list (see GangInfo.cpp)
static int GetRandomCivModel() {
    const std::vector<int>& civModels = GangManager::GetAmbientCivilianModelIds();
    if (civModels.empty()) return 30;
    return civModels[std::rand() % civModels.size()];
}

// IMPORTANT:
// We only rewrite when the game is *already* trying to spawn a gang model OR a civilian.
static inline bool IsGangModelIndex(unsigned int modelIdx)
{
    return GangManager::IsGangModelId((int)modelIdx);
}

static inline bool IsCivilianPedType(ePedType t)
{
    return (t == PEDTYPE_CIVMALE || t == PEDTYPE_CIVFEMALE);
}

static inline bool IsGangPedType(ePedType t)
{
    return (t >= PEDTYPE_GANG1 && t <= PEDTYPE_GANG9);
}

// In PopulationAddPedHook.cpp, add near the top (after includes)
static bool IsModelLoaded(int modelIndex) {
    if (modelIndex < 0 || modelIndex >= 20000) return false;  // safety

    CStreamingInfo& info = CStreaming::ms_aInfoForModel[modelIndex];

    // Model is considered loaded if:
    // - Load state == LOADSTATE_LOADED (1)
    // - And it has been requested at some point (flags or state indicate it's in memory)
    return (info.m_nLoadState == LOADSTATE_LOADED);
}

bool PopulationAddPedHook::IsInstalled() { return s_installed; }
void PopulationAddPedHook::SetEnabled(bool enabled) { s_enabled = enabled; }
bool PopulationAddPedHook::IsEnabled() { return s_enabled; }

void PopulationAddPedHook::Install()
{
    DebugLog::Write("=== Installing PopulationAddPedHook (CPopulation::AddPed) ===");

    if (s_installed) {
        DebugLog::Write("PopulationAddPedHook already installed at 0x%08X", s_hookedAddr);
        return;
    }

    const uint32_t addr_10 = 0x004F5280; // GTA III 1.0

    if (TryInstallAtAddress(addr_10)) {
        s_installed = true;
        s_hookedAddr = addr_10;
        DebugLog::Write("SUCCESS: PopulationAddPedHook installed at 0x%04X", addr_10);
    }
    else {
        DebugLog::Write("FAILED: Could not install PopulationAddPedHook");
    }
}

bool PopulationAddPedHook::TryInstallAtAddress(uint32_t addr)
{
    void* target = reinterpret_cast<void*>(addr);
    constexpr std::size_t kStolen = 7;  // your previous log showed 7 bytes stolen

    void* tramp = HookUtil::MakeTrampoline(target, kStolen);
    if (!tramp) {
        DebugLog::Write("Trampoline alloc failed at 0x%08X", addr);
        return false;
    }

    if (!HookUtil::WriteRelJmp(target, (void*)&AddPedHook)) {
        DebugLog::Write("WriteRelJmp failed at 0x%08X", addr);
        return false;
    }

    s_original = reinterpret_cast<AddPed_t>(tramp);
    DebugLog::Write("Trampoline at %p for 0x%08X (stolen=%zu)", tramp, addr, kStolen);
    return true;
}

CPed* __cdecl PopulationAddPedHook::AddPedHook(ePedType pedType, unsigned int modelIndexOrCopType, const CVector& coors)
{
    g_PopAddPed_HitCount++;
    g_PopAddPed_LastPedType = (uint32_t)pedType;
    g_PopAddPed_LastModelArg = (uint32_t)modelIndexOrCopType;

    if (s_bypassRewriteForAmbientInject) {
        return s_original ? s_original(pedType, modelIndexOrCopType, coors) : nullptr;
    }

    bool shouldOverride = false;
    bool wasCivilian = false;
    bool shouldDowngradeToCiv = false;  // NEW: for rate/density skips on gangs

    const Territory* t = nullptr;
    int ownerGang = -1;
    g_PopAddPed_LastOwnerGang = 0xFFFFFFFFu;

    if (s_enabled) {
        t = TerritorySystem::GetTerritoryAtPoint(coors);
        ownerGang = (t ? t->ownerGang : -1);
        if (ownerGang >= 0) {
            g_PopAddPed_LastOwnerGang = (uint32_t)ownerGang;
        }

        if (t && ownerGang >= (int)PEDTYPE_GANG1 && ownerGang <= (int)PEDTYPE_GANG9) {
            const ePedType targetType = (ePedType)ownerGang;

            // Case 1: Original spawn is a gang ped/model -> consider replacement
            if (IsGangPedType(pedType) || IsGangModelIndex(modelIndexOrCopType)) {
                g_PopAddPed_GangHitCount++;
                shouldOverride = true;
            }
            // Case 2: Original spawn is civilian -> lower chance to convert
            else if (IsCivilianPedType(pedType)) {
                if (plugin::RandomNumberInRange(0.0f, 1.0f) < REWRITE_PROB_CIV) {
                    shouldOverride = true;
                    wasCivilian = true;
                }
            }

            if (shouldOverride) {
                // Rate limiting: probabilistic skip for gang spawns -> downgrade to civ if skipped
                if (!wasCivilian && plugin::RandomNumberInRange(0.0f, 1.0f) > REWRITE_PROB_GANG) {
                    g_PopAddPed_SkippedDueToRate++;
                    shouldOverride = false;
                    shouldDowngradeToCiv = true;  // NEW
                    DebugLog::Write("AddPed: Rate-limited gang -> downgrading to civ (terr=%s)", t->id.c_str());
                }

                // Density check: don't overpopulate -> downgrade to civ if skipped
                if (shouldOverride || shouldDowngradeToCiv) {
                    CEntity* nearby[32]{};
                    short numNearby = 0;

                    CWorld::FindObjectsInRange(
                        coors,
                        DENSITY_CHECK_RADIUS,
                        true,               // b2D = true (ignore Z axis for faster 2D circle check)
                        &numNearby,         // output count
                        32,                 // maxCount
                        nearby,             // output array
                        false,              // buildings
                        false,              // vehicles
                        true,               // peds <- only want peds
                        false,              // objects
                        false               // dummies
                    );

                    int gangCount = 0;
                    for (short i = 0; i < numNearby; ++i) {
                        CEntity* ent = nearby[i];
                        if (ent && ent->m_nType == ENTITY_TYPE_PED) {  // safe type check
                            CPed* p = static_cast<CPed*>(ent);
                            if (p->m_ePedType == targetType) {
                                gangCount++;
                            }
                        }
                    }

                    if (gangCount >= MAX_GANG_IN_AREA) {
                        g_PopAddPed_SkippedDueToDensity++;
                        shouldOverride = false;
                        shouldDowngradeToCiv = true;  // NEW
                        DebugLog::Write("AddPed: Density skip -> downgrading to civ (%d/%d gangs in %.1fm, terr=%s)",
                            gangCount, MAX_GANG_IN_AREA, DENSITY_CHECK_RADIUS, t->id.c_str());
                    }
                }

                // Perform override if still valid
                if (shouldOverride) {
                    const int desiredModel = GangManager::GetRandomModelId(targetType);
                    if (desiredModel >= 0) {
                        const bool modelDiff = ((unsigned)desiredModel != modelIndexOrCopType);
                        const bool typeDiff = (pedType != targetType);

                        if (modelDiff || typeDiff) {
                            pedType = targetType;
                            modelIndexOrCopType = (unsigned)desiredModel;
                            g_PopAddPed_RewriteCount++;
                            if (wasCivilian) g_PopAddPed_CivRewriteCount++;

                            static unsigned int s_nextLogMs = 0;
                            const unsigned int now = CTimer::m_snTimeInMilliseconds;
                            if (now >= s_nextLogMs) {
                                s_nextLogMs = now + 1200;
                                DebugLog::Write(
                                    "AddPed REWRITE: terr=%s owner=%d pos(%.1f,%.1f,%.1f) -> type=%d model=%u (civ=%d)",
                                    t->id.c_str(), ownerGang, coors.x, coors.y, coors.z,
                                    (int)pedType, modelIndexOrCopType, (int)wasCivilian
                                );
                            }
                        }
                    }
                }
                // NEW: Downgrade wrong gang to random civ
                else if (shouldDowngradeToCiv && (IsGangPedType(pedType) || IsGangModelIndex(modelIndexOrCopType))) {
                    const int desiredCivModel = GetRandomCivModel();

                    // Only downgrade if the model is confirmed loaded
                    if (IsModelLoaded(desiredCivModel)) {
                        pedType = (std::rand() % 2 == 0) ? PEDTYPE_CIVMALE : PEDTYPE_CIVFEMALE;
                        modelIndexOrCopType = (unsigned)desiredCivModel;
                        g_PopAddPed_CivRewriteCount++;

                        DebugLog::Write(
                            "AddPed DOWNGRADE: terr=%s owner=%d pos(%.1f,%.1f,%.1f) -> civ type=%d model=%u",
                            t->id.c_str(), ownerGang, coors.x, coors.y, coors.z,
                            (int)pedType, modelIndexOrCopType
                        );
                    }
                    else {
                        DebugLog::Write("AddPed: Skipped downgrade - civ model %d not loaded (state=%d)",
                            desiredCivModel, CStreaming::ms_aInfoForModel[desiredCivModel].m_nLoadState);
                        // Optional: keep original gang ped or do nothing
                    }
                }
            }
        }
    }

    return s_original ? s_original(pedType, modelIndexOrCopType, coors) : nullptr;
}

static void TryAmbientInjectGangPed()
{
    static unsigned int s_nextInjectMs = 0;
    const unsigned int now = CTimer::m_snTimeInMilliseconds;
    if (now < s_nextInjectMs) return;
    s_nextInjectMs = now + AMBIENT_INJECT_INTERVAL_MS;

    CPlayerPed* player = CWorld::Players[0].m_pPed;
    if (!player) return;

    const CVector playerPos = player->GetPosition();
    const Territory* t = TerritorySystem::GetTerritoryAtPoint(playerPos);
    if (!t) return;

    const int ownerGang = t->ownerGang;
    if (ownerGang < (int)PEDTYPE_GANG1 || ownerGang > (int)PEDTYPE_GANG9) return;

    CEntity* nearby[32]{};
    short numNearby = 0;
    CWorld::FindObjectsInRange(
        playerPos,
        AMBIENT_INJECT_MAX_PLAYER_DIST,
        true,
        &numNearby,
        32,
        nearby,
        false,
        false,
        true,
        false,
        false
    );

    int ownerGangCount = 0;
    for (short i = 0; i < numNearby; ++i) {
        CEntity* ent = nearby[i];
        if (ent && ent->m_nType == ENTITY_TYPE_PED) {
            CPed* p = static_cast<CPed*>(ent);
            if (p->m_ePedType == (ePedType)ownerGang) {
                ownerGangCount++;
            }
        }
    }

    if (ownerGangCount >= MAX_GANG_IN_AREA) return;

    const float angle = plugin::RandomNumberInRange(0.0f, 6.283185307f);
    const float dist = plugin::RandomNumberInRange(AMBIENT_INJECT_RADIUS_MIN, AMBIENT_INJECT_RADIUS_MAX);

    CVector spawnPos = playerPos;
    spawnPos.x += std::cos(angle) * dist;
    spawnPos.y += std::sin(angle) * dist;

    if (spawnPos.x < t->minX) spawnPos.x = t->minX + 1.0f;
    if (spawnPos.x > t->maxX) spawnPos.x = t->maxX - 1.0f;
    if (spawnPos.y < t->minY) spawnPos.y = t->minY + 1.0f;
    if (spawnPos.y > t->maxY) spawnPos.y = t->maxY - 1.0f;

    bool foundGround = false;
    float gz = CWorld::FindGroundZFor3DCoord(spawnPos.x, spawnPos.y, playerPos.z + 50.0f, &foundGround);
    if (!foundGround) return;
    spawnPos.z = gz + 1.0f;

    const int modelId = GangManager::GetRandomModelId((ePedType)ownerGang);
    if (modelId < 0 || !IsModelLoaded(modelId)) return;

    s_bypassRewriteForAmbientInject = true;
    CPed* injected = CPopulation::AddPed((ePedType)ownerGang, (unsigned)modelId, spawnPos);
    s_bypassRewriteForAmbientInject = false;

    if (injected) {
        DebugLog::Write("AmbientInject: spawned gang ped terr=%s gang=%d model=%d at %.1f,%.1f,%.1f",
            t->id.c_str(), ownerGang, modelId, spawnPos.x, spawnPos.y, spawnPos.z);
    }
}

void PopulationAddPedHook::DebugTick()
{
    if (!s_installed) return;


    static unsigned int nextMs = 0;
    const unsigned int now = CTimer::m_snTimeInMilliseconds;
    if (now < nextMs) return;
    nextMs = now + 3000;

    DebugLog::Write("AddPed stats: hit=%u gang=%u rewrite=%u civRewrite=%u rateSkip=%u densitySkip=%u lastType=%u lastArg=%u lastOwner=%d",
        (unsigned)g_PopAddPed_HitCount,
        (unsigned)g_PopAddPed_GangHitCount,
        (unsigned)g_PopAddPed_RewriteCount,
        (unsigned)g_PopAddPed_CivRewriteCount,
        (unsigned)g_PopAddPed_SkippedDueToRate,
        (unsigned)g_PopAddPed_SkippedDueToDensity,
        (unsigned)g_PopAddPed_LastPedType,
        (unsigned)g_PopAddPed_LastModelArg,
        (int)g_PopAddPed_LastOwnerGang
    );
}