#include "TestFramework.h"
#include "../source/WaveDeathRule.h"

// Gang type constants mirrored here to avoid game SDK dependency
static constexpr int MAFIA  = 7; // PEDTYPE_GANG1
static constexpr int TRIADS = 8; // PEDTYPE_GANG2
static constexpr int DIABLOS = 9; // PEDTYPE_GANG3
static constexpr int NEUTRAL = -1;

void RunWaveDeathRuleTests(Test::Runner& t) {
    t.suite("WaveDeathRule");

    // ------------------------------------------------------------------
    // SA rule: dying on wave 0 (before completing wave 1) → defender keeps
    // ------------------------------------------------------------------
    t.run("wave 0 death: defending gang keeps territory", [&] {
        REQUIRE_EQ(ComputeWaveDeathOwner(0, MAFIA), MAFIA);
    });

    t.run("wave 0 death: works for any defending gang", [&] {
        REQUIRE_EQ(ComputeWaveDeathOwner(0, TRIADS),  TRIADS);
        REQUIRE_EQ(ComputeWaveDeathOwner(0, DIABLOS), DIABLOS);
    });

    // ------------------------------------------------------------------
    // SA rule: dying on wave 1+ → territory goes neutral (-1)
    // ------------------------------------------------------------------
    t.run("wave 1 death: territory goes neutral", [&] {
        REQUIRE_EQ(ComputeWaveDeathOwner(1, MAFIA), NEUTRAL);
    });

    t.run("wave 2 death: territory goes neutral", [&] {
        REQUIRE_EQ(ComputeWaveDeathOwner(2, MAFIA), NEUTRAL);
    });

    t.run("wave 1 death: neutral regardless of defending gang", [&] {
        REQUIRE_EQ(ComputeWaveDeathOwner(1, TRIADS),  NEUTRAL);
        REQUIRE_EQ(ComputeWaveDeathOwner(1, DIABLOS), NEUTRAL);
    });

    // ------------------------------------------------------------------
    // Boundary: exactly wave 1 is the threshold
    // ------------------------------------------------------------------
    t.run("boundary: wave 0 → defender, wave 1 → neutral", [&] {
        REQUIRE_NE(ComputeWaveDeathOwner(0, MAFIA), NEUTRAL);
        REQUIRE_EQ(ComputeWaveDeathOwner(1, MAFIA), NEUTRAL);
    });
}
