#include "statsview.h"
#include "../controller.h"
#include "dailytimechart.h"
#include "drumming/types.h"

#include <QComboBox>
#include <QFont>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QString>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>
#include <ctime>
#include <map>
#include <string>
#include <vector>

namespace drumming {

// Local-midnight epoch for the day containing `ts` (ported from draw.cpp).
static long long dayStartEpoch(long long ts) {
    time_t    t  = static_cast<time_t>(ts);
    struct tm lt = *localtime(&t);
    lt.tm_hour = 0; lt.tm_min = 0; lt.tm_sec = 0; lt.tm_isdst = -1;
    return static_cast<long long>(mktime(&lt));
}

static QString formatDay(long long dayEpoch, const char* fmt) {
    char   buf[64] = {};
    time_t t       = static_cast<time_t>(dayEpoch);
    strftime(buf, sizeof(buf), fmt, localtime(&t));
    return QString::fromUtf8(buf);
}

static QFrame* makeCard(const QString& title, QLabel*& valueOut, QWidget* parent) {
    auto* card = new QFrame(parent);
    card->setFrameShape(QFrame::StyledPanel);
    auto* lay = new QVBoxLayout(card);
    auto* titleLbl = new QLabel(title, card);
    titleLbl->setStyleSheet("color: #6b6b70;");
    valueOut = new QLabel("—", card);
    QFont vf = valueOut->font();
    vf.setPointSize(vf.pointSize() + 10);
    vf.setBold(true);
    valueOut->setFont(vf);
    lay->addWidget(titleLbl);
    lay->addWidget(valueOut);
    return card;
}

StatsView::StatsView(PracticeController& controller, QWidget* parent)
    : QWidget(parent), controller_(controller) {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(26, 14, 26, 14);
    root->setSpacing(14);

    auto* cards = new QHBoxLayout();
    cards->addWidget(makeCard("Total time played", totalTime_, this));
    cards->addWidget(makeCard("Sessions played",   sessions_,  this));
    cards->addWidget(makeCard("Average accuracy",  avgAcc_,    this));
    root->addLayout(cards);

    // ── Time-played-per-day chart with a period selector ─────────────────────
    auto* chartHeader = new QHBoxLayout();
    auto* chartTitle  = new QLabel("Time played per day", this);
    QFont ctf = chartTitle->font();
    ctf.setBold(true);
    chartTitle->setFont(ctf);
    chartHeader->addWidget(chartTitle);
    chartHeader->addStretch(1);
    period_ = new QComboBox(this);
    period_->addItem("Last 7 days",   7);
    period_->addItem("Last 30 days",  30);
    period_->addItem("Last 90 days",  90);
    period_->addItem("Last 365 days", 365);
    period_->setCurrentIndex(1);  // default: last 30 days
    connect(period_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int) { refreshChart(); });
    chartHeader->addWidget(period_);
    root->addLayout(chartHeader);

    chart_ = new DailyTimeChart(this);
    root->addWidget(chart_);

    table_ = new QTableWidget(this);
    table_->setColumnCount(5);
    table_->setHorizontalHeaderLabels(
        {"Date / Groove", "Sessions", "BPM", "Duration", "Accuracy"});
    table_->horizontalHeader()->setStretchLastSection(true);
    table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    table_->verticalHeader()->setVisible(false);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    root->addWidget(table_, 1);

    refresh();
}

void StatsView::refresh() {
    const App& app = controller_.app();

    // ── Summary cards ────────────────────────────────────────────────────────
    int   totalSessions = static_cast<int>(app.history.size());
    float totalTimeSec  = 0.f, totalAcc = 0.f;
    int   accCount      = 0;
    for (const auto& s : app.history) {
        totalTimeSec += static_cast<float>(s.durationSecs);
        if (s.totalNotes > 0) { totalAcc += s.accuracyPct; ++accCount; }
    }
    int totalMin  = static_cast<int>(totalTimeSec / 60.f);
    int avgAccPct = accCount > 0 ? static_cast<int>(totalAcc / accCount) : 0;

    totalTime_->setText(QString("%1h %2m").arg(totalMin / 60).arg(totalMin % 60));
    sessions_->setText(QString::number(totalSessions));
    avgAcc_->setText(QString("%1%").arg(avgAccPct));

    refreshChart();

    // ── Sessions grouped by (day, groove), newest day first ──────────────────
    struct Agg {
        int count = 0, dur = 0;
        float accSum = 0.f;
        int accN = 0, bpm = 0;
        bool bpmUniform = true;
    };
    std::map<long long, std::map<std::string, Agg>> byDay;
    for (const auto& s : app.history) {
        Agg& a = byDay[dayStartEpoch(s.timestampEpoch)][s.grooveName];
        a.count += 1;
        a.dur   += s.durationSecs;
        if (s.totalNotes > 0) { a.accSum += s.accuracyPct; ++a.accN; }
        if (a.count == 1) a.bpm = s.bpm;
        else if (a.bpm != s.bpm) a.bpmUniform = false;
    }

    table_->setRowCount(0);
    for (auto dit = byDay.rbegin(); dit != byDay.rend(); ++dit) {
        // Date header band as a spanning row.
        int hr = table_->rowCount();
        table_->insertRow(hr);
        auto* hdr = new QTableWidgetItem(formatDay(dit->first, "%A, %b %d"));
        QFont hf = hdr->font();
        hf.setBold(true);
        hdr->setFont(hf);
        table_->setItem(hr, 0, hdr);
        table_->setSpan(hr, 0, 1, 5);

        for (const auto& [groove, a] : dit->second) {
            int r = table_->rowCount();
            table_->insertRow(r);
            table_->setItem(r, 0, new QTableWidgetItem(
                "    " + QString::fromStdString(groove)));
            table_->setItem(r, 1, new QTableWidgetItem(QString::number(a.count)));
            table_->setItem(r, 2, new QTableWidgetItem(
                a.bpmUniform ? QString::number(a.bpm) : "—"));
            table_->setItem(r, 3, new QTableWidgetItem(
                QString::asprintf("%d:%02d", a.dur / 60, a.dur % 60)));
            int avg = a.accN > 0 ? static_cast<int>(a.accSum / a.accN) : 0;
            table_->setItem(r, 4, new QTableWidgetItem(
                a.accN > 0 ? QString("%1%").arg(avg) : "—"));
        }
    }
}

// Build one point per calendar day in the selected window — including days with
// no sessions, so the x-axis is evenly spaced and idle days dip to zero.
void StatsView::refreshChart() {
    const App& app  = controller_.app();
    int        days = period_->currentData().toInt();
    if (days <= 0) days = 30;

    // Sum durations (minutes) into each day's local-midnight bucket.
    std::map<long long, double> minutesByDay;
    for (const auto& s : app.history)
        minutesByDay[dayStartEpoch(s.timestampEpoch)] += s.durationSecs / 60.0;

    // Walk back from today's midnight one day at a time so every day appears.
    long long today = dayStartEpoch(static_cast<long long>(time(nullptr)));
    std::vector<DailyTimeChart::Point> points;
    points.reserve(days);
    for (int i = days - 1; i >= 0; --i) {
        time_t    t  = static_cast<time_t>(today);
        struct tm lt = *localtime(&t);
        lt.tm_mday -= i;
        lt.tm_isdst = -1;
        long long day = static_cast<long long>(mktime(&lt));
        auto      it  = minutesByDay.find(day);
        points.push_back({day, it != minutesByDay.end() ? it->second : 0.0});
    }
    chart_->setData(std::move(points));
}

} // namespace drumming
