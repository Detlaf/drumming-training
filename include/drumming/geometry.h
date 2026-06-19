#pragma once
#include <SFML/System/Vector2.hpp>
#include <SFML/Graphics/Rect.hpp>
#include <optional>
#include <utility>
#include "drumming/constants.h"
#include "drumming/types.h"

namespace drumming {

float voiceY(int vi);
float stepX(int step, int total);
std::pair<int, int> pickCell(sf::Vector2f p, int total);
std::pair<int, int> pickGridCell(sf::Vector2f p, int total);

// ── Shared layout rects ───────────────────────────────────────────────────────
// Single source of truth for the screen regions that both the drawing code
// (draw.cpp) and the hit-testing code (main.cpp) need, so click targets cannot
// drift away from what is actually drawn.

// Sidebar nav item i (0=Play/Editor, 1=Library, 2=Stats).
sf::FloatRect sidebarNavItemRect(int i);
// Screen for the nav item under point p, or nullopt when p hits no item.
std::optional<Screen> sidebarNavHit(sf::Vector2f p);

// Home screen: nav card i (0=Editor, 1=Library, 2=Stats) and the Resume button.
sf::FloatRect homeCardRect(int i);
sf::FloatRect homeResumeRect();

// Library screen.
sf::FloatRect libraryNewGrooveRect();
float         libraryListTop();
int           libraryMaxVisibleRows();
// Clamp a scroll offset to a valid first-visible index for `count` rows.
int           libraryListStart(int scroll, int count);
// Library index of the row under mouse-y, or -1 if none, given the current
// scroll and row count. Accounts for the same scroll clamping as drawing.
int           libraryRowAt(float my, int scroll, int count);
// Delete button for the row at `rowFromTop` (0 = first visible row).
sf::FloatRect libraryDeleteRect(int rowFromTop);

// Play screen: the Start/Stop session button in the controls strip.
sf::FloatRect sessionButtonRect();

} // namespace drumming
