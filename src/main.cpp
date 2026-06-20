#include <QApplication>
#include "controller.h"
#include "drumming/persistence.h"
#include "ui/mainwindow.h"

// NOTE: Step 5 (widget tree) needs a working entry point so the drum_viz target
// links and the new MainWindow can be exercised. This is the minimal main()
// from the migration plan's Step 6 sketch; Step 6 may refine it further (e.g.
// aboutToQuit wiring). The old SFML poll-loop main lived here previously.
int main(int argc, char** argv) {
    QApplication app(argc, argv);

    drumming::setDatabasePath(drumming::PracticeController::defaultDatabasePath());

    drumming::PracticeController controller;
    drumming::loadGrooves(controller.app());
    drumming::loadHistory(controller.app());

    drumming::MainWindow window(controller);
    window.show();

    return app.exec();
}
