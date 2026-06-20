#include "homeview.h"
#include "../controller.h"
#include "drumming/types.h"

#include <QFont>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QString>
#include <QVBoxLayout>

namespace drumming {

// One nav card: title, description, and a button that switches screen.
static QFrame* makeCard(const QString& title, const QString& desc, Screen target,
                        PracticeController& controller, QWidget* parent) {
    auto* card = new QFrame(parent);
    card->setFrameShape(QFrame::StyledPanel);
    auto* lay = new QVBoxLayout(card);

    auto* titleLbl = new QLabel(title, card);
    QFont tf = titleLbl->font();
    tf.setBold(true);
    tf.setPointSize(tf.pointSize() + 2);
    titleLbl->setFont(tf);
    lay->addWidget(titleLbl);

    auto* descLbl = new QLabel(desc, card);
    descLbl->setStyleSheet("color: #a3a3a8;");
    descLbl->setWordWrap(true);
    lay->addWidget(descLbl);

    lay->addStretch(1);

    auto* btn = new QPushButton("Open", card);
    QObject::connect(btn, &QPushButton::clicked, &controller, [&controller, target]() {
        controller.setScreen(target);
    });
    lay->addWidget(btn);
    return card;
}

HomeView::HomeView(PracticeController& controller, QWidget* parent)
    : QWidget(parent), controller_(controller) {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(26, 22, 26, 22);
    root->setSpacing(16);

    auto* greeting = new QLabel("Good evening.", this);
    QFont gf = greeting->font();
    gf.setPointSize(gf.pointSize() + 14);
    gf.setBold(true);
    greeting->setFont(gf);
    root->addWidget(greeting);

    subtitle_ = new QLabel(this);
    subtitle_->setStyleSheet("color: #6b6b70;");
    root->addWidget(subtitle_);

    auto* cards = new QHBoxLayout();
    cards->addWidget(makeCard("Play", "Pick a groove and play along.",
                              Screen::EDITOR, controller_, this));
    cards->addWidget(makeCard("Library", "Browse, edit & save grooves.",
                              Screen::LIBRARY, controller_, this));
    cards->addWidget(makeCard("Stats", "See your practice reports.",
                              Screen::STATS, controller_, this));
    root->addLayout(cards);

    auto* cont = new QFrame(this);
    cont->setFrameShape(QFrame::StyledPanel);
    auto* contLay = new QVBoxLayout(cont);
    auto* contTitle = new QLabel("CONTINUE WHERE YOU LEFT OFF", cont);
    contTitle->setStyleSheet("color: #a3a3a8;");
    contLay->addWidget(contTitle);

    auto* contRow = new QHBoxLayout();
    continueLabel_ = new QLabel(cont);
    contRow->addWidget(continueLabel_, 1);
    auto* resumeBtn = new QPushButton("Resume", cont);
    connect(resumeBtn, &QPushButton::clicked, &controller_, &PracticeController::resumeLast);
    contRow->addWidget(resumeBtn);
    contLay->addLayout(contRow);
    root->addWidget(cont);

    root->addStretch(1);

    refresh();
}

void HomeView::refresh() {
    const App& app = controller_.app();
    subtitle_->setText(app.midiConnected
                           ? "Ready for a session? Your kit is connected."
                           : "Connect a MIDI kit to start a session.");

    if (app.library.empty()) {
        continueLabel_->setText("No grooves saved yet.");
    } else {
        const auto& g = app.library.back();
        continueLabel_->setText(QString("%1  ·  %2 BPM · %3 bar%4")
                                    .arg(QString::fromStdString(g.name))
                                    .arg(g.bpm)
                                    .arg(g.measures)
                                    .arg(g.measures > 1 ? "s" : ""));
    }
}

} // namespace drumming
