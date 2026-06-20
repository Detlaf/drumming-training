#pragma once
#include <cstdint>

namespace drumming {

// Framework-free RGBA color. The GUI layer converts this to its toolkit's
// color type (e.g. QColor) at paint time; drumming_core stays toolkit-agnostic.
struct Color {
    std::uint8_t r, g, b, a = 255;
};

// Framework-free 2D point, used by the staff/grid geometry math.
struct Vec2 {
    float x, y;
};

} // namespace drumming
