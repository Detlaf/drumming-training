#include "libraryview.h"
#include "../controller.h"
#include "drumming/types.h"

#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPushButton>
#include <QStyle>
#include <QVBoxLayout>
#include <QString>

namespace drumming {

LibraryView::LibraryView(PracticeController& controller, QWidget* parent)
    : QWidget(parent), controller_(controller) {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(26, 14, 26, 14);
    root->setSpacing(12);

    auto* topRow = new QHBoxLayout();
    auto* heading = new QLabel("Saved grooves", this);
    topRow->addWidget(heading);
    topRow->addStretch(1);
    auto* newBtn = new QPushButton("+ New groove", this);
    connect(newBtn, &QPushButton::clicked, &controller_, &PracticeController::newGroove);
    topRow->addWidget(newBtn);
    root->addLayout(topRow);

    list_ = new QListWidget(this);
    // Double-clicking (or pressing the row's Open button) loads the groove.
    connect(list_, &QListWidget::itemActivated, this, [this](QListWidgetItem* item) {
        controller_.loadGroove(item->data(Qt::UserRole).toInt());
    });
    root->addWidget(list_, 1);

    refresh();
}

void LibraryView::refresh() {
    list_->clear();
    const App& app = controller_.app();

    if (app.library.empty()) {
        auto* item = new QListWidgetItem(
            "No grooves yet. Click Save in the editor to store one.", list_);
        item->setFlags(Qt::NoItemFlags);
        return;
    }

    for (int i = 0; i < static_cast<int>(app.library.size()); ++i) {
        const auto& g = app.library[i];

        auto* item = new QListWidgetItem(list_);
        item->setData(Qt::UserRole, i);

        // Per-row widget: name + summary on the left, Open / delete on the right.
        auto* rowWidget = new QWidget(list_);
        auto* row = new QHBoxLayout(rowWidget);
        row->setContentsMargins(8, 4, 8, 4);

        QString summary = QString("%1  ·  %2 BPM · %3 bar%4")
                              .arg(QString::fromStdString(g.name))
                              .arg(g.bpm)
                              .arg(g.measures)
                              .arg(g.measures > 1 ? "s" : "");
        auto* label = new QLabel(summary, rowWidget);
        if (g.name == app.currentGrooveName)
            label->setStyleSheet("font-weight: bold;");
        row->addWidget(label, 1);

        auto* editBtn = new QPushButton("✎", rowWidget);
        editBtn->setToolTip("Rename groove");
        editBtn->setFixedWidth(32);
        connect(editBtn, &QPushButton::clicked, this, [this, i]() {
            promptRename(i);
        });
        row->addWidget(editBtn);

        auto* openBtn = new QPushButton("Open", rowWidget);
        connect(openBtn, &QPushButton::clicked, this, [this, i]() {
            controller_.loadGroove(i);
        });
        row->addWidget(openBtn);

        auto* delBtn = new QPushButton("Delete", rowWidget);
        connect(delBtn, &QPushButton::clicked, this, [this, i]() {
            controller_.deleteGroove(i);
        });
        row->addWidget(delBtn);

        item->setSizeHint(rowWidget->sizeHint());
        list_->setItemWidget(item, rowWidget);
    }
}

void LibraryView::promptRename(int index) {
    const App& app = controller_.app();
    if (index < 0 || index >= static_cast<int>(app.library.size())) return;

    const QString current = QString::fromStdString(app.library[index].name);
    bool ok = false;
    const QString name = QInputDialog::getText(
        this, "Rename groove", "New name:", QLineEdit::Normal, current, &ok);
    if (ok && !name.trimmed().isEmpty())
        controller_.renameGroove(index, name);
}

} // namespace drumming
