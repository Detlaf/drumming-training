#include "controller.h"
#include <QTimer>
#include <RtMidi.h>
#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <iostream>
#include <utility>
#include "drumming/midi.h"
#include "drumming/persistence.h"
#include "drumming/scoring.h"

namespace drumming {

using Clock = std::chrono::steady_clock;

std::string PracticeController::defaultDatabasePath() {
    const char* home = std::getenv("HOME");
    if (home == nullptr) return "drumming.db";  // fall back to cwd if HOME is unset
    std::filesystem::path dir =
        std::filesystem::path(home) / "Library" / "Application Support" / "Drumming";
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    if (ec) return "drumming.db";
    return (dir / "drumming.db").string();
}

std::string PracticeController::diagnosticLogPath() {
    const char* home = std::getenv("HOME");
    std::filesystem::path dir =
        (home != nullptr ? std::filesystem::path(home) : std::filesystem::path("."))
        / "Library" / "Application Support" / "Drumming" / "diagnostics";
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    std::time_t t = std::time(nullptr);
    std::tm     tm{};
#if defined(_WIN32)
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    char stamp[32];
    std::strftime(stamp, sizeof(stamp), "diag-%Y%m%d-%H%M%S.log", &tm);
    return (dir / stamp).string();
}

void PracticeController::beginDiagnosticCapture() {
    if (!diagnosticsEnabled_ || diagLogger_.isActive()) return;
    DiagSessionMeta meta{static_cast<long long>(std::time(nullptr)),
                         app_.bpm, app_.measures, app_.totalSteps(),
                         app_.stepDurMs(), app_.currentGrooveName, app_.groove};
    if (!diagLogger_.start(diagnosticLogPath(), meta)) {
        std::cerr << "diagnostics: cannot open log file — disabling capture\n";
        diagnosticsEnabled_ = false;
        return;
    }
    diagLogger_.logLifecycle(LifecycleKind::PlayStart, 0.0f, 0);
}

void PracticeController::endDiagnosticCapture() {
    if (!diagLogger_.isActive()) return;
    float ms = std::chrono::duration<float, std::milli>(
                   Clock::now() - app_.playStart).count();
    diagLogger_.logLifecycle(LifecycleKind::PlayStop, ms, app_.loopCount);
    diagLogger_.stop();
}

PracticeController::PracticeController(QObject* parent) : QObject(parent) {
    // MIDI — optional; the app still works without a kit.
    midiIn_ = std::make_unique<RtMidiIn>();
    bool midiConnected = midiIn_->getPortCount() > 0;
    if (midiConnected) {
        midiIn_->openPort(0);
        midiIn_->setCallback(&midiCallback);
        midiIn_->ignoreTypes(false, false, false);
    } else {
        std::cerr << "No MIDI ports — running without kit\n";
    }
    app_.midiConnected = midiConnected;

    // Three timers replace the single SFML poll loop. They only do work on the
    // relevant screen, so leaving them all running is cheap and matches the old
    // loop's per-iteration guards. They fire once the QApplication event loop
    // starts in main().
    playTimer_ = new QTimer(this);
    playTimer_->setInterval(8);
    connect(playTimer_, &QTimer::timeout, this, &PracticeController::playTick);
    playTimer_->start();

    midiTimer_ = new QTimer(this);
    midiTimer_->setInterval(4);
    connect(midiTimer_, &QTimer::timeout, this, &PracticeController::pollMidiTick);
    midiTimer_->start();

    hotPlugTimer_ = new QTimer(this);
    hotPlugTimer_->setInterval(1000);
    connect(hotPlugTimer_, &QTimer::timeout, this, &PracticeController::hotPlugTick);
    hotPlugTimer_->start();
}

PracticeController::~PracticeController() = default;

// ── Navigation ──────────────────────────────────────────────────────────────

void PracticeController::setScreen(Screen screen) {
    if (app_.screen == screen) return;
    if (app_.screen == Screen::PLAY && screen != Screen::PLAY) endDiagnosticCapture();
    app_.screen = screen;
    emit screenChanged(screen);
}

void PracticeController::enterPlay() {
    app_.screen        = Screen::PLAY;
    app_.results.clear();
    app_.currentStep   = -1;
    app_.lastClickStep = -1;
    app_.loopCount     = 0;
    app_.sessionActive = false;
    app_.playStart     = Clock::now();
    app_.sessionStart  = app_.playStart;
    beginDiagnosticCapture();
    emit screenChanged(app_.screen);
}

void PracticeController::togglePlay() {
    switch (app_.screen) {
    case Screen::HOME:
    case Screen::LIBRARY:
    case Screen::STATS:
        setScreen(Screen::EDITOR);
        break;
    case Screen::EDITOR:
        enterPlay();
        break;
    case Screen::PLAY:
        endSession();
        endDiagnosticCapture();
        app_.screen      = Screen::EDITOR;
        app_.currentStep = -1;
        emit screenChanged(app_.screen);
        break;
    }
}

// ── Editor edits ──────────────────────────────────────────────────────────────

void PracticeController::setBpm(int bpm) {
    bpm = std::clamp(bpm, 40, 240);
    if (bpm == app_.bpm) return;
    app_.bpm = bpm;
    emit grooveChanged();
}

void PracticeController::setMeasures(int measures) {
    measures = std::clamp(measures, 1, 8);
    if (measures == app_.measures) return;
    bool shrinking = measures < app_.measures;
    app_.measures  = measures;
    if (shrinking) {
        // Drop notes that now fall past the (shorter) loop.
        for (auto it = app_.groove.begin(); it != app_.groove.end();)
            it = it->first >= app_.totalSteps() ? app_.groove.erase(it) : std::next(it);
    }
    emit grooveChanged();
}

void PracticeController::clearGroove() {
    app_.groove.clear();
    app_.results.clear();
    emit grooveChanged();
}

void PracticeController::toggleCell(int step, int voice) {
    auto key = std::make_pair(step, voice);
    if (app_.groove.count(key) != 0) app_.groove.erase(key);
    else                             app_.groove.insert(key);
    emit grooveChanged();
}

// ── Library ────────────────────────────────────────────────────────────────

void PracticeController::newGroove() {
    app_.groove.clear();
    app_.currentGrooveName.clear();
    app_.screen = Screen::EDITOR;
    emit grooveChanged();
    emit screenChanged(app_.screen);
}

void PracticeController::loadGroove(int index) {
    if (index < 0 || index >= static_cast<int>(app_.library.size())) return;
    const auto& sg = app_.library[index];
    app_.groove            = sg.groove;
    app_.bpm               = sg.bpm;
    app_.measures          = sg.measures;
    app_.currentGrooveName = sg.name;
    app_.screen            = Screen::EDITOR;
    emit grooveChanged();
    emit screenChanged(app_.screen);
}

void PracticeController::deleteGroove(int index) {
    if (index < 0 || index >= static_cast<int>(app_.library.size())) return;
    if (app_.currentGrooveName == app_.library[index].name)
        app_.currentGrooveName.clear();
    app_.library.erase(app_.library.begin() + index);
    saveGrooves(app_);
    emit grooveChanged();
}

void PracticeController::resumeLast() {
    if (app_.library.empty()) return;
    loadGroove(static_cast<int>(app_.library.size()) - 1);
}

void PracticeController::saveGrooveAs(const QString& name) {
    if (name.isEmpty()) return;
    std::string n = name.toStdString();
    bool found = false;
    for (auto& sg : app_.library) {
        if (sg.name == n) {
            sg.bpm = app_.bpm; sg.measures = app_.measures; sg.groove = app_.groove;
            found = true; break;
        }
    }
    if (!found)
        app_.library.push_back({n, app_.bpm, app_.measures, app_.groove});
    app_.currentGrooveName = n;
    saveGrooves(app_);
    emit grooveChanged();
}

// ── Session ────────────────────────────────────────────────────────────────

void PracticeController::startSession() {
    app_.sessionActive     = true;
    app_.sessionStart      = Clock::now();
    app_.sessionStartEpoch = static_cast<long long>(std::time(nullptr));
    app_.sCorrect = app_.sEarly = app_.sLate = app_.sWrong = 0;
    app_.results.clear();
}

void PracticeController::endSession() {
    if (!app_.sessionActive) return;
    long long startEpoch = app_.sessionStartEpoch;
    long long endEpoch   = static_cast<long long>(std::time(nullptr));
    int dur   = static_cast<int>(
        std::chrono::duration<float>(Clock::now() - app_.sessionStart).count());
    int ok    = app_.sCorrect;
    int total = app_.sCorrect + app_.sEarly + app_.sLate + app_.sWrong;

    SessionRecord rec{};
    rec.grooveName     = app_.currentGrooveName.empty() ? "<unsaved>" : app_.currentGrooveName;
    rec.bpm            = app_.bpm;
    rec.measures       = app_.measures;
    rec.correctHits    = ok;
    rec.totalNotes     = total;
    rec.accuracyPct    = accuracyPct(ok, total);
    rec.timestampEpoch = startEpoch;
    rec.durationSecs   = dur;
    rec.earlyHits      = app_.sEarly;
    rec.lateHits       = app_.sLate;
    rec.wrongPadHits   = app_.sWrong;
    rec.startedAtEpoch = startEpoch;
    rec.endedAtEpoch   = endEpoch;

    appendHistory(app_, rec);
    app_.sessionActive = false;
    emit statsChanged();
}

void PracticeController::toggleSession() {
    if (app_.sessionActive) endSession();
    else                    startSession();
}

void PracticeController::setDiagnosticsEnabled(bool enabled) {
    diagnosticsEnabled_ = enabled;
    if (enabled) {
        // If we're already playing, start capturing right away.
        if (app_.screen == Screen::PLAY) beginDiagnosticCapture();
    } else {
        diagLogger_.stop();
    }
}

// ── Timer slots (ported from the SFML poll loop) ──────────────────────────────

void PracticeController::pollMidiTick() {
    bool changed = false;
    for (auto& ev : pollMidi()) {
        if (app_.screen == Screen::PLAY && app_.currentStep >= 0) {
            // An unrecognized pad maps to voice -1; it can never match a groove
            // cell, so scoreHit marks it incorrect and it counts as a miss
            // rather than being silently dropped.
            int   vi = voiceIndex(ev.note);
            float ms = std::chrono::duration<float, std::milli>(
                           ev.time - app_.playStart).count();
            HitResult hr = scoreHit(ms, app_.groove, vi, app_.totalSteps(), app_.stepDurMs());
            app_.results.push_back(hr);

            if (diagLogger_.isActive())
                diagLogger_.logHit(makeHitLogEntry(ms, ev, hr, app_.sessionActive));

            // Accumulate session statistics while a session is running.
            if (app_.sessionActive) {
                switch (hr.cls) {
                case HitClass::Correct:         ++app_.sCorrect; break;
                case HitClass::EarlyCorrectPad: ++app_.sEarly;   break;
                case HitClass::LateCorrectPad:  ++app_.sLate;    break;
                case HitClass::WrongPad:        ++app_.sWrong;   break;
                }
            }
            changed = true;
        }
    }
    if (changed) emit tick();
}

void PracticeController::hotPlugTick() {
    // A dedicated 1 s timer replaces the old manual `lastMidiScan` throttle.
    switch (decideMidiAction(app_.midiConnected, midiIn_->getPortCount())) {
    case MidiPortAction::Open:
        midiIn_->openPort(0);
        midiIn_->setCallback(&midiCallback);
        midiIn_->ignoreTypes(false, false, false);
        app_.midiConnected = true;
        emit midiConnectionChanged(true);
        break;
    case MidiPortAction::Close:
        midiIn_->cancelCallback();
        midiIn_->closePort();
        app_.midiConnected = false;
        emit midiConnectionChanged(false);
        break;
    case MidiPortAction::None:
        break;
    }
}

void PracticeController::playTick() {
    if (app_.screen != Screen::PLAY) return;

    float ms      = std::chrono::duration<float, std::milli>(
                        Clock::now() - app_.playStart).count();
    int   rawStep = static_cast<int>(ms / app_.stepDurMs());
    int   loop    = rawStep / app_.totalSteps();
    int   step    = rawStep % app_.totalSteps();

    if (loop > app_.loopCount) {
        app_.loopCount = loop;
        app_.results.clear();  // live strip shows the current loop only
        if (diagLogger_.isActive()) {
            float loopMs = std::chrono::duration<float, std::milli>(
                               Clock::now() - app_.playStart).count();
            diagLogger_.logLifecycle(LifecycleKind::Loop, loopMs, loop);
        }
    }
    app_.currentStep = step;

    if (step != app_.lastClickStep && step % STEPS_PER_BEAT == 0) {
        app_.lastClickStep = step;
        if (step % STEPS_PER_MEASURE == 0) metronome_.playHi();
        else                               metronome_.playLo();
    }

    emit tick();
}

} // namespace drumming
