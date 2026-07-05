#pragma once
#include <QWidget>

class QComboBox;
class QLabel;
class QTableWidget;

namespace drumming {

class PracticeController;
class DailyTimeChart;

// Stats screen: summary cards (total time, sessions, average accuracy), a line
// chart of time played per day over a selectable period, and a table of
// sessions grouped by date & groove. Ports the aggregate math from the old
// drawStatsScreen into native widgets.
class StatsView : public QWidget {
    Q_OBJECT
public:
    explicit StatsView(PracticeController& controller, QWidget* parent = nullptr);

    void refresh();

private:
    void refreshChart();

    PracticeController& controller_;
    QLabel*             totalTime_  = nullptr;
    QLabel*             sessions_   = nullptr;
    QLabel*             avgAcc_     = nullptr;
    QLabel*             correctPad_ = nullptr;
    QLabel*             onTime_     = nullptr;
    QComboBox*          period_     = nullptr;
    DailyTimeChart*     chart_      = nullptr;
    QTableWidget*       table_      = nullptr;
};

} // namespace drumming
