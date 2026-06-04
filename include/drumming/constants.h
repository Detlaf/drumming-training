#pragma once

namespace drumming {

// Window
inline constexpr unsigned WINDOW_W = 1200;
inline constexpr unsigned WINDOW_H = 780;

// Chrome
inline constexpr float SIDEBAR_W      = 210.f;
inline constexpr float TITLEBAR_H     = 40.f;
inline constexpr float CONTENT_HEAD_H = 52.f;

// Staff layout (within content area, below chrome)
inline constexpr float STAFF_LEFT  = SIDEBAR_W + 90.f;   // 300
inline constexpr float STAFF_RIGHT = 1185.f;
inline constexpr float STAFF_TOP_Y = TITLEBAR_H + CONTENT_HEAD_H + 38.f;  // 130
inline constexpr float LINE_SP     = 12.f;

// Kit panel (below staff in editor/play screens)
inline constexpr float KIT_Y  = 390.f;
inline constexpr float KIT_H  = 290.f;
inline constexpr float CTRL_Y = KIT_Y + KIT_H + 8.f;

// Timing
inline constexpr int   STEPS_PER_BEAT    = 4;
inline constexpr int   BEATS_PER_MEASURE = 4;
inline constexpr int   STEPS_PER_MEASURE = STEPS_PER_BEAT * BEATS_PER_MEASURE;
inline constexpr float HIT_WINDOW_MS     = 80.f;

} // namespace drumming
