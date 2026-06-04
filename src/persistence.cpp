#include "drumming/persistence.h"
#include <algorithm>
#include <ctime>
#include <fstream>
#include <sstream>

namespace drumming {

static std::vector<std::string> splitTokens(const std::string& s) {
    std::istringstream iss(s);
    std::vector<std::string> out;
    std::string tok;
    while (iss >> tok) out.push_back(tok);
    return out;
}

void loadGrooves(App& app) {
    std::ifstream f("grooves.txt");
    if (!f) return;

    std::string line;
    SavedGroove cur;
    bool inGroove = false;

    auto flush = [&] {
        if (inGroove && !cur.name.empty()) { app.library.push_back(cur); cur = {}; }
        inGroove = false;
    };

    while (std::getline(f, line)) {
        if (line.empty() || line[0] == '#') { flush(); continue; }

        if (line.size() > 5 && line.substr(0, 5) == "NAME ") {
            flush();
            cur.name = line.substr(5);
            cur.bpm = 100; cur.measures = 2;
            inGroove = true;
        } else if (line.size() > 4 && line.substr(0, 4) == "BPM ") {
            try { cur.bpm = std::stoi(line.substr(4)); } catch (...) {}
        } else if (line.size() > 5 && line.substr(0, 5) == "BARS ") {
            try { cur.measures = std::stoi(line.substr(5)); } catch (...) {}
        } else if (line.size() >= 6 && line.substr(0, 6) == "NOTES ") {
            for (auto& t : splitTokens(line.substr(6))) {
                auto comma = t.find(',');
                if (comma != std::string::npos) {
                    try {
                        cur.groove.insert({std::stoi(t.substr(0, comma)),
                                           std::stoi(t.substr(comma + 1))});
                    } catch (...) {}
                }
            }
        } else if (line == "NOTES") {
            // empty groove — no-op
        }
    }
    flush();
}

void saveGrooves(const App& app) {
    std::ofstream f("grooves.txt");
    f << "# Drum Groove Library v1\n";
    for (auto& g : app.library) {
        f << "NAME " << g.name << "\n";
        f << "BPM "  << g.bpm  << "\n";
        f << "BARS " << g.measures << "\n";
        f << "NOTES";
        for (auto& [s, v] : g.groove) f << " " << s << "," << v;
        f << "\n\n";
    }
}

void loadHistory(App& app) {
    std::ifstream f("history.txt");
    if (!f) return;
    std::string line;
    while (std::getline(f, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::istringstream iss(line);
        SessionRecord r{};
        int accInt = 0;
        iss >> r.timestampEpoch >> r.grooveName >> r.bpm >> r.measures
            >> r.correctHits >> r.totalNotes >> accInt >> r.durationSecs;
        r.accuracyPct = (float)accInt;
        if (!r.grooveName.empty()) app.history.push_back(r);
    }
}

void appendHistory(App& app, const SessionRecord& rec) {
    app.history.push_back(rec);
    // Append to file; write header if file is new/empty
    {
        std::ifstream check("history.txt");
        bool empty = !check || check.peek() == std::ifstream::traits_type::eof();
        if (empty) {
            std::ofstream hdr("history.txt");
            hdr << "# Drum Session History v1\n";
        }
    }
    std::ofstream f("history.txt", std::ios::app);
    f << rec.timestampEpoch << "\t" << rec.grooveName << "\t"
      << rec.bpm << "\t" << rec.measures << "\t"
      << rec.correctHits << "\t" << rec.totalNotes << "\t"
      << (int)rec.accuracyPct << "\t" << rec.durationSecs << "\n";
}

} // namespace drumming
