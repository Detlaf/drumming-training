#pragma once
#include <SFML/Graphics.hpp>
#include "drumming/types.h"

namespace drumming {

// Existing note/staff/editor draw functions
void drawNote(sf::RenderTarget& w, float x, float y, const Voice& v, sf::Color c);
void drawStaff(sf::RenderTarget& w, const sf::Font& font, const App& app, int total);
void drawGroove(sf::RenderTarget& w, const App& app, int total);
void drawPlayhead(sf::RenderTarget& w, const App& app);
void drawResults(sf::RenderTarget& w, const App& app, int total);
void drawGrid(sf::RenderTarget& w, const sf::Font& font, const App& app, int total);
void drawControls(sf::RenderTarget& w, const sf::Font& font, const App& app);

// Editor/Play composite view (cards + staff + grid + controls)
void drawPracticeView(sf::RenderTarget& w, const sf::Font& font, const App& app);

// App chrome (always visible)
void drawTitleBar(sf::RenderTarget& w, const sf::Font& font);
void drawSidebar(sf::RenderTarget& w, const sf::Font& font, const App& app);
void drawContentHeader(sf::RenderTarget& w, const sf::Font& font, const App& app);

// Full screens
void drawHomeScreen(sf::RenderTarget& w, const sf::Font& font, const App& app);
void drawLibraryScreen(sf::RenderTarget& w, const sf::Font& font, const App& app);
void drawStatsScreen(sf::RenderTarget& w, const sf::Font& font, const App& app);
void drawNamingOverlay(sf::RenderTarget& w, const sf::Font& font, const App& app);

} // namespace drumming
