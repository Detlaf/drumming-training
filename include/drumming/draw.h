#pragma once
#include <SFML/Graphics.hpp>
#include "drumming/types.h"

namespace drumming {

// Existing note/staff/editor draw functions
void drawNote(sf::RenderWindow& w, float x, float y, const Voice& v, sf::Color c);
void drawStaff(sf::RenderWindow& w, const sf::Font& font, const App& app, int total);
void drawGroove(sf::RenderWindow& w, const App& app, int total);
void drawPlayhead(sf::RenderWindow& w, const App& app);
void drawResults(sf::RenderWindow& w, const App& app, int total);
void drawKit(sf::RenderWindow& w, const sf::Font& font, const App& app);
void drawControls(sf::RenderWindow& w, const sf::Font& font, const App& app);

// App chrome (always visible)
void drawTitleBar(sf::RenderWindow& w, const sf::Font& font);
void drawSidebar(sf::RenderWindow& w, const sf::Font& font, const App& app);
void drawContentHeader(sf::RenderWindow& w, const sf::Font& font, const App& app);

// Full screens
void drawHomeScreen(sf::RenderWindow& w, const sf::Font& font, const App& app);
void drawLibraryScreen(sf::RenderWindow& w, const sf::Font& font, const App& app);
void drawStatsScreen(sf::RenderWindow& w, const sf::Font& font, const App& app);
void drawNamingOverlay(sf::RenderWindow& w, const sf::Font& font, const App& app);

} // namespace drumming
