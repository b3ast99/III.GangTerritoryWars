// WaveConfig.h
#pragma once
#include "eWeaponType.h"
#include <vector>

namespace WaveConfig {
    struct WeaponOption {
        eWeaponType weapon;
        unsigned int ammo;
    };

    struct WaveSettings {
        int minCount;
        int maxCount;
        std::vector<WeaponOption> weapons;
    };

    enum DefenseLevel {
        DEFENSE_LIGHT = 0,
        DEFENSE_MODERATE = 1,
        DEFENSE_HEAVY = 2
    };

    void InitializeWaveConfigs(int defenseLevel = DEFENSE_MODERATE);
    const WaveSettings& GetWaveConfig(int waveIndex);
    WeaponOption ChooseRandomWeapon(int waveIndex);
}