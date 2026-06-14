#pragma once
#include <SFML/Graphics.hpp>
#include <array>
#include <chrono>
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include "drumming/constants.h"

namespace drumming {

struct Voice {
    int         midiNote;
    const char* name;
    float       yOff;   // pixels from STAFF_TOP_Y; negative = above staff
    bool        xHead;  // true = cymbal X-notehead
    sf::Color   color;
};

inline constexpr int NUM_VOICES = 9;
extern const std::array<Voice, NUM_VOICES> VOICES;

int voiceIndex(int midiNote);

enum class Screen : std::uint8_t { HOME, LIBRARY, STATS, EDITOR, PLAY };

struct HitResult {
    int   step, voice;
    float offsetMs;
    bool  correct;
};

struct MidiEv {
    int  note, vel;
    std::chrono::steady_clock::time_point time;
};

struct SavedGroove {
    std::string name;
    int bpm, measures;
    std::set<std::pair<int,int>> groove;
};

struct SessionRecord {
    std::string grooveName;
    int bpm, measures;
    int correctHits, totalNotes;
    float accuracyPct;
    long long timestampEpoch;
    int durationSecs;
};

struct App {
    Screen screen = Screen::HOME;
    int  bpm      = 100;
    int  measures = 2;

    std::set<std::pair<int, int>> groove; // {step, voiceIdx}

    std::chrono::steady_clock::time_point playStart;
    int currentStep   = -1;
    int lastClickStep = -1;
    int loopCount     = 0;
    std::vector<HitResult> results;

    // Library & session history
    std::vector<SavedGroove>   library;
    std::vector<SessionRecord> history;
    std::string currentGrooveName;
    bool sessionActive = false;
    std::chrono::steady_clock::time_point sessionStart;
    int libraryScroll = 0;

    // Groove naming overlay
    bool        namingMode = false;
    std::string nameBuffer;

    bool midiConnected = false;

    [[nodiscard]] int   totalSteps() const { return measures * STEPS_PER_MEASURE; }
    [[nodiscard]] float stepDurMs()  const {
        return 60000.f / static_cast<float>(bpm) / static_cast<float>(STEPS_PER_BEAT);
    }
};

} // namespace drumming
