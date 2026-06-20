#pragma once
#include <QMainWindow>
#include "drumming/types.h"

class QStackedWidget;
class QLabel;
class QToolButton;

namespace drumming {

class PracticeController;
class StaffGridWidget;
class LibraryView;
class StatsView;
class HomeView;

// Top-level window: a left sidebar of QToolButtons (Editor / Library / Stats)
// plus a QStackedWidget of screens with a per-screen header label. Replaces the
// SFML title bar / sidebar / content-header chrome and screen dispatch.
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(PracticeController& controller, QWidget* parent = nullptr);

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void closeEvent(QCloseEvent* event) override;

private slots:
    void onScreenChanged(Screen screen);
    void onGrooveChanged();
    void onStatsChanged();
    void onTick();
    void onMidiConnectionChanged(bool connected);

private:
    void updateHeader();
    void updateMidiStatus();

    PracticeController& controller_;

    QStackedWidget* stack_       = nullptr;
    QLabel*         headerTitle_ = nullptr;
    QLabel*         headerSub_   = nullptr;
    QLabel*         midiStatus_  = nullptr;

    QToolButton* navEditor_  = nullptr;
    QToolButton* navLibrary_ = nullptr;
    QToolButton* navStats_   = nullptr;

    HomeView*        homeView_    = nullptr;
    StaffGridWidget* practiceView_ = nullptr;
    LibraryView*     libraryView_ = nullptr;
    StatsView*       statsView_   = nullptr;
};

} // namespace drumming
