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

TEST_CASE("start escapes quotes and backslashes in groove name", "[diag]") {
    std::string path = tempLogPath("escape");
    DiagSessionMeta meta{1720000000LL, 100, 1, 16, 93.75f, "Funk \"1\"\\path", {}};
    DiagnosticLogger log;
    REQUIRE(log.start(path, meta));
    log.stop();

    auto lines = readLines(path);
    REQUIRE(lines.size() == 1);
    // The raw line must contain the escaped sequences, not bare quotes/backslashes.
    CHECK(lines[0].find("\"name\":\"Funk \\\"1\\\"\\\\path\"") != std::string::npos);
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
