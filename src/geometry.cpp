#include "drumming/geometry.h"
#include "drumming/types.h"
#include <cmath>

namespace drumming {

float voiceY(int vi) {
    return STAFF_TOP_Y + VOICES[vi].yOff;
}

float stepX(int step, int total) {
    return STAFF_LEFT + (step + 0.5f) * (STAFF_RIGHT - STAFF_LEFT) / total;
}

std::pair<int, int> pickCell(Vec2 p, int total) {
    if (p.y < STAFF_TOP_Y - 38.f || p.y > STAFF_TOP_Y + 88.f) return {-1, -1};

    float cellW = (STAFF_RIGHT - STAFF_LEFT) / total;
    int step = (int)((p.x - STAFF_LEFT) / cellW);
    if (step < 0 || step >= total) return {-1, -1};

    int   bestVi  = -1;
    float minDist = 13.f;
    for (int vi = 0; vi < NUM_VOICES; ++vi) {
        float d = std::abs(p.y - voiceY(vi));
        if (d < minDist) { minDist = d; bestVi = vi; }
    }
    return {step, bestVi};
}

std::pair<int, int> pickGridCell(Vec2 p, int total) {
    if (p.y < GRID_TOP || p.y >= GRID_TOP + NUM_VOICES * GRID_ROW_H) return {-1, -1};
    if (p.x < STAFF_LEFT || p.x >= STAFF_RIGHT) return {-1, -1};

    int vi   = (int)((p.y - GRID_TOP) / GRID_ROW_H);
    float cw = (STAFF_RIGHT - STAFF_LEFT) / total;
    int step = (int)((p.x - STAFF_LEFT) / cw);
    if (vi < 0 || vi >= NUM_VOICES || step < 0 || step >= total) return {-1, -1};
    return {step, vi};
}

}
