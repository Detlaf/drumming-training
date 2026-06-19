#include "drumming/scoring.h"
#include "drumming/constants.h"
#include <cmath>

namespace drumming {

HitResult scoreHit(float ms,
                   const std::set<std::pair<int, int>>& groove,
                   int voice,
                   int totalSteps,
                   float stepDurMs) {
    float stepF    = ms / stepDurMs;
    int   nearest  = (int)std::round(stepF) % totalSteps;
    float offsetMs = (stepF - std::round(stepF)) * stepDurMs;

    HitClass cls;
    if (groove.count({nearest, voice}) > 0) {
        if (std::abs(offsetMs) < HIT_WINDOW_MS) cls = HitClass::Correct;
        else if (offsetMs < 0)                  cls = HitClass::EarlyCorrectPad;
        else                                    cls = HitClass::LateCorrectPad;
    } else {
        cls = HitClass::WrongPad;  // includes unknown pad (voice == -1)
    }
    bool correct = (cls == HitClass::Correct);
    return {nearest, voice, offsetMs, correct, cls};
}

float accuracyPct(int correctHits, int totalNotes) {
    return totalNotes > 0 ? 100.f * correctHits / totalNotes : 0.f;
}

} // namespace drumming
