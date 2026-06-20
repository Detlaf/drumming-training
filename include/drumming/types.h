#pragma once
#include <array>
#include <chrono>
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include "drumming/color.h"
#include "drumming/constants.h"

namespace drumming {

struct Voice {
    int         midiNote;
    const char* name;
    float       yOff;   // pixels from STAFF_TOP_Y; negative = above staff
    bool        xHead;  // true = cymbal X-notehead
    Color       color;
};

inline constexpr int NUM_VOICES = 9;
extern const std::array<Voice, NUM_VOICES> VOICES;

int voiceIndex(int midiNote);

enum class Screen : std::uint8_t { HOME, LIBRARY, STATS, EDITOR, PLAY };

// How a single hit relates to the groove at its snapped step:
//   Correct          right pad, within HIT_WINDOW_MS
//   EarlyCorrectPad  right pad, struck too early (offset <= -HIT_WINDOW_MS)
//   LateCorrectPad   right pad, struck too late  (offset >=  HIT_WINDOW_MS)
//   WrongPad         no groove note on that {step,voice} (incl. unknown pad)
enum class HitClass : std::uint8_t { Correct, EarlyCorrectPad, LateCorrectPad, WrongPad };

struct HitResult {
    int      step, voice;
    float    offsetMs;
    bool     correct;
    HitClass cls;
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
    long long timestampEpoch;   // kept = startedAtEpoch for back-compat
    int durationSecs;
    // Hit breakdown (spec 2)
    int earlyHits = 0, lateHits = 0, wrongPadHits = 0;
    // Session start/end wall-clock epochs (spec 1)
    long long startedAtEpoch = 0, endedAtEpoch = 0;
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
    long long sessionStartEpoch = 0;
    // Hit tally accumulated across the whole active session (the per-loop
    // `results` buffer is cleared each loop for the live strip, so session totals
    // are kept separately here).
    int sCorrect = 0, sEarly = 0, sLate = 0, sWrong = 0;
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
