#pragma once
#include <string>
#include "drumming/types.h"

namespace drumming {

// Override the SQLite file the persistence functions use. Closes any open
// connection; the next load/save reopens against `path`. Mainly for tests.
void setDatabasePath(const std::string& path);

void loadGrooves(App& app);
void saveGrooves(const App& app);
void loadHistory(App& app);
void appendHistory(App& app, const SessionRecord& rec);

} // namespace drumming
