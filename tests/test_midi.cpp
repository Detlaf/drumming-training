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

TEST_CASE("midiCallback - low-velocity note-on is dropped as cross-talk", "[midi]") {
    drain();

    // A faint ghost trigger below MIN_HIT_VELOCITY (e.g. the hi-hat sensor
    // picking up a tom strike) must not be queued as a real hit.
    std::vector<unsigned char> ghost = makeMsg(0x90, 42, MIN_HIT_VELOCITY - 1);
    midiCallback(0.0, &ghost, nullptr);
    CHECK(pollMidi().empty());

    // A strike exactly at the threshold counts.
    std::vector<unsigned char> real = makeMsg(0x90, 42, MIN_HIT_VELOCITY);
    midiCallback(0.0, &real, nullptr);
    auto evs = pollMidi();
    REQUIRE(evs.size() == 1);
    CHECK(evs[0].note == 42);
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

// ── Hot-plug detection ──────────────────────────────────────────────────────────

TEST_CASE("decideMidiAction - opens when a port appears while disconnected", "[midi][hotplug]") {
    CHECK(decideMidiAction(/*currentlyConnected=*/false, /*portCount=*/1) == MidiPortAction::Open);
    CHECK(decideMidiAction(false, 3) == MidiPortAction::Open);
}

TEST_CASE("decideMidiAction - closes when the last port disappears while connected", "[midi][hotplug]") {
    CHECK(decideMidiAction(/*currentlyConnected=*/true, /*portCount=*/0) == MidiPortAction::Close);
}

TEST_CASE("decideMidiAction - no action when already connected and a port is present", "[midi][hotplug]") {
    CHECK(decideMidiAction(true, 1) == MidiPortAction::None);
    CHECK(decideMidiAction(true, 5) == MidiPortAction::None);
}

TEST_CASE("decideMidiAction - no action when disconnected and no ports", "[midi][hotplug]") {
    CHECK(decideMidiAction(false, 0) == MidiPortAction::None);
}

// ── Voice lookup ──────────────────────────────────────────────────────────────

TEST_CASE("VOICES - note-on for snare resolves to Snare voice", "[midi][voices]") {
    drain();

    std::vector<unsigned char> msg = makeMsg(0x90, 38, 100);
    midiCallback(0.0, &msg, nullptr);

    auto evs = pollMidi();
    REQUIRE(evs.size() == 1);

    int vi = voiceIndex(evs[0].note);
    REQUIRE(vi >= 0);
    CHECK(std::string(VOICES[vi].name) == "Snare");
}

TEST_CASE("VOICES - every voice note is found exactly once", "[voices]") {
    for (int i = 0; i < NUM_VOICES; ++i) {
        int count = 0;
        for (int j = 0; j < NUM_VOICES; ++j)
            if (VOICES[j].midiNote == VOICES[i].midiNote) ++count;
        INFO("Duplicate note " << VOICES[i].midiNote << " (" << VOICES[i].name << ")");
        CHECK(count == 1);
    }
}

TEST_CASE("VOICES - known drum notes map to expected voice names", "[voices]") {
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
        int vi = voiceIndex(e.note);
        INFO("note " << e.note);
        REQUIRE(vi >= 0);
        CHECK(std::string(VOICES[vi].name) == e.name);
    }
}

TEST_CASE("VOICES - unknown note returns no voice", "[voices]") {
    CHECK(voiceIndex(99) == -1);
    CHECK(voiceIndex(0)  == -1);
}

TEST_CASE("VOICES - hi-hat articulations all resolve to the Hi-Hat lane", "[voices]") {
    int hihat = voiceIndex(42); // closed (head)
    REQUIRE(hihat >= 0);
    CHECK(std::string(VOICES[hihat].name) == "Hi-Hat");

    // Open (head) and edge variants must land on the same lane, not vanish.
    CHECK(voiceIndex(46) == hihat); // open (head)
    CHECK(voiceIndex(22) == hihat); // closed edge
    CHECK(voiceIndex(26) == hihat); // open edge

    // Foot-close pedal stays on its own lane.
    int ped = voiceIndex(44);
    REQUIRE(ped >= 0);
    CHECK(std::string(VOICES[ped].name) == "HH Ped");
    CHECK(ped != hihat);
}
