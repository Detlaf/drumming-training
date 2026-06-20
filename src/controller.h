#pragma once
#include <QObject>
#include <QString>
#include <chrono>
#include <memory>
#include <string>
#include "audio.h"
#include "drumming/types.h"

class QTimer;
class RtMidiIn;

namespace drumming {

// Owns the live application state and the timers/MIDI plumbing that the old
// SFML `while (window.isOpen())` loop drove. The UI (MainWindow + views) calls
// the public slots to mutate state and connects to the signals to repaint.
// Lives in the drum_viz target only — keeps Qt out of drumming_core.
class PracticeController : public QObject {
    Q_OBJECT
public:
    explicit PracticeController(QObject* parent = nullptr);
    ~PracticeController() override;

    // When launched from Finder as a .app bundle the working directory is "/",
    // so keep the SQLite file under the user's (always writable) Application
    // Support directory instead. Moved here verbatim from main.cpp.
    static std::string defaultDatabasePath();

    // Mutable app state. Views read it to paint; main() runs loadGrooves /
    // loadHistory against it at startup.
    App&       app()       { return app_; }
    const App& app() const { return app_; }

public slots:
    // Screen navigation (sidebar buttons, Home cards, Esc-to-Home).
    void setScreen(Screen screen);

    // Space: HOME/LIBRARY/STATS → EDITOR, EDITOR → start PLAY, PLAY → stop.
    void togglePlay();

    // Editor edits.
    void setBpm(int bpm);            // clamped to [40, 240]
    void setMeasures(int measures);  // clamped to [1, 8]; prunes out-of-range notes
    void clearGroove();
    void toggleCell(int step, int voice);

    // Library actions.
    void newGroove();
    void loadGroove(int index);
    void deleteGroove(int index);
    void resumeLast();

    // Save the current groove under `name` (find-by-name or append), then persist.
    void saveGrooveAs(const QString& name);

    // Explicit practice session (Play screen Start/Stop button).
    void startSession();
    void endSession();
    void toggleSession();

signals:
    void screenChanged(Screen screen);
    void grooveChanged();
    void statsChanged();
    void tick();
    void midiConnectionChanged(bool connected);

private slots:
    void pollMidiTick();   // ~4 ms: score incoming hits
    void hotPlugTick();    // 1 s: open/close the MIDI port as kits come and go
    void playTick();       // ~8 ms: advance the playhead and click the metronome

private:
    void enterPlay();

    App                       app_;
    Metronome                 metronome_;
    std::unique_ptr<RtMidiIn> midiIn_;

    QTimer* playTimer_    = nullptr;
    QTimer* midiTimer_    = nullptr;
    QTimer* hotPlugTimer_ = nullptr;
};

} // namespace drumming
