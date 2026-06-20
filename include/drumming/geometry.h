#pragma once
#include <utility>
#include "drumming/color.h"
#include "drumming/constants.h"
#include "drumming/types.h"

namespace drumming {

// Staff/grid geometry. The retained-mode GUI layer handles all other screen
// regions (sidebar, library list, home cards, controls) with native widgets,
// so only these note-placement helpers remain framework-free here.
float voiceY(int vi);
float stepX(int step, int total);
std::pair<int, int> pickCell(Vec2 p, int total);
std::pair<int, int> pickGridCell(Vec2 p, int total);

} // namespace drumming
