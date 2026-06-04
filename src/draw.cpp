#include "drumming/draw.h"
#include "drumming/geometry.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <ctime>
#include <string>

namespace drumming {

// ── Theme colors ─────────────────────────────────────────────────────────────
static const sf::Color INK    {28,  28,  30 };
static const sf::Color INK2   {107, 107, 112};
static const sf::Color INK3   {163, 163, 168};
static const sf::Color BG     {255, 255, 255};
static const sf::Color BG2    {247, 247, 248};
static const sf::Color BG3    {240, 240, 241};
static const sf::Color LINE_C {230, 230, 232};
static const sf::Color HAIR_C {237, 237, 238};
static const sf::Color ACCENT {68,  110, 183};
static const sf::Color GOOD_C {50,  170, 110};
static const sf::Color WARN_C {200, 160, 50 };
static const sf::Color BAD_C  {200, 70,  60 };

// ── Draw helpers ──────────────────────────────────────────────────────────────
static void fillRect(sf::RenderWindow& w, float x, float y, float wd, float ht, sf::Color c) {
    sf::RectangleShape r({wd, ht});
    r.setPosition({x, y});
    r.setFillColor(c);
    w.draw(r);
}

static void strokeRect(sf::RenderWindow& w, float x, float y, float wd, float ht, sf::Color c, float thick = 1.f) {
    sf::RectangleShape r({wd - thick, ht - thick});
    r.setPosition({x + thick / 2.f, y + thick / 2.f});
    r.setFillColor(sf::Color::Transparent);
    r.setOutlineColor(c);
    r.setOutlineThickness(thick);
    w.draw(r);
}

static void hline(sf::RenderWindow& w, float x1, float x2, float y, sf::Color c) {
    sf::RectangleShape r({x2 - x1, 1.f});
    r.setPosition({x1, y});
    r.setFillColor(c);
    w.draw(r);
}

static void vline(sf::RenderWindow& w, float x, float y1, float y2, sf::Color c) {
    sf::RectangleShape r({1.f, y2 - y1});
    r.setPosition({x, y1});
    r.setFillColor(c);
    w.draw(r);
}

static void drawText(sf::RenderWindow& w, const sf::Font& font, const std::string& s,
                     float x, float y, unsigned sz, sf::Color c) {
    sf::Text t(font, s, sz);
    t.setFillColor(c);
    t.setPosition({x, y});
    w.draw(t);
}

static void drawTextCenter(sf::RenderWindow& w, const sf::Font& font, const std::string& s,
                           float cx, float y, unsigned sz, sf::Color c) {
    sf::Text t(font, s, sz);
    t.setFillColor(c);
    auto b = t.getLocalBounds();
    t.setPosition({cx - b.size.x / 2.f - b.position.x, y});
    w.draw(t);
}

// ── drawNote ──────────────────────────────────────────────────────────────────
void drawNote(sf::RenderWindow& w, float x, float y, const Voice& v, sf::Color c) {
    const float R = 5.f;
    if (v.xHead) {
        float d = R * 0.85f;
        for (float angle : {45.f, -45.f}) {
            sf::RectangleShape bar({d * 2.f, 2.f});
            bar.setOrigin({d, 1.f});
            bar.setPosition({x, y});
            bar.setRotation(sf::degrees(angle));
            bar.setFillColor(c);
            w.draw(bar);
        }
    } else {
        sf::CircleShape sh(R);
        sh.setOrigin({R, R});
        sh.setPosition({x, y});
        sh.setFillColor(c);
        w.draw(sh);
    }

    bool  down = v.yOff > 55.f;
    float sx   = down ? x - R : x + R;
    float sy2  = down ? y + 22.f : y - 22.f;
    sf::RectangleShape stem({1.5f, 22.f});
    stem.setFillColor(c);
    stem.setPosition({sx, std::min(y, sy2)});
    w.draw(stem);

    // ledger lines above staff
    if (v.yOff < 0.f) {
        float ly = STAFF_TOP_Y - LINE_SP;
        if (y <= ly + 2.f) {
            sf::RectangleShape ll({14.f, 1.f});
            ll.setPosition({x - 7.f, ly});
            ll.setFillColor(sf::Color(163, 163, 168, 180));
            w.draw(ll);
        }
        if (y <= STAFF_TOP_Y - 2.f * LINE_SP + 2.f) {
            sf::RectangleShape ll({14.f, 1.f});
            ll.setPosition({x - 7.f, STAFF_TOP_Y - 2.f * LINE_SP});
            ll.setFillColor(sf::Color(163, 163, 168, 180));
            w.draw(ll);
        }
    }
    // ledger lines below staff
    if (v.yOff > 4.f * LINE_SP) {
        float ly = STAFF_TOP_Y + 5.f * LINE_SP;
        sf::RectangleShape ll({14.f, 1.f});
        ll.setPosition({x - 7.f, ly});
        ll.setFillColor(sf::Color(163, 163, 168, 180));
        w.draw(ll);
        if (v.yOff > 5.f * LINE_SP + LINE_SP) {
            sf::RectangleShape ll2({14.f, 1.f});
            ll2.setPosition({x - 7.f, ly + LINE_SP});
            ll2.setFillColor(sf::Color(163, 163, 168, 180));
            w.draw(ll2);
        }
    }
}

// ── drawStaff ─────────────────────────────────────────────────────────────────
void drawStaff(sf::RenderWindow& w, const sf::Font& font, const App& app, int total) {
    // 5 staff lines
    for (int l = 0; l < 5; ++l) {
        float y = STAFF_TOP_Y + l * LINE_SP;
        hline(w, STAFF_LEFT - 22.f, STAFF_RIGHT + 4.f, y, sf::Color(140, 140, 145, 160));
    }

    // Voice labels
    for (int vi = 0; vi < NUM_VOICES; ++vi)
        drawText(w, font, VOICES[vi].name, SIDEBAR_W + 10.f, voiceY(vi) - 7.f, 10, INK3);

    float gridTop = STAFF_TOP_Y - 36.f;
    float gridBot = STAFF_TOP_Y + 4.f * LINE_SP + 90.f;
    float cellW   = (STAFF_RIGHT - STAFF_LEFT) / total;

    // Vertical grid lines
    for (int s = 0; s <= total; ++s) {
        float x    = STAFF_LEFT + s * cellW;
        bool  bar  = (s % STEPS_PER_MEASURE == 0);
        bool  beat = (s % STEPS_PER_BEAT == 0);
        if (bar) {
            fillRect(w, x - 1.f, gridTop, 2.f, gridBot - gridTop, LINE_C);
        } else {
            vline(w, x, gridTop, gridBot, beat ? sf::Color(200, 200, 202, 140) : sf::Color(210, 210, 212, 80));
        }
    }

    // Beat numbers
    for (int s = 0; s < total; s += STEPS_PER_BEAT)
        drawText(w, font, std::to_string(s / STEPS_PER_BEAT + 1),
                 STAFF_LEFT + s * cellW + 2.f, gridTop - 14.f, 10, INK3);

    // Measure labels
    for (int m = 0; m < app.measures; ++m)
        drawText(w, font, "m" + std::to_string(m + 1),
                 STAFF_LEFT + m * STEPS_PER_MEASURE * cellW + 2.f, gridTop - 26.f, 10, INK2);

    // Percussion clef (two short vertical bars)
    float clefX = STAFF_LEFT - 14.f;
    fillRect(w, clefX - 3.f, STAFF_TOP_Y + LINE_SP, 2.f, 2.f * LINE_SP, INK);
    fillRect(w, clefX + 2.f, STAFF_TOP_Y + LINE_SP, 2.f, 2.f * LINE_SP, INK);
}

// ── drawGroove ────────────────────────────────────────────────────────────────
void drawGroove(sf::RenderWindow& w, const App& app, int total) {
    float cellW = (STAFF_RIGHT - STAFF_LEFT) / total;
    for (auto& [step, vi] : app.groove) {
        float x = STAFF_LEFT + (step + 0.5f) * cellW;
        drawNote(w, x, voiceY(vi), VOICES[vi], VOICES[vi].color);
    }
}

// ── drawPlayhead ──────────────────────────────────────────────────────────────
void drawPlayhead(sf::RenderWindow& w, const App& app) {
    if (app.currentStep < 0) return;
    float x   = stepX(app.currentStep, app.totalSteps());
    float top = STAFF_TOP_Y - 40.f;
    float bot = STAFF_TOP_Y + 4.f * LINE_SP + 95.f;
    fillRect(w, x - 1.f, top, 2.f, bot - top, ACCENT);
}

// ── drawResults ───────────────────────────────────────────────────────────────
void drawResults(sf::RenderWindow& w, const App& app, int total) {
    float cellW = (STAFF_RIGHT - STAFF_LEFT) / total;
    for (auto& r : app.results) {
        float     x = STAFF_LEFT + (r.step + 0.5f) * cellW;
        float     y = voiceY(r.voice);
        sf::Color c = r.correct ? GOOD_C : BAD_C;
        sf::CircleShape ring(8.f);
        ring.setOrigin({8.f, 8.f});
        ring.setPosition({x, y});
        ring.setFillColor(sf::Color::Transparent);
        ring.setOutlineThickness(2.f);
        ring.setOutlineColor(c);
        w.draw(ring);
    }
}

// ── drawKit ───────────────────────────────────────────────────────────────────
void drawKit(sf::RenderWindow& w, const sf::Font& font, const App& app) {
    // Panel background (content area only)
    fillRect(w, SIDEBAR_W, KIT_Y, (float)WINDOW_W - SIDEBAR_W, KIT_H, BG2);
    hline(w, SIDEBAR_W, (float)WINDOW_W, KIT_Y, LINE_C);

    auto        now  = std::chrono::steady_clock::now();
    const float FADE = 400.f;

    for (int i = 0; i < NUM_KIT_PADS; ++i) {
        const KitPad& p   = KIT_PADS[i];
        sf::Vector2f  pos = {p.pos.x + SIDEBAR_W, p.pos.y + KIT_Y};
        float         bright = 0.15f;

        auto it = app.lastHit.find(p.note);
        if (it != app.lastHit.end()) {
            float ms = std::chrono::duration<float, std::milli>(now - it->second).count();
            float t  = std::max(0.f, 1.f - ms / FADE);
            auto  vi = app.lastVel.find(p.note);
            float v  = vi != app.lastVel.end() ? vi->second / 127.f : 1.f;
            bright   = 0.15f + 0.85f * t * v;
        }

        sf::Color c{(uint8_t)(p.col.r * bright),
                    (uint8_t)(p.col.g * bright),
                    (uint8_t)(p.col.b * bright)};
        sf::CircleShape sh(p.r);
        sh.setOrigin({p.r, p.r});
        sh.setPosition(pos);
        sh.setFillColor(c);
        sh.setOutlineThickness(1.5f);
        sh.setOutlineColor(LINE_C);
        w.draw(sh);

        drawText(w, font, p.name, pos.x - p.r, pos.y + p.r + 4.f, 10, INK3);
    }
}

// ── drawControls ──────────────────────────────────────────────────────────────
void drawControls(sf::RenderWindow& w, const sf::Font& font, const App& app) {
    float y  = CTRL_Y + 4.f;
    float ox = SIDEBAR_W + 8.f;

    drawText(w, font, app.screen == Screen::EDITOR ? "[SPACE] Play" : "[SPACE] Stop",
             ox, y, 13, INK2);
    drawText(w, font, "BPM: " + std::to_string(app.bpm) + "  [+/-]",  ox + 150.f, y, 13, INK2);
    drawText(w, font, "Bars: " + std::to_string(app.measures) + "  [M/N]", ox + 280.f, y, 13, INK2);
    drawText(w, font, "[C] Clear",  ox + 400.f, y, 13, INK2);
    drawText(w, font, "[S] Save  [L] Library  [H] Stats", ox + 490.f, y, 13, INK3);

    if (app.screen == Screen::PLAY && !app.results.empty()) {
        int ok  = 0;
        for (auto& r : app.results) ok += r.correct;
        int pct = 100 * ok / (int)app.results.size();
        drawText(w, font,
                 "Accuracy: " + std::to_string(pct) + "%  (" +
                 std::to_string(ok) + "/" + std::to_string((int)app.results.size()) + ")",
                 ox + 720.f, y, 13, GOOD_C);
    }

    if (!app.currentGrooveName.empty())
        drawText(w, font, "\"" + app.currentGrooveName + "\"",
                 (float)WINDOW_W - 210.f, y, 13, ACCENT);
}

// ── drawTitleBar ──────────────────────────────────────────────────────────────
void drawTitleBar(sf::RenderWindow& w, const sf::Font& font) {
    fillRect(w, 0, 0, (float)WINDOW_W, TITLEBAR_H, BG2);
    hline(w, 0, (float)WINDOW_W, TITLEBAR_H - 1.f, HAIR_C);

    // Traffic lights
    const float tx = 14.f, ty = TITLEBAR_H / 2.f;
    struct { float dx; sf::Color col; } dots[] = {
        {0.f,  {255, 95,  87 }},
        {18.f, {254, 188, 46 }},
        {36.f, {40,  200, 64 }},
    };
    for (auto& d : dots) {
        sf::CircleShape c(5.5f);
        c.setOrigin({5.5f, 5.5f});
        c.setPosition({tx + d.dx, ty});
        c.setFillColor(d.col);
        w.draw(c);
    }

    drawTextCenter(w, font, "Drumming", (float)WINDOW_W / 2.f, ty - 7.f, 13, INK2);
}

// ── drawSidebar ───────────────────────────────────────────────────────────────
void drawSidebar(sf::RenderWindow& w, const sf::Font& font, const App& app) {
    float sbTop = TITLEBAR_H;
    float sbH   = (float)WINDOW_H - sbTop;

    fillRect(w, 0, sbTop, SIDEBAR_W, sbH, BG2);
    vline(w, SIDEBAR_W - 1.f, sbTop, (float)WINDOW_H, HAIR_C);

    float px = 12.f;
    float y  = sbTop + 14.f;

    // Brand mark + name
    fillRect(w, px + 8.f, y + 4.f, 24.f, 24.f, INK);
    // Simple drum/metro mark inside the box
    fillRect(w, px + 13.f, y + 9.f,  2.f, 10.f, BG);
    fillRect(w, px + 18.f, y + 7.f,  2.f, 12.f, BG);
    fillRect(w, px + 23.f, y + 11.f, 2.f, 8.f,  BG);
    drawText(w, font, "Drumming", px + 38.f, y + 7.f, 14, INK);

    y += 50.f;
    drawText(w, font, "PRACTICE", px + 8.f, y, 10, INK3);
    y += 22.f;

    // Nav items
    struct NavItem { Screen sc; const char* label; };
    NavItem items[] = {
        {Screen::EDITOR,  "Play"},
        {Screen::LIBRARY, "Library"},
        {Screen::STATS,   "Stats"},
    };
    float itemH = 34.f, itemGap = 4.f;
    for (auto& item : items) {
        bool active = (app.screen == item.sc) ||
                      (app.screen == Screen::PLAY && item.sc == Screen::EDITOR);
        if (active) {
            fillRect(w, px, y, SIDEBAR_W - px * 2.f, itemH, BG);
            strokeRect(w, px, y, SIDEBAR_W - px * 2.f, itemH, LINE_C);
        }
        drawText(w, font, item.label, px + 14.f, y + 9.f, 13, active ? INK : INK2);
        y += itemH + itemGap;
    }

    // MIDI status chip at bottom
    float statusY = sbTop + sbH - 52.f;
    fillRect(w, px, statusY, SIDEBAR_W - px * 2.f, 34.f, BG);
    strokeRect(w, px, statusY, SIDEBAR_W - px * 2.f, 34.f, LINE_C);
    sf::CircleShape dot(3.5f);
    dot.setOrigin({3.5f, 3.5f});
    dot.setPosition({px + 12.f, statusY + 17.f});
    dot.setFillColor(GOOD_C);
    w.draw(dot);
    drawText(w, font, "MIDI connected", px + 22.f, statusY + 9.f, 11, INK2);
}

// ── drawContentHeader ─────────────────────────────────────────────────────────
void drawContentHeader(sf::RenderWindow& w, const sf::Font& font, const App& app) {
    float x  = SIDEBAR_W;
    float y  = TITLEBAR_H;
    float wd = (float)WINDOW_W - x;

    fillRect(w, x, y, wd, CONTENT_HEAD_H, BG);
    hline(w, x, (float)WINDOW_W, y + CONTENT_HEAD_H - 1.f, HAIR_C);

    std::string title, sub;
    switch (app.screen) {
        case Screen::HOME:
            title = "Home"; break;
        case Screen::LIBRARY:
            title = "Library"; break;
        case Screen::STATS:
            title = "Stats"; sub = "Last 30 days"; break;
        case Screen::EDITOR:
            title = app.currentGrooveName.empty() ? "Editor" : app.currentGrooveName;
            sub   = "Edit groove · " + std::to_string(app.bpm) + " BPM";
            break;
        case Screen::PLAY:
            title = app.currentGrooveName.empty() ? "Play" : app.currentGrooveName;
            sub   = "Play along · " + std::to_string(app.bpm) + " BPM";
            break;
    }

    drawText(w, font, title, x + 26.f, y + 9.f, 17, INK);
    if (!sub.empty())
        drawText(w, font, sub, x + 26.f, y + 32.f, 11, INK3);
}

// ── drawHomeScreen ────────────────────────────────────────────────────────────
void drawHomeScreen(sf::RenderWindow& w, const sf::Font& font, const App& app) {
    float cx = SIDEBAR_W + 26.f;
    float cy = TITLEBAR_H + CONTENT_HEAD_H + 22.f;
    float cw = (float)WINDOW_W - SIDEBAR_W - 52.f;

    drawText(w, font, "Good evening.", cx, cy, 26, INK);
    drawText(w, font, "Ready for a session? Your kit is connected.", cx, cy + 36.f, 13, INK2);

    // 3 section cards
    float cardY = cy + 78.f;
    float cardW = (cw - 28.f) / 3.f;
    float cardH = 112.f;

    struct Card { Screen sc; const char* title; const char* desc; };
    Card cards[] = {
        {Screen::EDITOR,  "Play",    "Pick a groove and play along."},
        {Screen::LIBRARY, "Library", "Browse, edit & save grooves."},
        {Screen::STATS,   "Stats",   "See your practice reports."},
    };
    for (int i = 0; i < 3; ++i) {
        float bx = cx + i * (cardW + 14.f);
        fillRect(w, bx, cardY, cardW, cardH, BG);
        strokeRect(w, bx, cardY, cardW, cardH, LINE_C);
        fillRect(w, bx + 14.f, cardY + 14.f, 32.f, 32.f, BG3);
        drawText(w, font, cards[i].title, bx + 14.f, cardY + 58.f, 14, INK);
        drawText(w, font, cards[i].desc,  bx + 14.f, cardY + 78.f, 11, INK3);
    }

    // "Continue" card
    float cwY = cardY + cardH + 20.f;
    fillRect(w, cx, cwY, cw, 82.f, BG);
    strokeRect(w, cx, cwY, cw, 82.f, LINE_C);
    drawText(w, font, "CONTINUE WHERE YOU LEFT OFF", cx + 14.f, cwY + 10.f, 10, INK3);

    if (app.library.empty()) {
        drawText(w, font, "No grooves saved yet.", cx + 14.f, cwY + 30.f, 13, INK2);
    } else {
        const auto& g = app.library.back();
        drawText(w, font, g.name, cx + 14.f, cwY + 28.f, 15, INK);
        drawText(w, font, std::to_string(g.bpm) + " BPM · " +
                 std::to_string(g.measures) + " bar(s)", cx + 14.f, cwY + 50.f, 12, INK2);
    }

    // Resume button
    float btnX = cx + cw - 114.f;
    fillRect(w, btnX, cwY + 24.f, 100.f, 32.f, INK);
    drawText(w, font, "Resume", btnX + 26.f, cwY + 31.f, 13, BG);
}

// ── drawLibraryScreen ─────────────────────────────────────────────────────────
void drawLibraryScreen(sf::RenderWindow& w, const sf::Font& font, const App& app) {
    float cx = SIDEBAR_W;
    float cy = TITLEBAR_H + CONTENT_HEAD_H;
    float cw = (float)WINDOW_W - SIDEBAR_W;

    // Search bar
    float searchY = cy + 14.f;
    fillRect(w, cx + 26.f, searchY, 300.f, 34.f, BG);
    strokeRect(w, cx + 26.f, searchY, 300.f, 34.f, LINE_C);
    drawText(w, font, "Search by name...", cx + 40.f, searchY + 9.f, 13, INK3);

    // New groove button
    float btnX = (float)WINDOW_W - 150.f;
    fillRect(w, btnX, searchY, 120.f, 34.f, INK);
    drawText(w, font, "+ New groove", btnX + 14.f, searchY + 9.f, 13, BG);

    // Column headers
    float colY   = searchY + 50.f;
    float col1   = cx + 26.f;
    drawText(w, font, "Name",         col1,          colY, 11, INK3);
    drawText(w, font, "Tempo · Bars", col1 + 440.f, colY, 11, INK3);
    drawText(w, font, "Last saved",   col1 + 580.f,  colY, 11, INK3);
    hline(w, col1, (float)WINDOW_W - 26.f, colY + 18.f, LINE_C);

    // Groove list
    float listTop = colY + 24.f;
    float rowH    = 44.f;
    int   maxVis  = std::max(1, (int)(((float)WINDOW_H - listTop - 20.f) / rowH));

    if (app.library.empty()) {
        fillRect(w, col1, listTop, cw - 52.f, rowH * 2.f, BG);
        strokeRect(w, col1, listTop, cw - 52.f, rowH * 2.f, LINE_C);
        drawText(w, font, "No grooves yet. Press S in the editor to save one.",
                 col1 + 14.f, listTop + 16.f, 13, INK3);
        return;
    }

    int start = std::max(0, std::min(app.libraryScroll,
                         (int)app.library.size() - maxVis));
    int end   = std::min((int)app.library.size(), start + maxVis);
    float listH = (end - start) * rowH;

    fillRect(w, col1, listTop, cw - 52.f, listH, BG);
    strokeRect(w, col1, listTop, cw - 52.f, listH, LINE_C);

    for (int i = start; i < end; ++i) {
        const auto& g = app.library[i];
        float ry = listTop + (i - start) * rowH;
        if (i > start) hline(w, col1, (float)WINDOW_W - 26.f, ry, HAIR_C);

        if (g.name == app.currentGrooveName)
            fillRect(w, col1, ry, cw - 52.f, rowH, BG3);

        drawText(w, font, g.name, col1 + 14.f, ry + 14.f, 13, INK);
        drawText(w, font, std::to_string(g.bpm) + " BPM · " +
                 std::to_string(g.measures) + " bar(s)",
                 col1 + 440.f, ry + 14.f, 12, INK2);

        // Delete button
        float delX = (float)WINDOW_W - 60.f;
        strokeRect(w, delX, ry + 9.f, 26.f, 26.f, LINE_C);
        drawText(w, font, "X", delX + 8.f, ry + 13.f, 11, INK3);
    }

    // Scroll hint
    if ((int)app.library.size() > maxVis)
        drawText(w, font, "↑↓ scroll", col1 + cw - 100.f,
                 listTop + listH + 6.f, 11, INK3);
}

// ── drawStatsScreen ───────────────────────────────────────────────────────────
void drawStatsScreen(sf::RenderWindow& w, const sf::Font& font, const App& app) {
    float cx = SIDEBAR_W + 26.f;
    float cy = TITLEBAR_H + CONTENT_HEAD_H + 14.f;
    float cw = (float)WINDOW_W - SIDEBAR_W - 52.f;

    // Compute aggregates
    int   totalSessions = (int)app.history.size();
    float totalTimeSec  = 0.f, totalAcc = 0.f;
    int   accCount      = 0;
    for (auto& s : app.history) {
        totalTimeSec += (float)s.durationSecs;
        if (s.totalNotes > 0) { totalAcc += s.accuracyPct; ++accCount; }
    }
    int totalMin  = (int)(totalTimeSec / 60.f);
    int avgAccPct = accCount > 0 ? (int)(totalAcc / accCount) : 0;

    // 3 stat cards
    float cardW = (cw - 28.f) / 3.f;
    float cardH = 92.f;
    struct Card { std::string label, value, delta; bool up; };
    Card stats[] = {
        {"Total time played",
         std::to_string(totalMin / 60) + "h " + std::to_string(totalMin % 60) + "m",
         totalMin > 0 ? "Keep playing!" : "No sessions yet", totalMin > 0},
        {"Sessions played",
         std::to_string(totalSessions),
         totalSessions > 0 ? "Total all time" : "Start your first session", true},
        {"Average accuracy",
         std::to_string(avgAccPct) + "%",
         accCount > 0 ? "Based on " + std::to_string(accCount) + " sessions" : "No data yet",
         avgAccPct >= 80},
    };

    for (int i = 0; i < 3; ++i) {
        float scx = cx + i * (cardW + 14.f);
        fillRect(w, scx, cy, cardW, cardH, BG);
        strokeRect(w, scx, cy, cardW, cardH, LINE_C);
        drawText(w, font, stats[i].label, scx + 14.f, cy + 12.f, 12, INK2);
        drawText(w, font, stats[i].value, scx + 14.f, cy + 34.f, 24, INK);
        drawText(w, font, stats[i].delta, scx + 14.f, cy + 70.f, 11,
                 stats[i].up ? GOOD_C : BAD_C);
    }

    // Recent sessions card
    float tableY = cy + cardH + 18.f;
    fillRect(w, cx, tableY, cw, 28.f, BG3);
    strokeRect(w, cx, tableY, cw, 28.f, LINE_C);
    drawText(w, font, "Recent sessions", cx + 14.f, tableY + 7.f, 13, INK);

    float hdrY = tableY + 36.f;
    drawText(w, font, "Groove",    cx + 14.f,        hdrY, 11, INK3);
    drawText(w, font, "BPM",       cx + cw - 260.f,  hdrY, 11, INK3);
    drawText(w, font, "Date",      cx + cw - 190.f,  hdrY, 11, INK3);
    drawText(w, font, "Accuracy",  cx + cw - 90.f,   hdrY, 11, INK3);
    hline(w, cx, cx + cw, hdrY + 16.f, LINE_C);

    float rowY = hdrY + 24.f;
    float rowH = 36.f;
    int   shown = 0;
    fillRect(w, cx, hdrY + 18.f, cw, std::max(1.f, (float)std::min((int)app.history.size(), 7)) * rowH, BG);
    strokeRect(w, cx, hdrY + 18.f, cw, std::max(1.f, (float)std::min((int)app.history.size(), 7)) * rowH, LINE_C);

    for (int i = (int)app.history.size() - 1; i >= 0 && shown < 7; --i, ++shown) {
        const auto& s = app.history[i];
        if (shown > 0) hline(w, cx, cx + cw, rowY, HAIR_C);
        drawText(w, font, s.grooveName,          cx + 14.f,       rowY + 10.f, 13, INK);
        drawText(w, font, std::to_string(s.bpm), cx + cw - 255.f, rowY + 10.f, 12, INK2);

        char buf[32] = {};
        time_t ts = (time_t)s.timestampEpoch;
        strftime(buf, sizeof(buf), "%b %d", localtime(&ts));
        drawText(w, font, buf, cx + cw - 185.f, rowY + 10.f, 12, INK2);
        drawText(w, font, std::to_string((int)s.accuracyPct) + "%", cx + cw - 86.f, rowY + 10.f, 13, INK);
        rowY += rowH;
    }

    if (app.history.empty())
        drawText(w, font, "No sessions recorded yet.", cx + 14.f, rowY + 8.f, 13, INK3);
}

// ── drawNamingOverlay ─────────────────────────────────────────────────────────
void drawNamingOverlay(sf::RenderWindow& w, const sf::Font& font, const App& app) {
    sf::RectangleShape dim({(float)WINDOW_W, (float)WINDOW_H});
    dim.setFillColor({0, 0, 0, 110});
    w.draw(dim);

    float bw = 500.f, bh = 92.f;
    float bx = ((float)WINDOW_W - bw) / 2.f, by = ((float)WINDOW_H - bh) / 2.f;
    fillRect(w, bx, by, bw, bh, BG);
    strokeRect(w, bx, by, bw, bh, LINE_C);

    drawText(w, font, "Save groove as:", bx + 16.f, by + 10.f, 12, INK3);
    drawText(w, font, app.nameBuffer + "_", bx + 16.f, by + 32.f, 15, INK);
    drawText(w, font, "Enter = confirm   Esc = cancel", bx + 16.f, by + 66.f, 11, INK3);
}

} // namespace drumming
