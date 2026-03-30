#include "TestFramework.h"
#include "../source/IniConfig.h"

void RunIniConfigTests(Test::Runner& t) {
    t.suite("IniConfig");

    t.run("reads int from correct section", [&] {
        IniConfig cfg;
        cfg.LoadFromString("[Settings]\nflash_count=5\n");
        REQUIRE_EQ(cfg.GetInt("Settings", "flash_count", 0), 5);
    });

    t.run("returns default when key missing", [&] {
        IniConfig cfg;
        cfg.LoadFromString("[Settings]\n");
        REQUIRE_EQ(cfg.GetInt("Settings", "missing", 42), 42);
    });

    t.run("returns default when section missing", [&] {
        IniConfig cfg;
        cfg.LoadFromString("[Other]\nkey=1\n");
        REQUIRE_EQ(cfg.GetInt("Settings", "key", 99), 99);
    });

    t.run("reads string value", [&] {
        IniConfig cfg;
        cfg.LoadFromString("[Logging]\nlog_level=DEBUG\n");
        REQUIRE_EQ(cfg.GetString("Logging", "log_level", "INFO"), std::string("DEBUG"));
    });

    t.run("returns string default when missing", [&] {
        IniConfig cfg;
        cfg.LoadFromString("");
        REQUIRE_EQ(cfg.GetString("Logging", "log_level", "INFO"), std::string("INFO"));
    });

    t.run("reads float value", [&] {
        IniConfig cfg;
        cfg.LoadFromString("[Physics]\nspeed=1.5\n");
        const float v = cfg.GetFloat("Physics", "speed", 0.0f);
        REQUIRE(v > 1.4f && v < 1.6f);
    });

    t.run("ignores comment lines (semicolon)", [&] {
        IniConfig cfg;
        cfg.LoadFromString("; this is a comment\n[S]\nkey=1\n");
        REQUIRE_EQ(cfg.GetInt("S", "key", 0), 1);
    });

    t.run("ignores comment lines (hash)", [&] {
        IniConfig cfg;
        cfg.LoadFromString("# also a comment\n[S]\nkey=2\n");
        REQUIRE_EQ(cfg.GetInt("S", "key", 0), 2);
    });

    t.run("trims whitespace around key and value", [&] {
        IniConfig cfg;
        cfg.LoadFromString("[S]\n  key  =  7  \n");
        REQUIRE_EQ(cfg.GetInt("S", "key", 0), 7);
    });

    t.run("multiple sections independent", [&] {
        IniConfig cfg;
        cfg.LoadFromString("[A]\nx=1\n[B]\nx=2\n");
        REQUIRE_EQ(cfg.GetInt("A", "x", 0), 1);
        REQUIRE_EQ(cfg.GetInt("B", "x", 0), 2);
    });

    t.run("empty string content returns defaults", [&] {
        IniConfig cfg;
        cfg.LoadFromString("");
        REQUIRE_EQ(cfg.GetInt("Any", "key", -1), -1);
    });
}
