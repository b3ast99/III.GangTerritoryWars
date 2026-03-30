#include "TestFramework.h"
#include "../source/WaveConfig.h"

void RunWaveConfigTests(Test::Runner& t) {
    t.suite("WaveConfig");

    // ------------------------------------------------------------------
    // Enemy counts per defense level — all three waves
    // ------------------------------------------------------------------
    t.run("light: wave 0 enemy range is 4-6", [&] {
        WaveConfig::InitializeWaveConfigs(WaveConfig::DEFENSE_LIGHT);
        const auto& w = WaveConfig::GetWaveConfig(0);
        REQUIRE_EQ(w.minCount, 4);
        REQUIRE_EQ(w.maxCount, 6);
    });

    t.run("light: wave 1 enemy range is 5-7", [&] {
        WaveConfig::InitializeWaveConfigs(WaveConfig::DEFENSE_LIGHT);
        const auto& w = WaveConfig::GetWaveConfig(1);
        REQUIRE_EQ(w.minCount, 5);
        REQUIRE_EQ(w.maxCount, 7);
    });

    t.run("light: wave 2 enemy range is 6-8", [&] {
        WaveConfig::InitializeWaveConfigs(WaveConfig::DEFENSE_LIGHT);
        const auto& w = WaveConfig::GetWaveConfig(2);
        REQUIRE_EQ(w.minCount, 6);
        REQUIRE_EQ(w.maxCount, 8);
    });

    t.run("moderate: wave 0 enemy range is 5-7", [&] {
        WaveConfig::InitializeWaveConfigs(WaveConfig::DEFENSE_MODERATE);
        const auto& w = WaveConfig::GetWaveConfig(0);
        REQUIRE_EQ(w.minCount, 5);
        REQUIRE_EQ(w.maxCount, 7);
    });

    t.run("moderate: wave 1 enemy range is 6-8", [&] {
        WaveConfig::InitializeWaveConfigs(WaveConfig::DEFENSE_MODERATE);
        const auto& w = WaveConfig::GetWaveConfig(1);
        REQUIRE_EQ(w.minCount, 6);
        REQUIRE_EQ(w.maxCount, 8);
    });

    t.run("moderate: wave 2 enemy range is 7-9", [&] {
        WaveConfig::InitializeWaveConfigs(WaveConfig::DEFENSE_MODERATE);
        const auto& w = WaveConfig::GetWaveConfig(2);
        REQUIRE_EQ(w.minCount, 7);
        REQUIRE_EQ(w.maxCount, 9);
    });

    t.run("heavy: wave 0 enemy range is 6-8", [&] {
        WaveConfig::InitializeWaveConfigs(WaveConfig::DEFENSE_HEAVY);
        const auto& w = WaveConfig::GetWaveConfig(0);
        REQUIRE_EQ(w.minCount, 6);
        REQUIRE_EQ(w.maxCount, 8);
    });

    t.run("heavy: wave 1 enemy range is 7-9", [&] {
        WaveConfig::InitializeWaveConfigs(WaveConfig::DEFENSE_HEAVY);
        const auto& w = WaveConfig::GetWaveConfig(1);
        REQUIRE_EQ(w.minCount, 7);
        REQUIRE_EQ(w.maxCount, 9);
    });

    t.run("heavy: wave 2 enemy range is 8-10", [&] {
        WaveConfig::InitializeWaveConfigs(WaveConfig::DEFENSE_HEAVY);
        const auto& w = WaveConfig::GetWaveConfig(2);
        REQUIRE_EQ(w.minCount, 8);
        REQUIRE_EQ(w.maxCount, 10);
    });

    // ------------------------------------------------------------------
    // Enemy counts escalate across waves within each defense level
    // ------------------------------------------------------------------
    t.run("escalation: each wave has more min enemies than the last (moderate)", [&] {
        WaveConfig::InitializeWaveConfigs(WaveConfig::DEFENSE_MODERATE);
        REQUIRE(WaveConfig::GetWaveConfig(1).minCount > WaveConfig::GetWaveConfig(0).minCount);
        REQUIRE(WaveConfig::GetWaveConfig(2).minCount > WaveConfig::GetWaveConfig(1).minCount);
    });

    t.run("escalation: each wave has more max enemies than the last (heavy)", [&] {
        WaveConfig::InitializeWaveConfigs(WaveConfig::DEFENSE_HEAVY);
        REQUIRE(WaveConfig::GetWaveConfig(1).maxCount > WaveConfig::GetWaveConfig(0).maxCount);
        REQUIRE(WaveConfig::GetWaveConfig(2).maxCount > WaveConfig::GetWaveConfig(1).maxCount);
    });

    t.run("escalation: higher defense level → more enemies at each wave", [&] {
        WaveConfig::InitializeWaveConfigs(WaveConfig::DEFENSE_LIGHT);
        int lightMin0 = WaveConfig::GetWaveConfig(0).minCount;
        WaveConfig::InitializeWaveConfigs(WaveConfig::DEFENSE_MODERATE);
        int modMin0 = WaveConfig::GetWaveConfig(0).minCount;
        WaveConfig::InitializeWaveConfigs(WaveConfig::DEFENSE_HEAVY);
        int heavyMin0 = WaveConfig::GetWaveConfig(0).minCount;
        REQUIRE(modMin0 > lightMin0);
        REQUIRE(heavyMin0 > modMin0);
    });

    // ------------------------------------------------------------------
    // Weapon loadouts — count and escalation
    // ------------------------------------------------------------------
    t.run("light: wave 0 has 2 weapon options (bat + pistol)", [&] {
        WaveConfig::InitializeWaveConfigs(WaveConfig::DEFENSE_LIGHT);
        REQUIRE_EQ((int)WaveConfig::GetWaveConfig(0).weapons.size(), 2);
        REQUIRE_EQ(WaveConfig::GetWaveConfig(0).weapons[0].weapon, WEAPONTYPE_BASEBALLBAT);
        REQUIRE_EQ(WaveConfig::GetWaveConfig(0).weapons[1].weapon, WEAPONTYPE_COLT45);
    });

    t.run("heavy: wave 0 uses only UZI (1 option)", [&] {
        WaveConfig::InitializeWaveConfigs(WaveConfig::DEFENSE_HEAVY);
        REQUIRE_EQ((int)WaveConfig::GetWaveConfig(0).weapons.size(), 1);
        REQUIRE_EQ(WaveConfig::GetWaveConfig(0).weapons[0].weapon, WEAPONTYPE_UZI);
    });

    t.run("heavy: wave 2 uses only AK47 (1 option)", [&] {
        WaveConfig::InitializeWaveConfigs(WaveConfig::DEFENSE_HEAVY);
        REQUIRE_EQ((int)WaveConfig::GetWaveConfig(2).weapons.size(), 1);
        REQUIRE_EQ(WaveConfig::GetWaveConfig(2).weapons[0].weapon, WEAPONTYPE_AK47);
    });

    t.run("moderate: wave 2 has UZI and AK47 options", [&] {
        WaveConfig::InitializeWaveConfigs(WaveConfig::DEFENSE_MODERATE);
        const auto& w = WaveConfig::GetWaveConfig(2);
        REQUIRE_EQ((int)w.weapons.size(), 2);
        REQUIRE_EQ(w.weapons[0].weapon, WEAPONTYPE_UZI);
        REQUIRE_EQ(w.weapons[1].weapon, WEAPONTYPE_AK47);
    });

    // ------------------------------------------------------------------
    // Ammo escalates with wave index
    // ------------------------------------------------------------------
    t.run("light: pistol ammo increases from wave 0 to wave 1", [&] {
        WaveConfig::InitializeWaveConfigs(WaveConfig::DEFENSE_LIGHT);
        // Wave 0: colt45 ammo=60, Wave 1: colt45 ammo=80
        REQUIRE(WaveConfig::GetWaveConfig(1).weapons[0].ammo >
                WaveConfig::GetWaveConfig(0).weapons[1].ammo); // w0[1]=colt45, w1[0]=colt45
    });

    // ------------------------------------------------------------------
    // Defense level clamping
    // ------------------------------------------------------------------
    t.run("clamp: defense level -1 treated as LIGHT", [&] {
        WaveConfig::InitializeWaveConfigs(-1);
        const auto& w = WaveConfig::GetWaveConfig(0);
        REQUIRE_EQ(w.minCount, 4); // LIGHT wave 0
    });

    t.run("clamp: defense level 3 treated as HEAVY", [&] {
        WaveConfig::InitializeWaveConfigs(3);
        const auto& w = WaveConfig::GetWaveConfig(0);
        REQUIRE_EQ(w.minCount, 6); // HEAVY wave 0
    });

    // ------------------------------------------------------------------
    // Out-of-range GetWaveConfig returns safe default
    // ------------------------------------------------------------------
    t.run("default: GetWaveConfig(-1) returns non-empty fallback", [&] {
        WaveConfig::InitializeWaveConfigs(WaveConfig::DEFENSE_MODERATE);
        const auto& w = WaveConfig::GetWaveConfig(-1);
        REQUIRE(w.minCount > 0);
        REQUIRE(!w.weapons.empty());
    });

    t.run("default: GetWaveConfig(3) returns non-empty fallback", [&] {
        WaveConfig::InitializeWaveConfigs(WaveConfig::DEFENSE_MODERATE);
        const auto& w = WaveConfig::GetWaveConfig(3);
        REQUIRE(w.minCount > 0);
        REQUIRE(!w.weapons.empty());
    });

    t.run("default: GetWaveConfig out-of-range has sane min <= max", [&] {
        const auto& w = WaveConfig::GetWaveConfig(99);
        REQUIRE(w.minCount <= w.maxCount);
    });
}
