#pragma once
#include <QTransform>
#include <QWidget>

class QSpinBox;
class QPushButton;
class QLabel;

namespace drumming {

class PracticeController;

// Custom-painted canvas for the Editor/Play staff + grid sequencer. Ports
// drawStaff/drawGrid/drawGroove/drawNote/drawPlayhead/drawResults from the old
// SFML draw.cpp into a single paintEvent using QPainter (antialiased, no
// RENDER_SCALE supersampling). Paints in the original absolute layout
// coordinates so the retained pickGridCell/pickCell math still applies.
class StaffCanvas : public QWidget {
    Q_OBJECT
public:
    explicit StaffCanvas(PracticeController& controller, QWidget* parent = nullptr);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

private:
    // Maps the fixed virtual layout rectangle (the absolute coordinates the
    // draw helpers and pick* geometry use) onto the widget's actual size,
    // scaled uniformly and centred so the staff + grid fill the window.
    QTransform contentTransform() const;

    PracticeController& controller_;
};

// Editor/Play screen: the StaffCanvas above a controls strip (BPM / measures
// spin boxes, Clear, Save, and Play/Stop + Start/Stop session buttons),
// replacing the old in-canvas drawControls.
class StaffGridWidget : public QWidget {
    Q_OBJECT
public:
    explicit StaffGridWidget(PracticeController& controller, QWidget* parent = nullptr);

    // Repaint the canvas and re-sync the controls to current app state.
    void refresh();

public slots:
    // Open the modal naming dialog and persist via controller.saveGrooveAs.
    // Public so the MainWindow 'S' shortcut can trigger it too.
    void promptSave();

private:
    void syncControls();

    PracticeController& controller_;
    StaffCanvas*        canvas_      = nullptr;
    QSpinBox*           bpmSpin_     = nullptr;
    QSpinBox*           measuresSpin_ = nullptr;
    QPushButton*        playBtn_     = nullptr;
    QPushButton*        sessionBtn_  = nullptr;
    QLabel*             accuracyLabel_ = nullptr;
    bool                syncing_     = false;
};

} // namespace drumming
