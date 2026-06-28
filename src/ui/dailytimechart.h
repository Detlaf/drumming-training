#pragma once
#include <QWidget>
#include <vector>

namespace drumming {

// A lightweight QPainter line chart of minutes practiced per day. Holds one
// point per day in the selected window (including zero-activity days) so the
// x-axis is evenly spaced and gaps read as dips to the baseline.
class DailyTimeChart : public QWidget {
    Q_OBJECT
public:
    struct Point {
        long long dayEpoch;  // local-midnight epoch for the day
        double    minutes;   // minutes played that day
    };

    explicit DailyTimeChart(QWidget* parent = nullptr);

    // Replace the plotted series; days are expected in ascending date order.
    void setData(std::vector<Point> points);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    std::vector<Point> points_;
};

} // namespace drumming
