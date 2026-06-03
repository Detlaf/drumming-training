#include "drumming/draw.h"
#include "drumming/geometry.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <string>

namespace drumming {

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
    float sy1  = y, sy2 = down ? y + 22.f : y - 22.f;
    sf::RectangleShape stem({1.5f, std::abs(sy2 - sy1)});
    stem.setFillColor(c);
    stem.setPosition({sx, std::min(sy1, sy2)});
    w.draw(stem);

    if (v.yOff < 0.f) {
        float ly = STAFF_TOP_Y - LINE_SP;
        if (y <= ly + 2.f) {
            sf::RectangleShape ll({14.f, 1.f});
            ll.setPosition({x - 7.f, ly});
            ll.setFillColor(sf::Color(180, 180, 180, 140));
            w.draw(ll);
        }
        if (y <= STAFF_TOP_Y - 2.f * LINE_SP + 2.f) {
            sf::RectangleShape ll({14.f, 1.f});
            ll.setPosition({x - 7.f, STAFF_TOP_Y - 2 * LINE_SP});
            ll.setFillColor(sf::Color(180, 180, 180, 140));
            w.draw(ll);
        }
    }
    if (v.yOff > 4.f * LINE_SP) {
        float ly = STAFF_TOP_Y + 5.f * LINE_SP;
        sf::RectangleShape ll({14.f, 1.f});
        ll.setPosition({x - 7.f, ly});
        ll.setFillColor(sf::Color(180, 180, 180, 140));
        w.draw(ll);
        if (v.yOff > 5.f * LINE_SP + LINE_SP) {
            sf::RectangleShape ll2({14.f, 1.f});
            ll2.setPosition({x - 7.f, ly + LINE_SP});
            ll2.setFillColor(sf::Color(180, 180, 180, 140));
            w.draw(ll2);
        }
    }
}

void drawStaff(sf::RenderWindow& w, const sf::Font& font, const App& app, int total) {
    for (int l = 0; l < 5; ++l) {
        float y = STAFF_TOP_Y + l * LINE_SP;
        sf::RectangleShape line({STAFF_RIGHT - STAFF_LEFT, 1.f});
        line.setPosition({STAFF_LEFT, y});
        line.setFillColor(sf::Color(160, 160, 160, 110));
        w.draw(line);
    }

    for (int vi = 0; vi < NUM_VOICES; ++vi) {
        sf::Text t(font, VOICES[vi].name, 10);
        t.setFillColor(sf::Color(190, 190, 190, 180));
        t.setPosition({4.f, voiceY(vi) - 7.f});
        w.draw(t);
    }

    float gridTop = STAFF_TOP_Y - 36.f;
    float gridBot = STAFF_TOP_Y + 4.f * LINE_SP + 90.f;
    float cellW   = (STAFF_RIGHT - STAFF_LEFT) / total;
    for (int s = 0; s <= total; ++s) {
        float x    = STAFF_LEFT + s * cellW;
        bool  bar  = (s % STEPS_PER_MEASURE == 0);
        bool  beat = (s % STEPS_PER_BEAT == 0);
        sf::Color lc = bar  ? sf::Color(210, 210, 210, 200)
                     : beat ? sf::Color(140, 140, 140, 110)
                            : sf::Color(70,  70,  70,  70);
        float thick = bar ? 2.f : 1.f;
        sf::RectangleShape vl({thick, gridBot - gridTop});
        vl.setPosition({x, gridTop});
        vl.setFillColor(lc);
        w.draw(vl);
    }

    for (int s = 0; s < total; s += STEPS_PER_BEAT) {
        sf::Text t(font, std::to_string(s / STEPS_PER_BEAT + 1), 10);
        t.setFillColor(sf::Color(140, 140, 140));
        t.setPosition({STAFF_LEFT + s * cellW + 2.f, gridTop - 14.f});
        w.draw(t);
    }

    for (int m = 0; m < app.measures; ++m) {
        int s = m * STEPS_PER_MEASURE;
        sf::Text t(font, "m" + std::to_string(m + 1), 10);
        t.setFillColor(sf::Color(200, 200, 200));
        t.setPosition({STAFF_LEFT + s * cellW + 2.f, gridTop - 26.f});
        w.draw(t);
    }
}

void drawGroove(sf::RenderWindow& w, const App& app, int total) {
    float cellW = (STAFF_RIGHT - STAFF_LEFT) / total;
    for (auto& [step, vi] : app.groove) {
        float x = STAFF_LEFT + (step + 0.5f) * cellW;
        drawNote(w, x, voiceY(vi), VOICES[vi], VOICES[vi].color);
    }
}

void drawPlayhead(sf::RenderWindow& w, const App& app) {
    if (app.currentStep < 0) return;
    float x   = stepX(app.currentStep, app.totalSteps());
    float top = STAFF_TOP_Y - 40.f;
    float bot = STAFF_TOP_Y + 4.f * LINE_SP + 95.f;
    sf::RectangleShape ph({2.f, bot - top});
    ph.setPosition({x - 1.f, top});
    ph.setFillColor(sf::Color(255, 210, 50, 200));
    w.draw(ph);
}

void drawResults(sf::RenderWindow& w, const App& app, int total) {
    float cellW = (STAFF_RIGHT - STAFF_LEFT) / total;
    for (auto& r : app.results) {
        float     x = STAFF_LEFT + (r.step + 0.5f) * cellW;
        float     y = voiceY(r.voice);
        sf::Color c = r.correct ? sf::Color(60, 255, 110, 180)
                                : sf::Color(255, 60, 60, 180);
        sf::CircleShape ring(8.f);
        ring.setOrigin({8.f, 8.f});
        ring.setPosition({x, y});
        ring.setFillColor(sf::Color::Transparent);
        ring.setOutlineThickness(2.f);
        ring.setOutlineColor(c);
        w.draw(ring);
    }
}

void drawKit(sf::RenderWindow& w, const sf::Font& font, const App& app) {
    sf::RectangleShape bg({(float)WINDOW_W, KIT_H});
    bg.setPosition({0.f, KIT_Y});
    bg.setFillColor(sf::Color(18, 18, 22));
    w.draw(bg);

    auto        now  = std::chrono::steady_clock::now();
    const float FADE = 400.f;

    for (int i = 0; i < NUM_KIT_PADS; ++i) {
        const KitPad& p   = KIT_PADS[i];
        sf::Vector2f  pos = {p.pos.x, p.pos.y + KIT_Y};
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
        sh.setOutlineThickness(2);
        sh.setOutlineColor(sf::Color(200, 200, 200, 70));
        w.draw(sh);

        sf::Text lbl(font, p.name, 11);
        lbl.setFillColor(sf::Color(210, 210, 210, 160));
        lbl.setPosition({pos.x - p.r, pos.y + p.r + 4.f});
        w.draw(lbl);
    }
}

void drawControls(sf::RenderWindow& w, const sf::Font& font, const App& app) {
    float y   = CTRL_Y + 4.f;
    auto  txt = [&](const std::string& s, float x, sf::Color c = {190, 190, 190}) {
        sf::Text t(font, s, 13);
        t.setFillColor(c);
        t.setPosition({x, y});
        w.draw(t);
    };

    txt((app.mode == Mode::EDIT ? "[SPACE] Play" : "[SPACE] Stop"), 8.f);
    txt("BPM: " + std::to_string(app.bpm) + "  [+/-]", 170.f);
    txt("Bars: " + std::to_string(app.measures) + "  [M/N]", 340.f);
    txt("[C] Clear", 490.f);

    if (app.mode == Mode::PLAY && !app.results.empty()) {
        int ok  = 0;
        for (auto& r : app.results) ok += r.correct;
        int pct = 100 * ok / (int)app.results.size();
        txt("Accuracy: " + std::to_string(pct) + "% (" +
            std::to_string(ok) + "/" + std::to_string((int)app.results.size()) + ")",
            620.f, {80, 240, 130});
    }

    if (app.mode == Mode::EDIT) {
        txt("Click to add/remove notes on the staff", 620.f, {120, 120, 120});
    }
}

} // namespace drumming
