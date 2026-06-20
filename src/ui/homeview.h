#pragma once
#include <QWidget>

class QLabel;

namespace drumming {

class PracticeController;

// Home screen: a greeting, three nav cards (Play / Library / Stats) and a
// "Continue where you left off" panel with a Resume button. Ports the layout
// and click logic from drawHomeScreen / main.cpp ~298–319 into native widgets.
class HomeView : public QWidget {
    Q_OBJECT
public:
    explicit HomeView(PracticeController& controller, QWidget* parent = nullptr);

    void refresh();

private:
    PracticeController& controller_;
    QLabel*             subtitle_  = nullptr;
    QLabel*             continueLabel_ = nullptr;
};

} // namespace drumming
