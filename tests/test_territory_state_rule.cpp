#include "TestFramework.h"
#include "../source/TerritoryStateRule.h"

static constexpr int MAFIA      = GANG1;
static constexpr int TRIADS     = GANG2;
static constexpr int DIABLOS    = GANG3;
static constexpr int YAKUZA     = GANG4;
static constexpr int COLOMBIANS = GANG5;
static constexpr int YARDIES    = GANG6;

void RunTerritoryStateRuleTests(Test::Runner& t) {

    // ==================================================================
    // ComputeTerritoryState
    // ==================================================================
    t.suite("TerritoryStateRule – ComputeTerritoryState");

    // Locked always wins
    t.run("locked territory -> LOCKED regardless of owner", [&] {
        REQUIRE(ComputeTerritoryState(MAFIA, 1, true) == TerritoryState::LOCKED);
        REQUIRE(ComputeTerritoryState(TRIADS, 3, true) == TerritoryState::LOCKED);
        REQUIRE(ComputeTerritoryState(OWNER_NEUTRAL, 2, true) == TerritoryState::LOCKED);
        REQUIRE(ComputeTerritoryState(OWNER_CLEARED, 3, true) == TerritoryState::LOCKED);
    });

    // Cleared
    t.run("cleared territory -> CLEARED", [&] {
        REQUIRE(ComputeTerritoryState(OWNER_CLEARED, 3, false) == TerritoryState::CLEARED);
        REQUIRE(ComputeTerritoryState(OWNER_CLEARED, 1, false) == TerritoryState::CLEARED);
    });

    // Neutral
    t.run("neutral territory -> NEUTRAL", [&] {
        REQUIRE(ComputeTerritoryState(OWNER_NEUTRAL, 1, false) == TerritoryState::NEUTRAL);
        REQUIRE(ComputeTerritoryState(OWNER_NEUTRAL, 3, false) == TerritoryState::NEUTRAL);
    });

    // Act 1: Mafia allied, Triads/Diablos hostile
    t.run("Act 1: Mafia territory -> ALLIED", [&] {
        REQUIRE(ComputeTerritoryState(MAFIA, 1, false) == TerritoryState::GANG_ALLIED);
    });

    t.run("Act 1: Triads territory -> HOSTILE", [&] {
        REQUIRE(ComputeTerritoryState(TRIADS, 1, false) == TerritoryState::GANG_HOSTILE);
    });

    t.run("Act 1: Diablos territory -> HOSTILE", [&] {
        REQUIRE(ComputeTerritoryState(DIABLOS, 1, false) == TerritoryState::GANG_HOSTILE);
    });

    t.run("Act 1: Yakuza territory -> ALLIED (not hostile, not our gang)", [&] {
        // Yakuza is neither hostile nor allied in Act 1 — defaults to ALLIED behavior
        REQUIRE(ComputeTerritoryState(YAKUZA, 1, false) == TerritoryState::GANG_ALLIED);
    });

    t.run("Act 1: Colombians territory -> ALLIED (not present)", [&] {
        REQUIRE(ComputeTerritoryState(COLOMBIANS, 1, false) == TerritoryState::GANG_ALLIED);
    });

    // Act 2: Yakuza allied, Mafia/Colombians/Yardies hostile
    t.run("Act 2: Yakuza territory -> ALLIED", [&] {
        REQUIRE(ComputeTerritoryState(YAKUZA, 2, false) == TerritoryState::GANG_ALLIED);
    });

    t.run("Act 2: Mafia territory -> HOSTILE", [&] {
        REQUIRE(ComputeTerritoryState(MAFIA, 2, false) == TerritoryState::GANG_HOSTILE);
    });

    t.run("Act 2: Colombians territory -> HOSTILE", [&] {
        REQUIRE(ComputeTerritoryState(COLOMBIANS, 2, false) == TerritoryState::GANG_HOSTILE);
    });

    t.run("Act 2: Yardies territory -> HOSTILE", [&] {
        REQUIRE(ComputeTerritoryState(YARDIES, 2, false) == TerritoryState::GANG_HOSTILE);
    });

    t.run("Act 2: Triads territory -> ALLIED (not hostile in Act 2)", [&] {
        REQUIRE(ComputeTerritoryState(TRIADS, 2, false) == TerritoryState::GANG_ALLIED);
    });

    t.run("Act 2: Diablos territory -> ALLIED (not hostile in Act 2)", [&] {
        REQUIRE(ComputeTerritoryState(DIABLOS, 2, false) == TerritoryState::GANG_ALLIED);
    });

    // Act 3: ALL gangs hostile (independent)
    t.run("Act 3: all gang territories -> HOSTILE", [&] {
        REQUIRE(ComputeTerritoryState(MAFIA, 3, false) == TerritoryState::GANG_HOSTILE);
        REQUIRE(ComputeTerritoryState(TRIADS, 3, false) == TerritoryState::GANG_HOSTILE);
        REQUIRE(ComputeTerritoryState(DIABLOS, 3, false) == TerritoryState::GANG_HOSTILE);
        REQUIRE(ComputeTerritoryState(YAKUZA, 3, false) == TerritoryState::GANG_HOSTILE);
        REQUIRE(ComputeTerritoryState(COLOMBIANS, 3, false) == TerritoryState::GANG_HOSTILE);
        REQUIRE(ComputeTerritoryState(YARDIES, 3, false) == TerritoryState::GANG_HOSTILE);
    });

    // Act 0: no hostilities — all gangs default to ALLIED
    t.run("Act 0: all gang territories -> ALLIED (no hostilities)", [&] {
        REQUIRE(ComputeTerritoryState(MAFIA, 0, false) == TerritoryState::GANG_ALLIED);
        REQUIRE(ComputeTerritoryState(TRIADS, 0, false) == TerritoryState::GANG_ALLIED);
        REQUIRE(ComputeTerritoryState(YAKUZA, 0, false) == TerritoryState::GANG_ALLIED);
    });

    // Act transitions: Mafia goes from ALLIED to HOSTILE
    t.run("Mafia territory: ALLIED in Act 1, HOSTILE in Act 2", [&] {
        REQUIRE(ComputeTerritoryState(MAFIA, 1, false) == TerritoryState::GANG_ALLIED);
        REQUIRE(ComputeTerritoryState(MAFIA, 2, false) == TerritoryState::GANG_HOSTILE);
    });

    t.run("Yakuza territory: ALLIED in Act 1-2, HOSTILE in Act 3", [&] {
        REQUIRE(ComputeTerritoryState(YAKUZA, 1, false) == TerritoryState::GANG_ALLIED);
        REQUIRE(ComputeTerritoryState(YAKUZA, 2, false) == TerritoryState::GANG_ALLIED);
        REQUIRE(ComputeTerritoryState(YAKUZA, 3, false) == TerritoryState::GANG_HOSTILE);
    });

    // ==================================================================
    // GetTerritoryBlipColor
    // ==================================================================
    t.suite("TerritoryStateRule – GetTerritoryBlipColor");

    t.run("hostile -> RED (0)", [&] {
        REQUIRE_EQ(GetTerritoryBlipColor(TerritoryState::GANG_HOSTILE), 0);
    });

    t.run("allied -> GREEN (1)", [&] {
        REQUIRE_EQ(GetTerritoryBlipColor(TerritoryState::GANG_ALLIED), 1);
    });

    t.run("cleared -> WHITE (3)", [&] {
        REQUIRE_EQ(GetTerritoryBlipColor(TerritoryState::CLEARED), 3);
    });

    t.run("neutral -> YELLOW (4)", [&] {
        REQUIRE_EQ(GetTerritoryBlipColor(TerritoryState::NEUTRAL), 4);
    });

    // ==================================================================
    // IsTerritoryVisible
    // ==================================================================
    t.suite("TerritoryStateRule – IsTerritoryVisible");

    t.run("locked territories are hidden", [&] {
        REQUIRE_FALSE(IsTerritoryVisible(TerritoryState::LOCKED));
    });

    t.run("all non-locked states are visible", [&] {
        REQUIRE(IsTerritoryVisible(TerritoryState::GANG_HOSTILE));
        REQUIRE(IsTerritoryVisible(TerritoryState::GANG_ALLIED));
        REQUIRE(IsTerritoryVisible(TerritoryState::CLEARED));
        REQUIRE(IsTerritoryVisible(TerritoryState::NEUTRAL));
    });

    // ==================================================================
    // ShouldSpawnHostilePeds / CanStartWarInTerritory
    // ==================================================================
    t.suite("TerritoryStateRule – SpawnHostile & CanStartWar");

    t.run("only hostile territories spawn hostile peds", [&] {
        REQUIRE(ShouldSpawnHostilePeds(TerritoryState::GANG_HOSTILE));
        REQUIRE_FALSE(ShouldSpawnHostilePeds(TerritoryState::GANG_ALLIED));
        REQUIRE_FALSE(ShouldSpawnHostilePeds(TerritoryState::CLEARED));
        REQUIRE_FALSE(ShouldSpawnHostilePeds(TerritoryState::NEUTRAL));
        REQUIRE_FALSE(ShouldSpawnHostilePeds(TerritoryState::LOCKED));
    });

    t.run("only hostile territories allow wars", [&] {
        REQUIRE(CanStartWarInTerritory(TerritoryState::GANG_HOSTILE));
        REQUIRE_FALSE(CanStartWarInTerritory(TerritoryState::GANG_ALLIED));
        REQUIRE_FALSE(CanStartWarInTerritory(TerritoryState::CLEARED));
        REQUIRE_FALSE(CanStartWarInTerritory(TerritoryState::NEUTRAL));
        REQUIRE_FALSE(CanStartWarInTerritory(TerritoryState::LOCKED));
    });
}
