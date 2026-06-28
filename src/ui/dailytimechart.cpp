#include "dailytimechart.h"
#include "theme.h"

#include <QFontMetrics>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QPolygonF>
#include <algorithm>
#include <cmath>
#include <ctime>

namespace drumming {

DailyTimeChart::DailyTimeChart(QWidget* parent) : QWidget(parent) {
    setMinimumHeight(220);
}

void DailyTimeChart::setData(std::vector<Point> points) {
    points_ = std::move(points);
    update();
}

static QString shortDay(long long dayEpoch) {
    char   buf[16] = {};
    time_t t       = static_cast<time_t>(dayEpoch);
    strftime(buf, sizeof(buf), "%b %d", localtime(&t));
    return QString::fromUtf8(buf);
}

void DailyTimeChart::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.fillRect(rect(), BG);

    if (points_.empty()) {
        p.setPen(INK2);
        p.drawText(rect(), Qt::AlignCenter, "No sessions in this period");
        return;
    }

    QFontMetrics fm(p.font());
    const int    padLeft   = 44;
    const int    padRight  = 14;
    const int    padTop    = 24;  // room for the "min" caption above the top tick
    const int    padBottom = 26;
    QRectF       plot(padLeft, padTop, width() - padLeft - padRight,
                      height() - padTop - padBottom);
    if (plot.width() <= 0 || plot.height() <= 0) return;

    // Y scale: round the max up to a friendly tick so the axis reads cleanly.
    double maxMin = 0.0;
    for (const auto& pt : points_) maxMin = std::max(maxMin, pt.minutes);
    double niceMax = maxMin <= 0.0 ? 10.0 : std::ceil(maxMin / 10.0) * 10.0;

    auto xAt = [&](size_t i) {
        if (points_.size() == 1) return plot.center().x();
        return plot.left() +
               plot.width() * static_cast<double>(i) / (points_.size() - 1);
    };
    auto yAt = [&](double minutes) {
        return plot.bottom() - plot.height() * (minutes / niceMax);
    };

    // ── Y-axis labels (0, mid, max) ──────────────────────────────────────────
    for (int i = 0; i <= 2; ++i) {
        double v = niceMax * i / 2.0;
        double y = yAt(v);
        p.setPen(INK2);
        QString lbl = QString::number(static_cast<int>(std::round(v)));
        p.drawText(QRectF(0, y - fm.height() / 2.0, padLeft - 6, fm.height()),
                   Qt::AlignRight | Qt::AlignVCenter, lbl);
    }
    // Unit caption sits above the plot, clear of the top tick value.
    p.setPen(INK2);
    p.drawText(QRectF(0, 2, padLeft + 30, fm.height()),
               Qt::AlignLeft | Qt::AlignTop, "  min");

    // ── Filled area under the line ───────────────────────────────────────────
    QPolygonF area;
    area << QPointF(xAt(0), plot.bottom());
    for (size_t i = 0; i < points_.size(); ++i)
        area << QPointF(xAt(i), yAt(points_[i].minutes));
    area << QPointF(xAt(points_.size() - 1), plot.bottom());
    QColor fill = ACCENT;
    fill.setAlpha(38);
    p.setPen(Qt::NoPen);
    p.setBrush(fill);
    p.drawPolygon(area);

    // ── The line ─────────────────────────────────────────────────────────────
    QPolygonF line;
    for (size_t i = 0; i < points_.size(); ++i)
        line << QPointF(xAt(i), yAt(points_[i].minutes));
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(ACCENT, 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.drawPolyline(line);

    // ── Point markers (only when the series is short enough to be legible) ────
    if (points_.size() <= 31) {
        p.setBrush(ACCENT);
        p.setPen(Qt::NoPen);
        for (size_t i = 0; i < points_.size(); ++i)
            p.drawEllipse(QPointF(xAt(i), yAt(points_[i].minutes)), 2.5, 2.5);
    }

    // ── X-axis labels: first, last, and a few evenly spaced in between ───────
    p.setPen(INK2);
    int approxLabels = std::max(2, std::min<int>(
        6, static_cast<int>(plot.width() / std::max(50, fm.horizontalAdvance("Jun 00") + 12))));
    int step = std::max<int>(1, static_cast<int>(points_.size()) / approxLabels);
    auto drawXLabel = [&](size_t i) {
        QString lbl = shortDay(points_[i].dayEpoch);
        double  x   = xAt(i);
        QRectF  r(x - 40, plot.bottom() + 4, 80, padBottom - 4);
        p.drawText(r, Qt::AlignHCenter | Qt::AlignTop, lbl);
    };
    for (size_t i = 0; i < points_.size(); i += step) drawXLabel(i);
    if ((points_.size() - 1) % step != 0) drawXLabel(points_.size() - 1);
}

} // namespace drumming
