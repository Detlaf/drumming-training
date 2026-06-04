#pragma once
#include "drumming/types.h"

namespace drumming {

void loadGrooves(App& app);
void saveGrooves(const App& app);
void loadHistory(App& app);
void appendHistory(App& app, const SessionRecord& rec);

} // namespace drumming
