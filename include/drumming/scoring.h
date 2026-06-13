#pragma once
#include <set>
#include <utility>
#include "drumming/types.h"

namespace drumming {

// Score a single MIDI hit against the groove.
//
// `ms` is the elapsed time since the loop started, `stepDurMs` the duration of
// one step. The hit is snapped to the nearest step (wrapped into [0, totalSteps))
// and counted as correct when the groove has a note on that {step, voice} cell
// and the timing offset is within HIT_WINDOW_MS.
HitResult scoreHit(float ms,
                   const std::set<std::pair<int, int>>& groove,
                   int voice,
                   int totalSteps,
                   float stepDurMs);

// Percentage of correct hits, 0 when no notes were played.
float accuracyPct(int correctHits, int totalNotes);

} // namespace drumming
