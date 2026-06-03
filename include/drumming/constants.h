#pragma once

namespace drumming {

// Window
inline constexpr unsigned WINDOW_W = 1200;
inline constexpr unsigned WINDOW_H = 780;

// Staff layout
inline constexpr float STAFF_LEFT  = 110.f;
inline constexpr float STAFF_RIGHT = 1185.f;
inline constexpr float STAFF_TOP_Y = 90.f;
inline constexpr float LINE_SP     = 12.f;

// Kit panel
inline constexpr float KIT_Y  = 390.f;
inline constexpr float KIT_H  = 290.f;
inline constexpr float CTRL_Y = KIT_Y + KIT_H + 8.f;

// Timing
inline constexpr int   STEPS_PER_BEAT    = 4;
inline constexpr int   BEATS_PER_MEASURE = 4;
inline constexpr int   STEPS_PER_MEASURE = STEPS_PER_BEAT * BEATS_PER_MEASURE;
inline constexpr float HIT_WINDOW_MS     = 80.f;

} // namespace drumming
