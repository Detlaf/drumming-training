#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <set>
#include <utility>
#include "drumming/scoring.h"
#include "drumming/constants.h"

using namespace drumming;
using Catch::Matchers::WithinAbs;

// A simple groove with a single note: snare (voice 5) on step 4.
static std::set<std::pair<int, int>> grooveAt(int step, int voice) {
    return {{step, voice}};
}

// ── scoreHit: step snapping ─────────────────────────────────────────────────

TEST_CASE("scoreHit - exact on-time hit snaps to that step with zero offset", "[scoring]") {
    const float dur = 125.f; // ms per step (120bpm, 4 steps/beat → 500/4)
    // Hit exactly on step 4.
    HitResult r = scoreHit(4 * dur, grooveAt(4, 5), 5, 16, dur);
    CHECK(r.step  == 4);
    CHECK(r.voice == 5);
    CHECK_THAT(r.offsetMs, WithinAbs(0.f, 1e-3f));
    CHECK(r.correct);
}

TEST_CASE("scoreHit - hit slightly early snaps to nearest step with negative offset", "[scoring]") {
    const float dur = 125.f;
    HitResult r = scoreHit(4 * dur - 20.f, grooveAt(4, 5), 5, 16, dur);
    CHECK(r.step == 4);
    CHECK_THAT(r.offsetMs, WithinAbs(-20.f, 1e-3f));
    CHECK(r.correct); // within HIT_WINDOW_MS (80ms)
}

TEST_CASE("scoreHit - hit slightly late snaps to nearest step with positive offset", "[scoring]") {
    const float dur = 125.f;
    HitResult r = scoreHit(4 * dur + 20.f, grooveAt(4, 5), 5, 16, dur);
    CHECK(r.step == 4);
    CHECK_THAT(r.offsetMs, WithinAbs(20.f, 1e-3f));
    CHECK(r.correct);
}

TEST_CASE("scoreHit - hit closer to the next step snaps forward", "[scoring]") {
    const float dur = 125.f;
    // 60% of the way from step 4 to step 5 → rounds to step 5.
    HitResult r = scoreHit(4 * dur + 0.6f * dur, grooveAt(5, 5), 5, 16, dur);
    CHECK(r.step == 5);
}

// ── scoreHit: correctness window ────────────────────────────────────────────

TEST_CASE("scoreHit - offset beyond HIT_WINDOW_MS is incorrect even on a groove cell", "[scoring]") {
    const float dur = 125.f; // half a step (62.5ms) is within window; push past 80ms.
    // Offset of +0.7*dur = 87.5ms but still rounds to step 4 (since <0.5 would
    // round down; 0.7 rounds up). Use a 200ms/step grid so a near-step offset
    // exceeds the window while still snapping to the same step.
    const float slow = 200.f;
    HitResult r = scoreHit(4 * slow + 90.f, grooveAt(4, 5), 5, 16, slow);
    CHECK(r.step == 4);
    CHECK_THAT(r.offsetMs, WithinAbs(90.f, 1e-3f));
    CHECK_FALSE(r.correct); // 90ms > HIT_WINDOW_MS (80ms)
}

TEST_CASE("scoreHit - on-time hit on an empty cell is incorrect", "[scoring]") {
    const float dur = 125.f;
    // Groove has the note on a different voice; hitting voice 5 here is wrong.
    HitResult r = scoreHit(4 * dur, grooveAt(4, 3), 5, 16, dur);
    CHECK(r.step == 4);
    CHECK_THAT(r.offsetMs, WithinAbs(0.f, 1e-3f));
    CHECK_FALSE(r.correct);
}

TEST_CASE("scoreHit - wrong voice on a populated step is incorrect", "[scoring]") {
    const float dur = 125.f;
    HitResult r = scoreHit(4 * dur, grooveAt(4, 5), 3, 16, dur);
    CHECK(r.voice == 3);
    CHECK_FALSE(r.correct);
}

// ── scoreHit: loop wrap-around ──────────────────────────────────────────────

TEST_CASE("scoreHit - step index wraps modulo totalSteps across loops", "[scoring]") {
    const float dur = 125.f;
    // Step 20 in a 16-step groove wraps to step 4.
    HitResult r = scoreHit(20 * dur, grooveAt(4, 5), 5, 16, dur);
    CHECK(r.step == 4);
    CHECK(r.correct);
}

TEST_CASE("scoreHit - hit on the final step of a loop does not wrap", "[scoring]") {
    const float dur = 125.f;
    HitResult r = scoreHit(15 * dur, grooveAt(15, 5), 5, 16, dur);
    CHECK(r.step == 15);
    CHECK(r.correct);
}

// ── accuracyPct ─────────────────────────────────────────────────────────────

TEST_CASE("accuracyPct - zero notes yields 0", "[scoring]") {
    CHECK_THAT(accuracyPct(0, 0), WithinAbs(0.f, 1e-4f));
}

TEST_CASE("accuracyPct - all correct yields 100", "[scoring]") {
    CHECK_THAT(accuracyPct(8, 8), WithinAbs(100.f, 1e-4f));
}

TEST_CASE("accuracyPct - partial correctness", "[scoring]") {
    CHECK_THAT(accuracyPct(3, 4), WithinAbs(75.f, 1e-4f));
    CHECK_THAT(accuracyPct(1, 3), WithinAbs(33.3333f, 1e-3f));
}

TEST_CASE("accuracyPct - none correct yields 0 with notes present", "[scoring]") {
    CHECK_THAT(accuracyPct(0, 5), WithinAbs(0.f, 1e-4f));
}
