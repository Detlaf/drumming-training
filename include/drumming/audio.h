#pragma once
#include <SFML/Audio.hpp>

namespace drumming {

sf::SoundBuffer makeClick(float freqHz, float durMs = 18.f, int sr = 44100);

} // namespace drumming
