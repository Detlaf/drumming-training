#include "drumming/audio.h"
#include "drumming/draw.h"
#include "drumming/geometry.h"
#include "drumming/midi.h"
#include "drumming/persistence.h"
#include "drumming/scoring.h"
#include "drumming/types.h"
#include <SFML/Audio.hpp>
#include <SFML/Graphics.hpp>
#include <RtMidi.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <iostream>
#include <string>
#include <tuple>

using Clock = std::chrono::steady_clock;

// When launched from Finder as a .app bundle, the working directory is "/", so a
// relative "drumming.db" can't be created. Keep the database in the user's
// Application Support directory instead, which is writable regardless of how the
// app was launched.
static std::string defaultDatabasePath() {
    const char* home = std::getenv("HOME");
    if (home == nullptr) return "drumming.db";  // fall back to cwd if HOME is unset
    std::filesystem::path dir =
        std::filesystem::path(home) / "Library" / "Application Support" / "Drumming";
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    if (ec) return "drumming.db";
    return (dir / "drumming.db").string();
}

// Begin an explicit session: reset the accumulated tally and record the start
// wall-clock time. Hits scored while sessionActive is true are tallied below.
static void startSession(drumming::App& g) {
    g.sessionActive     = true;
    g.sessionStart      = Clock::now();
    g.sessionStartEpoch = (long long)std::time(nullptr);
    g.sCorrect = g.sEarly = g.sLate = g.sWrong = 0;
    g.results.clear();
}

static void endSession(drumming::App& g) {
    if (!g.sessionActive) return;
    long long startEpoch = g.sessionStartEpoch;
    long long endEpoch   = (long long)std::time(nullptr);
    int dur   = (int)std::chrono::duration<float>(Clock::now() - g.sessionStart).count();
    int ok    = g.sCorrect;
    int total = g.sCorrect + g.sEarly + g.sLate + g.sWrong;

    drumming::SessionRecord rec{};
    rec.grooveName     = g.currentGrooveName.empty() ? "<unsaved>" : g.currentGrooveName;
    rec.bpm            = g.bpm;
    rec.measures       = g.measures;
    rec.correctHits    = ok;
    rec.totalNotes     = total;
    rec.accuracyPct    = drumming::accuracyPct(ok, total);
    rec.timestampEpoch = startEpoch;
    rec.durationSecs   = dur;
    rec.earlyHits      = g.sEarly;
    rec.lateHits       = g.sLate;
    rec.wrongPadHits   = g.sWrong;
    rec.startedAtEpoch = startEpoch;
    rec.endedAtEpoch   = endEpoch;

    drumming::appendHistory(g, rec);
    g.sessionActive = false;
}

int main() {
    // MIDI — optional; app still works without a kit
    RtMidiIn midiin;
    bool midiConnected = midiin.getPortCount() > 0;
    if (midiConnected) {
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

    // Supersampled off-screen target: the UI is drawn at RENDER_SCALE× into
    // `scene` (with anti-aliasing) and then downsampled into the window for
    // crisper edges and text. Its view keeps logical window coordinates so all
    // drawing code is unchanged.
    const unsigned scale = drumming::RENDER_SCALE;
    const sf::Vector2u sceneSize{drumming::WINDOW_W * scale, drumming::WINDOW_H * scale};
    sf::RenderTexture scene;
    bool sceneReady = false;
    for (unsigned aa : {8u, 4u, 0u}) {  // fall back if the GPU caps anti-aliasing
        sf::ContextSettings sceneSettings;
        sceneSettings.antiAliasingLevel = aa;
        if (scene.resize(sceneSize, sceneSettings)) { sceneReady = true; break; }
    }
    if (!sceneReady) {
        std::cerr << "Failed to create render target\n";
        return 1;
    }
    scene.setSmooth(true);
    scene.setView(sf::View(sf::FloatRect(
        {0.f, 0.f}, {(float)drumming::WINDOW_W, (float)drumming::WINDOW_H})));

    sf::Font font;
    if (!font.openFromFile("/System/Library/Fonts/Helvetica.ttc")) {
        std::cerr << "Failed to load font\n";
        return 1;
    }

    sf::SoundBuffer hiClickBuf = drumming::makeClick(1100.f);
    sf::SoundBuffer loClickBuf = drumming::makeClick(700.f);
    sf::Sound hiClick(hiClickBuf), loClick(loClickBuf);

    drumming::setDatabasePath(defaultDatabasePath());

    drumming::App g;
    g.midiConnected = midiConnected;
    auto lastMidiScan = Clock::now();
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

                // Play screen: Start/Stop session button
                if (g.screen == drumming::Screen::PLAY &&
                    mb->button == sf::Mouse::Button::Left &&
                    drumming::sessionButtonRect().contains({mx, my})) {
                    if (g.sessionActive) endSession(g);
                    else                 startSession(g);
                }

                // Sidebar nav
                if (g.screen != drumming::Screen::PLAY) {
                    if (auto hit = drumming::sidebarNavHit({mx, my}))
                        g.screen = *hit;
                }

                // Library row actions (load or delete)
                if (g.screen == drumming::Screen::LIBRARY && mx >= drumming::SIDEBAR_W) {
                    // "New groove" button
                    if (drumming::libraryNewGrooveRect().contains({mx, my})) {
                        g.groove.clear();
                        g.currentGrooveName.clear();
                        g.screen = drumming::Screen::EDITOR;
                    }

                    // Groove row
                    int count = (int)g.library.size();
                    int idx   = drumming::libraryRowAt(my, g.libraryScroll, count);
                    if (idx >= 0 && mx >= drumming::SIDEBAR_W + 26.f) {
                        int rowFromTop =
                            idx - drumming::libraryListStart(g.libraryScroll, count);
                        if (drumming::libraryDeleteRect(rowFromTop).contains({mx, my})) {
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

                // Home screen card clicks + Resume button
                if (g.screen == drumming::Screen::HOME && mx >= drumming::SIDEBAR_W) {
                    std::array<drumming::Screen, 3> cardSc = {
                        drumming::Screen::EDITOR,
                        drumming::Screen::LIBRARY,
                        drumming::Screen::STATS
                    };
                    for (int i = 0; i < 3; ++i)
                        if (drumming::homeCardRect(i).contains({mx, my}))
                            g.screen = cardSc[i];

                    // Resume button
                    if (!g.library.empty() &&
                        drumming::homeResumeRect().contains({mx, my})) {
                        const auto& sg = g.library.back();
                        g.groove  = sg.groove;
                        g.bpm     = sg.bpm;
                        g.measures = sg.measures;
                        g.currentGrooveName = sg.name;
                        g.screen  = drumming::Screen::EDITOR;
                    }
                }

                // Editor: place/remove notes — via the grid sequencer or the staff
                if (g.screen == drumming::Screen::EDITOR &&
                    mb->button == sf::Mouse::Button::Left &&
                    my >= drumming::TITLEBAR_H + drumming::CONTENT_HEAD_H) {
                    auto [step, vi] = drumming::pickGridCell({mx, my}, g.totalSteps());
                    if (step < 0 || vi < 0)
                        std::tie(step, vi) = drumming::pickCell({mx, my}, g.totalSteps());
                    if (step >= 0 && vi >= 0) {
                        auto key = std::make_pair(step, vi);
                        if (g.groove.count(key) != 0) g.groove.erase(key);
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

        // ── MIDI hot-plug ─────────────────────────────────────────────────────
        // Poll port availability ~1×/sec so plugging/unplugging a kit updates
        // the connection state without a restart.
        if (Clock::now() - lastMidiScan > std::chrono::seconds(1)) {
            lastMidiScan = Clock::now();
            switch (drumming::decideMidiAction(g.midiConnected, midiin.getPortCount())) {
            case drumming::MidiPortAction::Open:
                midiin.openPort(0);
                midiin.setCallback(&drumming::midiCallback);
                midiin.ignoreTypes(false, false, false);
                g.midiConnected = true;
                break;
            case drumming::MidiPortAction::Close:
                midiin.cancelCallback();
                midiin.closePort();
                g.midiConnected = false;
                break;
            case drumming::MidiPortAction::None:
                break;
            }
        }

        // ── MIDI ────────────────────────────────────────────────────────────
        for (auto& ev : drumming::pollMidi()) {
            if (g.screen == drumming::Screen::PLAY && g.currentStep >= 0) {
                // An unrecognized pad maps to voice -1; it can never match a
                // groove cell, so scoreHit marks it incorrect and it counts as a
                // miss against accuracy rather than being silently dropped.
                int   vi = drumming::voiceIndex(ev.note);
                float ms = std::chrono::duration<float, std::milli>(
                               ev.time - g.playStart).count();
                drumming::HitResult hr = drumming::scoreHit(
                    ms, g.groove, vi, g.totalSteps(), g.stepDurMs());
                g.results.push_back(hr);

                // Accumulate session statistics while a session is running.
                if (g.sessionActive) {
                    switch (hr.cls) {
                    case drumming::HitClass::Correct:         ++g.sCorrect; break;
                    case drumming::HitClass::EarlyCorrectPad: ++g.sEarly;   break;
                    case drumming::HitClass::LateCorrectPad:  ++g.sLate;    break;
                    case drumming::HitClass::WrongPad:        ++g.sWrong;   break;
                    }
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
                g.loopCount = loop;
                g.results.clear();  // live strip shows the current loop only
            }
            g.currentStep = step;

            if (step != g.lastClickStep && step % drumming::STEPS_PER_BEAT == 0) {
                g.lastClickStep = step;
                if (step % drumming::STEPS_PER_MEASURE == 0) hiClick.play();
                else                                          loClick.play();
            }
        }

        // ── Render ──────────────────────────────────────────────────────────
        scene.clear(sf::Color(247, 247, 248));

        // Chrome — always on top of everything
        drumming::drawTitleBar(scene, font);
        drumming::drawSidebar(scene, font, g);
        drumming::drawContentHeader(scene, font, g);

        switch (g.screen) {
        case drumming::Screen::HOME:
            drumming::drawHomeScreen(scene, font, g);
            break;
        case drumming::Screen::LIBRARY:
            drumming::drawLibraryScreen(scene, font, g);
            break;
        case drumming::Screen::STATS:
            drumming::drawStatsScreen(scene, font, g);
            break;
        case drumming::Screen::EDITOR:
        case drumming::Screen::PLAY:
            drumming::drawPracticeView(scene, font, g);
            break;
        }

        if (g.namingMode)
            drumming::drawNamingOverlay(scene, font, g);

        scene.display();

        // Downsample the supersampled scene into the window.
        window.clear(sf::Color(247, 247, 248));
        sf::Sprite sceneSprite(scene.getTexture());
        sceneSprite.setScale({1.f / scale, 1.f / scale});
        window.draw(sceneSprite);
        window.display();
    }

    return 0;
}
