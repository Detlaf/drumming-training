#include "mainwindow.h"
#include "homeview.h"
#include "libraryview.h"
#include "staffgridwidget.h"
#include "statsview.h"
#include "../controller.h"
#include "drumming/constants.h"

#include <QCloseEvent>
#include <QFont>
#include <QFrame>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QSizePolicy>
#include <QStackedWidget>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidget>

namespace drumming {

// Map a Screen to its QStackedWidget page. EDITOR and PLAY share the practice
// page (index 1); the painted canvas keys off app.screen to switch modes.
static int pageForScreen(Screen s) {
    switch (s) {
    case Screen::HOME:    return 0;
    case Screen::EDITOR:  return 1;
    case Screen::PLAY:    return 1;
    case Screen::LIBRARY: return 2;
    case Screen::STATS:   return 3;
    }
    return 0;
}

MainWindow::MainWindow(PracticeController& controller, QWidget* parent)
    : QMainWindow(parent), controller_(controller) {
    setWindowTitle("Drumming");
    resize(static_cast<int>(WINDOW_W), static_cast<int>(WINDOW_H));

    auto* central = new QWidget(this);
    auto* hroot   = new QHBoxLayout(central);
    hroot->setContentsMargins(0, 0, 0, 0);
    hroot->setSpacing(0);

    // ── Sidebar ──────────────────────────────────────────────────────────────
    auto* sidebar = new QWidget(central);
    sidebar->setFixedWidth(static_cast<int>(SIDEBAR_W));
    sidebar->setStyleSheet("background:#f7f7f8; border-right:1px solid #ededee;");
    auto* sbLay = new QVBoxLayout(sidebar);
    sbLay->setContentsMargins(12, 16, 12, 16);
    sbLay->setSpacing(6);

    auto* brand = new QLabel("Drumming", sidebar);
    QFont bf = brand->font();
    bf.setBold(true);
    bf.setPointSize(bf.pointSize() + 4);
    brand->setFont(bf);
    brand->setStyleSheet("color:#1c1c1e;");
    sbLay->addWidget(brand);

    auto* section = new QLabel("PRACTICE", sidebar);
    section->setStyleSheet("color:#a3a3a8;");
    sbLay->addSpacing(12);
    sbLay->addWidget(section);

    auto makeNav = [&](const QString& label, Screen target) {
        auto* btn = new QToolButton(sidebar);
        btn->setText(label);
        btn->setCheckable(true);
        btn->setToolButtonStyle(Qt::ToolButtonTextOnly);
        btn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        btn->setStyleSheet(
            "QToolButton{border:none;text-align:left;padding:8px 10px;border-radius:6px;color:#1c1c1e;}"
            "QToolButton:hover{background:#efeff0;}"
            "QToolButton:checked{background:#ffffff;border:1px solid #e6e6e8;color:#1c1c1e;}");
        connect(btn, &QToolButton::clicked, &controller_, [this, target]() {
            controller_.setScreen(target);
        });
        sbLay->addWidget(btn);
        return btn;
    };

    navEditor_  = makeNav("Play",    Screen::EDITOR);
    navLibrary_ = makeNav("Library", Screen::LIBRARY);
    navStats_   = makeNav("Stats",   Screen::STATS);

    sbLay->addStretch(1);

    midiStatus_ = new QLabel(sidebar);
    midiStatus_->setStyleSheet(
        "border:1px solid #e6e6e8; border-radius:6px; padding:8px; background:#fff;");
    sbLay->addWidget(midiStatus_);

    hroot->addWidget(sidebar);

    // ── Content area: header + stacked screens ───────────────────────────────
    auto* content = new QWidget(central);
    auto* cLay    = new QVBoxLayout(content);
    cLay->setContentsMargins(0, 0, 0, 0);
    cLay->setSpacing(0);

    auto* header = new QWidget(content);
    header->setStyleSheet("background:#ffffff; border-bottom:1px solid #ededee;");
    auto* hLay = new QVBoxLayout(header);
    hLay->setContentsMargins(26, 8, 26, 8);
    hLay->setSpacing(2);
    headerTitle_ = new QLabel(header);
    QFont htf = headerTitle_->font();
    htf.setPointSize(htf.pointSize() + 4);
    htf.setBold(true);
    headerTitle_->setFont(htf);
    headerTitle_->setStyleSheet("color:#1c1c1e;");
    headerSub_ = new QLabel(header);
    headerSub_->setStyleSheet("color:#a3a3a8;");
    hLay->addWidget(headerTitle_);
    hLay->addWidget(headerSub_);
    cLay->addWidget(header);

    stack_ = new QStackedWidget(content);
    homeView_     = new HomeView(controller_, stack_);
    practiceView_ = new StaffGridWidget(controller_, stack_);
    libraryView_  = new LibraryView(controller_, stack_);
    statsView_    = new StatsView(controller_, stack_);
    stack_->addWidget(homeView_);     // 0
    stack_->addWidget(practiceView_); // 1
    stack_->addWidget(libraryView_);  // 2
    stack_->addWidget(statsView_);    // 3
    cLay->addWidget(stack_, 1);

    hroot->addWidget(content, 1);
    setCentralWidget(central);

    // ── Controller signals → view refreshes ──────────────────────────────────
    connect(&controller_, &PracticeController::screenChanged,
            this, &MainWindow::onScreenChanged);
    connect(&controller_, &PracticeController::grooveChanged,
            this, &MainWindow::onGrooveChanged);
    connect(&controller_, &PracticeController::statsChanged,
            this, &MainWindow::onStatsChanged);
    connect(&controller_, &PracticeController::tick,
            this, &MainWindow::onTick);
    connect(&controller_, &PracticeController::midiConnectionChanged,
            this, &MainWindow::onMidiConnectionChanged);

    onScreenChanged(controller_.app().screen);
    updateMidiStatus();
}

// ── Slots ─────────────────────────────────────────────────────────────────────

void MainWindow::onScreenChanged(Screen screen) {
    stack_->setCurrentIndex(pageForScreen(screen));

    navEditor_->setChecked(screen == Screen::EDITOR || screen == Screen::PLAY);
    navLibrary_->setChecked(screen == Screen::LIBRARY);
    navStats_->setChecked(screen == Screen::STATS);

    if (screen == Screen::LIBRARY) libraryView_->refresh();
    if (screen == Screen::STATS)   statsView_->refresh();
    if (screen == Screen::HOME)    homeView_->refresh();
    practiceView_->refresh();
    updateHeader();
}

void MainWindow::onGrooveChanged() {
    practiceView_->refresh();
    libraryView_->refresh();
    homeView_->refresh();
    updateHeader();
}

void MainWindow::onStatsChanged() {
    statsView_->refresh();
}

void MainWindow::onTick() {
    practiceView_->refresh();
}

void MainWindow::onMidiConnectionChanged(bool) {
    updateMidiStatus();
    homeView_->refresh();
}

// ── Helpers ─────────────────────────────────────────────────────────────────

void MainWindow::updateHeader() {
    const App& app = controller_.app();
    QString title, sub;
    switch (app.screen) {
    case Screen::HOME:    title = "Home"; break;
    case Screen::LIBRARY: title = "Library"; break;
    case Screen::STATS:   title = "Stats"; sub = "Last 30 days"; break;
    case Screen::EDITOR:
        title = app.currentGrooveName.empty() ? "Editor"
                                              : QString::fromStdString(app.currentGrooveName);
        sub = QString("Edit groove · %1 BPM").arg(app.bpm);
        break;
    case Screen::PLAY:
        title = app.currentGrooveName.empty() ? "Play"
                                              : QString::fromStdString(app.currentGrooveName);
        sub = QString("Play along · %1 BPM").arg(app.bpm);
        break;
    }
    headerTitle_->setText(title);
    headerSub_->setText(sub);
    headerSub_->setVisible(!sub.isEmpty());
}

void MainWindow::updateMidiStatus() {
    bool connected = controller_.app().midiConnected;
    midiStatus_->setText(connected ? "● MIDI connected" : "● No MIDI device");
    midiStatus_->setStyleSheet(
        QString("border:1px solid #e6e6e8; border-radius:6px; padding:8px; background:#fff;"
                "color:%1;").arg(connected ? "#32aa6e" : "#c8463c"));
}

// ── Keyboard shortcuts (port of main.cpp key handling) ───────────────────────

void MainWindow::keyPressEvent(QKeyEvent* event) {
    switch (event->key()) {
    case Qt::Key_Space:
        controller_.togglePlay();
        break;
    case Qt::Key_S:
        if (controller_.app().screen == Screen::EDITOR)
            practiceView_->promptSave();
        break;
    case Qt::Key_L:
        controller_.setScreen(controller_.app().screen == Screen::LIBRARY
                                  ? Screen::EDITOR : Screen::LIBRARY);
        break;
    case Qt::Key_H:
        controller_.setScreen(controller_.app().screen == Screen::STATS
                                  ? Screen::EDITOR : Screen::STATS);
        break;
    case Qt::Key_Escape:
        if (controller_.app().screen != Screen::EDITOR &&
            controller_.app().screen != Screen::PLAY)
            controller_.setScreen(Screen::HOME);
        break;
    case Qt::Key_Equal:
    case Qt::Key_Plus:
        controller_.setBpm(controller_.app().bpm + 5);
        break;
    case Qt::Key_Minus:
        controller_.setBpm(controller_.app().bpm - 5);
        break;
    case Qt::Key_M:
        controller_.setMeasures(controller_.app().measures + 1);
        break;
    case Qt::Key_N:
        controller_.setMeasures(controller_.app().measures - 1);
        break;
    case Qt::Key_C:
        controller_.clearGroove();
        break;
    default:
        QMainWindow::keyPressEvent(event);
        return;
    }
    event->accept();
}

void MainWindow::closeEvent(QCloseEvent* event) {
    // Mirror the old window-close path: flush an in-progress session before exit.
    controller_.endSession();
    QMainWindow::closeEvent(event);
}

} // namespace drumming
