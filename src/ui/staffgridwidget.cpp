#include "staffgridwidget.h"
#include "theme.h"
#include "../controller.h"
#include "drumming/constants.h"
#include "drumming/geometry.h"
#include "drumming/types.h"

#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QPushButton>
#include <QSpinBox>
#include <QString>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>
#include <string>

namespace drumming {

// ── QPainter primitive helpers (port of draw.cpp ~28–80) ─────────────────────
static void fillRect(QPainter& p, float x, float y, float wd, float ht, const QColor& c) {
    p.fillRect(QRectF(x, y, wd, ht), c);
}

static void strokeRect(QPainter& p, float x, float y, float wd, float ht,
                       const QColor& c, float thick = 1.f) {
    QPen pen(c);
    pen.setWidthF(thick);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    // Inset by half the line width so the stroke stays within the cell, matching
    // the old SFML outline-thickness behaviour.
    p.drawRect(QRectF(x + thick / 2.f, y + thick / 2.f, wd - thick, ht - thick));
}

static void hline(QPainter& p, float x1, float x2, float y, const QColor& c) {
    QPen pen(c);
    pen.setWidthF(1.f);
    p.setPen(pen);
    p.drawLine(QPointF(x1, y + 0.5f), QPointF(x2, y + 0.5f));
}

static void vline(QPainter& p, float x, float y1, float y2, const QColor& c) {
    QPen pen(c);
    pen.setWidthF(1.f);
    p.setPen(pen);
    p.drawLine(QPointF(x + 0.5f, y1), QPointF(x + 0.5f, y2));
}

static void drawText(QPainter& p, const QString& s, float x, float y,
                     int sz, const QColor& c) {
    QFont f = p.font();
    f.setPixelSize(sz);
    p.setFont(f);
    p.setPen(c);
    // Old code positioned text by its top-left; offset by the ascent so the
    // glyph baseline lands consistently below (x, y).
    QFontMetricsF fm(f);
    p.drawText(QPointF(x, y + fm.ascent()), s);
}

static void drawTextCenter(QPainter& p, const QString& s, float cx, float y,
                           int sz, const QColor& c) {
    QFont f = p.font();
    f.setPixelSize(sz);
    p.setFont(f);
    p.setPen(c);
    QFontMetricsF fm(f);
    float tw = fm.horizontalAdvance(s);
    p.drawText(QPointF(cx - tw / 2.f, y + fm.ascent()), s);
}

static void fillCircle(QPainter& p, float cx, float cy, float r, const QColor& c) {
    p.setPen(Qt::NoPen);
    p.setBrush(c);
    p.drawEllipse(QPointF(cx, cy), r, r);
}

static void strokeCircle(QPainter& p, float cx, float cy, float r,
                         const QColor& c, float thick) {
    QPen pen(c);
    pen.setWidthF(thick);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(QPointF(cx, cy), r, r);
}

// ── drawNote (port of draw.cpp ~83–141) ──────────────────────────────────────
static void drawNote(QPainter& p, float x, float y, const Voice& v, const QColor& c) {
    const float R = 5.f;
    if (v.xHead) {
        float d = R * 0.85f;
        p.save();
        QPen pen(c);
        pen.setWidthF(2.f);
        p.setPen(pen);
        for (float angle : {45.f, -45.f}) {
            p.save();
            p.translate(x, y);
            p.rotate(angle);
            p.drawLine(QPointF(-d, 0.f), QPointF(d, 0.f));
            p.restore();
        }
        p.restore();
    } else {
        fillCircle(p, x, y, R, c);
    }

    bool  down = v.yOff > 55.f;
    float sx   = down ? x - R : x + R;
    float sy2  = down ? y + 22.f : y - 22.f;
    fillRect(p, sx, std::min(y, sy2), 1.5f, 22.f, c);

    const QColor ledger(163, 163, 168, 180);
    // ledger lines above staff
    if (v.yOff < 0.f) {
        float ly = STAFF_TOP_Y - LINE_SP;
        if (y <= ly + 2.f)
            fillRect(p, x - 7.f, ly, 14.f, 1.f, ledger);
        if (y <= STAFF_TOP_Y - 2.f * LINE_SP + 2.f)
            fillRect(p, x - 7.f, STAFF_TOP_Y - 2.f * LINE_SP, 14.f, 1.f, ledger);
    }
    // ledger lines below staff
    if (v.yOff > 4.f * LINE_SP) {
        float ly = STAFF_TOP_Y + 5.f * LINE_SP;
        fillRect(p, x - 7.f, ly, 14.f, 1.f, ledger);
        if (v.yOff > 5.f * LINE_SP + LINE_SP)
            fillRect(p, x - 7.f, ly + LINE_SP, 14.f, 1.f, ledger);
    }
}

// ── drawStaff (port of draw.cpp ~144–185) ────────────────────────────────────
static void drawStaff(QPainter& p, const App& app, int total) {
    for (int l = 0; l < 5; ++l) {
        float y = STAFF_TOP_Y + l * LINE_SP;
        hline(p, STAFF_LEFT - 22.f, STAFF_RIGHT + 4.f, y, QColor(140, 140, 145, 160));
    }

    for (int vi = 0; vi < NUM_VOICES; ++vi)
        drawText(p, VOICES[vi].name, SIDEBAR_W + 10.f, voiceY(vi) - 7.f, 10, INK3);

    float gridTop = STAFF_TOP_Y - 36.f;
    float gridBot = STAFF_TOP_Y + 4.f * LINE_SP + 90.f;
    float cellW   = (STAFF_RIGHT - STAFF_LEFT) / total;

    for (int s = 0; s <= total; ++s) {
        float x    = STAFF_LEFT + s * cellW;
        bool  bar  = (s % STEPS_PER_MEASURE == 0);
        bool  beat = (s % STEPS_PER_BEAT == 0);
        if (bar)
            fillRect(p, x - 1.f, gridTop, 2.f, gridBot - gridTop, LINE_C);
        else
            vline(p, x, gridTop, gridBot,
                  beat ? QColor(200, 200, 202, 140) : QColor(210, 210, 212, 80));
    }

    for (int s = 0; s < total; s += STEPS_PER_BEAT)
        drawText(p, QString::number(s / STEPS_PER_BEAT + 1),
                 STAFF_LEFT + s * cellW + 2.f, gridTop - 14.f, 10, INK3);

    for (int m = 0; m < app.measures; ++m)
        drawText(p, "m" + QString::number(m + 1),
                 STAFF_LEFT + m * STEPS_PER_MEASURE * cellW + 2.f, gridTop - 26.f, 10, INK2);

    float clefX = STAFF_LEFT - 14.f;
    fillRect(p, clefX - 3.f, STAFF_TOP_Y + LINE_SP, 2.f, 2.f * LINE_SP, INK);
    fillRect(p, clefX + 2.f, STAFF_TOP_Y + LINE_SP, 2.f, 2.f * LINE_SP, INK);
}

// ── drawGroove (port of draw.cpp ~188–194) ───────────────────────────────────
static void drawGroove(QPainter& p, const App& app, int total) {
    float cellW = (STAFF_RIGHT - STAFF_LEFT) / total;
    for (auto& [step, vi] : app.groove) {
        float x = STAFF_LEFT + (step + 0.5f) * cellW;
        drawNote(p, x, voiceY(vi), VOICES[vi], toQColor(VOICES[vi].color));
    }
}

// ── drawPlayhead (port of draw.cpp ~197–203) ─────────────────────────────────
static void drawPlayhead(QPainter& p, const App& app) {
    if (app.currentStep < 0) return;
    float x   = stepX(app.currentStep, app.totalSteps());
    float top = STAFF_TOP_Y - 40.f;
    float bot = STAFF_TOP_Y + 4.f * LINE_SP + 95.f;
    fillRect(p, x - 1.f, top, 2.f, bot - top, ACCENT);
}

// ── drawResults (port of draw.cpp ~206–221) ──────────────────────────────────
static void drawResults(QPainter& p, const App& app, int total) {
    float cellW = (STAFF_RIGHT - STAFF_LEFT) / total;
    for (auto& r : app.results) {
        if (r.voice < 0) continue; // unrecognized pad: counts as a miss, no ring
        float  x = STAFF_LEFT + (r.step + 0.5f) * cellW;
        float  y = voiceY(r.voice);
        QColor c = r.correct ? GOOD_C : BAD_C;
        strokeCircle(p, x, y, 8.f, c, 2.f);
    }
}

// ── drawGrid (port of draw.cpp ~226–270) ─────────────────────────────────────
static void drawGrid(QPainter& p, const App& app, int total) {
    float cellW = (STAFF_RIGHT - STAFF_LEFT) / total;
    float pad   = 2.f;

    int playStep = (app.screen == Screen::PLAY) ? app.currentStep : -1;

    for (int vi = 0; vi < NUM_VOICES; ++vi) {
        float ry = GRID_TOP + vi * GRID_ROW_H;

        drawText(p, VOICES[vi].name, SIDEBAR_W + 14.f, ry + GRID_ROW_H / 2.f - 7.f,
                 10, INK2);

        for (int s = 0; s < total; ++s) {
            float cx        = STAFF_LEFT + s * cellW;
            bool  on        = app.groove.count({s, vi}) > 0;
            bool  beatStart = (s % STEPS_PER_BEAT == 0);
            bool  underPlay = (s == playStep);

            QColor fill = on ? INK : (beatStart ? BG3 : BG);
            fillRect(p, cx + pad, ry + pad, cellW - 2.f * pad, GRID_ROW_H - 2.f * pad, fill);
            strokeRect(p, cx + pad, ry + pad, cellW - 2.f * pad, GRID_ROW_H - 2.f * pad,
                       on ? INK : LINE_C);

            if (underPlay)
                strokeRect(p, cx + pad, ry + pad, cellW - 2.f * pad, GRID_ROW_H - 2.f * pad,
                           ACCENT, 1.5f);

            if (on)
                fillCircle(p, cx + cellW / 2.f, ry + GRID_ROW_H / 2.f, 4.f, BG);
        }
    }

    float numY = GRID_TOP + NUM_VOICES * GRID_ROW_H + 4.f;
    for (int s = 0; s < total; s += STEPS_PER_BEAT)
        drawText(p, QString::number(s / STEPS_PER_BEAT % BEATS_PER_MEASURE + 1),
                 STAFF_LEFT + s * cellW + cellW / 2.f - 3.f, numY, 9, INK3);
}

// ── StaffCanvas ───────────────────────────────────────────────────────────────
StaffCanvas::StaffCanvas(PracticeController& controller, QWidget* parent)
    : QWidget(parent), controller_(controller) {
    // Minimum size keeps the painted layout (in absolute coordinates) fully
    // visible; the surrounding stacked widget supplies the rest of the chrome.
    setMinimumSize(static_cast<int>(STAFF_RIGHT) + 16,
                   static_cast<int>(GRID_TOP + NUM_VOICES * GRID_ROW_H + 28.f));
}

void StaffCanvas::paintEvent(QPaintEvent*) {
    const App& app = controller_.app();
    int total = app.totalSteps();

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);
    p.fillRect(rect(), BG2);

    // Card backgrounds (port of drawPracticeView ~360–373)
    fillRect(p, CARD_X, STAFF_CARD_Y, CARD_W, STAFF_CARD_H, BG);
    strokeRect(p, CARD_X, STAFF_CARD_Y, CARD_W, STAFF_CARD_H, LINE_C);

    float gridCardH = (GRID_TOP + NUM_VOICES * GRID_ROW_H + 18.f) - GRID_CARD_Y;
    fillRect(p, CARD_X, GRID_CARD_Y, CARD_W, gridCardH, BG);
    strokeRect(p, CARD_X, GRID_CARD_Y, CARD_W, gridCardH, LINE_C);

    drawText(p, "STAFF", CARD_X + 12.f, STAFF_CARD_Y + 8.f, 10, INK3);
    drawText(p, "GRID",  CARD_X + 12.f, GRID_CARD_Y + 7.f, 10, INK3);

    drawStaff(p, app, total);
    drawGroove(p, app, total);
    if (app.screen == Screen::PLAY) {
        drawResults(p, app, total);
        drawPlayhead(p, app);
    }
    drawGrid(p, app, total);
}

void StaffCanvas::mousePressEvent(QMouseEvent* event) {
    if (event->button() != Qt::LeftButton) return;
    const App& app = controller_.app();
    if (app.screen != Screen::EDITOR) return;

    float mx = static_cast<float>(event->position().x());
    float my = static_cast<float>(event->position().y());

    // Reuse the retained core geometry (port of main.cpp ~321–333): try the grid
    // sequencer first, then fall back to the staff.
    auto [step, vi] = pickGridCell({mx, my}, app.totalSteps());
    if (step < 0 || vi < 0)
        std::tie(step, vi) = pickCell({mx, my}, app.totalSteps());
    if (step >= 0 && vi >= 0)
        controller_.toggleCell(step, vi);
}

// ── StaffGridWidget ───────────────────────────────────────────────────────────
StaffGridWidget::StaffGridWidget(PracticeController& controller, QWidget* parent)
    : QWidget(parent), controller_(controller) {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    canvas_ = new StaffCanvas(controller_, this);
    root->addWidget(canvas_, 1);

    // ── Controls strip (replaces drawControls) ───────────────────────────────
    auto* strip = new QWidget(this);
    auto* row   = new QHBoxLayout(strip);
    row->setContentsMargins(12, 8, 12, 8);
    row->setSpacing(10);

    playBtn_ = new QPushButton("Play", strip);
    connect(playBtn_, &QPushButton::clicked, &controller_, &PracticeController::togglePlay);
    row->addWidget(playBtn_);

    row->addWidget(new QLabel("BPM", strip));
    bpmSpin_ = new QSpinBox(strip);
    bpmSpin_->setRange(40, 240);
    bpmSpin_->setSingleStep(5);
    connect(bpmSpin_, &QSpinBox::valueChanged, this, [this](int v) {
        if (!syncing_) controller_.setBpm(v);
    });
    row->addWidget(bpmSpin_);

    row->addWidget(new QLabel("Measures", strip));
    measuresSpin_ = new QSpinBox(strip);
    measuresSpin_->setRange(1, 8);
    connect(measuresSpin_, &QSpinBox::valueChanged, this, [this](int v) {
        if (!syncing_) controller_.setMeasures(v);
    });
    row->addWidget(measuresSpin_);

    auto* clearBtn = new QPushButton("Clear", strip);
    connect(clearBtn, &QPushButton::clicked, &controller_, &PracticeController::clearGroove);
    row->addWidget(clearBtn);

    auto* saveBtn = new QPushButton("Save", strip);
    connect(saveBtn, &QPushButton::clicked, this, &StaffGridWidget::promptSave);
    row->addWidget(saveBtn);

    row->addStretch(1);

    accuracyLabel_ = new QLabel(strip);
    accuracyLabel_->setStyleSheet("color: #6b6b70;");
    row->addWidget(accuracyLabel_);

    sessionBtn_ = new QPushButton("Start session", strip);
    connect(sessionBtn_, &QPushButton::clicked, &controller_, &PracticeController::toggleSession);
    row->addWidget(sessionBtn_);

    root->addWidget(strip, 0);

    syncControls();
}

void StaffGridWidget::syncControls() {
    const App& app = controller_.app();
    syncing_ = true;
    bpmSpin_->setValue(app.bpm);
    measuresSpin_->setValue(app.measures);
    syncing_ = false;

    bool playing = (app.screen == Screen::PLAY);
    playBtn_->setText(playing ? "Stop" : "Play");
    sessionBtn_->setVisible(playing);
    sessionBtn_->setText(app.sessionActive ? "Stop session" : "Start session");

    // Live accuracy summary (port of drawControls ~326–354).
    if (playing && !app.results.empty()) {
        int good = 0, early = 0, late = 0, wrong = 0;
        float sumAbs = 0.f;
        for (const auto& r : app.results) {
            switch (r.cls) {
            case HitClass::Correct:         ++good;  break;
            case HitClass::EarlyCorrectPad: ++early; break;
            case HitClass::LateCorrectPad:  ++late;  break;
            case HitClass::WrongPad:        ++wrong; break;
            }
            sumAbs += std::abs(r.offsetMs);
        }
        int n = static_cast<int>(app.results.size());
        accuracyLabel_->setText(
            QString("%1  On time %2%  Early %3%  Late %4%  Wrong %5%  ±%6 ms")
                .arg(app.sessionActive ? "Session live" : "Live accuracy")
                .arg(100 * good / n)
                .arg(100 * early / n)
                .arg(100 * late / n)
                .arg(100 * wrong / n)
                .arg(static_cast<int>(sumAbs / n)));
    } else {
        accuracyLabel_->clear();
    }
}

void StaffGridWidget::refresh() {
    syncControls();
    canvas_->update();
}

void StaffGridWidget::promptSave() {
    const App& app = controller_.app();
    if (app.screen != Screen::EDITOR || app.groove.empty()) return;

    bool ok = false;
    QString name = QInputDialog::getText(
        this, "Save groove", "Save groove as:", QLineEdit::Normal,
        QString::fromStdString(app.currentGrooveName), &ok);
    if (ok && !name.isEmpty())
        controller_.saveGrooveAs(name);
}

} // namespace drumming
