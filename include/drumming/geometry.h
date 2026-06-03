#pragma once
#include <SFML/System/Vector2.hpp>
#include <utility>
#include "drumming/constants.h"

namespace drumming {

float voiceY(int vi);
float stepX(int step, int total);
std::pair<int, int> pickCell(sf::Vector2f p, int total);

} // namespace drumming
