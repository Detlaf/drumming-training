# CLAUDE.md

Guidance for working in this repository. See `README.md` for full build,
run, and distribution details.

## Project Context / Build

This is a C++/Qt project (migrated from SFML). When working here:

- Use **Qt 6** APIs (Widgets, Gui, Multimedia).
- Require **CMake ≥ 3.15**.
- Before pushing, verify the SFML/Qt/RtMidi platform configurations build on
  CI.

## Verification

After implementing any change, always verify before committing:

1. **Build** the project:

   ```bash
   cmake --build build
   ```

   (On macOS, configure once with
   `cmake -S . -B build -DCMAKE_PREFIX_PATH="$HOME/Qt/6.11.1/macos"`.)

2. **Run the test suite** and confirm it passes:

   ```bash
   ctest --test-dir build --output-on-failure
   ```

   (Or run the `drumming_tests` executable directly.)

3. **Capture a screenshot** for any visual/UI change by launching the app
   (`open build/Drumming.app`) so the change can be confirmed visually.

Only commit once the build succeeds and the tests pass.

## Domain Notes / MIDI

For MIDI drum detection, keep in mind that e-kits don't always follow the
General MIDI percussion map:

- Kits may emit **nonstandard note numbers** (e.g., note 22 instead of 42 for
  the hi-hat). Normalize incoming notes to the canonical pad before scoring.
- Kits exhibit **cross-talk** — one pad's hit triggering spurious events on
  another. Apply **velocity gating** to reject low-velocity false hits.
