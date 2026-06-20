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

// Cross-talk gate. Electronic-kit triggers pick up vibration from neighbouring
// pads (and the hi-hat pedal), firing faint phantom note-ons — most visibly a
// ghost hi-hat hit whenever a tom or the snare is struck. Reject note-ons below
// this velocity so those ghosts don't get scored as real hits. Genuine strikes,
// even soft ghost notes a drummer plays on purpose, sit comfortably above it.
inline constexpr int   MIN_HIT_VELOCITY  = 12;

} // namespace drumming
