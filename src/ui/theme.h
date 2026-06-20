#pragma once
#include <QColor>
#include "drumming/color.h"

namespace drumming {

// ── Theme colors (ported from draw.cpp) ──────────────────────────────────────
// Kept as QColor so the widgets paint with the same palette the SFML renderer
// used. INK/BG/ACCENT/GOOD_C etc. map 1:1 to the former sf::Color constants.
inline const QColor INK    {28,  28,  30 };
inline const QColor INK2   {107, 107, 112};
inline const QColor INK3   {163, 163, 168};
inline const QColor BG     {255, 255, 255};
inline const QColor BG2    {247, 247, 248};
inline const QColor BG3    {240, 240, 241};
inline const QColor LINE_C {230, 230, 232};
inline const QColor HAIR_C {237, 237, 238};
inline const QColor ACCENT {68,  110, 183};
inline const QColor GOOD_C {50,  170, 110};
inline const QColor WARN_C {200, 160, 50 };
inline const QColor BAD_C  {200, 70,  60 };

// Convert a framework-free core Color to a QColor at paint time.
inline QColor toQColor(const Color& c) {
    return QColor(c.r, c.g, c.b, c.a);
}

} // namespace drumming
