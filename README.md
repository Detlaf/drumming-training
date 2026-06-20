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
- Qt 6 (Widgets, Gui, Multimedia). On Debian/Ubuntu install
  `qt6-base-dev qt6-multimedia-dev`. On macOS use an **official** Qt build
  (not Homebrew): Homebrew splits Qt across many kegs, which breaks
  `macdeployqt`. The easiest way to get an official build is `aqtinstall`:

  ```bash
  python3 -m pip install --user aqtinstall
  python3 -m aqt install-qt mac desktop 6.11.1 --modules qtmultimedia \
      --outputdir "$HOME/Qt"
  ```

  This installs to `~/Qt/6.11.1/macos`. (Pick a version that matches your
  macOS SDK — older Qt, e.g. 6.8 LTS, links the removed `AGL` framework and
  fails to build on recent SDKs.)
- SQLite3 (system library)
- macOS or Linux (Linux uses ALSA; install ALSA dev packages)

RtMidi (bundled) and Catch2 are fetched/built automatically by CMake.

### Build

```bash
cmake -S . -B build
cmake --build build
```

On macOS, point CMake at the official Qt installed above:

```bash
cmake -S . -B build -DCMAKE_PREFIX_PATH="$HOME/Qt/6.11.1/macos"
cmake --build build
```

### Run

On macOS the build produces a double-clickable application bundle,
`build/Drumming.app`. To make the bundle self-contained (copying the Qt
frameworks into it so it runs on machines without Qt installed), run
`macdeployqt` once after building. Use the `macdeployqt` from the same
official Qt you built against (not whatever is on `PATH`):

```bash
~/Qt/6.11.1/macos/bin/macdeployqt build/Drumming.app
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

### Distribution (macOS: Developer ID signing + notarization)

The `macdeployqt` bundle runs fine locally, but to share it with other people
without Gatekeeper warnings ("Apple cannot check it for malicious software")
it must be signed with a **Developer ID Application** certificate and
**notarized** by Apple. One-time setup:

1. **Join the Apple Developer Program** (paid, ~$99/yr) at
   <https://developer.apple.com/programs/>.
2. **Create a Developer ID Application certificate** and install it in your
   login keychain — easiest via Xcode › Settings › Accounts › (your team) ›
   Manage Certificates › `+` › *Developer ID Application*. Verify with:

   ```bash
   security find-identity -v -p codesigning   # should list "Developer ID Application: …"
   ```
3. **Store notarization credentials** once (uses an app-specific password from
   <https://account.apple.com> › Sign-In and Security; or an App Store Connect
   API key):

   ```bash
   xcrun notarytool store-credentials drumming-notary \
       --apple-id "you@example.com" --team-id "ABCDE12345" \
       --password "abcd-efgh-ijkl-mnop"
   ```

Then sign, notarize, and staple in one step after building + `macdeployqt`:

```bash
tools/sign_and_notarize.sh build/Drumming.app
```

The script signs all nested frameworks/dylibs/plugins inside-out with the
hardened runtime, submits the bundle to Apple's notary service, waits for the
result, staples the ticket, and verifies with `spctl`. Override the identity or
profile via the `IDENTITY` / `NOTARY_PROFILE` environment variables (see the
header of the script). Distribute the resulting bundle inside a `.dmg` or
`.zip`.

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
