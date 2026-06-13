# drumming-training

A desktop practice app for electronic drum kits. It captures live MIDI from the
kit, lets you build grooves on a music staff / step grid, and scores how
accurately you play them back against a metronome. Grooves and practice sessions
are saved to a local SQLite database, with a library and stats view to track
progress over time.

The UI is a single SFML window styled after a macOS app: a sidebar for
navigation, a title bar, and per-screen content. The whole scene is rendered to
a 2× supersampled off-screen target and downsampled, so text and edges stay
sharp on standard-DPI displays.

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
- SQLite3 (system library)
- macOS or Linux (Linux uses ALSA; install ALSA + X11 input dev packages)

SFML 3.0, RtMidi (bundled), and Catch2 are fetched/built automatically by CMake.

### Build

```bash
cmake -S . -B build
cmake --build build
```

### Run

```bash
./build/drum_viz
```

A small MIDI debug tool is also built:

```bash
./build/drums_in
```

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
                    scoring, persistence, audio, draw)
src/                implementation; main.cpp drives the app loop
tests/              Catch2 unit tests (midi, geometry, scoring, types, persistence)
tools/              drums_in MIDI debug utility
third_party/        bundled RtMidi
docs/               design wireframe (index.html)
```

The core logic lives in the `drumming_core` static library (shared between
`drum_viz` and the test suite); `main.cpp`, `audio.cpp`, and `draw.cpp` make up
the executable's UI and audio layer.
