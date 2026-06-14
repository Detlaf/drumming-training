#include "drumming/geometry.h"
#include "drumming/types.h"
#include <algorithm>
#include <cmath>

namespace drumming {

float voiceY(int vi) {
    return STAFF_TOP_Y + VOICES[vi].yOff;
}

float stepX(int step, int total) {
    return STAFF_LEFT + (step + 0.5f) * (STAFF_RIGHT - STAFF_LEFT) / total;
}

std::pair<int, int> pickCell(sf::Vector2f p, int total) {
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

std::pair<int, int> pickGridCell(sf::Vector2f p, int total) {
    if (p.y < GRID_TOP || p.y >= GRID_TOP + NUM_VOICES * GRID_ROW_H) return {-1, -1};
    if (p.x < STAFF_LEFT || p.x >= STAFF_RIGHT) return {-1, -1};

    int vi   = (int)((p.y - GRID_TOP) / GRID_ROW_H);
    float cw = (STAFF_RIGHT - STAFF_LEFT) / total;
    int step = (int)((p.x - STAFF_LEFT) / cw);
    if (vi < 0 || vi >= NUM_VOICES || step < 0 || step >= total) return {-1, -1};
    return {step, vi};
}

// ── Shared layout rects ───────────────────────────────────────────────────────

sf::FloatRect sidebarNavItemRect(int i) {
    float y = SIDEBAR_NAV_TOP + i * (SIDEBAR_NAV_ITEM_H + SIDEBAR_NAV_ITEM_GAP);
    return {{SIDEBAR_PAD, y}, {SIDEBAR_W - SIDEBAR_PAD * 2.f, SIDEBAR_NAV_ITEM_H}};
}

std::optional<Screen> sidebarNavHit(sf::Vector2f p) {
    if (p.x >= SIDEBAR_W || p.y < TITLEBAR_H) return std::nullopt;
    const Screen order[] = {Screen::EDITOR, Screen::LIBRARY, Screen::STATS};
    for (int i = 0; i < 3; ++i) {
        sf::FloatRect r = sidebarNavItemRect(i);
        if (p.y >= r.position.y && p.y < r.position.y + r.size.y) return order[i];
    }
    return std::nullopt;
}

// Home content origin and column width, shared by the cards and Resume button.
static float homeContentX() { return SIDEBAR_W + 26.f; }
static float homeColumnW()  { return (float)WINDOW_W - SIDEBAR_W - 52.f; }
static float homeCardTop()  { return TITLEBAR_H + CONTENT_HEAD_H + 22.f + 78.f; }
static constexpr float HOME_CARD_H = 112.f;

sf::FloatRect homeCardRect(int i) {
    float cardW = (homeColumnW() - 28.f) / 3.f;
    float bx    = homeContentX() + i * (cardW + 14.f);
    return {{bx, homeCardTop()}, {cardW, HOME_CARD_H}};
}

sf::FloatRect homeResumeRect() {
    float cwY  = homeCardTop() + HOME_CARD_H + 20.f;
    float btnX = homeContentX() + homeColumnW() - 114.f;
    return {{btnX, cwY + 24.f}, {100.f, 32.f}};
}

static float librarySearchY() { return TITLEBAR_H + CONTENT_HEAD_H + 14.f; }
static float libraryCol1()    { return SIDEBAR_W + 26.f; }

sf::FloatRect libraryNewGrooveRect() {
    return {{(float)WINDOW_W - 150.f, librarySearchY()}, {120.f, 34.f}};
}

float libraryListTop() { return librarySearchY() + 50.f + 24.f; }

int libraryMaxVisibleRows() {
    return std::max(1, (int)(((float)WINDOW_H - libraryListTop() - 20.f) / LIBRARY_ROW_H));
}

int libraryListStart(int scroll, int count) {
    return std::max(0, std::min(scroll, count - libraryMaxVisibleRows()));
}

int libraryRowAt(float my, int scroll, int count) {
    if (my < libraryListTop()) return -1;
    int rowFromTop = (int)((my - libraryListTop()) / LIBRARY_ROW_H);
    int idx        = libraryListStart(scroll, count) + rowFromTop;
    return (idx >= 0 && idx < count) ? idx : -1;
}

sf::FloatRect libraryDeleteRect(int rowFromTop) {
    float ry = libraryListTop() + rowFromTop * LIBRARY_ROW_H;
    return {{(float)WINDOW_W - 60.f, ry + 9.f}, {LIBRARY_DEL_BTN_W, 26.f}};
}

}
