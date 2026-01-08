// WaveConfig.cpp - Clean version with proper percentages
#include "WaveConfig.h"
#include "plugin.h"
#include <algorithm>

namespace WaveConfig {
    static WaveSettings s_waveConfigs[3];

    static float RandRange(float a, float b) {
        return plugin::RandomNumberInRange(a, b);
    }

    void InitializeWaveConfigs(int defenseLevel) {
        defenseLevel = std::clamp(defenseLevel, 0, 2);

        // Weapon Table adapted to III from SA Wiki - https://gta.fandom.com/wiki/Gang_Warfare_in_GTA_San_Andreas

        // Wave 1
        if (defenseLevel == DEFENSE_LIGHT) {
            // Lightly Defended: Baseball bats, Pistols (50/50)
            s_waveConfigs[0] = { 4, 6, {
                {WEAPONTYPE_BASEBALLBAT, 1},
                {WEAPONTYPE_COLT45, 60}
            } };
        }
        else if (defenseLevel == DEFENSE_MODERATE) {
            // Moderately Defended: Pistols, Micro SMGs (UZI) (50/50)
            s_waveConfigs[0] = { 5, 7, {
                {WEAPONTYPE_COLT45, 60},
                {WEAPONTYPE_UZI, 90}
            } };
        }
        else { // DEFENSE_HEAVY
            // Heavily Defended: Micro SMGs (UZI) only (100%)
            s_waveConfigs[0] = { 6, 8, {
                {WEAPONTYPE_UZI, 90}
            } };
        }

        // Wave 2
        if (defenseLevel == DEFENSE_LIGHT) {
            // Lightly Defended: Pistols, Micro SMGs (UZI) (50/50)
            s_waveConfigs[1] = { 5, 7, {
                {WEAPONTYPE_COLT45, 80},
                {WEAPONTYPE_UZI, 120}
            } };
        }
        else if (defenseLevel == DEFENSE_MODERATE) {
            // Moderately Defended: Micro SMGs (UZI) only (100%)
            s_waveConfigs[1] = { 6, 8, {
                {WEAPONTYPE_UZI, 120}
            } };
        }
        else { // DEFENSE_HEAVY
            // Heavily Defended: SMGs (UZI), AK-47s (50/50)
            s_waveConfigs[1] = { 7, 9, {
                {WEAPONTYPE_UZI, 150},
                {WEAPONTYPE_AK47, 180}
            } };
        }

        // Wave 3
        if (defenseLevel == DEFENSE_LIGHT) {
            // Lightly Defended: Micro SMGs (UZI) only (100%)
            s_waveConfigs[2] = { 6, 8, {
                {WEAPONTYPE_UZI, 150}
            } };
        }
        else if (defenseLevel == DEFENSE_MODERATE) {
            // Moderately Defended: SMGs (UZI), AK-47s (50/50)
            s_waveConfigs[2] = { 7, 9, {
                {WEAPONTYPE_UZI, 180},
                {WEAPONTYPE_AK47, 200}
            } };
        }
        else { // DEFENSE_HEAVY
            // Heavily Defended: All AK-47s (100%)
            s_waveConfigs[2] = { 8, 10, {
                {WEAPONTYPE_AK47, 200}
            } };
        }
    }

    const WaveSettings& GetWaveConfig(int waveIndex) {
        if (waveIndex < 0 || waveIndex >= 3) {
            static WaveSettings defaultConfig = { 2, 4, {{WEAPONTYPE_COLT45, 999999}} };
            return defaultConfig;
        }
        return s_waveConfigs[waveIndex];
    }

    WeaponOption ChooseRandomWeapon(int waveIndex) {
        const auto& config = GetWaveConfig(waveIndex);
        if (config.weapons.empty()) {
            return { WEAPONTYPE_COLT45, 999999 };
        }

        // Simple random selection from available weapons
        // Each weapon in the list has equal probability
        size_t idx = static_cast<size_t>(RandRange(0.0f, static_cast<float>(config.weapons.size())));
        if (idx >= config.weapons.size()) idx = config.weapons.size() - 1;

        return config.weapons[idx];
    }
}