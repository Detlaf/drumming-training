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
