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

// Escapes a string for safe embedding inside a JSON string literal.
std::string jsonEscape(const std::string& s) {
    static const char* hexDigits = "0123456789abcdef";
    std::string out;
    out.reserve(s.size());
    for (unsigned char c : s) {
        switch (c) {
        case '"':  out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\n': out += "\\n";  break;
        case '\r': out += "\\r";  break;
        case '\t': out += "\\t";  break;
        default:
            if (c < 0x20) {
                out += "\\u00";
                out += hexDigits[(c >> 4) & 0xF];
                out += hexDigits[c & 0xF];
            } else {
                out += static_cast<char>(c);
            }
        }
    }
    return out;
}
} // namespace

bool DiagnosticLogger::start(const std::string& path, const DiagSessionMeta& meta) {
    if (out_.is_open()) out_.close();
    out_.open(path, std::ios::out | std::ios::trunc);
    if (!out_.is_open()) return false;

    const std::string name = jsonEscape(meta.grooveName.empty() ? "<unsaved>" : meta.grooveName);
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
