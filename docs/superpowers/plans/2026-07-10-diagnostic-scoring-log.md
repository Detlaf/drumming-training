# Diagnostic Scoring Log Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add an opt-in diagnostic that records how each hit is scored against the current groove during Play, as a JSON Lines `.log` file toggled from a Play-screen control.

**Architecture:** A Qt-free `DiagnosticLogger` class (plus a pure `makeHitLogEntry` helper) lives in `drumming_core` so it is unit-testable. `PracticeController` (in the Qt-only `drum_viz` target) owns a logger instance and calls it from the scoring/lifecycle points it already runs (`enterPlay`, `pollMidiTick`, `playTick`, Play-exit). A checkable "Diagnostics" control in `StaffGridWidget` drives a `setDiagnosticsEnabled` slot.

**Tech Stack:** C++17, Qt 6 Widgets, Catch2 (`drumming_tests`), CMake ≥ 3.15, `std::ofstream`.

## Global Constraints

- Use **Qt 6** APIs only in `drum_viz` sources; `drumming_core` stays **Qt-free** (it links into `drumming_tests`, which has no GUI toolkit).
- **No new external dependencies** — JSON is hand-written (fields are scalars or small `[step, voice]` arrays).
- Logging must **never crash or interrupt practice**: file-open failure prints to `stderr` and disables the toggle.
- Tests use **Catch2** (`#include <catch2/catch_test_macros.hpp>`), run via `ctest --test-dir build --output-on-failure`.
- Build verification: `cmake --build build` then `ctest --test-dir build --output-on-failure`.
- JSONL: exactly one JSON object per line, flushed per line.
- `tMs` is milliseconds since `app_.playStart` (same clock the scorer uses).

---

### Task 1: `DiagnosticLogger` core class + `makeHitLogEntry` helper

**Files:**
- Create: `include/drumming/diagnostic_logger.h`
- Create: `src/diagnostic_logger.cpp`
- Test: `tests/test_diagnostic_logger.cpp`
- Modify: `CMakeLists.txt` (add `src/diagnostic_logger.cpp` to `drumming_core`; add `tests/test_diagnostic_logger.cpp` to `drumming_tests`)

**Interfaces:**
- Consumes: `HitResult`, `HitClass`, `MidiEv` from `drumming/types.h`.
- Produces (later tasks rely on these exact names/types):
  - `struct DiagSessionMeta { long long startedAtEpoch; int bpm; int measures; int totalSteps; float stepDurMs; std::string grooveName; std::set<std::pair<int,int>> groove; };`
  - `struct HitLogEntry { float tMs; int note; int vel; int voice; int snapStep; float offsetMs; HitClass cls; bool sessionActive; };`
  - `enum class LifecycleKind { PlayStart, PlayStop, Loop };`
  - `class DiagnosticLogger { public: bool start(const std::string& path, const DiagSessionMeta&); void logHit(const HitLogEntry&); void logLifecycle(LifecycleKind, float tMs, int loop); void stop(); bool isActive() const; };`
  - `HitLogEntry makeHitLogEntry(float tMs, const MidiEv& ev, const HitResult& hr, bool sessionActive);`
  - `const char* hitClassName(HitClass);`

- [ ] **Step 1: Create the header**

Create `include/drumming/diagnostic_logger.h`:

```cpp
#pragma once
#include <fstream>
#include <set>
#include <string>
#include <utility>
#include "drumming/types.h"

namespace drumming {

// Context recorded once, in the header line, when a capture starts.
struct DiagSessionMeta {
    long long                     startedAtEpoch;  // wall-clock secs (UTC header)
    int                           bpm;
    int                           measures;
    int                           totalSteps;
    float                         stepDurMs;
    std::string                   grooveName;      // "" -> serialized as "<unsaved>"
    std::set<std::pair<int, int>> groove;          // {step, voice} pairs
};

// One scored MIDI hit: the full input -> output of scoreHit for this event.
struct HitLogEntry {
    float    tMs;           // ms since playStart
    int      note;          // raw MIDI note
    int      vel;           // raw MIDI velocity
    int      voice;         // mapped voice, -1 if unrecognized
    int      snapStep;      // matched/nearest step
    float    offsetMs;      // timing offset to the matched note
    HitClass cls;           // Correct / EarlyCorrectPad / LateCorrectPad / WrongPad
    bool     sessionActive; // was a formal practice session running
};

enum class LifecycleKind { PlayStart, PlayStop, Loop };

// Canonical JSON string for a HitClass (matches the enum names).
const char* hitClassName(HitClass cls);

// Build a hit-log entry from raw MIDI + scoring output. Qt-free and pure so the
// "log reflects real scoring" guarantee is unit-testable without the controller.
HitLogEntry makeHitLogEntry(float tMs, const MidiEv& ev, const HitResult& hr,
                            bool sessionActive);

// Writes JSON Lines to a .log file. One JSON object per line, flushed per line.
// Never throws for I/O problems; start() returns false on open failure and all
// other methods no-op while inactive.
class DiagnosticLogger {
public:
    bool start(const std::string& path, const DiagSessionMeta& meta);
    void logHit(const HitLogEntry& e);
    void logLifecycle(LifecycleKind kind, float tMs, int loop);
    void stop();
    bool isActive() const { return out_.is_open(); }

private:
    std::ofstream out_;
};

} // namespace drumming
```

- [ ] **Step 2: Write the failing tests**

Create `tests/test_diagnostic_logger.cpp`:

```cpp
#include <catch2/catch_test_macros.hpp>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include "drumming/diagnostic_logger.h"

using namespace drumming;

namespace {
// Read every non-empty line of a file back.
std::vector<std::string> readLines(const std::string& path) {
    std::vector<std::string> lines;
    std::ifstream in(path);
    std::string   line;
    while (std::getline(in, line))
        if (!line.empty()) lines.push_back(line);
    return lines;
}

// Unique temp path for one test.
std::string tempLogPath(const char* tag) {
    auto p = std::filesystem::temp_directory_path() /
             (std::string("diagtest-") + tag + ".log");
    std::filesystem::remove(p);
    return p.string();
}
} // namespace

TEST_CASE("hitClassName maps every HitClass to its enum name", "[diag]") {
    CHECK(std::string(hitClassName(HitClass::Correct)) == "Correct");
    CHECK(std::string(hitClassName(HitClass::EarlyCorrectPad)) == "EarlyCorrectPad");
    CHECK(std::string(hitClassName(HitClass::LateCorrectPad)) == "LateCorrectPad");
    CHECK(std::string(hitClassName(HitClass::WrongPad)) == "WrongPad");
}

TEST_CASE("makeHitLogEntry copies MIDI + scoring fields", "[diag]") {
    MidiEv    ev{38, 90, {}};
    HitResult hr{4, 3, 12.5f, true, HitClass::Correct};
    HitLogEntry e = makeHitLogEntry(1234.5f, ev, hr, true);
    CHECK(e.tMs == 1234.5f);
    CHECK(e.note == 38);
    CHECK(e.vel == 90);
    CHECK(e.voice == 3);
    CHECK(e.snapStep == 4);
    CHECK(e.offsetMs == 12.5f);
    CHECK(e.cls == HitClass::Correct);
    CHECK(e.sessionActive == true);
}

TEST_CASE("start writes one session header line with groove context", "[diag]") {
    std::string path = tempLogPath("header");
    DiagSessionMeta meta{1720000000LL, 100, 2, 32, 93.75f, "Funk 1", {{4, 0}, {8, 3}}};
    DiagnosticLogger log;
    REQUIRE(log.start(path, meta));
    log.stop();

    auto lines = readLines(path);
    REQUIRE(lines.size() == 1);
    CHECK(lines[0].find("\"type\":\"session\"") != std::string::npos);
    CHECK(lines[0].find("\"bpm\":100") != std::string::npos);
    CHECK(lines[0].find("\"measures\":2") != std::string::npos);
    CHECK(lines[0].find("\"totalSteps\":32") != std::string::npos);
    CHECK(lines[0].find("\"name\":\"Funk 1\"") != std::string::npos);
    CHECK(lines[0].find("[4,0]") != std::string::npos);
    CHECK(lines[0].find("[8,3]") != std::string::npos);
    std::filesystem::remove(path);
}

TEST_CASE("empty groove name serializes as <unsaved>", "[diag]") {
    std::string path = tempLogPath("unsaved");
    DiagSessionMeta meta{1720000000LL, 100, 1, 16, 93.75f, "", {}};
    DiagnosticLogger log;
    REQUIRE(log.start(path, meta));
    log.stop();
    auto lines = readLines(path);
    REQUIRE(lines.size() == 1);
    CHECK(lines[0].find("\"name\":\"<unsaved>\"") != std::string::npos);
    std::filesystem::remove(path);
}

TEST_CASE("logHit writes one hit line per call with all fields", "[diag]") {
    std::string path = tempLogPath("hit");
    DiagSessionMeta meta{1720000000LL, 100, 2, 32, 93.75f, "G", {}};
    DiagnosticLogger log;
    REQUIRE(log.start(path, meta));
    log.logHit({1234.5f, 38, 90, 3, 4, 12.3f, HitClass::Correct, true});
    log.stop();

    auto lines = readLines(path);
    REQUIRE(lines.size() == 2);  // header + hit
    const std::string& h = lines[1];
    CHECK(h.find("\"type\":\"hit\"") != std::string::npos);
    CHECK(h.find("\"note\":38") != std::string::npos);
    CHECK(h.find("\"vel\":90") != std::string::npos);
    CHECK(h.find("\"voice\":3") != std::string::npos);
    CHECK(h.find("\"snapStep\":4") != std::string::npos);
    CHECK(h.find("\"class\":\"Correct\"") != std::string::npos);
    CHECK(h.find("\"sessionActive\":true") != std::string::npos);
    std::filesystem::remove(path);
}

TEST_CASE("logLifecycle writes typed timeline lines", "[diag]") {
    std::string path = tempLogPath("life");
    DiagSessionMeta meta{1720000000LL, 100, 2, 32, 93.75f, "G", {}};
    DiagnosticLogger log;
    REQUIRE(log.start(path, meta));
    log.logLifecycle(LifecycleKind::Loop, 3000.0f, 1);
    log.logLifecycle(LifecycleKind::PlayStop, 8123.4f, 2);
    log.stop();

    auto lines = readLines(path);
    REQUIRE(lines.size() == 3);
    CHECK(lines[1].find("\"type\":\"loop\"") != std::string::npos);
    CHECK(lines[1].find("\"loop\":1") != std::string::npos);
    CHECK(lines[2].find("\"type\":\"playStop\"") != std::string::npos);
    std::filesystem::remove(path);
}

TEST_CASE("start on an unwritable path returns false and stays inactive", "[diag]") {
    DiagSessionMeta meta{1720000000LL, 100, 2, 32, 93.75f, "G", {}};
    DiagnosticLogger log;
    CHECK_FALSE(log.start("/no/such/dir/nope.log", meta));
    CHECK_FALSE(log.isActive());
    // Methods must no-op while inactive (no crash).
    log.logHit({0, 0, 0, 0, 0, 0, HitClass::WrongPad, false});
    log.logLifecycle(LifecycleKind::PlayStop, 0, 0);
}
```

- [ ] **Step 3: Wire both files into CMake**

In `CMakeLists.txt`, add to the `drumming_core` source list (after `src/scoring.cpp`):

```cmake
    src/diagnostic_logger.cpp
```

And add to the `drumming_tests` source list (after `tests/test_scoring.cpp`):

```cmake
    tests/test_diagnostic_logger.cpp
```

- [ ] **Step 4: Run tests to verify they fail**

Run: `cmake --build build 2>&1 | tail -20`
Expected: FAIL — `diagnostic_logger.cpp` does not exist / undefined references to `drumming::DiagnosticLogger::start`, `makeHitLogEntry`, `hitClassName`.

- [ ] **Step 5: Implement the logger**

Create `src/diagnostic_logger.cpp`:

```cpp
#include "drumming/diagnostic_logger.h"
#include <ctime>

namespace drumming {

const char* hitClassName(HitClass cls) {
    switch (cls) {
    case HitClass::Correct:         return "Correct";
    case HitClass::EarlyCorrectPad: return "EarlyCorrectPad";
    case HitClass::LateCorrectPad:  return "LateCorrectPad";
    case HitClass::WrongPad:        return "WrongPad";
    }
    return "Unknown";
}

HitLogEntry makeHitLogEntry(float tMs, const MidiEv& ev, const HitResult& hr,
                            bool sessionActive) {
    return HitLogEntry{tMs, ev.note, ev.vel, hr.voice,
                       hr.step, hr.offsetMs, hr.cls, sessionActive};
}

namespace {
// ISO-8601 UTC timestamp, e.g. 2026-07-10T14:03:22Z.
std::string isoUtc(long long epoch) {
    std::time_t t = static_cast<std::time_t>(epoch);
    std::tm     tm{};
#if defined(_WIN32)
    gmtime_s(&tm, &t);
#else
    gmtime_r(&t, &tm);
#endif
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm);
    return buf;
}

const char* lifecycleName(LifecycleKind kind) {
    switch (kind) {
    case LifecycleKind::PlayStart: return "playStart";
    case LifecycleKind::PlayStop:  return "playStop";
    case LifecycleKind::Loop:      return "loop";
    }
    return "unknown";
}
} // namespace

bool DiagnosticLogger::start(const std::string& path, const DiagSessionMeta& meta) {
    if (out_.is_open()) out_.close();
    out_.open(path, std::ios::out | std::ios::trunc);
    if (!out_.is_open()) return false;

    const std::string name = meta.grooveName.empty() ? "<unsaved>" : meta.grooveName;
    out_ << "{\"type\":\"session\""
         << ",\"startedAt\":\"" << isoUtc(meta.startedAtEpoch) << "\""
         << ",\"groove\":{\"bpm\":" << meta.bpm
         << ",\"measures\":" << meta.measures
         << ",\"totalSteps\":" << meta.totalSteps
         << ",\"stepDurMs\":" << meta.stepDurMs
         << ",\"name\":\"" << name << "\""
         << ",\"notes\":[";
    bool first = true;
    for (const auto& [step, voice] : meta.groove) {
        if (!first) out_ << ",";
        out_ << "[" << step << "," << voice << "]";
        first = false;
    }
    out_ << "]}}\n";
    out_.flush();
    return true;
}

void DiagnosticLogger::logHit(const HitLogEntry& e) {
    if (!out_.is_open()) return;
    out_ << "{\"type\":\"hit\""
         << ",\"tMs\":" << e.tMs
         << ",\"note\":" << e.note
         << ",\"vel\":" << e.vel
         << ",\"voice\":" << e.voice
         << ",\"snapStep\":" << e.snapStep
         << ",\"offsetMs\":" << e.offsetMs
         << ",\"class\":\"" << hitClassName(e.cls) << "\""
         << ",\"sessionActive\":" << (e.sessionActive ? "true" : "false")
         << "}\n";
    out_.flush();
}

void DiagnosticLogger::logLifecycle(LifecycleKind kind, float tMs, int loop) {
    if (!out_.is_open()) return;
    out_ << "{\"type\":\"" << lifecycleName(kind) << "\""
         << ",\"tMs\":" << tMs
         << ",\"loop\":" << loop
         << "}\n";
    out_.flush();
}

void DiagnosticLogger::stop() {
    if (out_.is_open()) out_.close();
}

} // namespace drumming
```

- [ ] **Step 6: Run tests to verify they pass**

Run: `cmake --build build && ctest --test-dir build --output-on-failure -R diag`
Expected: PASS — all `[diag]` test cases pass.

- [ ] **Step 7: Commit**

```bash
git add include/drumming/diagnostic_logger.h src/diagnostic_logger.cpp tests/test_diagnostic_logger.cpp CMakeLists.txt
git commit -m "Add DiagnosticLogger: JSONL scoring capture core"
```

---

### Task 2: Wire the logger into `PracticeController`

**Files:**
- Modify: `src/controller.h` (add include, member, flag, slot decl, path helper decl)
- Modify: `src/controller.cpp` (implement `setDiagnosticsEnabled`, path helper; call logger in `enterPlay`, `pollMidiTick`, `playTick`, Play-exit in `togglePlay`)

**Interfaces:**
- Consumes from Task 1: `DiagnosticLogger`, `DiagSessionMeta`, `makeHitLogEntry`, `LifecycleKind`.
- Produces (Task 3 relies on this exact slot): `void PracticeController::setDiagnosticsEnabled(bool enabled);` and `bool PracticeController::diagnosticsEnabled() const;`

This task has no Catch2 test: `PracticeController` is Qt-only and is not linked into `drumming_tests`. The scoring→log-entry mapping is already unit-tested via `makeHitLogEntry` in Task 1. Verify by build + a manual Play-through (Step 6).

- [ ] **Step 1: Add member, flag, slot, and helper to the header**

In `src/controller.h`, add the include near the top (after `#include "audio.h"`):

```cpp
#include "drumming/diagnostic_logger.h"
```

Add a public slot in the `public slots:` block (after `void toggleSession();`):

```cpp
    // Diagnostics capture toggle (Play-screen control).
    void setDiagnosticsEnabled(bool enabled);
```

Add a public accessor in the `public:` block (after `const App& app() const {...}`):

```cpp
    bool diagnosticsEnabled() const { return diagnosticsEnabled_; }
```

Add private members (in the `private:` block, after `Metronome metronome_;`):

```cpp
    DiagnosticLogger diagLogger_;
    bool             diagnosticsEnabled_ = false;
```

Add a private helper declaration (in the `private:` block, after `void enterPlay();`):

```cpp
    // Opens a fresh timestamped capture file if diagnostics are enabled.
    void beginDiagnosticCapture();
    // Path like ~/Library/Application Support/Drumming/diagnostics/diag-YYYYMMDD-HHMMSS.log
    static std::string diagnosticLogPath();
```

- [ ] **Step 2: Implement the path helper and capture start in `controller.cpp`**

In `src/controller.cpp`, after the `defaultDatabasePath()` definition (ends near line 27), add:

```cpp
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
```

- [ ] **Step 3: Implement `setDiagnosticsEnabled` and hook the lifecycle**

In `src/controller.cpp`, add after `toggleSession()` (near line 233):

```cpp
void PracticeController::setDiagnosticsEnabled(bool enabled) {
    diagnosticsEnabled_ = enabled;
    if (enabled) {
        // If we're already playing, start capturing right away.
        if (app_.screen == Screen::PLAY) beginDiagnosticCapture();
    } else {
        diagLogger_.stop();
    }
}
```

In `enterPlay()` (near line 72), add before the closing `emit screenChanged(...)`:

```cpp
    beginDiagnosticCapture();
```

In `togglePlay()`, the `case Screen::PLAY:` branch (near line 94), add after `endSession();`:

```cpp
        if (diagLogger_.isActive()) {
            float ms = std::chrono::duration<float, std::milli>(
                           Clock::now() - app_.playStart).count();
            diagLogger_.logLifecycle(LifecycleKind::PlayStop, ms, app_.loopCount);
            diagLogger_.stop();
        }
```

- [ ] **Step 4: Log hits and loop rollovers**

In `pollMidiTick()` (near line 248), right after `app_.results.push_back(hr);`, add:

```cpp
            if (diagLogger_.isActive())
                diagLogger_.logHit(makeHitLogEntry(ms, ev, hr, app_.sessionActive));
```

In `playTick()`, inside the `if (loop > app_.loopCount)` block (near line 295), after `app_.results.clear();`, add:

```cpp
        if (diagLogger_.isActive()) {
            float loopMs = std::chrono::duration<float, std::milli>(
                               Clock::now() - app_.playStart).count();
            diagLogger_.logLifecycle(LifecycleKind::Loop, loopMs, loop);
        }
```

- [ ] **Step 5: Build**

Run: `cmake --build build 2>&1 | tail -20`
Expected: PASS — `drum_viz` and `drumming_tests` compile cleanly.

- [ ] **Step 6: Manual verification (temporary hook)**

Confirm the wiring end-to-end before the UI exists. Temporarily enable diagnostics at construction: in the `PracticeController` constructor add `diagnosticsEnabled_ = true;` at the end, then:

Run: `open build/Drumming.app` (or run `build/drum_viz`), enter Play, let it loop once.
Expected: a file `~/Library/Application Support/Drumming/diagnostics/diag-*.log` exists, its first line is `{"type":"session"...}`, followed by `playStart`, any `hit` lines, at least one `loop` line, and a `playStop` line on exit.

Then **remove** the temporary `diagnosticsEnabled_ = true;` line.

- [ ] **Step 7: Commit**

```bash
git add src/controller.h src/controller.cpp
git commit -m "Wire DiagnosticLogger into PracticeController"
```

---

### Task 3: Play-screen "Diagnostics" toggle

**Files:**
- Modify: `src/ui/staffgridwidget.h` (add `QCheckBox* diagCheck_` member; forward-declare `QCheckBox`)
- Modify: `src/ui/staffgridwidget.cpp` (create the checkbox in the controls strip; wire to `setDiagnosticsEnabled`; show only on Play in `syncControls`)

**Interfaces:**
- Consumes from Task 2: slot `PracticeController::setDiagnosticsEnabled(bool)`, accessor `diagnosticsEnabled()`.

This task has no Catch2 test (Qt widget). Verify by build + visual check (Step 4).

- [ ] **Step 1: Add the member to the header**

In `src/ui/staffgridwidget.h`, add a forward declaration near the other Qt forward decls (by `class QPushButton;`):

```cpp
class QCheckBox;
```

Add the member after `QPushButton* sessionBtn_ = nullptr;` (near line 60):

```cpp
    QCheckBox*   diagCheck_   = nullptr;
```

- [ ] **Step 2: Create and wire the checkbox**

In `src/ui/staffgridwidget.cpp`, add the include near the other Qt widget includes (by `#include <QPushButton>`):

```cpp
#include <QCheckBox>
```

In the controls-strip setup, after the `sessionBtn_` block (after `row->addWidget(sessionBtn_);`, near line 380), add:

```cpp
    diagCheck_ = new QCheckBox("Diagnostics", strip);
    diagCheck_->setToolTip("Capture a JSONL scoring log for this Play session");
    connect(diagCheck_, &QCheckBox::toggled, &controller_,
            &PracticeController::setDiagnosticsEnabled);
    row->addWidget(diagCheck_);
```

- [ ] **Step 3: Show the toggle only on Play**

In `syncControls()`, after the `sessionBtn_->setVisible(playing);` line (near line 400), add:

```cpp
    diagCheck_->setVisible(playing);
    // Reflect controller state (e.g. auto-disabled on file-open failure).
    if (diagCheck_->isChecked() != controller_.diagnosticsEnabled()) {
        QSignalBlocker block(diagCheck_);
        diagCheck_->setChecked(controller_.diagnosticsEnabled());
    }
```

Add the include for `QSignalBlocker` near the other Qt includes if not already present:

```cpp
#include <QSignalBlocker>
```

- [ ] **Step 4: Build and visually verify**

Run: `cmake --build build && ctest --test-dir build --output-on-failure`
Expected: PASS — full build and all tests green.

Then: `open build/Drumming.app`, go to Play. Confirm a "Diagnostics" checkbox appears next to the session button (and is hidden in the Editor). Tick it, play a loop, untick or exit Play, and confirm a `diag-*.log` file was written under `~/Library/Application Support/Drumming/diagnostics/`. Capture a screenshot of the Play screen showing the checkbox.

- [ ] **Step 5: Commit**

```bash
git add src/ui/staffgridwidget.h src/ui/staffgridwidget.cpp
git commit -m "Add Play-screen Diagnostics capture toggle"
```

---

## Self-Review Notes

- **Spec coverage:** §1 purpose (scoring/timing) → Task 1 schema + Task 2 hit logging; §2 components (`DiagnosticLogger`, controller member/slot, UI control) → Tasks 1/2/3; §3 JSONL schema (session/hit/lifecycle lines) → Task 1 `start`/`logHit`/`logLifecycle`; §4 file location/lifecycle (timestamped path, on/off, leave-Play, open-failure→stderr+revert) → Task 2 `diagnosticLogPath`/`beginDiagnosticCapture`/`setDiagnosticsEnabled`/togglePlay exit; §5 testing → Task 1 Catch2 tests + `makeHitLogEntry` unit test (controller/UI verified manually, since they are Qt-only and not in `drumming_tests`).
- **Deviation from spec:** spec §2 named `drum_viz` for `DiagnosticLogger`; moved to `drumming_core` so it is unit-testable and to keep the "log reflects scoring" test Qt-free. This strengthens §5 without changing behavior.
- **Type consistency:** `DiagSessionMeta`, `HitLogEntry`, `LifecycleKind`, `makeHitLogEntry`, `hitClassName`, `setDiagnosticsEnabled`, `diagnosticsEnabled`, `diagLogger_`, `diagCheck_` used identically across tasks.
