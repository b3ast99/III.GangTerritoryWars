#include "plugin.h"
#include "GangInfo.h"
#include "CMessages.h"
#include "DebugLog.h"
#include "WaveManager.h"
#include "TerritorySystem.h"
#include "WarSystem.h"
#include "CRadar.h"
#include "CWorld.h"
#include "CModelInfo.h"
#include "CBaseModelInfo.h"
#include "PedDeathTracker.h"
#include "DamageHook.h"
#include "DirectDamageTracker.h"
#include "TerritoryPersistence.h"
#include "PopulationAddPedHook.h"
#include "GangVehicleModelHook.h"
#include "TerritoryAmbientSpawner.h"
#include "CStreaming.h"

#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <vector>

static bool g_isTearingDown = false;
using namespace plugin;

static bool JustPressed(int vk)
{
    return (GetAsyncKeyState(vk) & 1) != 0;
}

static int ResolveModelId(const char* modelName)
{
    if (!modelName || !modelName[0]) {
        CMessages::AddMessageJumpQ("Model: No name", 1500, 0);
        return -1;
    }

    CMessages::AddMessageJumpQ("Model: Looking up...", 1500, 0);

    int index = -1;
    CBaseModelInfo* mi = CModelInfo::GetModelInfo(modelName, &index);

    if (index >= 0) {
        CMessages::AddMessageJumpQ("Model: Found", 1500, 0);
    }
    else {
        CMessages::AddMessageJumpQ("Model: NOT found", 1500, 0);
    }

    return index;
}

class GangTerritoryWarsMain
{
public:
    GangTerritoryWarsMain()
    {
        Events::initRwEvent += [] {
            DebugLog::Initialize("III.GangTerritoryWars.log");
            // Seed rand once at init
            std::srand(static_cast<unsigned int>(std::time(nullptr)));
            GangManager::Initialize();
            WaveManager::Initialize();
            TerritorySystem::Init();
            TerritoryPersistence::Init();
            WarSystem::Init();

            PopulationAddPedHook::Install();
            GangVehicleModelHook::Install();
            TerritoryAmbientSpawner::Init();

            DirectDamageTracker::Initialize();
            PedDeathTracker::Initialize();
            DamageHook::Install();

            DebugLog::Write("GangTerritoryWars loaded");
            };

        Events::d3dLostEvent += [] {
            DebugLog::Write("Direct3D device lost");
            };

        Events::d3dResetEvent += [] {
            DebugLog::Write("Direct3D device reset");
            };

        Events::shutdownRwEvent += [] {
            DebugLog::Write("Plugin shutdown triggered via shutdownRwEvent");
            g_isTearingDown = true;

            TerritoryAmbientSpawner::Shutdown();
            TerritoryPersistence::Shutdown();
            TerritorySystem::Shutdown();
            PedDeathTracker::Shutdown();
            DirectDamageTracker::Shutdown();
            WaveManager::Shutdown();
            DebugLog::Shutdown();
            };

        Events::gameProcessEvent += [] {
            if (g_isTearingDown) return;

            // One-time model preloading on first game tick
            static bool s_modelsPreloaded = false;
            if (!s_modelsPreloaded) {
                DebugLog::Write("Starting one-time model preload (first tick)...");

                // Gang ped + vehicle models
                for (int i = 0; i < 3; ++i) {
                    const GangInfo& gang = GangManager::s_gangs[i];
                    for (int modelId : gang.modelIds) {
                        if (modelId >= 0 && CModelInfo::GetModelInfo(modelId)) {
                            CStreaming::RequestModel(modelId, GAME_REQUIRED | KEEP_IN_MEMORY);
                            DebugLog::Write("Preloaded gang ped model: %d (gang %d)", modelId, i);
                        }
                    }
                    for (int vehicleModelId : gang.vehicleModelIds) {
                        if (vehicleModelId >= 0 && CModelInfo::GetModelInfo(vehicleModelId)) {
                            CStreaming::RequestModel(vehicleModelId, GAME_REQUIRED | KEEP_IN_MEMORY);
                            DebugLog::Write("Preloaded gang vehicle model: %d (gang %d)", vehicleModelId, i);
                        }
                    }
                }

                // Civilian models used by ambient downgrade logic
                const std::vector<int>& civModels = GangManager::GetAmbientCivilianModelIds();

                for (int mid : civModels) {
                    if (mid >= 0 && CModelInfo::GetModelInfo(mid)) {
                        CStreaming::RequestModel(mid, GAME_REQUIRED | KEEP_IN_MEMORY);
                        DebugLog::Write("Preloaded civ model: %d", mid);
                    }
                }

                CStreaming::LoadAllRequestedModels(false);  // Non-blocking
                DebugLog::Write("Model preload complete (first tick)");

                s_modelsPreloaded = true;
            }

            GangManager::TryLateResolveModels();
            PopulationAddPedHook::DebugTick();
            TerritoryAmbientSpawner::Process();
            TerritorySystem::Process();
            TerritoryPersistence::Process();
            WarSystem::Process();
            WaveManager::Process();

            DirectDamageTracker::Process();
            PedDeathTracker::Process();

            // Existing hotkeys you already had
            if (JustPressed(VK_F8)) {
                WaveManager::CancelWar();
                CMessages::AddMessageJumpQ("Gang war cancelled", 1400, 0);
            }
            if (JustPressed(VK_F6)) {
                TerritorySystem::ToggleOverlay();
            }
            if (JustPressed(VK_F9)) {
                CPlayerPed* player = CWorld::Players[0].m_pPed;
                if (player) {
                    const CVector pos = player->GetPosition();
                    char msg[64];
                    sprintf(msg, "Player Pos: %.1f %.1f %.1f", pos.x, pos.y, pos.z);
                    CMessages::AddMessageJumpQ(msg, 5000, 0);
                }
            }

            // Territory editor (NUMPAD)
            if (JustPressed(VK_NUMPAD0)) TerritorySystem::EditorToggle();
            if (JustPressed(VK_NUMPAD7)) TerritorySystem::EditorSetCornerAAtPlayer();
            if (JustPressed(VK_NUMPAD9)) TerritorySystem::EditorSetCornerBAtPlayer();
            if (JustPressed(VK_NUMPAD5)) TerritorySystem::EditorCommitTerritory();
            if (JustPressed(VK_NUMPAD8)) TerritorySystem::EditorDeleteClosestToPlayer();
            if (JustPressed(VK_NUMPAD1)) TerritorySystem::ForceReloadNow();
            if (JustPressed(VK_NUMPAD2)) TerritorySystem::ToggleOverlay();
            };

        Events::drawRadarMapEvent += []() {
            if (g_isTearingDown) return;
            TerritorySystem::DrawRadarOverlay();
            };
    }
} gangTerritoryWarsMain;