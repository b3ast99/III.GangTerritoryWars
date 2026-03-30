#pragma once
// Minimal unit test framework — no external dependencies.
// Replace with Catch2/GTest later if desired; API is intentionally similar.

#include <iostream>
#include <functional>
#include <string>
#include <stdexcept>
#include <sstream>

// -----------------------------------------------------------------------
// Assertions
// -----------------------------------------------------------------------
#define REQUIRE(cond) \
    do { \
        if (!(cond)) { \
            std::ostringstream _msg; \
            _msg << "REQUIRE(" #cond ") failed at " << __FILE__ << ":" << __LINE__; \
            throw std::runtime_error(_msg.str()); \
        } \
    } while(0)

#define REQUIRE_EQ(a, b) \
    do { \
        auto _a = (a); auto _b = (b); \
        if (!(_a == _b)) { \
            std::ostringstream _msg; \
            _msg << "REQUIRE_EQ(" #a ", " #b ") failed: " \
                 << _a << " != " << _b \
                 << " at " << __FILE__ << ":" << __LINE__; \
            throw std::runtime_error(_msg.str()); \
        } \
    } while(0)

#define REQUIRE_NE(a, b) \
    do { \
        auto _a = (a); auto _b = (b); \
        if (_a == _b) { \
            std::ostringstream _msg; \
            _msg << "REQUIRE_NE(" #a ", " #b ") failed: both == " \
                 << _a << " at " << __FILE__ << ":" << __LINE__; \
            throw std::runtime_error(_msg.str()); \
        } \
    } while(0)

#define REQUIRE_FALSE(cond) REQUIRE(!(cond))

// -----------------------------------------------------------------------
// Test runner
// -----------------------------------------------------------------------
namespace Test {

struct Runner {
    int passed  = 0;
    int failed  = 0;
    int suites  = 0;
    const char* currentSuite = "";

    void suite(const char* name) {
        currentSuite = name;
        ++suites;
        std::cout << "\n[suite] " << name << "\n";
    }

    void run(const char* name, std::function<void()> fn) {
        try {
            fn();
            ++passed;
            std::cout << "  PASS  " << name << "\n";
        } catch (const std::exception& e) {
            ++failed;
            std::cout << "  FAIL  " << name << "\n"
                      << "        " << e.what() << "\n";
        } catch (...) {
            ++failed;
            std::cout << "  FAIL  " << name << "\n"
                      << "        (unknown exception)\n";
        }
    }

    int report() const {
        std::cout << "\n========================================\n";
        std::cout << "  " << passed << " passed  |  " << failed << " failed"
                  << "  (" << suites << " suites)\n";
        std::cout << "========================================\n";
        return (failed > 0) ? 1 : 0;
    }
};

} // namespace Test
