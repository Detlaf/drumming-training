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
    bool  correct  = groove.count({nearest, voice}) &&
                     std::abs(offsetMs) < HIT_WINDOW_MS;
    return {nearest, voice, offsetMs, correct};
}

float accuracyPct(int correctHits, int totalNotes) {
    return totalNotes > 0 ? 100.f * correctHits / totalNotes : 0.f;
}

} // namespace drumming
