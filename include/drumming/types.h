#pragma once
#include <SFML/Graphics.hpp>
#include <chrono>
#include <map>
#include <set>
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

extern const Voice VOICES[];
extern const int   NUM_VOICES;

int voiceIndex(int midiNote);

struct KitPad {
    int          note;
    const char*  name;
    sf::Vector2f pos;
    float        r;
    sf::Color    col;
};

extern const KitPad KIT_PADS[];
extern const int    NUM_KIT_PADS;

enum class Mode { EDIT, PLAY };

struct HitResult {
    int   step, voice;
    float offsetMs;
    bool  correct;
};

struct MidiEv {
    int  note, vel;
    std::chrono::steady_clock::time_point time;
};

struct App {
    Mode mode     = Mode::EDIT;
    int  bpm      = 100;
    int  measures = 2;

    std::set<std::pair<int, int>> groove; // {step, voiceIdx}

    std::chrono::steady_clock::time_point playStart;
    int currentStep   = -1;
    int lastClickStep = -1;
    int loopCount     = 0;
    std::vector<HitResult> results;

    std::map<int, std::chrono::steady_clock::time_point> lastHit;
    std::map<int, int> lastVel;

    int   totalSteps() const { return measures * STEPS_PER_MEASURE; }
    float stepDurMs()  const { return 60000.f / bpm / STEPS_PER_BEAT; }
};

} // namespace drumming
