#pragma once

namespace drumming {

// Window
inline constexpr unsigned WINDOW_W = 1200;
inline constexpr unsigned WINDOW_H = 780;

// Supersampling factor: the scene is rendered to an off-screen target this many
// times larger than the window, then downsampled. Sharpens edges and text on
// standard-DPI displays and adds anti-aliasing everywhere.
inline constexpr unsigned RENDER_SCALE = 2;

// Chrome
inline constexpr float SIDEBAR_W      = 210.f;
inline constexpr float TITLEBAR_H     = 40.f;
inline constexpr float CONTENT_HEAD_H = 52.f;

// Staff layout (within content area, below chrome)
inline constexpr float STAFF_LEFT  = SIDEBAR_W + 90.f;   // 300
inline constexpr float STAFF_RIGHT = 1185.f;
inline constexpr float STAFF_TOP_Y = TITLEBAR_H + CONTENT_HEAD_H + 70.f;  // 162
inline constexpr float LINE_SP     = 12.f;

// Practice cards (staff + grid sequencer) in editor/play screens
inline constexpr float CARD_X      = SIDEBAR_W + 12.f;          // 222
inline constexpr float CARD_W      = (float)WINDOW_W - CARD_X - 12.f;
inline constexpr float STAFF_CARD_Y = 96.f;
inline constexpr float STAFF_CARD_H = 216.f;
inline constexpr float GRID_CARD_Y  = STAFF_CARD_Y + STAFF_CARD_H + 14.f;  // 326
inline constexpr float GRID_TOP     = GRID_CARD_Y + 26.f;       // first cell row (350)
inline constexpr float GRID_ROW_H   = 28.f;
inline constexpr float CTRL_Y       = 632.f;

// Timing
inline constexpr int   STEPS_PER_BEAT    = 4;
inline constexpr int   BEATS_PER_MEASURE = 4;
inline constexpr int   STEPS_PER_MEASURE = STEPS_PER_BEAT * BEATS_PER_MEASURE;
inline constexpr float HIT_WINDOW_MS     = 80.f;

} // namespace drumming
