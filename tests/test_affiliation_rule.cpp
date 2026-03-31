#include "TestFramework.h"
#include "../source/AffiliationRule.h"

static constexpr int MAFIA      = GANG1;  // 7
static constexpr int TRIADS     = GANG2;  // 8
static constexpr int DIABLOS    = GANG3;  // 9
static constexpr int YAKUZA     = GANG4;  // 10
static constexpr int COLOMBIANS = GANG5;  // 11
static constexpr int YARDIES    = GANG6;  // 12

void RunAffiliationRuleTests(Test::Runner& t) {

    // ==================================================================
    // GetAffiliatedGangForAct
    // ==================================================================
    t.suite("AffiliationRule – GetAffiliatedGangForAct");

    t.run("Act 0 (NONE): no affiliation", [&] {
        REQUIRE_EQ(GetAffiliatedGangForAct(0), -1);
    });

    t.run("Act 1: affiliated with Mafia", [&] {
        REQUIRE_EQ(GetAffiliatedGangForAct(1), MAFIA);
    });

    t.run("Act 2: affiliated with Yakuza", [&] {
        REQUIRE_EQ(GetAffiliatedGangForAct(2), YAKUZA);
    });

    t.run("Act 3: independent (no affiliation)", [&] {
        REQUIRE_EQ(GetAffiliatedGangForAct(3), -1);
    });

    t.run("invalid act returns -1", [&] {
        REQUIRE_EQ(GetAffiliatedGangForAct(-1), -1);
        REQUIRE_EQ(GetAffiliatedGangForAct(99), -1);
    });

    // ==================================================================
    // IsGangHostileInAct
    // ==================================================================
    t.suite("AffiliationRule – IsGangHostileInAct");

    // Act 0: no hostilities
    t.run("Act 0: no gangs hostile", [&] {
        REQUIRE_FALSE(IsGangHostileInAct(0, MAFIA));
        REQUIRE_FALSE(IsGangHostileInAct(0, TRIADS));
        REQUIRE_FALSE(IsGangHostileInAct(0, DIABLOS));
        REQUIRE_FALSE(IsGangHostileInAct(0, YAKUZA));
        REQUIRE_FALSE(IsGangHostileInAct(0, COLOMBIANS));
        REQUIRE_FALSE(IsGangHostileInAct(0, YARDIES));
    });

    // Act 1: Triads + Diablos hostile, Mafia allied
    t.run("Act 1: Mafia NOT hostile (allied)", [&] {
        REQUIRE_FALSE(IsGangHostileInAct(1, MAFIA));
    });

    t.run("Act 1: Triads hostile", [&] {
        REQUIRE(IsGangHostileInAct(1, TRIADS));
    });

    t.run("Act 1: Diablos hostile", [&] {
        REQUIRE(IsGangHostileInAct(1, DIABLOS));
    });

    t.run("Act 1: Yakuza NOT hostile (not present in Portland)", [&] {
        REQUIRE_FALSE(IsGangHostileInAct(1, YAKUZA));
    });

    t.run("Act 1: Colombians NOT hostile", [&] {
        REQUIRE_FALSE(IsGangHostileInAct(1, COLOMBIANS));
    });

    t.run("Act 1: Yardies NOT hostile", [&] {
        REQUIRE_FALSE(IsGangHostileInAct(1, YARDIES));
    });

    // Act 2: Mafia + Colombians + Yardies hostile, Yakuza allied
    t.run("Act 2: Mafia hostile", [&] {
        REQUIRE(IsGangHostileInAct(2, MAFIA));
    });

    t.run("Act 2: Triads NOT hostile", [&] {
        REQUIRE_FALSE(IsGangHostileInAct(2, TRIADS));
    });

    t.run("Act 2: Diablos NOT hostile", [&] {
        REQUIRE_FALSE(IsGangHostileInAct(2, DIABLOS));
    });

    t.run("Act 2: Yakuza NOT hostile (allied)", [&] {
        REQUIRE_FALSE(IsGangHostileInAct(2, YAKUZA));
    });

    t.run("Act 2: Colombians hostile", [&] {
        REQUIRE(IsGangHostileInAct(2, COLOMBIANS));
    });

    t.run("Act 2: Yardies hostile", [&] {
        REQUIRE(IsGangHostileInAct(2, YARDIES));
    });

    // Act 3: ALL gangs hostile
    t.run("Act 3: all gangs hostile", [&] {
        REQUIRE(IsGangHostileInAct(3, MAFIA));
        REQUIRE(IsGangHostileInAct(3, TRIADS));
        REQUIRE(IsGangHostileInAct(3, DIABLOS));
        REQUIRE(IsGangHostileInAct(3, YAKUZA));
        REQUIRE(IsGangHostileInAct(3, COLOMBIANS));
        REQUIRE(IsGangHostileInAct(3, YARDIES));
    });

    // Invalid gang types
    t.run("non-gang ped types are never hostile", [&] {
        REQUIRE_FALSE(IsGangHostileInAct(1, 0));  // PLAYER
        REQUIRE_FALSE(IsGangHostileInAct(1, 6));   // COP
        REQUIRE_FALSE(IsGangHostileInAct(3, 4));   // CIVMALE
        REQUIRE_FALSE(IsGangHostileInAct(3, 13));  // GANG7 (out of our range)
    });

    // ==================================================================
    // IsGangAlliedInAct
    // ==================================================================
    t.suite("AffiliationRule – IsGangAlliedInAct");

    t.run("Act 0: no allies", [&] {
        REQUIRE_FALSE(IsGangAlliedInAct(0, MAFIA));
        REQUIRE_FALSE(IsGangAlliedInAct(0, YAKUZA));
    });

    t.run("Act 1: Mafia is allied", [&] {
        REQUIRE(IsGangAlliedInAct(1, MAFIA));
    });

    t.run("Act 1: Triads NOT allied", [&] {
        REQUIRE_FALSE(IsGangAlliedInAct(1, TRIADS));
    });

    t.run("Act 2: Yakuza is allied", [&] {
        REQUIRE(IsGangAlliedInAct(2, YAKUZA));
    });

    t.run("Act 2: Mafia NOT allied", [&] {
        REQUIRE_FALSE(IsGangAlliedInAct(2, MAFIA));
    });

    t.run("Act 3: no allies (independent)", [&] {
        REQUIRE_FALSE(IsGangAlliedInAct(3, MAFIA));
        REQUIRE_FALSE(IsGangAlliedInAct(3, YAKUZA));
        REQUIRE_FALSE(IsGangAlliedInAct(3, TRIADS));
    });

    // ==================================================================
    // CanAttackTerritoryOwner
    // ==================================================================
    t.suite("AffiliationRule – CanAttackTerritoryOwner");

    t.run("can attack hostile gang territory", [&] {
        REQUIRE(CanAttackTerritoryOwner(TRIADS, 1, false));
        REQUIRE(CanAttackTerritoryOwner(DIABLOS, 1, false));
    });

    t.run("cannot attack allied gang territory", [&] {
        REQUIRE_FALSE(CanAttackTerritoryOwner(MAFIA, 1, false));
        REQUIRE_FALSE(CanAttackTerritoryOwner(YAKUZA, 2, false));
    });

    t.run("cannot attack locked territory", [&] {
        REQUIRE_FALSE(CanAttackTerritoryOwner(TRIADS, 1, true));
    });

    t.run("cannot attack neutral territory", [&] {
        REQUIRE_FALSE(CanAttackTerritoryOwner(OWNER_NEUTRAL, 1, false));
    });

    t.run("cannot attack cleared territory", [&] {
        REQUIRE_FALSE(CanAttackTerritoryOwner(OWNER_CLEARED, 3, false));
    });

    t.run("Act 3: can attack any gang territory", [&] {
        REQUIRE(CanAttackTerritoryOwner(MAFIA, 3, false));
        REQUIRE(CanAttackTerritoryOwner(TRIADS, 3, false));
        REQUIRE(CanAttackTerritoryOwner(DIABLOS, 3, false));
        REQUIRE(CanAttackTerritoryOwner(YAKUZA, 3, false));
        REQUIRE(CanAttackTerritoryOwner(COLOMBIANS, 3, false));
        REQUIRE(CanAttackTerritoryOwner(YARDIES, 3, false));
    });

    t.run("Act 0: cannot attack anything (no hostilities)", [&] {
        REQUIRE_FALSE(CanAttackTerritoryOwner(MAFIA, 0, false));
        REQUIRE_FALSE(CanAttackTerritoryOwner(TRIADS, 0, false));
    });

    t.run("Act 2: cannot attack non-hostile gangs from Act 1", [&] {
        REQUIRE_FALSE(CanAttackTerritoryOwner(TRIADS, 2, false));
        REQUIRE_FALSE(CanAttackTerritoryOwner(DIABLOS, 2, false));
    });

    t.run("Act 2: can attack Mafia (now hostile)", [&] {
        REQUIRE(CanAttackTerritoryOwner(MAFIA, 2, false));
    });

    // ==================================================================
    // GetCaptureOwnerForAct
    // ==================================================================
    t.suite("AffiliationRule – GetCaptureOwnerForAct");

    t.run("Act 1: Mafia captures territory", [&] {
        REQUIRE_EQ(GetCaptureOwnerForAct(1), MAFIA);
    });

    t.run("Act 2: Yakuza captures territory", [&] {
        REQUIRE_EQ(GetCaptureOwnerForAct(2), YAKUZA);
    });

    t.run("Act 3: territory is cleared (independent)", [&] {
        REQUIRE_EQ(GetCaptureOwnerForAct(3), OWNER_CLEARED);
    });

    t.run("Act 0: returns -1 (shouldn't happen)", [&] {
        REQUIRE_EQ(GetCaptureOwnerForAct(0), -1);
    });
}
