#include "drumming/audio.h"
#include <cmath>
#include <cstdint>
#include <vector>

namespace drumming {

sf::SoundBuffer makeClick(float freqHz, float durMs, int sr) {
    int n = (int)(sr * durMs / 1000.f);
    std::vector<std::int16_t> s(n);
    for (int i = 0; i < n; ++i) {
        float t   = (float)i / sr;
        float env = std::exp(-t * 180.f);
        s[i] = (std::int16_t)(28000 * env * std::sin(2.f * 3.14159265f * freqHz * t));
    }
    sf::SoundBuffer buf;
    buf.loadFromSamples(s.data(), (std::uint64_t)n, 1, (unsigned)sr,
                        {sf::SoundChannel::Mono});
    return buf;
}

} // namespace drumming
