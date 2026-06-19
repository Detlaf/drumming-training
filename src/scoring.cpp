#include "drumming/scoring.h"
#include "drumming/constants.h"
#include <cmath>

namespace drumming {

HitResult scoreHit(float ms,
                   const std::set<std::pair<int, int>>& groove,
                   int voice,
                   int totalSteps,
                   float stepDurMs) {
    // Grid position the hit is nearest to — used to place its marker and as the
    // location reported for wrong-pad hits.
    float stepF      = ms / stepDurMs;
    int   nearest    = (((int)std::round(stepF)) % totalSteps + totalSteps) % totalSteps;
    float gridOffset = (stepF - std::round(stepF)) * stepDurMs;

    // Classify by timing relative to the nearest groove note *of the same voice*
    // rather than the nearest grid step. Snapping to the grid would misclassify a
    // merely mistimed hit (early/late) as a wrong-pad hit at fast tempos, where
    // the hit window can exceed half a step and a late hit rounds onto an empty
    // neighbouring step. Matching against the actual note keeps early/late
    // detection correct at every tempo. The trade-off: striking a pad that the
    // groove uses elsewhere always counts as a (mistimed) correct-pad hit, so the
    // wrong-pad bucket means "a pad the groove never plays".
    const float loopMs  = (float)totalSteps * stepDurMs;
    float       wrapped = std::fmod(ms, loopMs);
    if (wrapped < 0.f) wrapped += loopMs;

    bool  found      = false;
    int   matchStep  = nearest;
    float bestOffset = 0.f;
    for (const auto& [s, v] : groove) {
        if (v != voice) continue;
        // Signed distance hit-minus-note, wrapped into [-loopMs/2, loopMs/2) so a
        // hit just before the downbeat matches the note at the start of the bar.
        float d = wrapped - (float)s * stepDurMs;
        if (d >=  loopMs / 2.f) d -= loopMs;
        if (d <  -loopMs / 2.f) d += loopMs;
        if (!found || std::abs(d) < std::abs(bestOffset)) {
            bestOffset = d;
            matchStep  = s;
            found      = true;
        }
    }

    if (!found) {
        // No note for this pad anywhere in the groove (includes unknown pad,
        // voice == -1): a genuine wrong-pad hit.
        return {nearest, voice, gridOffset, false, HitClass::WrongPad};
    }

    HitClass cls;
    if (std::abs(bestOffset) < HIT_WINDOW_MS) cls = HitClass::Correct;
    else if (bestOffset < 0.f)                cls = HitClass::EarlyCorrectPad;
    else                                      cls = HitClass::LateCorrectPad;

    return {matchStep, voice, bestOffset, cls == HitClass::Correct, cls};
}

float accuracyPct(int correctHits, int totalNotes) {
    return totalNotes > 0 ? 100.f * correctHits / totalNotes : 0.f;
}

} // namespace drumming
