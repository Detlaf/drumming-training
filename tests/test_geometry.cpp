#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <SFML/System/Vector2.hpp>
#include "drumming/geometry.h"
#include "drumming/constants.h"
#include "drumming/types.h"

using namespace drumming;
using Catch::Matchers::WithinAbs;

// ── voiceY ────────────────────────────────────────────────────────────────────

TEST_CASE("voiceY - returns STAFF_TOP_Y + voice yOff", "[geometry]") {
    for (int i = 0; i < NUM_VOICES; ++i) {
        float expected = STAFF_TOP_Y + VOICES[i].yOff;
        CHECK_THAT(voiceY(i), WithinAbs(expected, 1e-4f));
    }
}

TEST_CASE("voiceY - known spot-checks", "[geometry]") {
    // Crash: vi=0, yOff=-30 → y=60
    CHECK_THAT(voiceY(0), WithinAbs(STAFF_TOP_Y - 30.f, 1e-4f));
    // Snare: vi=5, yOff=30 → y=120
    CHECK_THAT(voiceY(5), WithinAbs(STAFF_TOP_Y + 30.f, 1e-4f));
    // HH Ped: vi=8, yOff=78 → y=168
    CHECK_THAT(voiceY(8), WithinAbs(STAFF_TOP_Y + 78.f, 1e-4f));
}

TEST_CASE("voiceY - voices are ordered top to bottom", "[geometry]") {
    for (int i = 1; i < NUM_VOICES; ++i)
        CHECK(voiceY(i) > voiceY(i - 1));
}

// ── stepX ─────────────────────────────────────────────────────────────────────

TEST_CASE("stepX - first and last steps are inside staff bounds", "[geometry]") {
    for (int total : {8, 16, 32}) {
        CHECK(stepX(0, total)        > STAFF_LEFT);
        CHECK(stepX(total - 1, total) < STAFF_RIGHT);
    }
}

TEST_CASE("stepX - steps are evenly spaced", "[geometry]") {
    const int total = 16;
    const float cellW = (STAFF_RIGHT - STAFF_LEFT) / total;
    for (int s = 1; s < total; ++s)
        CHECK_THAT(stepX(s, total) - stepX(s - 1, total), WithinAbs(cellW, 1e-3f));
}

TEST_CASE("stepX - step 0 is half a cell from the left edge", "[geometry]") {
    for (int total : {8, 16, 32}) {
        float halfCell = (STAFF_RIGHT - STAFF_LEFT) / (2.f * total);
        CHECK_THAT(stepX(0, total), WithinAbs(STAFF_LEFT + halfCell, 1e-3f));
    }
}

TEST_CASE("stepX - last step is half a cell from the right edge", "[geometry]") {
    for (int total : {8, 16, 32}) {
        float halfCell = (STAFF_RIGHT - STAFF_LEFT) / (2.f * total);
        CHECK_THAT(stepX(total - 1, total), WithinAbs(STAFF_RIGHT - halfCell, 1e-3f));
    }
}

// ── pickCell ──────────────────────────────────────────────────────────────────

static sf::Vector2f pt(float x, float y) { return {x, y}; }

// y sentinel values
static constexpr float Y_ABOVE = STAFF_TOP_Y - 39.f;  // just above valid band
static constexpr float Y_BELOW = STAFF_TOP_Y + 89.f;  // just below valid band
static constexpr float Y_SNARE = STAFF_TOP_Y + 30.f;  // Snare row (vi=5)
static constexpr float Y_CRASH = STAFF_TOP_Y - 30.f;  // Crash row (vi=0)

TEST_CASE("pickCell - y above staff returns {-1,-1}", "[geometry]") {
    CHECK(pickCell(pt(STAFF_LEFT + 10.f, Y_ABOVE), 16) == std::make_pair(-1, -1));
}

TEST_CASE("pickCell - y below staff returns {-1,-1}", "[geometry]") {
    CHECK(pickCell(pt(STAFF_LEFT + 10.f, Y_BELOW), 16) == std::make_pair(-1, -1));
}

TEST_CASE("pickCell - x left of staff returns {-1,-1}", "[geometry]") {
    // Must be more than one full cell to the left — C++ truncation of small
    // negatives toward zero gives step=0, not step=-1.
    float cellW = (STAFF_RIGHT - STAFF_LEFT) / 16.f;
    CHECK(pickCell(pt(STAFF_LEFT - cellW - 1.f, Y_SNARE), 16) == std::make_pair(-1, -1));
}

TEST_CASE("pickCell - x right of staff returns {-1,-1}", "[geometry]") {
    CHECK(pickCell(pt(STAFF_RIGHT + 1.f, Y_SNARE), 16) == std::make_pair(-1, -1));
}

TEST_CASE("pickCell - center of step 0 maps to step 0", "[geometry]") {
    const int total = 16;
    auto [step, vi] = pickCell(pt(stepX(0, total), Y_SNARE), total);
    CHECK(step == 0);
    CHECK(vi   == 5); // Snare
}

TEST_CASE("pickCell - center of last step maps to last step", "[geometry]") {
    const int total = 16;
    auto [step, vi] = pickCell(pt(stepX(total - 1, total), Y_SNARE), total);
    CHECK(step == total - 1);
    CHECK(vi   == 5); // Snare
}

TEST_CASE("pickCell - every step center resolves to correct step index", "[geometry]") {
    const int total = 16;
    for (int s = 0; s < total; ++s) {
        auto [step, vi] = pickCell(pt(stepX(s, total), Y_SNARE), total);
        INFO("step " << s);
        CHECK(step == s);
    }
}

TEST_CASE("pickCell - Crash row resolves to voice 0", "[geometry]") {
    auto [step, vi] = pickCell(pt(stepX(0, 16), Y_CRASH), 16);
    CHECK(step == 0);
    CHECK(vi   == 0);
}

TEST_CASE("pickCell - HH Ped row resolves to voice 8", "[geometry]") {
    float y = STAFF_TOP_Y + 78.f; // HH Ped yOff
    auto [step, vi] = pickCell(pt(stepX(0, 16), y), 16);
    CHECK(step == 0);
    CHECK(vi   == 8);
}

TEST_CASE("pickCell - total=8 and total=32 both resolve correctly", "[geometry]") {
    for (int total : {8, 32}) {
        for (int s = 0; s < total; ++s) {
            auto [step, vi] = pickCell(pt(stepX(s, total), Y_SNARE), total);
            INFO("total=" << total << " step=" << s);
            CHECK(step == s);
        }
    }
}
