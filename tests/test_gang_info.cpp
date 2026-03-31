#include "TestFramework.h"
#include "../source/GangInfo.h"

#include <algorithm>
#include <set>

// -------------------------------------------------------------------------
// Seed GangManager::s_gangs[] directly — no engine calls, no Initialize().
// Values match the real mod: Mafia peds 10/11 + vehicle 134,
// Triads peds 12/13 + vehicle 136, Diablos peds 14/15 + vehicle 137.
// -------------------------------------------------------------------------
static void SetupGangs() {
    GangManager::s_gangs[0].gangType       = PEDTYPE_GANG1;
    GangManager::s_gangs[0].displayName    = "Mafia";
    GangManager::s_gangs[0].modelIds       = { 10, 11 };
    GangManager::s_gangs[0].vehicleModelIds = { 134 };

    GangManager::s_gangs[1].gangType       = PEDTYPE_GANG2;
    GangManager::s_gangs[1].displayName    = "Triads";
    GangManager::s_gangs[1].modelIds       = { 12, 13 };
    GangManager::s_gangs[1].vehicleModelIds = { 132 }; // BELLYUP (fixed from YAKUZA)

    GangManager::s_gangs[2].gangType       = PEDTYPE_GANG3;
    GangManager::s_gangs[2].displayName    = "Diablos";
    GangManager::s_gangs[2].modelIds       = { 14, 15 };
    GangManager::s_gangs[2].vehicleModelIds = { 137 };

    GangManager::s_gangs[3].gangType       = PEDTYPE_GANG4;
    GangManager::s_gangs[3].displayName    = "Yakuza";
    GangManager::s_gangs[3].modelIds       = { 16, 17 };
    GangManager::s_gangs[3].vehicleModelIds = { 136 };

    GangManager::s_gangs[4].gangType       = PEDTYPE_GANG5;
    GangManager::s_gangs[4].displayName    = "Colombians";
    GangManager::s_gangs[4].modelIds       = { 18, 19 };
    GangManager::s_gangs[4].vehicleModelIds = { 138 };

    GangManager::s_gangs[5].gangType       = PEDTYPE_GANG6;
    GangManager::s_gangs[5].displayName    = "Yardies";
    GangManager::s_gangs[5].modelIds       = { 20, 21 };
    GangManager::s_gangs[5].vehicleModelIds = { 135 };
}

void RunGangInfoTests(Test::Runner& t) {
    SetupGangs();

    // ------------------------------------------------------------------
    // GetGangInfo
    // ------------------------------------------------------------------
    t.suite("GangManager – GetGangInfo");

    t.run("returns entry for GANG1", [&] {
        const GangInfo* g = GangManager::GetGangInfo(PEDTYPE_GANG1);
        REQUIRE(g != nullptr);
        REQUIRE_EQ(g->gangType, PEDTYPE_GANG1);
    });

    t.run("returns entry for GANG2", [&] {
        const GangInfo* g = GangManager::GetGangInfo(PEDTYPE_GANG2);
        REQUIRE(g != nullptr);
        REQUIRE_EQ(g->gangType, PEDTYPE_GANG2);
    });

    t.run("returns entry for GANG3", [&] {
        const GangInfo* g = GangManager::GetGangInfo(PEDTYPE_GANG3);
        REQUIRE(g != nullptr);
        REQUIRE_EQ(g->gangType, PEDTYPE_GANG3);
    });

    t.run("returns entry for GANG4", [&] {
        const GangInfo* g = GangManager::GetGangInfo(PEDTYPE_GANG4);
        REQUIRE(g != nullptr);
        REQUIRE_EQ(g->gangType, PEDTYPE_GANG4);
    });

    t.run("returns entry for GANG5", [&] {
        const GangInfo* g = GangManager::GetGangInfo(PEDTYPE_GANG5);
        REQUIRE(g != nullptr);
        REQUIRE_EQ(g->gangType, PEDTYPE_GANG5);
    });

    t.run("returns entry for GANG6", [&] {
        const GangInfo* g = GangManager::GetGangInfo(PEDTYPE_GANG6);
        REQUIRE(g != nullptr);
        REQUIRE_EQ(g->gangType, PEDTYPE_GANG6);
    });

    t.run("returns null for unknown ped type", [&] {
        REQUIRE(GangManager::GetGangInfo(PEDTYPE_CIVMALE) == nullptr);
        REQUIRE(GangManager::GetGangInfo(PEDTYPE_COP)     == nullptr);
        REQUIRE(GangManager::GetGangInfo(PEDTYPE_GANG9)   == nullptr);
    });

    // ------------------------------------------------------------------
    // IsGangModelId — ped models
    // ------------------------------------------------------------------
    t.suite("GangManager – IsGangModelId (ped models)");

    t.run("registered ped models are recognised", [&] {
        REQUIRE(GangManager::IsGangModelId(10));  // Mafia
        REQUIRE(GangManager::IsGangModelId(11));
        REQUIRE(GangManager::IsGangModelId(12));  // Triads
        REQUIRE(GangManager::IsGangModelId(13));
        REQUIRE(GangManager::IsGangModelId(14));  // Diablos
        REQUIRE(GangManager::IsGangModelId(15));
        REQUIRE(GangManager::IsGangModelId(16));  // Yakuza
        REQUIRE(GangManager::IsGangModelId(17));
        REQUIRE(GangManager::IsGangModelId(18));  // Colombians
        REQUIRE(GangManager::IsGangModelId(19));
        REQUIRE(GangManager::IsGangModelId(20));  // Yardies
        REQUIRE(GangManager::IsGangModelId(21));
    });

    t.run("vehicle models are not ped models", [&] {
        REQUIRE_FALSE(GangManager::IsGangModelId(132)); // BELLYUP
        REQUIRE_FALSE(GangManager::IsGangModelId(134)); // MAFIA
        REQUIRE_FALSE(GangManager::IsGangModelId(135)); // YARDIE
        REQUIRE_FALSE(GangManager::IsGangModelId(136)); // YAKUZA
        REQUIRE_FALSE(GangManager::IsGangModelId(137)); // DIABLOS
        REQUIRE_FALSE(GangManager::IsGangModelId(138)); // COLUMB
    });

    t.run("civilian model IDs are not gang models", [&] {
        // Civilian pool runs roughly 30-82 in GTA III
        for (int id = 30; id <= 50; ++id) {
            REQUIRE_FALSE(GangManager::IsGangModelId(id));
        }
    });

    t.run("negative model ID returns false", [&] {
        REQUIRE_FALSE(GangManager::IsGangModelId(-1));
    });

    // ------------------------------------------------------------------
    // IsGangVehicleModel
    // ------------------------------------------------------------------
    t.suite("GangManager – IsGangVehicleModel");

    t.run("registered vehicle models are recognised", [&] {
        REQUIRE(GangManager::IsGangVehicleModel(134)); // Mafia
        REQUIRE(GangManager::IsGangVehicleModel(132)); // Triads (BELLYUP)
        REQUIRE(GangManager::IsGangVehicleModel(137)); // Diablos
        REQUIRE(GangManager::IsGangVehicleModel(136)); // Yakuza
        REQUIRE(GangManager::IsGangVehicleModel(138)); // Colombians
        REQUIRE(GangManager::IsGangVehicleModel(135)); // Yardies
    });

    t.run("ped models are not vehicle models", [&] {
        REQUIRE_FALSE(GangManager::IsGangVehicleModel(10));
        REQUIRE_FALSE(GangManager::IsGangVehicleModel(15));
    });

    t.run("common civilian vehicles are not gang vehicles", [&] {
        // MODEL_SENTINEL=95, MODEL_TAXI=110, MODEL_KURUMA=111
        REQUIRE_FALSE(GangManager::IsGangVehicleModel(95));
        REQUIRE_FALSE(GangManager::IsGangVehicleModel(110));
        REQUIRE_FALSE(GangManager::IsGangVehicleModel(111));
    });

    t.run("negative ID returns false", [&] {
        REQUIRE_FALSE(GangManager::IsGangVehicleModel(-1));
    });

    // ------------------------------------------------------------------
    // GetGangForVehicleModel — the core of the occupant-matching fix
    // ------------------------------------------------------------------
    t.suite("GangManager – GetGangForVehicleModel");

    t.run("Mafia vehicle (134) maps to GANG1 (7)", [&] {
        REQUIRE_EQ(GangManager::GetGangForVehicleModel(134), (int)PEDTYPE_GANG1);
    });

    t.run("Triads vehicle (132/BELLYUP) maps to GANG2 (8)", [&] {
        REQUIRE_EQ(GangManager::GetGangForVehicleModel(132), (int)PEDTYPE_GANG2);
    });

    t.run("Diablos vehicle (137) maps to GANG3 (9)", [&] {
        REQUIRE_EQ(GangManager::GetGangForVehicleModel(137), (int)PEDTYPE_GANG3);
    });

    t.run("Yakuza vehicle (136) maps to GANG4 (10)", [&] {
        REQUIRE_EQ(GangManager::GetGangForVehicleModel(136), (int)PEDTYPE_GANG4);
    });

    t.run("Colombian vehicle (138) maps to GANG5 (11)", [&] {
        REQUIRE_EQ(GangManager::GetGangForVehicleModel(138), (int)PEDTYPE_GANG5);
    });

    t.run("Yardie vehicle (135) maps to GANG6 (12)", [&] {
        REQUIRE_EQ(GangManager::GetGangForVehicleModel(135), (int)PEDTYPE_GANG6);
    });

    t.run("civilian vehicle returns -1", [&] {
        REQUIRE_EQ(GangManager::GetGangForVehicleModel(95),  -1); // Sentinel
        REQUIRE_EQ(GangManager::GetGangForVehicleModel(110), -1); // Taxi
        REQUIRE_EQ(GangManager::GetGangForVehicleModel(128), -1); // Cabbie
    });

    t.run("negative ID returns -1", [&] {
        REQUIRE_EQ(GangManager::GetGangForVehicleModel(-1), -1);
    });

    t.run("ped model ID returns -1 (not a vehicle)", [&] {
        REQUIRE_EQ(GangManager::GetGangForVehicleModel(10), -1);
        REQUIRE_EQ(GangManager::GetGangForVehicleModel(14), -1);
    });

    // ------------------------------------------------------------------
    // GetRandomModelId — stays within the gang's registered ped set
    // ------------------------------------------------------------------
    t.suite("GangManager – GetRandomModelId");

    t.run("GANG1 model always in {10, 11}", [&] {
        std::srand(42);
        for (int i = 0; i < 50; ++i) {
            const int m = GangManager::GetRandomModelId(PEDTYPE_GANG1);
            REQUIRE(m == 10 || m == 11);
        }
    });

    t.run("GANG2 model always in {12, 13}", [&] {
        std::srand(42);
        for (int i = 0; i < 50; ++i) {
            const int m = GangManager::GetRandomModelId(PEDTYPE_GANG2);
            REQUIRE(m == 12 || m == 13);
        }
    });

    t.run("GANG3 model always in {14, 15}", [&] {
        std::srand(42);
        for (int i = 0; i < 50; ++i) {
            const int m = GangManager::GetRandomModelId(PEDTYPE_GANG3);
            REQUIRE(m == 14 || m == 15);
        }
    });

    t.run("GANG4 model always in {16, 17}", [&] {
        std::srand(42);
        for (int i = 0; i < 50; ++i) {
            const int m = GangManager::GetRandomModelId(PEDTYPE_GANG4);
            REQUIRE(m == 16 || m == 17);
        }
    });

    t.run("GANG5 model always in {18, 19}", [&] {
        std::srand(42);
        for (int i = 0; i < 50; ++i) {
            const int m = GangManager::GetRandomModelId(PEDTYPE_GANG5);
            REQUIRE(m == 18 || m == 19);
        }
    });

    t.run("GANG6 model always in {20, 21}", [&] {
        std::srand(42);
        for (int i = 0; i < 50; ++i) {
            const int m = GangManager::GetRandomModelId(PEDTYPE_GANG6);
            REQUIRE(m == 20 || m == 21);
        }
    });

    t.run("both models returned for GANG1 over enough draws", [&] {
        std::srand(0);
        std::set<int> seen;
        for (int i = 0; i < 200; ++i) seen.insert(GangManager::GetRandomModelId(PEDTYPE_GANG1));
        REQUIRE(seen.count(10) > 0);
        REQUIRE(seen.count(11) > 0);
    });

    t.run("returns -1 for unregistered gang type", [&] {
        REQUIRE_EQ(GangManager::GetRandomModelId(PEDTYPE_CIVMALE), -1);
        REQUIRE_EQ(GangManager::GetRandomModelId(PEDTYPE_GANG9),   -1);
    });

    t.run("returns -1 when model list cleared", [&] {
        auto saved = GangManager::s_gangs[0].modelIds;
        GangManager::s_gangs[0].modelIds.clear();
        REQUIRE_EQ(GangManager::GetRandomModelId(PEDTYPE_GANG1), -1);
        GangManager::s_gangs[0].modelIds = saved; // restore
    });

    // ------------------------------------------------------------------
    // GetRandomGangVehicle — stays within the gang's registered vehicle set
    // ------------------------------------------------------------------
    t.suite("GangManager – GetRandomGangVehicle");

    t.run("GANG1 vehicle is always 134", [&] {
        std::srand(42);
        for (int i = 0; i < 30; ++i) {
            REQUIRE_EQ(GangManager::GetRandomGangVehicle(PEDTYPE_GANG1), 134);
        }
    });

    t.run("GANG2 vehicle is always 132 (BELLYUP)", [&] {
        std::srand(42);
        for (int i = 0; i < 30; ++i) {
            REQUIRE_EQ(GangManager::GetRandomGangVehicle(PEDTYPE_GANG2), 132);
        }
    });

    t.run("GANG3 vehicle is always 137", [&] {
        std::srand(42);
        for (int i = 0; i < 30; ++i) {
            REQUIRE_EQ(GangManager::GetRandomGangVehicle(PEDTYPE_GANG3), 137);
        }
    });

    t.run("GANG4 vehicle is always 136 (YAKUZA)", [&] {
        std::srand(42);
        for (int i = 0; i < 30; ++i) {
            REQUIRE_EQ(GangManager::GetRandomGangVehicle(PEDTYPE_GANG4), 136);
        }
    });

    t.run("GANG5 vehicle is always 138 (COLUMB)", [&] {
        std::srand(42);
        for (int i = 0; i < 30; ++i) {
            REQUIRE_EQ(GangManager::GetRandomGangVehicle(PEDTYPE_GANG5), 138);
        }
    });

    t.run("GANG6 vehicle is always 135 (YARDIE)", [&] {
        std::srand(42);
        for (int i = 0; i < 30; ++i) {
            REQUIRE_EQ(GangManager::GetRandomGangVehicle(PEDTYPE_GANG6), 135);
        }
    });

    t.run("returns -1 for unregistered gang type", [&] {
        REQUIRE_EQ(GangManager::GetRandomGangVehicle(PEDTYPE_COP),   -1);
        REQUIRE_EQ(GangManager::GetRandomGangVehicle(PEDTYPE_GANG9), -1);
    });

    t.run("returns -1 when vehicle list cleared", [&] {
        auto saved = GangManager::s_gangs[2].vehicleModelIds;
        GangManager::s_gangs[2].vehicleModelIds.clear();
        REQUIRE_EQ(GangManager::GetRandomGangVehicle(PEDTYPE_GANG3), -1);
        GangManager::s_gangs[2].vehicleModelIds = saved; // restore
    });

    // ------------------------------------------------------------------
    // GetAmbientCivilianModelIds — sanity checks
    // ------------------------------------------------------------------
    t.suite("GangManager – GetAmbientCivilianModelIds");

    t.run("civilian list is non-empty", [&] {
        REQUIRE(GangManager::GetAmbientCivilianModelIds().size() > 0);
    });

    t.run("civilian list contains no gang ped models", [&] {
        const auto& civs = GangManager::GetAmbientCivilianModelIds();
        for (int id : civs) {
            REQUIRE_FALSE(GangManager::IsGangModelId(id));
        }
    });

    t.run("civilian list contains no gang vehicle models", [&] {
        const auto& civs = GangManager::GetAmbientCivilianModelIds();
        for (int id : civs) {
            REQUIRE_FALSE(GangManager::IsGangVehicleModel(id));
        }
    });

    // ------------------------------------------------------------------
    // IsValidGangType
    // ------------------------------------------------------------------
    t.suite("GangManager – IsValidGangType");

    t.run("GANG1-6 are valid", [&] {
        REQUIRE(GangManager::IsValidGangType(PEDTYPE_GANG1));
        REQUIRE(GangManager::IsValidGangType(PEDTYPE_GANG2));
        REQUIRE(GangManager::IsValidGangType(PEDTYPE_GANG3));
        REQUIRE(GangManager::IsValidGangType(PEDTYPE_GANG4));
        REQUIRE(GangManager::IsValidGangType(PEDTYPE_GANG5));
        REQUIRE(GangManager::IsValidGangType(PEDTYPE_GANG6));
    });

    t.run("GANG7-9 are not valid", [&] {
        REQUIRE_FALSE(GangManager::IsValidGangType(PEDTYPE_GANG7));
        REQUIRE_FALSE(GangManager::IsValidGangType(PEDTYPE_GANG8));
        REQUIRE_FALSE(GangManager::IsValidGangType(PEDTYPE_GANG9));
    });

    t.run("non-gang types are not valid", [&] {
        REQUIRE_FALSE(GangManager::IsValidGangType(PEDTYPE_PLAYER1));
        REQUIRE_FALSE(GangManager::IsValidGangType(PEDTYPE_CIVMALE));
        REQUIRE_FALSE(GangManager::IsValidGangType(PEDTYPE_COP));
    });
}
