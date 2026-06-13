#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "drumming/types.h"
#include "drumming/constants.h"

using namespace drumming;
using Catch::Matchers::WithinAbs;

// ── App::totalSteps ─────────────────────────────────────────────────────────

TEST_CASE("App::totalSteps - measures times STEPS_PER_MEASURE", "[types][app]") {
    App g;
    g.measures = 1;
    CHECK(g.totalSteps() == STEPS_PER_MEASURE);
    g.measures = 2;
    CHECK(g.totalSteps() == 2 * STEPS_PER_MEASURE);
    g.measures = 4;
    CHECK(g.totalSteps() == 4 * STEPS_PER_MEASURE);
}

TEST_CASE("App::totalSteps - default app has 2 measures", "[types][app]") {
    App g;
    CHECK(g.totalSteps() == 2 * STEPS_PER_MEASURE);
}

// ── App::stepDurMs ──────────────────────────────────────────────────────────

TEST_CASE("App::stepDurMs - one 16th note duration at the given tempo", "[types][app]") {
    App g;
    g.bpm = 120; // quarter = 500ms, 16th = 125ms
    CHECK_THAT(g.stepDurMs(), WithinAbs(125.f, 1e-3f));

    g.bpm = 60;  // quarter = 1000ms, 16th = 250ms
    CHECK_THAT(g.stepDurMs(), WithinAbs(250.f, 1e-3f));

    g.bpm = 100; // quarter = 600ms, 16th = 150ms
    CHECK_THAT(g.stepDurMs(), WithinAbs(150.f, 1e-3f));
}

TEST_CASE("App::stepDurMs - faster tempo means shorter steps", "[types][app]") {
    App slow, fast;
    slow.bpm = 80;
    fast.bpm = 160;
    CHECK(fast.stepDurMs() < slow.stepDurMs());
    // Doubling tempo halves the step duration.
    CHECK_THAT(fast.stepDurMs(), WithinAbs(slow.stepDurMs() / 2.f, 1e-3f));
}
