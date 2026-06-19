#include "drumming/persistence.h"
#include <sqlite3.h>
#include <cstdio>
#include <ctime>
#include <sstream>
#include <string>

namespace drumming {

// ── Database connection ──────────────────────────────────────────────────────
// A single lazily-opened connection lives for the lifetime of the process.
// The schema is created on first open. Two tables:
//
//   grooves   one row per saved groove; the step/voice pairs are serialised
//             into the `notes` column as space-separated "step,voice" tokens
//             so a groove stays a single row.
//
//   sessions  one row per practiced session.

static std::string gDbPath = "drumming.db";
static sqlite3*    gHandle  = nullptr;

static const char* kSchema =
    "CREATE TABLE IF NOT EXISTS grooves ("
    "  id         INTEGER PRIMARY KEY,"
    "  name       TEXT    NOT NULL UNIQUE,"
    "  bpm        INTEGER NOT NULL DEFAULT 100,"
    "  measures   INTEGER NOT NULL DEFAULT 2,"
    "  notes      TEXT    NOT NULL DEFAULT '',"
    "  created_at INTEGER NOT NULL DEFAULT (strftime('%s','now'))"
    ");"
    "CREATE TABLE IF NOT EXISTS sessions ("
    "  id            INTEGER PRIMARY KEY,"
    "  groove_name   TEXT    NOT NULL,"
    "  bpm           INTEGER NOT NULL,"
    "  measures      INTEGER NOT NULL,"
    "  correct_hits  INTEGER NOT NULL,"
    "  total_notes   INTEGER NOT NULL,"
    "  accuracy_pct  REAL    NOT NULL,"
    "  played_at     INTEGER NOT NULL,"
    "  duration_secs INTEGER NOT NULL,"
    "  early_hits    INTEGER NOT NULL DEFAULT 0,"
    "  late_hits     INTEGER NOT NULL DEFAULT 0,"
    "  wrong_pad_hits INTEGER NOT NULL DEFAULT 0,"
    "  started_at    INTEGER NOT NULL DEFAULT 0,"
    "  ended_at      INTEGER NOT NULL DEFAULT 0"
    ");"
    "CREATE INDEX IF NOT EXISTS idx_sessions_groove   ON sessions(groove_name);"
    "CREATE INDEX IF NOT EXISTS idx_sessions_playedat ON sessions(played_at);";

static sqlite3* db() {
    if (gHandle) return gHandle;

    if (sqlite3_open(gDbPath.c_str(), &gHandle) != SQLITE_OK) {
        std::fprintf(stderr, "sqlite: cannot open %s: %s\n",
                     gDbPath.c_str(), sqlite3_errmsg(gHandle));
        sqlite3_close(gHandle);
        gHandle = nullptr;
        return nullptr;
    }

    char* err = nullptr;
    if (sqlite3_exec(gHandle, kSchema, nullptr, nullptr, &err) != SQLITE_OK) {
        std::fprintf(stderr, "sqlite: schema error: %s\n", err ? err : "?");
        sqlite3_free(err);
    }

    // Migrate databases created before these columns existed. ADD COLUMN fails
    // harmlessly with "duplicate column name" once they're present, so the
    // errors are swallowed deliberately.
    static const char* kMigrations[] = {
        "ALTER TABLE sessions ADD COLUMN early_hits     INTEGER NOT NULL DEFAULT 0;",
        "ALTER TABLE sessions ADD COLUMN late_hits      INTEGER NOT NULL DEFAULT 0;",
        "ALTER TABLE sessions ADD COLUMN wrong_pad_hits INTEGER NOT NULL DEFAULT 0;",
        "ALTER TABLE sessions ADD COLUMN started_at     INTEGER NOT NULL DEFAULT 0;",
        "ALTER TABLE sessions ADD COLUMN ended_at       INTEGER NOT NULL DEFAULT 0;",
    };
    for (const char* m : kMigrations)
        sqlite3_exec(gHandle, m, nullptr, nullptr, nullptr);

    return gHandle;
}

// Point persistence at a different database file, closing any open connection
// so the next call reopens against the new path. Used by tests to isolate state
// in a temporary database; in normal use the default "drumming.db" is kept.
void setDatabasePath(const std::string& path) {
    if (gHandle) {
        sqlite3_close(gHandle);
        gHandle = nullptr;
    }
    gDbPath = path;
}

// ── Notes (de)serialisation ──────────────────────────────────────────────────
static std::string serialiseNotes(const std::set<std::pair<int, int>>& g) {
    std::ostringstream os;
    bool first = true;
    for (auto& [s, v] : g) {
        if (!first) os << ' ';
        os << s << ',' << v;
        first = false;
    }
    return os.str();
}

static std::set<std::pair<int, int>> deserialiseNotes(const std::string& s) {
    std::set<std::pair<int, int>> g;
    std::istringstream is(s);
    std::string tok;
    while (is >> tok) {
        auto comma = tok.find(',');
        if (comma == std::string::npos) continue;
        try {
            g.insert({std::stoi(tok.substr(0, comma)),
                      std::stoi(tok.substr(comma + 1))});
        } catch (...) {}
    }
    return g;
}

// ── Grooves ──────────────────────────────────────────────────────────────────
void loadGrooves(App& app) {
    sqlite3* d = db();
    if (!d) return;

    sqlite3_stmt* st = nullptr;
    const char* sql =
        "SELECT name, bpm, measures, notes FROM grooves ORDER BY id;";
    if (sqlite3_prepare_v2(d, sql, -1, &st, nullptr) != SQLITE_OK) return;

    while (sqlite3_step(st) == SQLITE_ROW) {
        SavedGroove g;
        g.name     = reinterpret_cast<const char*>(sqlite3_column_text(st, 0));
        g.bpm      = sqlite3_column_int(st, 1);
        g.measures = sqlite3_column_int(st, 2);
        const unsigned char* notes = sqlite3_column_text(st, 3);
        g.groove = deserialiseNotes(notes ? reinterpret_cast<const char*>(notes) : "");
        app.library.push_back(std::move(g));
    }
    sqlite3_finalize(st);
}

// Full sync: the in-memory library is the source of truth, so we replace the
// table contents inside a transaction. This mirrors the previous "rewrite the
// whole file" behaviour and naturally handles adds, edits and deletes.
void saveGrooves(const App& app) {
    sqlite3* d = db();
    if (!d) return;

    sqlite3_exec(d, "BEGIN;", nullptr, nullptr, nullptr);
    sqlite3_exec(d, "DELETE FROM grooves;", nullptr, nullptr, nullptr);

    sqlite3_stmt* st = nullptr;
    const char* sql =
        "INSERT INTO grooves (name, bpm, measures, notes) VALUES (?,?,?,?);";
    if (sqlite3_prepare_v2(d, sql, -1, &st, nullptr) == SQLITE_OK) {
        for (auto& g : app.library) {
            std::string notes = serialiseNotes(g.groove);
            sqlite3_bind_text(st, 1, g.name.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_int (st, 2, g.bpm);
            sqlite3_bind_int (st, 3, g.measures);
            sqlite3_bind_text(st, 4, notes.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_step(st);
            sqlite3_reset(st);
        }
        sqlite3_finalize(st);
    }

    sqlite3_exec(d, "COMMIT;", nullptr, nullptr, nullptr);
}

// ── Sessions ─────────────────────────────────────────────────────────────────
void loadHistory(App& app) {
    sqlite3* d = db();
    if (!d) return;

    sqlite3_stmt* st = nullptr;
    const char* sql =
        "SELECT groove_name, bpm, measures, correct_hits, total_notes,"
        "       accuracy_pct, played_at, duration_secs,"
        "       early_hits, late_hits, wrong_pad_hits, started_at, ended_at"
        "  FROM sessions ORDER BY played_at;";
    if (sqlite3_prepare_v2(d, sql, -1, &st, nullptr) != SQLITE_OK) return;

    while (sqlite3_step(st) == SQLITE_ROW) {
        SessionRecord r{};
        r.grooveName     = reinterpret_cast<const char*>(sqlite3_column_text(st, 0));
        r.bpm            = sqlite3_column_int(st, 1);
        r.measures       = sqlite3_column_int(st, 2);
        r.correctHits    = sqlite3_column_int(st, 3);
        r.totalNotes     = sqlite3_column_int(st, 4);
        r.accuracyPct    = (float)sqlite3_column_double(st, 5);
        r.timestampEpoch = sqlite3_column_int64(st, 6);
        r.durationSecs   = sqlite3_column_int(st, 7);
        r.earlyHits      = sqlite3_column_int(st, 8);
        r.lateHits       = sqlite3_column_int(st, 9);
        r.wrongPadHits   = sqlite3_column_int(st, 10);
        r.startedAtEpoch = sqlite3_column_int64(st, 11);
        r.endedAtEpoch   = sqlite3_column_int64(st, 12);
        app.history.push_back(std::move(r));
    }
    sqlite3_finalize(st);
}

void appendHistory(App& app, const SessionRecord& rec) {
    app.history.push_back(rec);

    sqlite3* d = db();
    if (!d) return;

    sqlite3_stmt* st = nullptr;
    const char* sql =
        "INSERT INTO sessions (groove_name, bpm, measures, correct_hits,"
        "  total_notes, accuracy_pct, played_at, duration_secs,"
        "  early_hits, late_hits, wrong_pad_hits, started_at, ended_at)"
        " VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?);";
    if (sqlite3_prepare_v2(d, sql, -1, &st, nullptr) != SQLITE_OK) return;

    sqlite3_bind_text  (st,  1, rec.grooveName.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int   (st,  2, rec.bpm);
    sqlite3_bind_int   (st,  3, rec.measures);
    sqlite3_bind_int   (st,  4, rec.correctHits);
    sqlite3_bind_int   (st,  5, rec.totalNotes);
    sqlite3_bind_double(st,  6, rec.accuracyPct);
    sqlite3_bind_int64 (st,  7, rec.timestampEpoch);
    sqlite3_bind_int   (st,  8, rec.durationSecs);
    sqlite3_bind_int   (st,  9, rec.earlyHits);
    sqlite3_bind_int   (st, 10, rec.lateHits);
    sqlite3_bind_int   (st, 11, rec.wrongPadHits);
    sqlite3_bind_int64 (st, 12, rec.startedAtEpoch);
    sqlite3_bind_int64 (st, 13, rec.endedAtEpoch);
    sqlite3_step(st);
    sqlite3_finalize(st);
}

} // namespace drumming
