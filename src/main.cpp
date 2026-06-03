#include "drumming/audio.h"
#include "drumming/draw.h"
#include "drumming/geometry.h"
#include "drumming/midi.h"
#include "drumming/types.h"
#include <SFML/Audio.hpp>
#include <SFML/Graphics.hpp>
#include <RtMidi.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>

int main() {
    RtMidiIn midiin;
    if (midiin.getPortCount() == 0) { std::cerr << "No MIDI ports\n"; return 1; }
    midiin.openPort(0);
    midiin.setCallback(&drumming::midiCallback);
    midiin.ignoreTypes(false, false, false);

    sf::RenderWindow window(
        sf::VideoMode({drumming::WINDOW_W, drumming::WINDOW_H}),
        "Drum Groove Editor");
    window.setFramerateLimit(60);

    sf::Font font;
    if (!font.openFromFile("/System/Library/Fonts/Helvetica.ttc")) {
        std::cerr << "Failed to load font\n";
        return 1;
    }

    sf::SoundBuffer hiClickBuf = drumming::makeClick(1100.f);
    sf::SoundBuffer loClickBuf = drumming::makeClick(700.f);
    sf::Sound hiClick(hiClickBuf), loClick(loClickBuf);

    using Clock = std::chrono::steady_clock;
    drumming::App g;

    while (window.isOpen()) {
        // ── Events ──────────────────────────────────────────────────────────
        while (auto ev = window.pollEvent()) {
            if (ev->is<sf::Event::Closed>()) { window.close(); break; }

            if (const auto* k = ev->getIf<sf::Event::KeyPressed>()) {
                switch (k->code) {
                case sf::Keyboard::Key::Space:
                    if (g.mode == drumming::Mode::EDIT) {
                        g.mode          = drumming::Mode::PLAY;
                        g.results.clear();
                        g.currentStep   = -1;
                        g.lastClickStep = -1;
                        g.loopCount     = 0;
                        g.playStart     = Clock::now();
                    } else {
                        g.mode        = drumming::Mode::EDIT;
                        g.currentStep = -1;
                    }
                    break;
                case sf::Keyboard::Key::Equal:
                case sf::Keyboard::Key::Add:
                    g.bpm = std::min(240, g.bpm + 5); break;
                case sf::Keyboard::Key::Hyphen:
                case sf::Keyboard::Key::Subtract:
                    g.bpm = std::max(40, g.bpm - 5); break;
                case sf::Keyboard::Key::M:
                    g.measures = std::min(8, g.measures + 1); break;
                case sf::Keyboard::Key::N:
                    if (g.measures > 1) {
                        --g.measures;
                        for (auto it = g.groove.begin(); it != g.groove.end(); )
                            it = it->first >= g.totalSteps() ? g.groove.erase(it) : std::next(it);
                    }
                    break;
                case sf::Keyboard::Key::C:
                    g.groove.clear(); g.results.clear(); break;
                default: break;
                }
            }

            if (g.mode == drumming::Mode::EDIT) {
                if (const auto* mb = ev->getIf<sf::Event::MouseButtonPressed>()) {
                    if (mb->button == sf::Mouse::Button::Left) {
                        auto [step, vi] = drumming::pickCell(
                            {(float)mb->position.x, (float)mb->position.y},
                            g.totalSteps());
                        if (step >= 0 && vi >= 0) {
                            auto key = std::make_pair(step, vi);
                            if (g.groove.count(key)) g.groove.erase(key);
                            else                     g.groove.insert(key);
                        }
                    }
                }
            }
        }

        // ── MIDI ────────────────────────────────────────────────────────────
        for (auto& ev : drumming::pollMidi()) {
            g.lastHit[ev.note] = ev.time;
            g.lastVel[ev.note] = ev.vel;

            if (g.mode == drumming::Mode::PLAY && g.currentStep >= 0) {
                int vi = drumming::voiceIndex(ev.note);
                if (vi >= 0) {
                    float ms       = std::chrono::duration<float, std::milli>(
                                         ev.time - g.playStart).count();
                    float stepF    = ms / g.stepDurMs();
                    int   nearest  = (int)std::round(stepF) % g.totalSteps();
                    float offsetMs = (stepF - std::round(stepF)) * g.stepDurMs();
                    bool  correct  = g.groove.count({nearest, vi}) &&
                                     std::abs(offsetMs) < drumming::HIT_WINDOW_MS;
                    g.results.push_back({nearest, vi, offsetMs, correct});
                }
            }
        }

        // ── Play advance ────────────────────────────────────────────────────
        if (g.mode == drumming::Mode::PLAY) {
            float ms      = std::chrono::duration<float, std::milli>(
                                Clock::now() - g.playStart).count();
            int   rawStep = (int)(ms / g.stepDurMs());
            int   loop    = rawStep / g.totalSteps();
            int   step    = rawStep % g.totalSteps();

            if (loop > g.loopCount) { g.loopCount = loop; g.results.clear(); }
            g.currentStep = step;

            if (step != g.lastClickStep && step % drumming::STEPS_PER_BEAT == 0) {
                g.lastClickStep = step;
                if (step % drumming::STEPS_PER_MEASURE == 0) hiClick.play();
                else                                          loClick.play();
            }
        }

        // ── Render ──────────────────────────────────────────────────────────
        window.clear(sf::Color(28, 28, 32));
        drumming::drawStaff(window, font, g, g.totalSteps());
        drumming::drawGroove(window, g, g.totalSteps());
        if (g.mode == drumming::Mode::PLAY) {
            drumming::drawResults(window, g, g.totalSteps());
            drumming::drawPlayhead(window, g);
        }
        drumming::drawKit(window, font, g);
        drumming::drawControls(window, font, g);
        window.display();
    }

    return 0;
}
