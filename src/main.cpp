#include "drumming/audio.h"
#include "drumming/draw.h"
#include "drumming/geometry.h"
#include "drumming/midi.h"
#include "drumming/persistence.h"
#include "drumming/types.h"
#include <SFML/Audio.hpp>
#include <SFML/Graphics.hpp>
#include <RtMidi.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <ctime>
#include <iostream>

using Clock = std::chrono::steady_clock;

static void endSession(drumming::App& g) {
    if (!g.sessionActive) return;
    int dur = (int)std::chrono::duration<float>(Clock::now() - g.sessionStart).count();
    int ok  = 0;
    for (auto& r : g.results) ok += r.correct;
    int total = (int)g.results.size();

    drumming::SessionRecord rec{};
    rec.grooveName     = g.currentGrooveName.empty() ? "<unsaved>" : g.currentGrooveName;
    rec.bpm            = g.bpm;
    rec.measures       = g.measures;
    rec.correctHits    = ok;
    rec.totalNotes     = total;
    rec.accuracyPct    = total > 0 ? 100.f * ok / total : 0.f;
    rec.timestampEpoch = (long long)std::time(nullptr);
    rec.durationSecs   = dur;

    drumming::appendHistory(g, rec);
    g.sessionActive = false;
}

// Hit-test sidebar nav items; returns target Screen or current screen if no hit.
static drumming::Screen sidebarNavHit(float mx, float my) {
    using namespace drumming;
    if (mx >= SIDEBAR_W || my < TITLEBAR_H) return Screen::HOME; // sentinel for "no match"

    float y      = TITLEBAR_H + 14.f + 50.f + 22.f; // matches drawSidebar layout
    float itemH  = 34.f, itemGap = 4.f;
    Screen navSc[] = {Screen::EDITOR, Screen::LIBRARY, Screen::STATS};
    for (auto sc : navSc) {
        if (my >= y && my < y + itemH) return sc;
        y += itemH + itemGap;
    }
    return Screen::HOME; // no match
}

int main() {
    // MIDI — optional; app still works without a kit
    RtMidiIn midiin;
    if (midiin.getPortCount() > 0) {
        midiin.openPort(0);
        midiin.setCallback(&drumming::midiCallback);
        midiin.ignoreTypes(false, false, false);
    } else {
        std::cerr << "No MIDI ports — running without kit\n";
    }

    sf::RenderWindow window(
        sf::VideoMode({drumming::WINDOW_W, drumming::WINDOW_H}),
        "Drumming");
    window.setFramerateLimit(60);

    sf::Font font;
    if (!font.openFromFile("/System/Library/Fonts/Helvetica.ttc")) {
        std::cerr << "Failed to load font\n";
        return 1;
    }

    sf::SoundBuffer hiClickBuf = drumming::makeClick(1100.f);
    sf::SoundBuffer loClickBuf = drumming::makeClick(700.f);
    sf::Sound hiClick(hiClickBuf), loClick(loClickBuf);

    drumming::App g;
    drumming::loadGrooves(g);
    drumming::loadHistory(g);

    while (window.isOpen()) {
        // ── Events ──────────────────────────────────────────────────────────
        while (auto ev = window.pollEvent()) {
            if (ev->is<sf::Event::Closed>()) {
                endSession(g);
                window.close();
                break;
            }

            // ── Key pressed ─────────────────────────────────────────────────
            if (const auto* k = ev->getIf<sf::Event::KeyPressed>()) {
                // Naming overlay captures all keys
                if (g.namingMode) {
                    if (k->code == sf::Keyboard::Key::Escape) {
                        g.nameBuffer.clear();
                        g.namingMode = false;
                    } else if (k->code == sf::Keyboard::Key::Enter && !g.nameBuffer.empty()) {
                        bool found = false;
                        for (auto& sg : g.library) {
                            if (sg.name == g.nameBuffer) {
                                sg.bpm = g.bpm; sg.measures = g.measures; sg.groove = g.groove;
                                found = true; break;
                            }
                        }
                        if (!found)
                            g.library.push_back({g.nameBuffer, g.bpm, g.measures, g.groove});
                        g.currentGrooveName = g.nameBuffer;
                        g.nameBuffer.clear();
                        g.namingMode = false;
                        drumming::saveGrooves(g);
                    }
                    break; // consume all other keys while naming
                }

                switch (k->code) {
                case sf::Keyboard::Key::Space:
                    if (g.screen == drumming::Screen::HOME    ||
                        g.screen == drumming::Screen::LIBRARY ||
                        g.screen == drumming::Screen::STATS) {
                        g.screen = drumming::Screen::EDITOR;
                    } else if (g.screen == drumming::Screen::EDITOR) {
                        g.screen        = drumming::Screen::PLAY;
                        g.results.clear();
                        g.currentStep   = -1;
                        g.lastClickStep = -1;
                        g.loopCount     = 0;
                        g.sessionActive = false;
                        g.playStart     = Clock::now();
                        g.sessionStart  = g.playStart;
                    } else if (g.screen == drumming::Screen::PLAY) {
                        endSession(g);
                        g.screen      = drumming::Screen::EDITOR;
                        g.currentStep = -1;
                    }
                    break;

                case sf::Keyboard::Key::S:
                    if (g.screen == drumming::Screen::EDITOR && !g.groove.empty()) {
                        g.namingMode = true;
                        g.nameBuffer = g.currentGrooveName;
                    }
                    break;

                case sf::Keyboard::Key::L:
                    g.screen = (g.screen == drumming::Screen::LIBRARY)
                                   ? drumming::Screen::EDITOR
                                   : drumming::Screen::LIBRARY;
                    break;

                case sf::Keyboard::Key::H:
                    g.screen = (g.screen == drumming::Screen::STATS)
                                   ? drumming::Screen::EDITOR
                                   : drumming::Screen::STATS;
                    break;

                case sf::Keyboard::Key::Escape:
                    if (g.screen != drumming::Screen::EDITOR &&
                        g.screen != drumming::Screen::PLAY)
                        g.screen = drumming::Screen::HOME;
                    break;

                case sf::Keyboard::Key::Equal:
                case sf::Keyboard::Key::Add:
                    g.bpm = std::min(240, g.bpm + 5); break;
                case sf::Keyboard::Key::Hyphen:
                case sf::Keyboard::Key::Subtract:
                    g.bpm = std::max(40,  g.bpm - 5); break;
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

            // ── Text entered (groove naming) ─────────────────────────────────
            if (const auto* te = ev->getIf<sf::Event::TextEntered>()) {
                if (g.namingMode) {
                    uint32_t ch = te->unicode;
                    if (ch == 8 && !g.nameBuffer.empty()) {
                        g.nameBuffer.pop_back();
                    } else if (ch >= 32 && ch < 127 && g.nameBuffer.size() < 30) {
                        g.nameBuffer += (char)ch;
                    }
                }
            }

            // ── Mouse button ─────────────────────────────────────────────────
            if (const auto* mb = ev->getIf<sf::Event::MouseButtonPressed>()) {
                float mx = (float)mb->position.x;
                float my = (float)mb->position.y;

                // Sidebar nav
                if (mx < drumming::SIDEBAR_W && my > drumming::TITLEBAR_H &&
                    g.screen != drumming::Screen::PLAY) {
                    drumming::Screen hit = sidebarNavHit(mx, my);
                    if (hit != drumming::Screen::HOME) // HOME = sentinel for "no match"
                        g.screen = hit;
                }

                // Library row actions (load or delete)
                if (g.screen == drumming::Screen::LIBRARY && mx >= drumming::SIDEBAR_W) {
                    float col1    = drumming::SIDEBAR_W + 26.f;
                    float searchY = drumming::TITLEBAR_H + drumming::CONTENT_HEAD_H + 14.f;
                    float colY    = searchY + 50.f;
                    float listTop = colY + 24.f;
                    float rowH    = 44.f;
                    float delX    = (float)drumming::WINDOW_W - 60.f;

                    // "New groove" button
                    float btnX = (float)drumming::WINDOW_W - 150.f;
                    if (mx >= btnX && mx < btnX + 120.f &&
                        my >= searchY && my < searchY + 34.f) {
                        g.groove.clear();
                        g.currentGrooveName.clear();
                        g.screen = drumming::Screen::EDITOR;
                    }

                    // Groove row
                    if (my >= listTop && mx >= col1) {
                        int idx = (int)((my - listTop) / rowH) + g.libraryScroll;
                        if (idx >= 0 && idx < (int)g.library.size()) {
                            if (mx >= delX && mx < delX + 26.f) {
                                // Delete
                                if (g.currentGrooveName == g.library[idx].name)
                                    g.currentGrooveName.clear();
                                g.library.erase(g.library.begin() + idx);
                                drumming::saveGrooves(g);
                            } else {
                                // Load
                                const auto& sg = g.library[idx];
                                g.groove  = sg.groove;
                                g.bpm     = sg.bpm;
                                g.measures = sg.measures;
                                g.currentGrooveName = sg.name;
                                g.screen  = drumming::Screen::EDITOR;
                            }
                        }
                    }
                }

                // Home screen card clicks + Resume button
                if (g.screen == drumming::Screen::HOME && mx >= drumming::SIDEBAR_W) {
                    float cx    = drumming::SIDEBAR_W + 26.f;
                    float cy2   = drumming::TITLEBAR_H + drumming::CONTENT_HEAD_H + 22.f;
                    float cw    = (float)drumming::WINDOW_W - drumming::SIDEBAR_W - 52.f;
                    float cardY = cy2 + 78.f;
                    float cardW = (cw - 28.f) / 3.f;
                    float cardH = 112.f;

                    drumming::Screen cardSc[] = {
                        drumming::Screen::EDITOR,
                        drumming::Screen::LIBRARY,
                        drumming::Screen::STATS
                    };
                    for (int i = 0; i < 3; ++i) {
                        float bx = cx + i * (cardW + 14.f);
                        if (mx >= bx && mx < bx + cardW && my >= cardY && my < cardY + cardH)
                            g.screen = cardSc[i];
                    }

                    // Resume button
                    float cwY  = cardY + cardH + 20.f;
                    float btnX = cx + cw - 114.f;
                    if (!g.library.empty() &&
                        mx >= btnX && mx < btnX + 100.f &&
                        my >= cwY + 24.f && my < cwY + 56.f) {
                        const auto& sg = g.library.back();
                        g.groove  = sg.groove;
                        g.bpm     = sg.bpm;
                        g.measures = sg.measures;
                        g.currentGrooveName = sg.name;
                        g.screen  = drumming::Screen::EDITOR;
                    }
                }

                // Editor: place/remove notes
                if (g.screen == drumming::Screen::EDITOR &&
                    mb->button == sf::Mouse::Button::Left &&
                    my >= drumming::TITLEBAR_H + drumming::CONTENT_HEAD_H) {
                    auto [step, vi] = drumming::pickCell({mx, my}, g.totalSteps());
                    if (step >= 0 && vi >= 0) {
                        auto key = std::make_pair(step, vi);
                        if (g.groove.count(key)) g.groove.erase(key);
                        else                     g.groove.insert(key);
                    }
                }
            }

            // ── Mouse wheel (library scroll) ─────────────────────────────────
            if (const auto* ws = ev->getIf<sf::Event::MouseWheelScrolled>()) {
                if (g.screen == drumming::Screen::LIBRARY)
                    g.libraryScroll = std::max(0, g.libraryScroll - (int)ws->delta);
            }
        }

        // ── MIDI ────────────────────────────────────────────────────────────
        for (auto& ev : drumming::pollMidi()) {
            g.lastHit[ev.note] = ev.time;
            g.lastVel[ev.note] = ev.vel;

            if (g.screen == drumming::Screen::PLAY && g.currentStep >= 0) {
                int vi = drumming::voiceIndex(ev.note);
                if (vi >= 0) {
                    float ms      = std::chrono::duration<float, std::milli>(
                                        ev.time - g.playStart).count();
                    float stepF   = ms / g.stepDurMs();
                    int   nearest = (int)std::round(stepF) % g.totalSteps();
                    float offsetMs = (stepF - std::round(stepF)) * g.stepDurMs();
                    bool  correct  = g.groove.count({nearest, vi}) &&
                                     std::abs(offsetMs) < drumming::HIT_WINDOW_MS;
                    g.results.push_back({nearest, vi, offsetMs, correct});
                }
            }
        }

        // ── Play advance ────────────────────────────────────────────────────
        if (g.screen == drumming::Screen::PLAY) {
            float ms      = std::chrono::duration<float, std::milli>(
                                Clock::now() - g.playStart).count();
            int   rawStep = (int)(ms / g.stepDurMs());
            int   loop    = rawStep / g.totalSteps();
            int   step    = rawStep % g.totalSteps();

            if (loop > g.loopCount) {
                g.loopCount     = loop;
                g.results.clear();
                g.sessionActive = true;
            }
            g.currentStep = step;

            if (step != g.lastClickStep && step % drumming::STEPS_PER_BEAT == 0) {
                g.lastClickStep = step;
                if (step % drumming::STEPS_PER_MEASURE == 0) hiClick.play();
                else                                          loClick.play();
            }
        }

        // ── Render ──────────────────────────────────────────────────────────
        window.clear(sf::Color(247, 247, 248));

        // Chrome — always on top of everything
        drumming::drawTitleBar(window, font);
        drumming::drawSidebar(window, font, g);
        drumming::drawContentHeader(window, font, g);

        switch (g.screen) {
        case drumming::Screen::HOME:
            drumming::drawHomeScreen(window, font, g);
            break;
        case drumming::Screen::LIBRARY:
            drumming::drawLibraryScreen(window, font, g);
            break;
        case drumming::Screen::STATS:
            drumming::drawStatsScreen(window, font, g);
            break;
        case drumming::Screen::EDITOR:
        case drumming::Screen::PLAY:
            drumming::drawStaff(window, font, g, g.totalSteps());
            drumming::drawGroove(window, g, g.totalSteps());
            if (g.screen == drumming::Screen::PLAY) {
                drumming::drawResults(window, g, g.totalSteps());
                drumming::drawPlayhead(window, g);
            }
            drumming::drawKit(window, font, g);
            drumming::drawControls(window, font, g);
            break;
        }

        if (g.namingMode)
            drumming::drawNamingOverlay(window, font, g);

        window.display();
    }

    return 0;
}
