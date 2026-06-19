#include "drumming/draw.h"
#include "drumming/geometry.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <ctime>
#include <map>
#include <string>
#include <vector>

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
static void fillRect(sf::RenderTarget& w, float x, float y, float wd, float ht, sf::Color c) {
    sf::RectangleShape r({wd, ht});
    r.setPosition({x, y});
    r.setFillColor(c);
    w.draw(r);
}

static void strokeRect(sf::RenderTarget& w, float x, float y, float wd, float ht, sf::Color c, float thick = 1.f) {
    sf::RectangleShape r({wd - thick, ht - thick});
    r.setPosition({x + thick / 2.f, y + thick / 2.f});
    r.setFillColor(sf::Color::Transparent);
    r.setOutlineColor(c);
    r.setOutlineThickness(thick);
    w.draw(r);
}

static void hline(sf::RenderTarget& w, float x1, float x2, float y, sf::Color c) {
    sf::RectangleShape r({x2 - x1, 1.f});
    r.setPosition({x1, y});
    r.setFillColor(c);
    w.draw(r);
}

static void vline(sf::RenderTarget& w, float x, float y1, float y2, sf::Color c) {
    sf::RectangleShape r({1.f, y2 - y1});
    r.setPosition({x, y1});
    r.setFillColor(c);
    w.draw(r);
}

// Text is rasterized at RENDER_SCALE× the requested size and scaled back down,
// so glyphs are sampled from a high-resolution atlas — crisp once the scene is
// downsampled to the window. Layout stays in logical (unscaled) coordinates.
static void drawText(sf::RenderTarget& w, const sf::Font& font, const std::string& s,
                     float x, float y, unsigned sz, sf::Color c) {
    sf::Text t(font, s, sz * RENDER_SCALE);
    t.setScale({1.f / RENDER_SCALE, 1.f / RENDER_SCALE});
    t.setFillColor(c);
    t.setPosition({x, y});
    w.draw(t);
}

static void drawTextCenter(sf::RenderTarget& w, const sf::Font& font, const std::string& s,
                           float cx, float y, unsigned sz, sf::Color c) {
    sf::Text t(font, s, sz * RENDER_SCALE);
    t.setScale({1.f / RENDER_SCALE, 1.f / RENDER_SCALE});
    t.setFillColor(c);
    auto b = t.getLocalBounds();
    // Bounds are in the oversized glyph space; scale back to logical units.
    const float k = 1.f / RENDER_SCALE;
    t.setPosition({cx - (b.size.x / 2.f + b.position.x) * k, y});
    w.draw(t);
}

// ── drawNote ──────────────────────────────────────────────────────────────────
void drawNote(sf::RenderTarget& w, float x, float y, const Voice& v, sf::Color c) {
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
void drawStaff(sf::RenderTarget& w, const sf::Font& font, const App& app, int total) {
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
void drawGroove(sf::RenderTarget& w, const App& app, int total) {
    float cellW = (STAFF_RIGHT - STAFF_LEFT) / total;
    for (auto& [step, vi] : app.groove) {
        float x = STAFF_LEFT + (step + 0.5f) * cellW;
        drawNote(w, x, voiceY(vi), VOICES[vi], VOICES[vi].color);
    }
}

// ── drawPlayhead ──────────────────────────────────────────────────────────────
void drawPlayhead(sf::RenderTarget& w, const App& app) {
    if (app.currentStep < 0) return;
    float x   = stepX(app.currentStep, app.totalSteps());
    float top = STAFF_TOP_Y - 40.f;
    float bot = STAFF_TOP_Y + 4.f * LINE_SP + 95.f;
    fillRect(w, x - 1.f, top, 2.f, bot - top, ACCENT);
}

// ── drawResults ───────────────────────────────────────────────────────────────
void drawResults(sf::RenderTarget& w, const App& app, int total) {
    float cellW = (STAFF_RIGHT - STAFF_LEFT) / total;
    for (auto& r : app.results) {
        if (r.voice < 0) continue; // unrecognized pad: counts as a miss, no ring
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

// ── drawGrid ────────────────────────────────────────────────────────────────
// Step sequencer aligned beneath the staff: one row per voice, one cell per
// 16th-note step. Replaces the old physical-kit panel with the design's grid.
void drawGrid(sf::RenderTarget& w, const sf::Font& font, const App& app, int total) {
    float cellW = (STAFF_RIGHT - STAFF_LEFT) / total;
    float pad   = 2.f;

    int   playStep = (app.screen == Screen::PLAY) ? app.currentStep : -1;

    for (int vi = 0; vi < NUM_VOICES; ++vi) {
        float ry = GRID_TOP + vi * GRID_ROW_H;

        // Voice label in the gutter
        drawText(w, font, VOICES[vi].name, SIDEBAR_W + 14.f, ry + GRID_ROW_H / 2.f - 7.f,
                 10, INK2);

        for (int s = 0; s < total; ++s) {
            float cx        = STAFF_LEFT + s * cellW;
            bool  on        = app.groove.count({s, vi}) > 0;
            bool  beatStart = (s % STEPS_PER_BEAT == 0);
            bool  underPlay = (s == playStep);

            sf::Color fill = on ? INK : (beatStart ? BG3 : BG);
            fillRect(w, cx + pad, ry + pad, cellW - 2.f * pad, GRID_ROW_H - 2.f * pad, fill);
            strokeRect(w, cx + pad, ry + pad, cellW - 2.f * pad, GRID_ROW_H - 2.f * pad,
                       on ? INK : LINE_C);

            if (underPlay)
                strokeRect(w, cx + pad, ry + pad, cellW - 2.f * pad, GRID_ROW_H - 2.f * pad,
                           ACCENT, 1.5f);

            if (on) {
                float d = 4.f;
                sf::CircleShape dot(d);
                dot.setOrigin({d, d});
                dot.setPosition({cx + cellW / 2.f, ry + GRID_ROW_H / 2.f});
                dot.setFillColor(BG);
                w.draw(dot);
            }
        }
    }

    // Beat numbers beneath the grid
    float numY = GRID_TOP + NUM_VOICES * GRID_ROW_H + 4.f;
    for (int s = 0; s < total; s += STEPS_PER_BEAT)
        drawText(w, font, std::to_string(s / STEPS_PER_BEAT % BEATS_PER_MEASURE + 1),
                 STAFF_LEFT + s * cellW + cellW / 2.f - 3.f, numY, 9, INK3);
}

// pill — small rounded-ish chip with a label; returns its width.
static float pill(sf::RenderTarget& w, const sf::Font& font, float x, float y,
                  const std::string& s, sf::Color border = LINE_C, sf::Color ink = INK2) {
    sf::Text t(font, s, 12);
    float tw = t.getLocalBounds().size.x;
    float pw = tw + 22.f;
    fillRect(w, x, y, pw, 24.f, BG);
    strokeRect(w, x, y, pw, 24.f, border);
    drawText(w, font, s, x + 11.f, y + 5.f, 12, ink);
    return pw;
}

// accChip — coloured dot + label + value, for the live-accuracy strip.
static float accChip(sf::RenderTarget& w, const sf::Font& font, float x, float y,
                     const std::string& label, int pct, sf::Color c) {
    sf::CircleShape dot(4.f);
    dot.setOrigin({4.f, 4.f});
    dot.setPosition({x + 4.f, y + 9.f});
    dot.setFillColor(c);
    w.draw(dot);
    drawText(w, font, label, x + 14.f, y + 2.f, 12, INK2);
    sf::Text lt(font, label, 12);
    float lx = x + 14.f + lt.getLocalBounds().size.x + 8.f;
    drawText(w, font, std::to_string(pct) + "%", lx, y + 2.f, 12, INK);
    sf::Text vt(font, std::to_string(pct) + "%", 12);
    return (lx + vt.getLocalBounds().size.x + 22.f) - x;
}

// ── drawControls ──────────────────────────────────────────────────────────────
void drawControls(sf::RenderTarget& w, const sf::Font& font, const App& app) {
    float y  = CTRL_Y;
    float x  = CARD_X;

    hline(w, CARD_X, CARD_X + CARD_W, y - 12.f, HAIR_C);

    // Transport state + groove parameters as chips
    x += pill(w, font, x, y, app.screen == Screen::EDITOR ? "[Space] Play" : "[Space] Stop",
              LINE_C, INK) + 8.f;
    x += pill(w, font, x, y, "♩ " + std::to_string(app.bpm) + " BPM  [+/-]") + 8.f;
    x += pill(w, font, x, y, "4/4") + 8.f;
    x += pill(w, font, x, y, std::to_string(app.measures) + " bar" +
              (app.measures > 1 ? "s" : "") + "  [M/N]") + 8.f;
    x += pill(w, font, x, y, "Clear  [C]") + 8.f;

    // Start/Stop session button (top-right of the strip)
    if (app.screen == Screen::PLAY) {
        sf::FloatRect b = sessionButtonRect();
        sf::Color btnC = app.sessionActive ? BAD_C : GOOD_C;
        fillRect(w, b.position.x, b.position.y, b.size.x, b.size.y, btnC);
        strokeRect(w, b.position.x, b.position.y, b.size.x, b.size.y, btnC);
        drawTextCenter(w, font, app.sessionActive ? "Stop session" : "Start session",
                       b.position.x + b.size.x / 2.f, b.position.y + 5.f, 12, BG);
    }

    if (app.screen == Screen::PLAY && !app.results.empty()) {
        // Live accuracy strip, by hit class: on-time / early / late / wrong pad.
        int good = 0, early = 0, late = 0, wrong = 0;
        float sumAbs = 0.f;
        for (const auto& r : app.results) {
            switch (r.cls) {
            case HitClass::Correct:         ++good;  break;
            case HitClass::EarlyCorrectPad: ++early; break;
            case HitClass::LateCorrectPad:  ++late;  break;
            case HitClass::WrongPad:        ++wrong; break;
            }
            sumAbs += std::abs(r.offsetMs);
        }
        int n  = (int)app.results.size();
        float ax = CARD_X + 12.f;
        float ay = y + 30.f;
        drawText(w, font, app.sessionActive ? "Session live" : "Live accuracy",
                 ax, ay + 2.f, 12, INK);
        ax += 96.f;
        ax += accChip(w, font, ax, ay, "On time",   100 * good  / n, GOOD_C);
        ax += accChip(w, font, ax, ay, "Early",     100 * early / n, WARN_C);
        ax += accChip(w, font, ax, ay, "Late",      100 * late  / n, BAD_C);
        ax += accChip(w, font, ax, ay, "Wrong pad", 100 * wrong / n, INK2);
        drawText(w, font, "±" + std::to_string((int)(sumAbs / n)) + " ms avg",
                 ax + 8.f, ay + 2.f, 12, INK3);
    } else {
        drawText(w, font, "[S] Save   [L] Library   [H] Stats",
                 x + 6.f, y + 5.f, 12, INK3);
    }
}

// ── drawPracticeView ──────────────────────────────────────────────────────────
// Composite editor/play layout: a staff card above a grid-sequencer card,
// matching the design's split-view editor.
void drawPracticeView(sf::RenderTarget& w, const sf::Font& font, const App& app) {
    int total = app.totalSteps();

    // Card backgrounds
    fillRect(w, CARD_X, STAFF_CARD_Y, CARD_W, STAFF_CARD_H, BG);
    strokeRect(w, CARD_X, STAFF_CARD_Y, CARD_W, STAFF_CARD_H, LINE_C);

    float gridCardH = (GRID_TOP + NUM_VOICES * GRID_ROW_H + 18.f) - GRID_CARD_Y;
    fillRect(w, CARD_X, GRID_CARD_Y, CARD_W, gridCardH, BG);
    strokeRect(w, CARD_X, GRID_CARD_Y, CARD_W, gridCardH, LINE_C);

    // Card labels (mono-ish, faint) — like the design's STAFF / GRID tags
    drawText(w, font, "STAFF", CARD_X + 12.f, STAFF_CARD_Y + 8.f, 10, INK3);
    drawText(w, font, "GRID",  CARD_X + 12.f, GRID_CARD_Y + 7.f, 10, INK3);

    drawStaff(w, font, app, total);
    drawGroove(w, app, total);
    if (app.screen == Screen::PLAY) {
        drawResults(w, app, total);
        drawPlayhead(w, app);
    }
    drawGrid(w, font, app, total);
    drawControls(w, font, app);
}

// ── drawTitleBar ──────────────────────────────────────────────────────────────
void drawTitleBar(sf::RenderTarget& w, const sf::Font& font) {
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
void drawSidebar(sf::RenderTarget& w, const sf::Font& font, const App& app) {
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
    for (int i = 0; i < 3; ++i) {
        const auto& item = items[i];
        sf::FloatRect r  = sidebarNavItemRect(i);
        bool active = (app.screen == item.sc) ||
                      (app.screen == Screen::PLAY && item.sc == Screen::EDITOR);
        if (active) {
            fillRect(w, r.position.x, r.position.y, r.size.x, r.size.y, BG);
            strokeRect(w, r.position.x, r.position.y, r.size.x, r.size.y, LINE_C);
        }
        drawText(w, font, item.label, r.position.x + 14.f, r.position.y + 9.f, 13,
                 active ? INK : INK2);
    }

    // MIDI status chip at bottom
    float statusY = sbTop + sbH - 52.f;
    fillRect(w, px, statusY, SIDEBAR_W - px * 2.f, 34.f, BG);
    strokeRect(w, px, statusY, SIDEBAR_W - px * 2.f, 34.f, LINE_C);
    sf::CircleShape dot(3.5f);
    dot.setOrigin({3.5f, 3.5f});
    dot.setPosition({px + 12.f, statusY + 17.f});
    dot.setFillColor(app.midiConnected ? GOOD_C : BAD_C);
    w.draw(dot);
    drawText(w, font, app.midiConnected ? "MIDI connected" : "No MIDI device",
             px + 22.f, statusY + 9.f, 11, INK2);
}

// ── drawContentHeader ─────────────────────────────────────────────────────────
void drawContentHeader(sf::RenderTarget& w, const sf::Font& font, const App& app) {
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
void drawHomeScreen(sf::RenderTarget& w, const sf::Font& font, const App& app) {
    float cx = SIDEBAR_W + 26.f;
    float cy = TITLEBAR_H + CONTENT_HEAD_H + 22.f;
    float cw = (float)WINDOW_W - SIDEBAR_W - 52.f;

    drawText(w, font, "Good evening.", cx, cy, 26, INK);
    drawText(w, font,
             app.midiConnected ? "Ready for a session? Your kit is connected."
                               : "Connect a MIDI kit to start a session.",
             cx, cy + 36.f, 13, INK2);

    // 3 section cards
    struct Card { Screen sc; const char* title; const char* desc; };
    Card cards[] = {
        {Screen::EDITOR,  "Play",    "Pick a groove and play along."},
        {Screen::LIBRARY, "Library", "Browse, edit & save grooves."},
        {Screen::STATS,   "Stats",   "See your practice reports."},
    };
    sf::FloatRect card0 = homeCardRect(0);
    for (int i = 0; i < 3; ++i) {
        sf::FloatRect r = homeCardRect(i);
        float bx = r.position.x, cardY = r.position.y;
        fillRect(w, bx, cardY, r.size.x, r.size.y, BG);
        strokeRect(w, bx, cardY, r.size.x, r.size.y, LINE_C);
        fillRect(w, bx + 14.f, cardY + 14.f, 32.f, 32.f, BG3);
        drawText(w, font, cards[i].title, bx + 14.f, cardY + 58.f, 14, INK);
        drawText(w, font, cards[i].desc,  bx + 14.f, cardY + 78.f, 11, INK3);
    }

    // "Continue" card
    float cwY = card0.position.y + card0.size.y + 20.f;
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
    sf::FloatRect resume = homeResumeRect();
    fillRect(w, resume.position.x, resume.position.y, resume.size.x, resume.size.y, INK);
    drawText(w, font, "Resume", resume.position.x + 26.f, resume.position.y + 7.f, 13, BG);
}

// ── drawLibraryScreen ─────────────────────────────────────────────────────────
void drawLibraryScreen(sf::RenderTarget& w, const sf::Font& font, const App& app) {
    float cx = SIDEBAR_W;
    float cy = TITLEBAR_H + CONTENT_HEAD_H;
    float cw = (float)WINDOW_W - SIDEBAR_W;

    // Search bar
    float searchY = cy + 14.f;
    fillRect(w, cx + 26.f, searchY, 300.f, 34.f, BG);
    strokeRect(w, cx + 26.f, searchY, 300.f, 34.f, LINE_C);
    drawText(w, font, "Search by name...", cx + 40.f, searchY + 9.f, 13, INK3);

    // New groove button
    sf::FloatRect newBtn = libraryNewGrooveRect();
    fillRect(w, newBtn.position.x, newBtn.position.y, newBtn.size.x, newBtn.size.y, INK);
    drawText(w, font, "+ New groove", newBtn.position.x + 14.f, newBtn.position.y + 9.f,
             13, BG);

    // Column headers
    float colY   = searchY + 50.f;
    float col1   = cx + 26.f;
    drawText(w, font, "Name",         col1,          colY, 11, INK3);
    drawText(w, font, "Tempo · Bars", col1 + 440.f, colY, 11, INK3);
    drawText(w, font, "Last saved",   col1 + 580.f,  colY, 11, INK3);
    hline(w, col1, (float)WINDOW_W - 26.f, colY + 18.f, LINE_C);

    // Groove list
    float listTop = libraryListTop();
    float rowH    = LIBRARY_ROW_H;
    int   maxVis  = libraryMaxVisibleRows();

    if (app.library.empty()) {
        fillRect(w, col1, listTop, cw - 52.f, rowH * 2.f, BG);
        strokeRect(w, col1, listTop, cw - 52.f, rowH * 2.f, LINE_C);
        drawText(w, font, "No grooves yet. Press S in the editor to save one.",
                 col1 + 14.f, listTop + 16.f, 13, INK3);
        return;
    }

    int start = libraryListStart(app.libraryScroll, (int)app.library.size());
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
        sf::FloatRect del = libraryDeleteRect(i - start);
        strokeRect(w, del.position.x, del.position.y, del.size.x, del.size.y, LINE_C);
        drawText(w, font, "X", del.position.x + 8.f, del.position.y + 4.f, 11, INK3);
    }

    // Scroll hint
    if ((int)app.library.size() > maxVis)
        drawText(w, font, "↑↓ scroll", col1 + cw - 100.f,
                 listTop + listH + 6.f, 11, INK3);
}

// ── Stats helpers ─────────────────────────────────────────────────────────────
// Local-midnight epoch (seconds) for the day containing `ts`. Sessions are
// bucketed by this so they group per calendar day in the viewer's timezone.
static long long dayStartEpoch(long long ts) {
    time_t    t  = (time_t)ts;
    struct tm lt = *localtime(&t);
    lt.tm_hour = 0; lt.tm_min = 0; lt.tm_sec = 0; lt.tm_isdst = -1;
    return (long long)mktime(&lt);
}

static std::string formatDay(long long dayEpoch, const char* fmt) {
    char   buf[32] = {};
    time_t t       = (time_t)dayEpoch;
    strftime(buf, sizeof(buf), fmt, localtime(&t));
    return buf;
}

// ── drawStatsScreen ───────────────────────────────────────────────────────────
void drawStatsScreen(sf::RenderTarget& w, const sf::Font& font, const App& app) {
    float cx = SIDEBAR_W + 26.f;
    float cy = TITLEBAR_H + CONTENT_HEAD_H + 14.f;
    float cw = (float)WINDOW_W - SIDEBAR_W - 52.f;

    // Compute aggregates
    int   totalSessions = (int)app.history.size();
    float totalTimeSec  = 0.f, totalAcc = 0.f;
    int   accCount      = 0;
    for (const auto& s : app.history) {
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

    // ── Playing time per day (line chart) ────────────────────────────────────
    float chartY = cy + cardH + 18.f;
    float chartH = 188.f;
    fillRect(w, cx, chartY, cw, chartH, BG);
    strokeRect(w, cx, chartY, cw, chartH, LINE_C);
    drawText(w, font, "Playing time per day", cx + 14.f, chartY + 10.f, 13, INK);

    if (app.history.empty()) {
        drawText(w, font, "No sessions recorded yet.", cx + 14.f, chartY + 40.f, 13, INK3);
    } else {
        // Minutes per calendar day, over the last `kDays` ending today.
        constexpr int kDays = 14;
        std::map<long long, float> minsByDay;
        for (const auto& s : app.history)
            minsByDay[dayStartEpoch(s.timestampEpoch)] += (float)s.durationSecs / 60.f;

        long long today = dayStartEpoch((long long)std::time(nullptr));
        // Anchor the right edge to the more recent of today / latest session.
        if (!minsByDay.empty())
            today = std::max(today, minsByDay.rbegin()->first);

        std::vector<long long> days(kDays);
        std::vector<float>     mins(kDays, 0.f);
        float maxMin = 1.f;
        for (int i = 0; i < kDays; ++i) {
            long long d = today - (long long)(kDays - 1 - i) * 86400LL;
            days[i] = d;
            auto it = minsByDay.find(d);
            if (it != minsByDay.end()) mins[i] = it->second;
            maxMin = std::max(maxMin, mins[i]);
        }

        // Plot frame
        float plotL = cx + 50.f;
        float plotR = cx + cw - 18.f;
        float plotT = chartY + 34.f;
        float plotB = chartY + chartH - 26.f;
        float plotW = plotR - plotL;
        float plotH = plotB - plotT;

        // Horizontal gridlines + y-axis labels (minutes), 4 divisions.
        for (int g = 0; g <= 4; ++g) {
            float gy  = plotB - plotH * (float)g / 4.f;
            int   val = (int)std::round(maxMin * (float)g / 4.f);
            hline(w, plotL, plotR, gy, g == 0 ? LINE_C : HAIR_C);
            drawText(w, font, std::to_string(val) + "m", cx + 14.f, gy - 7.f, 10, INK3);
        }

        auto px = [&](int i) { return plotL + plotW * (float)i / (float)(kDays - 1); };
        auto py = [&](float m) { return plotB - plotH * (m / maxMin); };

        // Polyline + markers
        for (int i = 0; i < kDays; ++i) {
            if (i > 0) {
                sf::Vertex seg[2];
                seg[0].position = {px(i - 1), py(mins[i - 1])};
                seg[1].position = {px(i),     py(mins[i])};
                seg[0].color = ACCENT; seg[1].color = ACCENT;
                w.draw(seg, 2, sf::PrimitiveType::Lines);
            }
        }
        for (int i = 0; i < kDays; ++i) {
            sf::CircleShape dot(3.f);
            dot.setOrigin({3.f, 3.f});
            dot.setPosition({px(i), py(mins[i])});
            dot.setFillColor(mins[i] > 0.f ? ACCENT : INK3);
            w.draw(dot);
        }

        // X-axis date labels (every other day to avoid crowding)
        for (int i = 0; i < kDays; i += 2)
            drawText(w, font, formatDay(days[i], "%b %d"),
                     px(i) - 14.f, plotB + 6.f, 9, INK3);
    }

    // ── Sessions grouped by date and groove ──────────────────────────────────
    float tableY = chartY + chartH + 16.f;
    fillRect(w, cx, tableY, cw, 28.f, BG3);
    strokeRect(w, cx, tableY, cw, 28.f, LINE_C);
    drawText(w, font, "Sessions by date & groove", cx + 14.f, tableY + 7.f, 13, INK);

    // Column anchors
    float colCount = cw - 360.f;
    float colBpm   = cw - 270.f;
    float colDur   = cw - 180.f;
    float colAcc   = cw - 80.f;

    float hdrY = tableY + 36.f;
    drawText(w, font, "Groove",    cx + 28.f,    hdrY, 11, INK3);
    drawText(w, font, "Sessions",  cx + colCount, hdrY, 11, INK3);
    drawText(w, font, "BPM",       cx + colBpm,   hdrY, 11, INK3);
    drawText(w, font, "Duration",  cx + colDur,   hdrY, 11, INK3);
    drawText(w, font, "Accuracy",  cx + colAcc,   hdrY, 11, INK3);
    hline(w, cx, cx + cw, hdrY + 16.f, LINE_C);

    if (app.history.empty()) {
        drawText(w, font, "No sessions recorded yet.", cx + 14.f, hdrY + 26.f, 13, INK3);
        return;
    }

    // Aggregate per (day, groove). Iterate newest-first; std::map keeps days
    // ascending and grooves alphabetical, so we walk days in reverse.
    struct Agg { int count = 0, dur = 0; float accSum = 0.f; int accN = 0; int bpm = 0; bool bpmUniform = true; };
    std::map<long long, std::map<std::string, Agg>> byDay;
    for (const auto& s : app.history) {
        Agg& a = byDay[dayStartEpoch(s.timestampEpoch)][s.grooveName];
        a.count += 1;
        a.dur   += s.durationSecs;
        if (s.totalNotes > 0) { a.accSum += s.accuracyPct; ++a.accN; }
        if (a.count == 1) a.bpm = s.bpm;
        else if (a.bpm != s.bpm) a.bpmUniform = false;
    }

    float rowY   = hdrY + 26.f;
    float rowH   = 30.f;
    float limitY = (float)WINDOW_H - 16.f;   // stop before running off-screen
    for (auto dit = byDay.rbegin(); dit != byDay.rend() && rowY < limitY; ++dit) {
        // Date header band
        fillRect(w, cx, rowY, cw, 22.f, BG2);
        drawText(w, font, formatDay(dit->first, "%A, %b %d"), cx + 14.f, rowY + 5.f, 11, INK2);
        rowY += 26.f;

        for (const auto& [groove, a] : dit->second) {
            if (rowY >= limitY) break;
            drawText(w, font, groove, cx + 28.f, rowY, 13, INK);
            drawText(w, font, std::to_string(a.count), cx + colCount + 5.f, rowY, 12, INK2);
            drawText(w, font, a.bpmUniform ? std::to_string(a.bpm) : "—",
                     cx + colBpm + 5.f, rowY, 12, INK2);

            char durBuf[16] = {};
            snprintf(durBuf, sizeof(durBuf), "%d:%02d", a.dur / 60, a.dur % 60);
            drawText(w, font, durBuf, cx + colDur + 5.f, rowY, 12, INK2);

            int avg = a.accN > 0 ? (int)(a.accSum / a.accN) : 0;
            drawText(w, font, a.accN > 0 ? std::to_string(avg) + "%" : "—",
                     cx + colAcc + 5.f, rowY, 13, INK);
            rowY += rowH;
        }
    }
}

// ── drawNamingOverlay ─────────────────────────────────────────────────────────
void drawNamingOverlay(sf::RenderTarget& w, const sf::Font& font, const App& app) {
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
