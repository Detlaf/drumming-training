# drumming-training

A desktop practice app for electronic drum kits. It captures live MIDI from the
kit, lets you build grooves on a music staff / step grid, and scores how
accurately you play them back against a metronome. Grooves and practice sessions
are saved to a local SQLite database, with a library and stats view to track
progress over time.

The UI is a Qt 6 Widgets application styled after a macOS app: a sidebar for
navigation, a title bar, and per-screen content. The practice staff/grid is a
custom-painted widget; everything else is native Qt widgets.

## Features

- **Live MIDI capture** — reads hits from a connected e-drum kit via RtMidi.
- **Groove editor** — place/remove notes on a staff and step sequencer; adjust
  tempo and measure count.
- **Play mode** — a metronome click drives a sweeping playhead; incoming hits
  are scored green (correct, within ±80 ms) or red (wrong pad or mistimed).
- **Groove library** — name, save, load, and delete grooves.
- **Session history & stats** — completed play-throughs are recorded with
  accuracy and duration, viewable on the Stats screen.
- **SQLite persistence** — grooves and session history live in a local database.

## Screens

The sidebar switches between:

- **Home** — landing screen with MIDI status and quick links.
- **Play / Editor** — build a groove (Editor) and play along (Play).
- **Library** — browse and manage saved grooves.
- **Stats** — practice reports.

## Building & running

### Requirements

- A C++17 compiler and CMake ≥ 3.15
- Qt 6 (Widgets, Gui, Multimedia) — install it from your platform's package
  manager or the Qt installer (e.g. `brew install qt` on macOS, or
  `qt6-base-dev qt6-multimedia-dev` on Debian/Ubuntu)
- SQLite3 (system library)
- macOS or Linux (Linux uses ALSA; install ALSA dev packages)

RtMidi (bundled) and Catch2 are fetched/built automatically by CMake.

### Build

```bash
cmake -S . -B build
cmake --build build
```

### Run

On macOS the build produces a double-clickable application bundle,
`build/Drumming.app`. To make the bundle self-contained (copying the Qt
frameworks into it so it runs on machines without Qt installed), run
`macdeployqt` once after building:

```bash
macdeployqt build/Drumming.app
```

Launch it from Finder (or drag it to `/Applications`):

```bash
open build/Drumming.app
# or install it once, then launch from Finder/Spotlight:
cp -R build/Drumming.app /Applications/
```

The executable inside the bundle can also be run directly:

```bash
./build/Drumming.app/Contents/MacOS/Drumming
```

A small MIDI debug tool is also built:

```bash
./build/drums_in
```

> **Data location.** Grooves and session history are stored in
> `~/Library/Application Support/Drumming/drumming.db` so the app works the same
> whether launched from Finder or the command line.

### Tests

```bash
ctest --test-dir build
```

## Controls

| Key              | Action                                          |
|------------------|-------------------------------------------------|
| Click staff/grid | Add / remove a note (Editor)                    |
| Space            | Home/Library/Stats → Editor → Play → Editor     |
| `S`              | Save groove (Editor; opens naming prompt)       |
| `L`              | Toggle Library                                  |
| `H`              | Toggle Stats                                    |
| `Esc`            | Back to Home (from Library/Stats)               |
| `+` / `-`        | BPM up / down (5 BPM steps, clamped 40–240)     |
| `M` / `N`        | Add / remove a measure (1–8)                    |
| `C`              | Clear groove                                    |

In **Play** mode a metronome click fires on each beat (accented on beat 1 of
each measure). The playhead sweeps the groove; incoming MIDI hits are scored
against the expected notes, and a completed loop is recorded as a session.

## Project layout

```
include/drumming/   public headers (types, constants, midi, geometry,
                    scoring, persistence, color)
src/                implementation; main.cpp builds the Qt app, ui/ holds widgets
tests/              Catch2 unit tests (midi, geometry, scoring, types, persistence)
tools/              drums_in MIDI debug utility
third_party/        bundled RtMidi
docs/               design wireframe (index.html)
```

The core logic lives in the `drumming_core` static library (shared between
`drum_viz` and the test suite) and is kept Qt-free; `main.cpp`, `controller.cpp`,
`audio.cpp`, and the `ui/` widgets make up the executable's Qt UI and audio layer.
