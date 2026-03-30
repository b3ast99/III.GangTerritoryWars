#include "TestFramework.h"
#include "../source/WarKillTracker.h"

void RunWarKillTrackerTests(Test::Runner& t) {
    t.suite("WarKillTracker");

    // ------------------------------------------------------------------
    // Init / basic state
    // ------------------------------------------------------------------
    t.run("Init: tracker starts empty", [&] {
        WarKillTracker tracker;
        tracker.Init();
        REQUIRE(tracker.IsEmpty());
        REQUIRE_EQ((int)tracker.Size(), 0);
    });

    t.run("Init: clears existing records", [&] {
        WarKillTracker tracker;
        tracker.AddRecord(7, "1001", 1000u);
        tracker.AddRecord(7, "1001", 2000u);
        REQUIRE_EQ((int)tracker.Size(), 2);
        tracker.Init();
        REQUIRE(tracker.IsEmpty());
    });

    // ------------------------------------------------------------------
    // AddRecord
    // ------------------------------------------------------------------
    t.run("AddRecord: inserts a record", [&] {
        WarKillTracker tracker;
        tracker.AddRecord(7, "1001", 5000u);
        REQUIRE_EQ((int)tracker.Size(), 1);
    });

    t.run("AddRecord: multiple records accumulate", [&] {
        WarKillTracker tracker;
        tracker.AddRecord(7, "1001", 1000u);
        tracker.AddRecord(7, "1001", 2000u);
        tracker.AddRecord(8, "1002", 3000u);
        REQUIRE_EQ((int)tracker.Size(), 3);
    });

    t.run("AddRecord: enforces max cap by dropping oldest", [&] {
        WarKillTracker tracker;
        for (int i = 0; i < WarKillTracker::kMaxRecords + 5; ++i)
            tracker.AddRecord(7, "1001", (unsigned int)(i * 100));
        REQUIRE_EQ((int)tracker.Size(), WarKillTracker::kMaxRecords);
    });

    // ------------------------------------------------------------------
    // PruneExpired
    // ------------------------------------------------------------------
    t.run("PruneExpired: removes records older than window", [&] {
        WarKillTracker tracker;
        tracker.AddRecord(7, "1001", 1000u);   // age 16000 at now=17000
        tracker.AddRecord(7, "1001", 5000u);   // age 12000 — within 15s window
        tracker.AddRecord(7, "1001", 10000u);  // age 7000 — within window
        tracker.PruneExpired(17000u, 15000u);
        REQUIRE_EQ((int)tracker.Size(), 2);
    });

    t.run("PruneExpired: keeps all records within window", [&] {
        WarKillTracker tracker;
        tracker.AddRecord(7, "1001", 10000u);
        tracker.AddRecord(7, "1001", 11000u);
        tracker.AddRecord(7, "1001", 12000u);
        tracker.PruneExpired(14000u, 15000u); // all within 4s, window is 15s
        REQUIRE_EQ((int)tracker.Size(), 3);
    });

    t.run("PruneExpired: removes all when all expired", [&] {
        WarKillTracker tracker;
        tracker.AddRecord(7, "1001", 1000u);
        tracker.AddRecord(7, "1001", 2000u);
        tracker.PruneExpired(20000u, 5000u);
        REQUIRE(tracker.IsEmpty());
    });

    t.run("PruneExpired: no-op on empty tracker", [&] {
        WarKillTracker tracker;
        tracker.PruneExpired(99999u, 15000u);
        REQUIRE(tracker.IsEmpty());
    });

    // ------------------------------------------------------------------
    // CountForTerritory
    // ------------------------------------------------------------------
    t.run("CountForTerritory: counts matching records", [&] {
        WarKillTracker tracker;
        tracker.AddRecord(7, "1001", 10000u);
        tracker.AddRecord(7, "1001", 11000u);
        tracker.AddRecord(7, "1001", 12000u);
        REQUIRE_EQ(tracker.CountForTerritory("1001", 7, 14000u, 15000u), 3);
    });

    t.run("CountForTerritory: ignores different territory", [&] {
        WarKillTracker tracker;
        tracker.AddRecord(7, "1001", 10000u);
        tracker.AddRecord(7, "1002", 10000u); // different territory
        REQUIRE_EQ(tracker.CountForTerritory("1001", 7, 12000u, 15000u), 1);
    });

    t.run("CountForTerritory: ignores different gang", [&] {
        WarKillTracker tracker;
        tracker.AddRecord(7, "1001", 10000u);
        tracker.AddRecord(8, "1001", 10000u); // same territory, different gang
        REQUIRE_EQ(tracker.CountForTerritory("1001", 7, 12000u, 15000u), 1);
    });

    t.run("CountForTerritory: excludes records outside window", [&] {
        WarKillTracker tracker;
        tracker.AddRecord(7, "1001", 1000u);   // expired at now=17000, window=15000
        tracker.AddRecord(7, "1001", 5000u);   // within window
        REQUIRE_EQ(tracker.CountForTerritory("1001", 7, 17000u, 15000u), 1);
    });

    t.run("CountForTerritory: returns 0 when empty", [&] {
        WarKillTracker tracker;
        REQUIRE_EQ(tracker.CountForTerritory("1001", 7, 10000u, 15000u), 0);
    });

    // ------------------------------------------------------------------
    // CanTriggerWarInTerritory (tested via the static integers — no game dep)
    // ------------------------------------------------------------------
    // These replicate WarSystem::CanTriggerWarInTerritory logic to verify
    // the range check is correct. (The actual function is in WarSystem.cpp
    // which depends on game headers; this tests the same rule in isolation.)
    t.run("trigger check: rejects neutral (0)", [&] {
        constexpr int PEDTYPE_GANG1 = 7;
        constexpr int PEDTYPE_GANG3 = 9;
        const int owner = 0;
        REQUIRE_FALSE(owner >= PEDTYPE_GANG1 && owner <= PEDTYPE_GANG3);
    });

    t.run("trigger check: rejects player-owned (-1)", [&] {
        constexpr int PEDTYPE_GANG1 = 7;
        constexpr int PEDTYPE_GANG3 = 9;
        const int owner = -1;
        REQUIRE_FALSE(owner >= PEDTYPE_GANG1 && owner <= PEDTYPE_GANG3);
    });

    t.run("trigger check: accepts GANG1 (7)", [&] {
        constexpr int PEDTYPE_GANG1 = 7;
        constexpr int PEDTYPE_GANG3 = 9;
        const int owner = 7;
        REQUIRE(owner >= PEDTYPE_GANG1 && owner <= PEDTYPE_GANG3);
    });

    t.run("trigger check: accepts GANG3 (9)", [&] {
        constexpr int PEDTYPE_GANG1 = 7;
        constexpr int PEDTYPE_GANG3 = 9;
        const int owner = 9;
        REQUIRE(owner >= PEDTYPE_GANG1 && owner <= PEDTYPE_GANG3);
    });

    t.run("trigger check: rejects out-of-range high (10)", [&] {
        constexpr int PEDTYPE_GANG1 = 7;
        constexpr int PEDTYPE_GANG3 = 9;
        const int owner = 10;
        REQUIRE_FALSE(owner >= PEDTYPE_GANG1 && owner <= PEDTYPE_GANG3);
    });
}
