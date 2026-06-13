#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <filesystem>
#include <string>
#include <unistd.h>
#include "drumming/persistence.h"
#include "drumming/types.h"

using namespace drumming;
using Catch::Matchers::WithinAbs;

namespace fs = std::filesystem;

// Point persistence at a fresh, empty database file unique to this test and
// return its path. Each call removes any pre-existing file so the schema is
// created clean. The connection is reset via setDatabasePath.
static std::string freshDb() {
    static int counter = 0;
    fs::path p = fs::temp_directory_path() /
                 ("drumtest_" + std::to_string(::getpid()) + "_" +
                  std::to_string(counter++) + ".db");
    std::error_code ec;
    fs::remove(p, ec);
    setDatabasePath(p.string());
    return p.string();
}

// ── Grooves ─────────────────────────────────────────────────────────────────

TEST_CASE("saveGrooves/loadGrooves - a groove round-trips", "[persistence]") {
    freshDb();

    App out;
    SavedGroove g;
    g.name     = "Backbeat";
    g.bpm      = 120;
    g.measures = 2;
    g.groove   = {{0, 5}, {4, 5}, {8, 5}, {2, 7}}; // snare + bass cells
    out.library.push_back(g);
    saveGrooves(out);

    App in;
    loadGrooves(in);

    REQUIRE(in.library.size() == 1);
    CHECK(in.library[0].name     == "Backbeat");
    CHECK(in.library[0].bpm      == 120);
    CHECK(in.library[0].measures == 2);
    CHECK(in.library[0].groove   == g.groove);
}

TEST_CASE("saveGrooves/loadGrooves - empty groove round-trips with no notes", "[persistence]") {
    freshDb();

    App out;
    SavedGroove g;
    g.name     = "Empty";
    g.bpm      = 90;
    g.measures = 1;
    // no notes
    out.library.push_back(g);
    saveGrooves(out);

    App in;
    loadGrooves(in);

    REQUIRE(in.library.size() == 1);
    CHECK(in.library[0].name == "Empty");
    CHECK(in.library[0].groove.empty());
}

TEST_CASE("saveGrooves/loadGrooves - multiple grooves preserve insertion order", "[persistence]") {
    freshDb();

    App out;
    for (const char* n : {"First", "Second", "Third"}) {
        SavedGroove g;
        g.name = n;
        g.bpm = 100;
        g.measures = 2;
        out.library.push_back(g);
    }
    saveGrooves(out);

    App in;
    loadGrooves(in);

    REQUIRE(in.library.size() == 3);
    CHECK(in.library[0].name == "First");
    CHECK(in.library[1].name == "Second");
    CHECK(in.library[2].name == "Third");
}

TEST_CASE("saveGrooves - is a full replace, not an append", "[persistence]") {
    freshDb();

    App first;
    { SavedGroove g; g.name = "Old"; g.bpm = 100; g.measures = 2; first.library.push_back(g); }
    saveGrooves(first);

    App second;
    { SavedGroove g; g.name = "New"; g.bpm = 110; g.measures = 4; second.library.push_back(g); }
    saveGrooves(second);

    App in;
    loadGrooves(in);

    REQUIRE(in.library.size() == 1);
    CHECK(in.library[0].name == "New");
}

TEST_CASE("loadGrooves - empty database yields an empty library", "[persistence]") {
    freshDb();

    App in;
    loadGrooves(in);
    CHECK(in.library.empty());
}

// ── Sessions ────────────────────────────────────────────────────────────────

static SessionRecord makeRecord(const std::string& name, long long when) {
    SessionRecord r{};
    r.grooveName     = name;
    r.bpm            = 128;
    r.measures       = 2;
    r.correctHits    = 12;
    r.totalNotes     = 16;
    r.accuracyPct    = 75.f;
    r.timestampEpoch = when;
    r.durationSecs   = 42;
    return r;
}

TEST_CASE("appendHistory/loadHistory - a session record round-trips", "[persistence]") {
    freshDb();

    App out;
    appendHistory(out, makeRecord("Backbeat", 1700000000LL));

    App in;
    loadHistory(in);

    REQUIRE(in.history.size() == 1);
    const SessionRecord& r = in.history[0];
    CHECK(r.grooveName     == "Backbeat");
    CHECK(r.bpm            == 128);
    CHECK(r.measures       == 2);
    CHECK(r.correctHits    == 12);
    CHECK(r.totalNotes     == 16);
    CHECK_THAT(r.accuracyPct, WithinAbs(75.f, 1e-3f));
    CHECK(r.timestampEpoch == 1700000000LL);
    CHECK(r.durationSecs   == 42);
}

TEST_CASE("appendHistory - appends to the in-memory history immediately", "[persistence]") {
    freshDb();

    App app;
    appendHistory(app, makeRecord("Live", 1700000000LL));
    REQUIRE(app.history.size() == 1);
    CHECK(app.history[0].grooveName == "Live");
}

TEST_CASE("loadHistory - records come back ordered by played_at ascending", "[persistence]") {
    freshDb();

    App out;
    appendHistory(out, makeRecord("C", 3000));
    appendHistory(out, makeRecord("A", 1000));
    appendHistory(out, makeRecord("B", 2000));

    App in;
    loadHistory(in);

    REQUIRE(in.history.size() == 3);
    CHECK(in.history[0].grooveName == "A");
    CHECK(in.history[1].grooveName == "B");
    CHECK(in.history[2].grooveName == "C");
}

TEST_CASE("loadHistory - empty database yields an empty history", "[persistence]") {
    freshDb();

    App in;
    loadHistory(in);
    CHECK(in.history.empty());
}

TEST_CASE("setDatabasePath - switching databases isolates data", "[persistence]") {
    std::string dbA = freshDb();
    App a;
    { SavedGroove g; g.name = "OnlyInA"; g.bpm = 100; g.measures = 2; a.library.push_back(g); }
    saveGrooves(a);

    // Switch to a different, empty database.
    freshDb();
    App b;
    loadGrooves(b);
    CHECK(b.library.empty());

    // Switch back to the first database; its groove is still there.
    setDatabasePath(dbA);
    App again;
    loadGrooves(again);
    REQUIRE(again.library.size() == 1);
    CHECK(again.library[0].name == "OnlyInA");
}
