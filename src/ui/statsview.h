#pragma once
#include <QWidget>

class QLabel;
class QTableWidget;

namespace drumming {

class PracticeController;

// Stats screen: summary cards (total time, sessions, average accuracy) over a
// table of sessions grouped by date & groove. Ports the aggregate math from the
// old drawStatsScreen into native widgets.
class StatsView : public QWidget {
    Q_OBJECT
public:
    explicit StatsView(PracticeController& controller, QWidget* parent = nullptr);

    void refresh();

private:
    PracticeController& controller_;
    QLabel*             totalTime_  = nullptr;
    QLabel*             sessions_   = nullptr;
    QLabel*             avgAcc_     = nullptr;
    QTableWidget*       table_      = nullptr;
};

} // namespace drumming
