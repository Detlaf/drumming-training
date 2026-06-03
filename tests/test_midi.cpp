#include <catch2/catch_test_macros.hpp>
#include <vector>
#include "drumming/midi.h"
#include "drumming/types.h"

using namespace drumming;

static std::vector<unsigned char> makeMsg(unsigned char status, unsigned char note, unsigned char vel) {
    return {status, note, vel};
}

// Drain any leftover state from previous tests
static void drain() { pollMidi(); }

TEST_CASE("midiCallback - note-on events are queued", "[midi]") {
    drain();

    std::vector<unsigned char> msg = makeMsg(0x90, 38, 100); // snare, ch1
    midiCallback(0.0, &msg, nullptr);

    auto evs = pollMidi();
    REQUIRE(evs.size() == 1);
    CHECK(evs[0].note == 38);
    CHECK(evs[0].vel  == 100);
}

TEST_CASE("midiCallback - note-on with vel=0 is ignored (note-off convention)", "[midi]") {
    drain();

    std::vector<unsigned char> msg = makeMsg(0x90, 38, 0);
    midiCallback(0.0, &msg, nullptr);

    CHECK(pollMidi().empty());
}

TEST_CASE("midiCallback - non-note-on status byte is ignored", "[midi]") {
    drain();

    std::vector<unsigned char> noteOff  = makeMsg(0x80, 38, 64);
    std::vector<unsigned char> cc       = makeMsg(0xB0, 7, 127);
    std::vector<unsigned char> pitchBnd = makeMsg(0xE0, 0, 64);
    midiCallback(0.0, &noteOff,  nullptr);
    midiCallback(0.0, &cc,       nullptr);
    midiCallback(0.0, &pitchBnd, nullptr);

    CHECK(pollMidi().empty());
}

TEST_CASE("midiCallback - short message (< 3 bytes) is ignored", "[midi]") {
    drain();

    std::vector<unsigned char> msg = {0x90, 38}; // only 2 bytes
    midiCallback(0.0, &msg, nullptr);

    CHECK(pollMidi().empty());
}

TEST_CASE("midiCallback - note-on on any channel is accepted", "[midi]") {
    drain();

    // Channel 10 (0-indexed 9) is the standard drum channel — status 0x99
    std::vector<unsigned char> msg = makeMsg(0x99, 36, 80); // bass drum
    midiCallback(0.0, &msg, nullptr);

    auto evs = pollMidi();
    REQUIRE(evs.size() == 1);
    CHECK(evs[0].note == 36);
}

TEST_CASE("pollMidi - clears the queue after returning events", "[midi]") {
    drain();

    std::vector<unsigned char> msg = makeMsg(0x90, 42, 90);
    midiCallback(0.0, &msg, nullptr);

    auto first  = pollMidi();
    auto second = pollMidi();

    REQUIRE(first.size()  == 1);
    CHECK(second.empty());
}

TEST_CASE("pollMidi - multiple queued events returned in order", "[midi]") {
    drain();

    std::vector<unsigned char> m1 = makeMsg(0x90, 38, 100); // snare
    std::vector<unsigned char> m2 = makeMsg(0x90, 36,  80); // bass
    std::vector<unsigned char> m3 = makeMsg(0x90, 42,  64); // hi-hat
    midiCallback(0.0, &m1, nullptr);
    midiCallback(0.0, &m2, nullptr);
    midiCallback(0.0, &m3, nullptr);

    auto evs = pollMidi();
    REQUIRE(evs.size() == 3);
    CHECK(evs[0].note == 38);
    CHECK(evs[1].note == 36);
    CHECK(evs[2].note == 42);
}

// ── Pad lookup ────────────────────────────────────────────────────────────────

static const KitPad* findPad(int note) {
    for (int i = 0; i < NUM_KIT_PADS; ++i)
        if (KIT_PADS[i].note == note) return &KIT_PADS[i];
    return nullptr;
}

TEST_CASE("KIT_PADS - note-on for snare resolves to Snare pad", "[midi][pads]") {
    drain();

    std::vector<unsigned char> msg = makeMsg(0x90, 38, 100);
    midiCallback(0.0, &msg, nullptr);

    auto evs = pollMidi();
    REQUIRE(evs.size() == 1);

    const KitPad* pad = findPad(evs[0].note);
    REQUIRE(pad != nullptr);
    CHECK(std::string(pad->name) == "Snare");
}

TEST_CASE("KIT_PADS - every KIT_PAD note is found exactly once", "[pads]") {
    for (int i = 0; i < NUM_KIT_PADS; ++i) {
        int count = 0;
        for (int j = 0; j < NUM_KIT_PADS; ++j)
            if (KIT_PADS[j].note == KIT_PADS[i].note) ++count;
        INFO("Duplicate note " << KIT_PADS[i].note << " (" << KIT_PADS[i].name << ")");
        CHECK(count == 1);
    }
}

TEST_CASE("KIT_PADS - known drum notes map to expected pad names", "[pads]") {
    struct { int note; const char* name; } expected[] = {
        {36, "Bass"},
        {38, "Snare"},
        {42, "Hi-Hat"},
        {44, "HH Ped"},
        {45, "Mid Tom"},
        {48, "Hi Tom"},
        {43, "Lo Tom"},
        {49, "Crash"},
        {51, "Ride"},
    };
    for (auto& e : expected) {
        const KitPad* pad = findPad(e.note);
        INFO("note " << e.note);
        REQUIRE(pad != nullptr);
        CHECK(std::string(pad->name) == e.name);
    }
}

TEST_CASE("KIT_PADS - unknown note returns null pad", "[pads]") {
    CHECK(findPad(99) == nullptr);
    CHECK(findPad(0)  == nullptr);
}
