#include "TestFramework.h"
#include "../source/KillCreditRule.h"

void RunKillCreditRuleTests(Test::Runner& t) {
    t.suite("KillCreditRule");

    // ------------------------------------------------------------------
    // Ratio rule: player dealt >= 60% of total damage
    // ------------------------------------------------------------------
    t.run("ratio: 100% player damage grants credit", [&] {
        REQUIRE(EvaluateKillCredit(100.f, 100.f));
    });

    t.run("ratio: exactly 60% grants credit", [&] {
        REQUIRE(EvaluateKillCredit(60.f, 100.f));
    });

    t.run("ratio: 61% grants credit", [&] {
        REQUIRE(EvaluateKillCredit(61.f, 100.f));
    });

    t.run("ratio: 59% does not grant credit (no flat fallback)", [&] {
        // 59% ratio, player damage = 14.75 (below 25 flat threshold)
        REQUIRE_FALSE(EvaluateKillCredit(14.75f, 25.f));
    });

    t.run("ratio: 0% player damage, no credit", [&] {
        REQUIRE_FALSE(EvaluateKillCredit(0.f, 100.f));
    });

    // ------------------------------------------------------------------
    // Flat fallback rule: player dealt >= 25 damage regardless of ratio
    // ------------------------------------------------------------------
    t.run("flat: exactly 25 damage grants credit", [&] {
        // ratio would be 25% (25/100) — below threshold, but flat rule kicks in
        REQUIRE(EvaluateKillCredit(25.f, 100.f));
    });

    t.run("flat: 50 damage grants credit regardless of ratio", [&] {
        // ratio = 10% (50/500) — very low, but 50 >= 25
        REQUIRE(EvaluateKillCredit(50.f, 500.f));
    });

    t.run("flat: 24.9 damage does not grant credit if ratio also fails", [&] {
        // ratio = 24.9% (24.9/100), flat = 24.9 < 25
        REQUIRE_FALSE(EvaluateKillCredit(24.9f, 100.f));
    });

    t.run("flat: 0 damage, 0 total — no credit", [&] {
        REQUIRE_FALSE(EvaluateKillCredit(0.f, 0.f));
    });

    // ------------------------------------------------------------------
    // Edge cases
    // ------------------------------------------------------------------
    t.run("edge: total damage zero, player damage zero — no credit", [&] {
        REQUIRE_FALSE(EvaluateKillCredit(0.f, 0.f));
    });

    t.run("edge: player damage > total (shouldn't happen, but both rules handle it)", [&] {
        // ratio > 100%, which is >= 0.60 → credit
        REQUIRE(EvaluateKillCredit(110.f, 100.f));
    });

    // ------------------------------------------------------------------
    // Realistic scenarios
    // ------------------------------------------------------------------
    t.run("scenario: player finishes low-health ped (small damage, big ratio)", [&] {
        // Ped had 5 hp left, player dealt 5. Total recent damage = 5. 100% ratio.
        REQUIRE(EvaluateKillCredit(5.f, 5.f));
    });

    t.run("scenario: player and NPC share damage, player majority", [&] {
        // Player: 70, NPC: 30, total: 100. 70% ratio → credit.
        REQUIRE(EvaluateKillCredit(70.f, 100.f));
    });

    t.run("scenario: player and NPC share damage, NPC majority (and player below flat)", [&] {
        // Player: 20, NPC: 80, total: 100. 20% ratio, 20 < 25 flat → no credit.
        REQUIRE_FALSE(EvaluateKillCredit(20.f, 100.f));
    });

    t.run("scenario: player spray-fires into crowd, low per-target ratio but >= 25 flat", [&] {
        // Player: 30, others: 70, total: 100. 30% ratio fails, but 30 >= 25 flat → credit.
        REQUIRE(EvaluateKillCredit(30.f, 100.f));
    });
}
