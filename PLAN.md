# Plan: Groove Library, Session History & Statistics

## Context

The drumming practice app (`drum_viz.cpp`) currently has zero persistence — grooves vanish on exit, practice sessions are unrecorded, and there are no stats. This plan adds four related features: a named groove library (save/load/delete), per-session accuracy history, and a stats display — all without adding any new dependencies.

---

## New Data Structures

Add after `struct HitResult` (line 67), before `struct App`:

```cpp
struct SavedGroove {
    std::string name;
    int bpm, measures;
    std::set<std::pair<int,int>> groove;
};

struct SessionRecord {
    std::string grooveName;    // "<unsaved>" if not saved
    int bpm, measures;
    int correctHits, totalNotes;
    float accuracyPct;
    long long timestampEpoch;
    int durationSecs;
};
```

Add to `struct App` (after line 85):

```cpp
std::vector<SavedGroove>   library;
std::vector<SessionRecord> history;
std::string currentGrooveName;   // "" = unsaved
bool sessionActive   = false;
std::chrono::steady_clock::time_point sessionStart;
bool showLibrary     = false;
bool showStats       = false;
int  libraryScroll   = 0;
bool namingMode      = false;
std::string nameBuffer;
```

---

## New Constants (top of file, line 15 area)

```cpp
const unsigned WINDOW_W = 1560;   // was 1200; right 360px is library panel
const unsigned MAIN_W   = 1200;   // left panel width (all existing coords stay unchanged)
```

All existing drawing coordinates use values up to `STAFF_RIGHT = 1185`, so no existing draw functions need pixel adjustments — except one:

- **`drawKit` line 308**: change `(float)WINDOW_W` → `(float)MAIN_W` so the kit background doesn't bleed into the library panel.

---

## New Includes

Add at top of file:
```cpp
#include <fstream>
#include <sstream>
#include <ctime>
```

---

## File Formats

### `./grooves.txt`
```
# Drum Groove Library v1
NAME Rock Beat
BPM 120
BARS 2
NOTES 0,0 4,2 8,0 12,2

NAME Funk Lite
BPM 95
BARS 1
NOTES 0,2 3,5

```
Each groove block ends with a blank line. `NOTES` is space-separated `step,voiceIdx` pairs. Empty groove = `NOTES` with no pairs.

### `./history.txt`
```
# Drum Session History v1
1717200000	Rock Beat	120	2	28	30	93	145
```
Columns (tab-delimited): `epoch_ts  groove_name  bpm  bars  correct  total  accuracy_int  duration_secs`

---

## New Helper Functions

```cpp
// Split string by tab
std::vector<std::string> splitTab(const std::string& s);

// File I/O
void loadGrooves();                         // reads ./grooves.txt into g.library
void saveGrooves();                         // rewrites ./grooves.txt (called after every mutation)
void loadHistory();                         // reads ./history.txt into g.history
void appendHistory(const SessionRecord&);   // appends one line to ./history.txt + pushes to g.history

// Per-groove stats
struct GrooveStats { int sessions; float best, avg, last; };
GrooveStats computeGrooveStats(const std::string& name);
```

---

## Session Lifecycle

**Session start** — in the Space key `EDIT→PLAY` branch (line 399):
```cpp
g.sessionStart  = Clock::now();
g.sessionActive = false;
```

**First loop completes** — in the loop-advance block (line 474):
```cpp
if (loop > g.loopCount) {
    g.loopCount     = loop;
    g.results.clear();
    g.sessionActive = true;  // ADD
}
```

**Session end** — extract into `void endSession()`, called from:
1. Space key `PLAY→EDIT` branch (before `g.mode = Mode::EDIT`)
2. `sf::Event::Closed` handler (before `window.close()`)

`endSession()` is a no-op if `!g.sessionActive`. On end: compute accuracy from `g.results`, populate `SessionRecord`, call `appendHistory()`.

---

## UI Layout

**Library panel** (x=1210..1550, always visible when `g.showLibrary`):
- Background rect 340×740, dark color
- Title: "GROOVE LIBRARY"
- `[Save]` button row (activates naming mode; disabled when `g.groove.empty()`)
- Divider
- Scrollable list: one row per `g.library` entry
  - Groove name (left), BPM + bars (right, subdued)
  - `[X]` delete button (18×18) at row right edge
  - Click on row = load groove (blocked in PLAY mode)
  - Highlight when `g.currentGrooveName == entry.name`
- Mouse-wheel scroll, clamped to list bounds
- "↑↓ scroll" hint if list overflows

**Stats overlay** (toggle with `H`, drawn over full window):
- Semi-transparent dark rect 1080×640, centered
- Left column: per-groove table (Name / Sessions / Best% / Avg% / Last%) — one row per saved groove
- Right column: overall stats (total sessions, total practice time, overall accuracy, most practiced groove, best single session)
- `H` or `Escape` dismisses

**Naming overlay** (when `g.namingMode`):
- Centered box 500×80 at y≈330
- "Save groove as: [nameBuffer_]"
- "Enter = confirm   Esc = cancel"

---

## Event Handling Additions

**In `KeyPressed` — add at top of switch, before all existing cases:**
```cpp
if (g.namingMode) {
    if (k->code == sf::Keyboard::Key::Enter && !g.nameBuffer.empty()) {
        // overwrite if name exists, else push_back; set currentGrooveName; saveGrooves()
    } else if (k->code == sf::Keyboard::Key::Escape) {
        g.nameBuffer.clear(); g.namingMode = false;
    }
    break;  // consume all other keys
}
```

**New key cases** (after the naming guard):
```cpp
case sf::Keyboard::Key::S:
    if (g.mode == Mode::EDIT && !g.groove.empty()) {
        g.namingMode = true; g.nameBuffer = g.currentGrooveName;
    } break;
case sf::Keyboard::Key::L:
    g.showLibrary = !g.showLibrary; break;
case sf::Keyboard::Key::H:
    g.showStats = !g.showStats; break;
```

**`sf::Event::TextEntered`** — new block checked only when `g.namingMode`:
- Backspace (unicode 8) removes last char
- Printable ASCII appended to `g.nameBuffer` (max 30 chars)

**`sf::Event::MouseWheelScrolled`** — scroll library list when mouse is within panel x-range.

**`MouseButtonPressed`** — add library panel hit-testing block (load / delete / save-button) before the existing EDIT-mode note-placement block.

---

## `drawControls` Changes (lines 342–367)

- Add key hints: `[S] Save  [L] Library  [H] Stats`
- When `g.currentGrooveName` non-empty: show `"Groove: <name>"` in subdued cyan
- In PLAY mode with a groove name set: show name alongside the accuracy readout

---

## Render Loop Changes (lines 484–494)

```cpp
window.clear(sf::Color(28, 28, 32));
drawStaff(window, font, g.totalSteps());
drawGroove(window, g.totalSteps());
if (g.mode == Mode::PLAY) {
    drawResults(window, g.totalSteps());
    drawPlayhead(window);
}
drawKit(window, font);
drawControls(window, font);
if (g.showLibrary) drawLibraryPanel(window, font);
if (g.showStats)   drawStatsOverlay(window, font);
if (g.namingMode)  drawNamingOverlay(window, font);
window.display();
```

---

## Initialization (in `main()`, after font load)

```cpp
loadGrooves();
loadHistory();
```

---

## Critical File

- `/Users/kate/coding/drumming-training/drum_viz.cpp` — all changes here, single file

---

## Verification

1. **Build**: `g++ -std=c++17 drum_viz.cpp RtMidi.cpp -lsfml-* -framework CoreMIDI ...` — zero new warnings
2. **First run** (no files): library panel empty, no crash, `grooves.txt` / `history.txt` not yet created
3. **Save groove**: add notes, press `S`, type "Test Beat", Enter → `grooves.txt` created, panel shows entry
4. **Load groove**: click "Test Beat" → staff updates, `g.currentGrooveName` shown in controls
5. **Delete**: click `[X]` → entry removed from panel and file
6. **Session happy path**: play through 1+ loops, press Space → `history.txt` appended with correct row
7. **Session abort**: stop before completing a loop → nothing appended to `history.txt`
8. **Stats overlay**: press `H` → overlay shows session data; press `H` again → dismissed
9. **Quit during session**: close window after 1 loop → session saved to `history.txt`
10. **Naming collision**: save "Rock Beat" twice → one entry in library, not two
11. **Empty groove guard**: press `S` with no notes → naming mode does not activate
12. **Library during PLAY**: `L` opens panel; clicking a groove does NOT load it (guarded by mode check)
