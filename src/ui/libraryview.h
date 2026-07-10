#pragma once
#include <QWidget>

class QListWidget;

namespace drumming {

class PracticeController;

// Library screen: a QListWidget of saved grooves (each row carries a tempo/bars
// summary and a delete button) plus a "New groove" button. Replaces
// drawLibraryScreen and the deleted library row math; load/delete/new logic is
// delegated to PracticeController (port of main.cpp ~266–296).
class LibraryView : public QWidget {
    Q_OBJECT
public:
    explicit LibraryView(PracticeController& controller, QWidget* parent = nullptr);

    void refresh();

private:
    // Pop up an inline dialog to rename the groove at `index`.
    void promptRename(int index);

    PracticeController& controller_;
    QListWidget*        list_ = nullptr;
};

} // namespace drumming
