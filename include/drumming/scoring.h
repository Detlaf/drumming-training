#pragma once
#include <set>
#include <utility>
#include "drumming/types.h"

namespace drumming {

// Score a single MIDI hit against the groove.
//
// `ms` is the elapsed time since the loop started, `stepDurMs` the duration of
// one step. The hit is matched to the nearest groove note of the same voice
// (wrapping around the loop) and classified by its timing offset to that note:
// Correct within HIT_WINDOW_MS, otherwise EarlyCorrectPad / LateCorrectPad. A
// pad the groove never plays (or an unknown pad, voice == -1) is WrongPad. The
// returned `step`/`offsetMs` describe the matched note (or the nearest grid step
// for wrong-pad hits). Matching against the note rather than snapping to the grid
// keeps early/late detection correct at fast tempos.
HitResult scoreHit(float ms,
                   const std::set<std::pair<int, int>>& groove,
                   int voice,
                   int totalSteps,
                   float stepDurMs);

// Percentage of correct hits, 0 when no notes were played.
float accuracyPct(int correctHits, int totalNotes);

} // namespace drumming
