#include "WaveManager.h"
#include "WaveConfig.h"
#include "WaveSpawning.h"
#include "WaveCombat.h"
#include "GangInfo.h"
#include "TerritorySystem.h"
#include "DebugLog.h"
#include "CMessages.h"
#include "CPickups.h"
#include "CTimer.h"
#include "CWorld.h"
#include "CPlayerPed.h"
#include "plugin.h"

#include <cmath>

// Message timing constants
static constexpr unsigned int WAVE_MESSAGE_DISPLAY_MS = 3000;
static constexpr unsigned int FLEE_MESSAGE_DISPLAY_MS = 3000;
static constexpr unsigned int DEATH_MESSAGE_DISPLAY_MS = 3000;
static constexpr unsigned int VICTORY_MESSAGE_DISPLAY_MS = 3000;
static constexpr unsigned int CHECK_INTERVAL_MS = 500;

// Static member definitions
WaveManager::WarState WaveManager::s_state = WaveManager::WarState::Idle;
int WaveManager::s_currentWave = -1;
int WaveManager::s_enemiesSpawned = 0;
int WaveManager::s_enemiesTarget = 0;
int WaveManager::s_originalWantedLevel = 0;
int WaveManager::s_originalChaosLevel = 0;
unsigned char WaveManager::s_originalWantedFlags = 0;
bool WaveManager::s_wantedLevelFrozen = false;
unsigned int WaveManager::s_nextActionTime = 0;
ePedType WaveManager::s_defendingGang = PEDTYPE_GANG1;
const Territory* WaveManager::s_activeTerritory = nullptr;
bool WaveManager::s_isShuttingDown = false;
unsigned int WaveManager::s_showWaveMessageAtTime = 0;
int WaveManager::s_pendingWaveMessage = -1;
CVector WaveManager::s_warCenter;
float WaveManager::s_warRadius = 0.0f;

// Clusters
std::vector<CVector> WaveManager::s_clusterCenters;
std::vector<int> WaveManager::s_clusterSizes;
size_t WaveManager::s_currentClusterIndex = 0;
unsigned int WaveManager::s_nextClusterSpawnTime = 0;
int WaveManager::s_enemiesSpawnedInWave = 0;

// Pickups
int WaveManager::s_healthPickupHandle = -1;
int WaveManager::s_armorPickupHandle = -1;
bool WaveManager::s_pickupsActive = false;
unsigned int WaveManager::s_pickupCleanupTime = 0;

void WaveManager::Initialize() {
    WaveConfig::InitializeWaveConfigs();
    WaveCombat::Initialize();

    s_state = WarState::Idle;
    s_activeTerritory = nullptr;
    s_currentWave = -1;
    s_enemiesSpawned = 0;
    s_enemiesTarget = 0;
    s_enemiesSpawnedInWave = 0;
    s_nextActionTime = 0;

    s_showWaveMessageAtTime = 0;
    s_pendingWaveMessage = -1;

    s_nextClusterSpawnTime = 0;
    s_currentClusterIndex = 0;
    s_clusterCenters.clear();
    s_clusterSizes.clear();

    s_healthPickupHandle = -1;
    s_armorPickupHandle = -1;
    s_pickupsActive = false;
    s_pickupCleanupTime = 0;

    s_originalChaosLevel = 0;
    s_originalWantedFlags = 0;
    s_originalWantedLevel = 0;
    s_wantedLevelFrozen = false;

    s_isShuttingDown = false;

    DebugLog::Write("WaveManager initialized");
}

bool WaveManager::IsWarActive() {
    return s_state != WarState::Idle && s_state != WarState::Completed;
}

int WaveManager::GetCurrentWaveIndex() {
    return s_currentWave;
}

int WaveManager::GetWaveTargetCount() {
    return s_enemiesTarget;
}

int WaveManager::GetWaveSpawnedSoFar() {
    return s_enemiesSpawned;
}

int WaveManager::GetAliveCount() {
    return WaveCombat::GetAliveCount();
}

const Territory* WaveManager::GetActiveTerritory() {
    return s_activeTerritory;
}

ePedType WaveManager::GetDefendingGang() {
    return s_defendingGang;
}

bool WaveManager::IsWantedLevelSuppressionNeeded() {
    return s_wantedLevelFrozen && IsWarActive();
}

void WaveManager::StartWar(ePedType defendingGang, const Territory* territory) {
    if (s_isShuttingDown || IsWarActive()) return;

    if (!territory) {
        DebugLog::Write("ERROR: StartWar called with null territory");
        return;
    }

    // If we were in post-war cleanup state from a previous war, nuke it now.
    CleanupWarPickups();        // clears handles, pickupCleanupTime, pickupsActive
    s_pickupCleanupTime = 0;

    s_defendingGang = defendingGang;
    s_activeTerritory = territory;
    s_currentWave = -1;

    // Ensure territory is marked as under attack
    TerritorySystem::SetUnderAttack(s_activeTerritory, true);
    s_warCenter.x = (s_activeTerritory->minX + s_activeTerritory->maxX) / 2.0f;
    s_warCenter.y = (s_activeTerritory->minY + s_activeTerritory->maxY) / 2.0f;
    s_warCenter.z = 0.0f;

    // Calculate radius as half the territory diagonal
    float width = s_activeTerritory->maxX - s_activeTerritory->minX;
    float height = s_activeTerritory->maxY - s_activeTerritory->minY;
    s_warRadius = sqrtf(width * width + height * height) / 2.0f;

    // Add buffer (SA seems to use generous bounds)
    s_warRadius *= s_fleeRadiusMultiplier;

    // NEW: Use territory defense level for wave config
    int defenseLevel = WaveConfig::DEFENSE_MODERATE; // Default
    if (s_activeTerritory) {
        // Clamp to valid range
        defenseLevel = s_activeTerritory->defenseLevel;
        if (defenseLevel < 0) defenseLevel = 0;
        if (defenseLevel > 2) defenseLevel = 2;
    }

    WaveConfig::InitializeWaveConfigs(defenseLevel);

    s_state = WarState::BetweenWaves;
    // Store original wanted state (level, flags, and chaos)
    CPlayerPed* player = CWorld::Players[0].m_pPed;
    if (player && player->m_pWanted) {
        s_originalWantedLevel = player->m_pWanted->m_nWantedLevel;
        s_originalWantedFlags = player->m_pWanted->m_nWantedFlags;
        s_originalChaosLevel = player->m_pWanted->m_nChaosLevel;
        s_wantedLevelFrozen = true;

        DebugLog::Write("War started - freezing wanted: level=%d, flags=0x%02X, chaos=%d",
            s_originalWantedLevel, s_originalWantedFlags, s_originalChaosLevel);
    }

    DebugLog::Write("War started against gang %d in territory %s (defense: %d)",
        (int)defendingGang, territory ? territory->id.c_str() : "unknown", defenseLevel);
}

void WaveManager::CancelWar() {
    // Clean up enemies and pickup spawns
    WaveCombat::CleanupAllEnemies(false);
    CleanupWarPickups();

    // Clean up under attack in all territories
    TerritorySystem::ClearAllWarsAndTransientState();

    // Reset underAttack flag
    if (s_activeTerritory) {
        TerritorySystem::SetUnderAttack(s_activeTerritory, false);
    }

    // Unfreeze wanted level
    s_wantedLevelFrozen = false;

    // Reset to idle
    s_state = WarState::Idle;
    s_activeTerritory = nullptr;
    s_currentWave = -1;
    s_enemiesSpawned = 0;
    s_enemiesTarget = 0;

    DebugLog::Write("War cancelled - wanted system unfrozen");
}

void WaveManager::CompleteWar() {
    if (s_isShuttingDown || s_state != WarState::VictoryDelay) {
        DebugLog::Write("CompleteWar called in wrong state: %d", (int)s_state);
        return;
    }

    DebugLog::Write("Completing war cleanup");

    // Capture territory
    if (s_activeTerritory) {
        int playerGang = TerritorySystem::GetPlayerGang();
        if (playerGang >= 0) {
            DebugLog::Write("Capturing territory %s for gang %d",
                s_activeTerritory->id.c_str(), playerGang);

            TerritorySystem::SetTerritoryOwner(s_activeTerritory, playerGang);
            TerritorySystem::SetUnderAttack(s_activeTerritory, false);
        }
    }

    // Cleanup enemies
    WaveCombat::CleanupAllEnemies(false);

    // Unfreeze wanted level
    s_wantedLevelFrozen = false;

    // Start 60-second pickup cleanup timer (SA: post-war only)
    const bool anyPickupStillExists =
        (ResolvePickup(s_healthPickupHandle) != nullptr) ||
        (ResolvePickup(s_armorPickupHandle) != nullptr);

    s_pickupsActive = anyPickupStillExists;

    if (anyPickupStillExists) {
        s_pickupCleanupTime = CTimer::m_snTimeInMilliseconds + 60000;
        DebugLog::Write("Post-war pickup despawn in 60 seconds");
    }
    else {
        s_pickupCleanupTime = 0;
    }


    // Reset all counters
    s_currentWave = -1;
    s_enemiesSpawnedInWave = 0;
    s_enemiesSpawned = 0;
    s_enemiesTarget = 0;
    s_clusterCenters.clear();
    s_clusterSizes.clear();
    s_currentClusterIndex = 0;

    // Set state to Completed
    s_state = WarState::Completed;
    s_activeTerritory = nullptr;

    DebugLog::Write("War cleanup complete - wanted system unfrozen");
}

void WaveManager::BeginWave(int waveIndex) {
    if (waveIndex < 0 || waveIndex >= s_maxWaves) {
        DebugLog::Write("ERROR: BeginWave called with invalid wave index: %d", waveIndex);
        return;
    }

    s_currentWave = waveIndex;
    const auto& config = WaveConfig::GetWaveConfig(waveIndex);

    // Adjust spawn count with randomness
    s_enemiesTarget = plugin::RandomNumberInRange(config.minCount, config.maxCount);
    if (s_enemiesTarget <= 0) s_enemiesTarget = config.minCount;

    // Bonus enemies for later waves
    if (waveIndex >= 1 && plugin::RandomNumberInRange(0.0f, 1.0f) < 0.3f) {
        s_enemiesTarget += 1;
    }

    if (waveIndex == 0) {
        SpawnInitialHealthPickup();
    }
    else if (waveIndex == 1 || waveIndex == 2) {
        SpawnWaveArmorPickup();
    }

    DebugLog::Write("Beginning wave %d - target %d enemies",
        waveIndex + 1, s_enemiesTarget);

    // Plan the wave (find cluster centers and sizes)
    auto plan = WaveSpawning::PlanWaveSpawn(
        s_defendingGang,
        s_activeTerritory,
        waveIndex,
        s_enemiesTarget
    );

    // Store cluster info for staggered spawning
    s_clusterCenters = plan.clusterCenters;
    s_clusterSizes = plan.clusterSizes;
    s_currentClusterIndex = 0;
    s_enemiesSpawnedInWave = 0;

    // Spawn first cluster immediately
    SpawnNextCluster();
}

void WaveManager::SpawnNextCluster() {
    if (s_currentClusterIndex >= s_clusterCenters.size()) {
        // All clusters spawned, enter combat state
        s_state = WarState::Combat;
        DebugLog::Write("All clusters spawned, wave %d combat begins", s_currentWave + 1);
        return;
    }

    // Spawn current cluster
    auto results = WaveSpawning::SpawnSingleClusterEnemies(
        s_defendingGang,
        s_activeTerritory,
        s_currentWave,
        s_clusterCenters[s_currentClusterIndex],
        s_clusterSizes[s_currentClusterIndex]
    );

    // Add spawned enemies to combat tracker
    for (const auto& spawn : results) {
        WaveCombat::AddEnemy(spawn.ped, s_defendingGang);
        s_enemiesSpawnedInWave++;
        s_enemiesSpawned++;
    }

    DebugLog::Write("Spawned cluster %d/%d with %d enemies",
        s_currentClusterIndex + 1, s_clusterCenters.size(), results.size());

    // Move to next cluster
    s_currentClusterIndex++;

    // Schedule next cluster if there are more
    if (s_currentClusterIndex < s_clusterCenters.size()) {
        s_state = WarState::Spawning;
        s_nextClusterSpawnTime = CTimer::m_snTimeInMilliseconds + s_clusterDelayMs;
        DebugLog::Write("Next cluster in %d ms...", s_clusterDelayMs);
    }
    else {
        // All clusters spawned, go to combat
        s_state = WarState::Combat;
    }
}

float WaveManager::Dist2D(const CVector& a, const CVector& b) {
    float dx = a.x - b.x;
    float dy = a.y - b.y;
    return sqrtf(dx * dx + dy * dy);
}

void WaveManager::CheckWaveCompletion() {
    // DEFENSIVE: Don't check if we shouldn't be checking
    if (s_state != WarState::Combat && s_state != WarState::Spawning) {
        DebugLog::Write("CheckWaveCompletion called in wrong state: %d", (int)s_state);
        return;
    }

    int alive = GetAliveCount();
    unsigned int now = CTimer::m_snTimeInMilliseconds;

    // Static variable to track last log time
    static unsigned int s_lastLogTime = 0;

    // Only log every 1000ms (1 second)
    if (now - s_lastLogTime >= 1000) {
        DebugLog::Write("[TIME: %u] CheckWaveCompletion: alive=%d, state=%d, currentWave=%d",
            now, alive, (int)s_state, s_currentWave);
        s_lastLogTime = now;
    }

    if (alive == 0) {
        // Store which wave just completed
        int completedWave = s_currentWave;

        // DEFENSIVE: Validate wave index
        if (completedWave < 0 || completedWave >= s_maxWaves) {
            DebugLog::Write("ERROR: Invalid wave index: %d", completedWave);
            CancelWar();
            return;
        }

        if (completedWave < s_maxWaves - 1) {
            // Normal wave completion (not the last wave)
            s_state = WarState::BetweenWaves;

            // Set the full wave delay for next wave spawn
            s_nextActionTime = now + s_waveDelayMs;

            // ONLY schedule message for completedWave (not the final one)
            ScheduleWaveCompletionMessage(completedWave, now);

            DebugLog::Write("[TIME: %u] Wave %d completed, next wave in %d ms, message in %d ms",
                now, completedWave + 1, s_waveDelayMs, s_waveCompletionMessageDelayMs);
        }
        else {
            // Final wave completed - DO NOT schedule any wave completion message!
            DebugLog::Write("[TIME: %u] FINAL WAVE %d completed, entering victory delay",
                now, completedWave + 1);

            // Clear any pending wave messages to be safe
            s_showWaveMessageAtTime = 0;
            s_pendingWaveMessage = -1;

            // Enter victory delay with proper delay
            s_state = WarState::VictoryDelay;
            s_nextActionTime = now + s_victoryDelayMs;
        }
    }
}

// Helper function to schedule wave completion messages
void WaveManager::ScheduleWaveCompletionMessage(int completedWave, unsigned int now) {
    // Don't schedule if this is invalid or if we're shutting down
    if (completedWave < 0 || s_isShuttingDown) {
        return;
    }

    // Only schedule for actual waves that should have messages
    // (e.g., wave 0 = "first wave", wave 1 = "second wave")
    static constexpr int MAX_WAVE_FOR_MESSAGE = 1; // Only waves 0 and 1 get messages
    if (completedWave <= MAX_WAVE_FOR_MESSAGE) {
        // Store which wave message to show and when
        s_showWaveMessageAtTime = now + s_waveCompletionMessageDelayMs;
        s_pendingWaveMessage = completedWave;

        DebugLog::Write("[TIME: %u] Scheduled wave %d message for time %u",
            now, completedWave + 1, s_showWaveMessageAtTime);
    }
    else {
        DebugLog::Write("[TIME: %u] No message scheduled for wave %d",
            now, completedWave + 1);
    }
}

// Function to actually show the wave completion message
void WaveManager::ShowWaveCompletionMessage(int waveIndex) {
    unsigned int now = CTimer::m_snTimeInMilliseconds;

    switch (waveIndex) {
    case 0:
        DebugLog::Write("[TIME: %u] Showing first wave completion message (delayed)", now);
        CMessages::AddMessageJumpQ("You survived the first wave!", WAVE_MESSAGE_DISPLAY_MS, 0);
        break;
    case 1:
        DebugLog::Write("[TIME: %u] Showing second wave completion message (delayed)", now);
        CMessages::AddMessageJumpQ("You survived the second wave!", WAVE_MESSAGE_DISPLAY_MS, 0);
        break;
        // Add more cases if you have more waves
    default:
        DebugLog::Write("[TIME: %u] Wave %d completed (no specific message)", now, waveIndex + 1);
        break;
    }
}

void WaveManager::FreezeWantedLevelDuringWar() {
    if (!s_wantedLevelFrozen) return;

    CPlayerPed* player = CWorld::Players[0].m_pPed;
    if (!player || !player->m_pWanted) {
        s_wantedLevelFrozen = false;
        return;
    }

    // Store current values for comparison
    int currentLevel = player->m_pWanted->m_nWantedLevel;
    unsigned char currentFlags = player->m_pWanted->m_nWantedFlags;
    int currentChaos = player->m_pWanted->m_nChaosLevel;

    // CONSTANTLY enforce all three wanted values
    bool changed = false;

    if (currentLevel != s_originalWantedLevel) {
        player->m_pWanted->m_nWantedLevel = s_originalWantedLevel;
        changed = true;
    }

    // For flags: preserve only the "searching" bit (0x01), clear other temporary states
    unsigned char targetFlags = s_originalWantedFlags & 0x01; // Keep only searching state
    if (currentFlags != targetFlags) {
        player->m_pWanted->m_nWantedFlags = targetFlags;
        changed = true;
    }

    if (currentChaos != s_originalChaosLevel) {
        player->m_pWanted->m_nChaosLevel = s_originalChaosLevel;
        changed = true;
    }

    // Optional: Log when we make changes (reduce spam)
    static unsigned int lastLogTime = 0;
    if (changed) {
        unsigned int now = CTimer::m_snTimeInMilliseconds;
        if (now - lastLogTime > 5000) { // Log every 5 seconds max
            DebugLog::Write("Wanted frozen: level=%d, flags=0x%02X->0x%02X, chaos=%d",
                s_originalWantedLevel, currentFlags, targetFlags, s_originalChaosLevel);
            lastLogTime = now;
        }
    }
}

CPickup* WaveManager::ResolvePickup(int handle) {
    if (handle < 0) return nullptr;

    int index = CPickups::GetActualPickupIndex(handle);
    if (index < 0 || index >= 336) return nullptr;

    CPickup& p = CPickups::aPickUps[index];
    if (p.m_nPickupType == PICKUP_NONE) return nullptr;

    return &p;
}

void WaveManager::SpawnInitialHealthPickup() {
    if (!s_activeTerritory || s_isShuttingDown) return;

    // Wave 1 start in SA: health only.
    CleanupPickup(s_healthPickupHandle);
    CleanupPickup(s_armorPickupHandle);

    CVector spawnPos = FindPickupPositionInTerritory(s_activeTerritory, nullptr);

    if (spawnPos.x != 0.0f || spawnPos.y != 0.0f) {
        s_healthPickupHandle = SpawnPickupAtPosition_Handle(spawnPos, PICKUP_ONCE, 1362, 50);
        if (s_healthPickupHandle != -1) {
            DebugLog::Write("Initial health pickup spawned at %.1f, %.1f, %.1f",
                spawnPos.x, spawnPos.y, spawnPos.z);
            s_pickupsActive = true;
        }
    }
}

void WaveManager::SpawnWaveArmorPickup() {
    if (!s_activeTerritory || s_isShuttingDown) return;

    // Wave 2 start in SA: armor only; previous wave’s pickup should be gone by now.
    CleanupPickup(s_healthPickupHandle);
    CleanupPickup(s_armorPickupHandle);

    // Avoid spawning right on top of the player’s last health pickup location doesn’t matter now,
    // but we can still avoid a dummy position if you want. For now, just use nullptr.
    CVector spawnPos = FindPickupPositionInTerritory(s_activeTerritory, nullptr);

    if (spawnPos.x != 0.0f || spawnPos.y != 0.0f) {
        s_armorPickupHandle = SpawnPickupAtPosition_Handle(spawnPos, PICKUP_ONCE, 1364, 50);
        if (s_armorPickupHandle != -1) {
            DebugLog::Write("Armor pickup spawned at %.1f, %.1f, %.1f",
                spawnPos.x, spawnPos.y, spawnPos.z);
            s_pickupsActive = true;
        }
    }
}


CVector WaveManager::FindPickupPositionInTerritory(const Territory* territory, CPickup* avoidPickup) {
    if (!territory) return CVector(0, 0, 0);

    CPlayerPed* player = CWorld::Players[0].m_pPed;
    if (!player) return CVector(0, 0, 0);

    CVector playerPos = player->GetPosition();
    CVector avoidPos = avoidPickup ? avoidPickup->m_vecPos : CVector(0, 0, 0);

    DebugLog::Write("FindPickup: player at (%.1f, %.1f), territory bounds (%.1f-%.1f, %.1f-%.1f)",
        playerPos.x, playerPos.y,
        territory->minX, territory->maxX, territory->minY, territory->maxY);

    // SA: Try 20 random positions around player (8-20m radius)
    // NO COLLISION CHECK - SA skips this in dense areas
    for (int attempt = 0; attempt < 20; attempt++) {
        float angle = plugin::RandomNumberInRange(0.0f, 6.283185f);
        float distance = plugin::RandomNumberInRange(8.0f, 20.0f);

        CVector candidate;
        candidate.x = playerPos.x + cosf(angle) * distance;
        candidate.y = playerPos.y + sinf(angle) * distance;
        candidate.z = playerPos.z;

        // Must be inside territory bounds
        if (candidate.x < territory->minX || candidate.x > territory->maxX ||
            candidate.y < territory->minY || candidate.y > territory->maxY) {
            continue;
        }

        // Avoid other pickup (8m minimum)
        if (avoidPickup) {
            float distToAvoid = Dist2D(candidate, avoidPos);
            if (distToAvoid < 8.0f) {
                continue;
            }
        }

        // Find ground Z
        float groundZ;
        if (!WaveSpawning::FindGroundZForCoord(candidate.x, candidate.y, candidate.z + 50.0f, groundZ)) {
            continue;
        }
        candidate.z = groundZ + 0.5f;

        // NO COLLISION CHECK - just accept the position
        DebugLog::Write("  SUCCESS at attempt %d: (%.1f, %.1f, %.1f)",
            attempt, candidate.x, candidate.y, candidate.z);
        return candidate;
    }

    // FALLBACK 1: Territory center
    DebugLog::Write("All 20 attempts failed, using territory center fallback");
    CVector center;
    center.x = (territory->minX + territory->maxX) * 0.5f;
    center.y = (territory->minY + territory->maxY) * 0.5f;
    center.z = 100.0f;

    float groundZ;
    if (WaveSpawning::FindGroundZForCoord(center.x, center.y, center.z, groundZ)) {
        center.z = groundZ + 0.5f;
        return center;
    }

    // FALLBACK 2: Near player with random offset
    DebugLog::Write("Territory center fallback failed, using near-player fallback");
    CVector nearPlayer = playerPos;
    nearPlayer.x += plugin::RandomNumberInRange(-15.0f, 15.0f);
    nearPlayer.y += plugin::RandomNumberInRange(-15.0f, 15.0f);

    if (WaveSpawning::FindGroundZForCoord(nearPlayer.x, nearPlayer.y, nearPlayer.z + 50.0f, groundZ)) {
        nearPlayer.z = groundZ + 0.5f;
        return nearPlayer;
    }

    // FALLBACK 3: Player position
    DebugLog::Write("WARNING: All pickup position attempts failed, using player position");
    return playerPos;
}

// Spawn a single pickup
int WaveManager::SpawnPickupAtPosition_Handle(const CVector& pos, int pickupType, int modelId, int quantity) {
    int handle = CPickups::GenerateNewOne(pos, modelId, pickupType, quantity);
    if (handle == -1) return -1;

    if (CPickup* p = ResolvePickup(handle)) {
        p->m_nQuantity = quantity;
    }
    return handle;
}

// Cleanup all pickups
void WaveManager::CleanupWarPickups() {
    const bool hadAny =
        (ResolvePickup(s_healthPickupHandle) != nullptr) ||
        (ResolvePickup(s_armorPickupHandle) != nullptr);

    CleanupPickup(s_healthPickupHandle);
    CleanupPickup(s_armorPickupHandle);

    s_pickupsActive = false;
    s_pickupCleanupTime = 0;

    if (hadAny) { DebugLog::Write("All war pickups cleaned up"); }
}

// Remove a single pickup
void WaveManager::CleanupPickup(int& handle) {
    if (handle < 0) {
        handle = -1;
        return;
    }

    CPickup* p = ResolvePickup(handle);
    if (!p) {
        handle = -1;
        return;
    }

    p->m_bRemoved = true;
    p->m_nPickupType = PICKUP_NONE;

    if (p->m_pObject) {
        CWorld::Remove(p->m_pObject);
        p->m_pObject = nullptr;
    }

    handle = -1;
}


// Update pickup cleanup timer
void WaveManager::UpdatePickupCleanup() {
    // Runs only when a post-war despawn timer is armed.
    if (!s_pickupsActive || s_pickupCleanupTime == 0) return;

    unsigned int now = CTimer::m_snTimeInMilliseconds;
    if (now >= s_pickupCleanupTime) {
        DebugLog::Write("Post-war pickup timer elapsed - removing war pickups");
        CleanupWarPickups();
    }
}

void WaveManager::CheckPlayerDeath() {
    CPlayerPed* player = CWorld::Players[0].m_pPed;
    if (!player) return;

    // Check health and ped state using m_ePedState
    if (player->m_fHealth <= 0.0f || player->m_ePedState == PEDSTATE_DEAD || player->m_ePedState == PEDSTATE_DIE) {
        // Player died during war
        DebugLog::Write("Player died during gang war - territory goes neutral");

        // Show message
        CMessages::AddMessageJumpQ("You died during the gang war!", DEATH_MESSAGE_DISPLAY_MS, 0);

        // Set territory to neutral (-1)
        if (s_activeTerritory) {
            TerritorySystem::SetTerritoryOwner(s_activeTerritory, -1);
            TerritorySystem::SetUnderAttack(s_activeTerritory, false);
        }

        // Clean up enemies and pickup spawns
        WaveCombat::CleanupAllEnemies(false);
        CleanupWarPickups();

        // Reset everything
        s_state = WarState::Idle;
        s_activeTerritory = nullptr;
        s_wantedLevelFrozen = false;
        s_originalWantedLevel = 0;
        s_originalChaosLevel = 0;
        s_originalWantedFlags = 0;

        DebugLog::Write("War ended due to player death");
    }
}

void WaveManager::CheckForFleeing() {
    if (!s_activeTerritory) return;
    if (s_state == WarState::Idle || s_state == WarState::Completed) return;

    CPlayerPed* player = CWorld::Players[0].m_pPed;
    if (!player) return;

    CVector playerPos = player->GetPosition();
    float distance = Dist2D(playerPos, s_warCenter);

    static unsigned int s_fleeMessageShownTime = 0;
    static bool s_fleeMessageShown = false;
    const unsigned int now = CTimer::m_snTimeInMilliseconds;

    // Simple SA-style check: if player is outside radius, they fled
    if (distance > s_warRadius) {
        if (!s_fleeMessageShown) {
            CMessages::AddMessageJumpQ("You fled the gang war!", FLEE_MESSAGE_DISPLAY_MS, 0);
            s_fleeMessageShown = true;
            s_fleeMessageShownTime = now;
        }

        // Cancel war 1 second after showing message
        if (now - s_fleeMessageShownTime >= 1000) {
            CancelWar();
            s_fleeMessageShown = false; // Reset for next war
        }
    }
    else {
        s_fleeMessageShown = false; // Reset if player comes back
    }
}

void WaveManager::Update() {
    if (s_isShuttingDown) return;

    // Always service pickup cleanup timer (even if Idle/Completed)
    UpdatePickupCleanup();

    if (s_state == WarState::Idle) return;

    unsigned int now = CTimer::m_snTimeInMilliseconds;

    // If we're Completed, nothing else should run
    if (s_state == WarState::Completed) return;

    // Check player death every second
    static unsigned int s_lastDeathCheckTime = 0;
    if (now - s_lastDeathCheckTime >= 1000) {
        CheckPlayerDeath();
        s_lastDeathCheckTime = now;
    }

    // Check fleeing every 500ms
    static unsigned int s_lastFleeCheckTime = 0;
    if (now - s_lastFleeCheckTime >= s_fleeCheckIntervalMs) {
        CheckForFleeing();
        s_lastFleeCheckTime = now;
    }

    // Check for pending wave completion messages
    if (s_showWaveMessageAtTime > 0 && now >= s_showWaveMessageAtTime) {
        ShowWaveCompletionMessage(s_pendingWaveMessage);
        s_showWaveMessageAtTime = 0;
        s_pendingWaveMessage = -1;
    }

    // Freeze wanted level during war
    if (s_wantedLevelFrozen) {
        FreezeWantedLevelDuringWar();
    }

    // Update combat system (cleans up dead enemies)
    WaveCombat::Update(now);

    // Get player pointer once at the beginning
    CPlayerPed* player = CWorld::Players[0].m_pPed;

    // State machine
    switch (s_state) {
    case WarState::Spawning:
        // Wait for next cluster spawn time
        if (now >= s_nextClusterSpawnTime) {
            SpawnNextCluster();
        }
        break;

    case WarState::Combat:
        CheckWaveCompletion();
        // Reassert aggression during combat
        if (player) {
            WaveCombat::ReassertAggro(player);
        }
        break;

    case WarState::BetweenWaves:
        if (now >= s_nextActionTime) {
            if (s_currentWave < 0) {
                BeginWave(0);
            }
            else {
                BeginWave(s_currentWave + 1);
            }
        }
        break;

    case WarState::VictoryDelay:
        if (now >= s_nextActionTime) {
            const char* victoryMsg = "     This hood is yours!     ";
            CMessages::AddMessageJumpQ(victoryMsg, VICTORY_MESSAGE_DISPLAY_MS, 0);
            DebugLog::Write("[TIME: %u] Showing victory message: %s", now, victoryMsg);

            CompleteWar();
            return;
        }
        break;

    default:
        break;
    }
}

void WaveManager::Shutdown() {
    DebugLog::Write("WaveManager shutdown - cleaning up enemies");

    s_isShuttingDown = true;
    WaveCombat::Shutdown();
    CleanupWarPickups();

    // Unfreeze wanted level
    s_wantedLevelFrozen = false;

    s_state = WarState::Idle;
    s_activeTerritory = nullptr;
    s_defendingGang = PEDTYPE_GANG1;
    s_currentWave = -1;
    s_enemiesSpawned = 0;
    s_enemiesTarget = 0;

    DebugLog::Write("WaveManager shutdown complete");
}