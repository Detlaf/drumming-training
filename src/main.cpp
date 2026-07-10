#include <QApplication>

#include "controller.h"
#include "drumming/persistence.h"
#include "ui/mainwindow.h"

// The 449-line SFML window + `while (window.isOpen())` poll loop is gone: the
// runtime now lives in PracticeController (timers/MIDI/session) and the widget
// tree in MainWindow. main() just wires them together and hands control to the
// Qt event loop.
int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    // Keep the SQLite file under the user's (always writable) Application
    // Support directory — when launched from Finder as a .app bundle the working
    // directory is "/", so a relative path can't be created.
    drumming::setDatabasePath(drumming::PracticeController::defaultDatabasePath());

    drumming::PracticeController controller;

    // Load persisted state into the controller's App before the window is built,
    // so the views paint the saved library on first construction. (Building the
    // window runs an initial refresh; if grooves loaded afterward, the default
    // Home screen would stay stuck showing "No grooves saved yet.")
    drumming::loadGrooves(controller.app());
    drumming::loadHistory(controller.app());

    drumming::MainWindow window(controller);

    // Flush any in-progress practice session when the app quits (ports the old
    // SFML close handler at main.cpp:136). endSession is a no-op when no session
    // is active, so this is safe regardless of the screen at quit time.
    QObject::connect(&app, &QCoreApplication::aboutToQuit, &controller,
                     &drumming::PracticeController::endSession);

    window.show();
    return app.exec();
}
