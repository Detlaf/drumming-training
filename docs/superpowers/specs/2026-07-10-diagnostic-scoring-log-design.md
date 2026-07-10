# Diagnostic Scoring Log — Design

**Date:** 2026-07-10
**Status:** Approved (pending implementation plan)

## 1. Purpose & scope

A developer/support diagnostic that records **how each hit is scored against the
current groove** during Play, so scoring and timing bugs can be reproduced from a
file.

It is **not** a user-facing practice log and **not** a raw-MIDI dump for
cross-talk debugging. It targets the scoring pipeline: given a MIDI event, why did
it score the way it did?

- Output: **JSON Lines** (`.log`), one event object per line.
- Activation: a checkable **Play-screen control** ("Diagnostics"), off by default.
- Zero impact on the scoring core (`drumming_core` stays Qt-free and
  side-effect-free).

## 2. Architecture

New self-contained class `DiagnosticLogger`, owned by `PracticeController` (the
`drum_viz` target), mirroring how `Metronome` is already a controller member. File
I/O and JSON serialization live entirely in this class. The controller calls it
from the points that already hold the scoring data (`pollMidiTick`, `playTick`,
play enter/exit). The controller knows nothing about JSON; the logger knows nothing
about Qt timers or scoring math.

Rejected alternatives:

- **Log inside `scoring.cpp`** — breaks the deliberate Qt-free, side-effect-free
  boundary of `drumming_core` and its pure-function testability.
- **Signal/slot event sink** — over-engineered for a single consumer (YAGNI).

### Components

- **`DiagnosticLogger`** (new). Responsibilities: open/close a timestamped file,
  serialize events to JSONL, flush per line. Proposed interface:
  - `bool start(const std::string& path, const SessionMeta& meta)` — open file,
    write the header line. Returns false on open failure.
  - `void logHit(const HitLogEntry& e)` — one scored hit.
  - `void logLifecycle(LifecycleKind kind, float tMs, int loop)` — play
    start/stop and loop rollover.
  - `void stop()` — close file.
  - `bool isActive() const`.
- **`PracticeController`** gains:
  - members `DiagnosticLogger logger_;` and `bool diagnosticsEnabled_ = false;`
  - slot `void setDiagnosticsEnabled(bool);`
  - calls into `logger_` from `enterPlay`, `pollMidiTick`, `playTick`, and the
    Play-exit path in `togglePlay`.
- **Play view UI** gains a checkable "Diagnostics" control near the Start/Stop
  session button, wired to `setDiagnosticsEnabled`.

## 3. Data captured (JSONL schema)

`tMs` is milliseconds since `app_.playStart` — the same clock the scorer uses — so
log timing lines up with scoring timing.

### Header line (written on `start`)

```json
{"type":"session","startedAt":"2026-07-10T14:03:22Z","groove":{"bpm":100,"measures":2,"totalSteps":32,"stepDurMs":93.75,"name":"Funk 1","notes":[[4,0],[8,3]]}}
```

`notes` is the groove as `[step, voice]` pairs (from `app_.groove`). `name` is
`app_.currentGrooveName` or `"<unsaved>"` when empty.

### Hit line (one per scored MIDI event in `pollMidiTick`)

```json
{"type":"hit","tMs":1234.5,"note":38,"vel":90,"voice":3,"snapStep":4,"offsetMs":12.3,"class":"Correct","sessionActive":true}
```

Captures the full scoring input→output: raw `note`/`vel`, the mapped `voice`
(`-1` if the pad is unrecognized), the snapped `snapStep`, `offsetMs`, and the
`HitClass` (`Correct` | `EarlyCorrectPad` | `LateCorrectPad` | `WrongPad`).
`sessionActive` records whether a formal practice session was running.

### Lifecycle lines (timeline context)

```json
{"type":"playStart","tMs":0.0,"loop":0}
{"type":"loop","tMs":3000.0,"loop":1}
{"type":"playStop","tMs":8123.4,"loop":2}
```

## 4. File location & lifecycle

- One file per capture, named `diag-YYYYMMDD-HHMMSS.log`, under
  `~/Library/Application Support/Drumming/diagnostics/`, reusing the
  writable-directory logic already in `PracticeController::defaultDatabasePath()`.
- **Toggle ON while on the Play screen** → open a new file, write the header and a
  `playStart` line, log subsequent hits/loops.
- **Toggle OFF, or leaving the Play screen** → `stop()` (write `playStop`, close).
  Turning it on again starts a fresh file.
- **Toggle ON while not on Play** → nothing is written until Play begins; entering
  Play with the toggle on opens the file at that point.
- **Open failure** → message to `stderr`, the toggle silently reverts to off, and
  practice is never interrupted (logging must never crash the app).

## 5. Testing

- **Unit-test `DiagnosticLogger` directly**: feed it a header plus a sequence of
  `HitLogEntry` and lifecycle events, read the file back, and assert each line is
  valid JSON with the expected fields and values. Follows the existing
  `ctest` / `drumming_tests` pattern.
- **Controller integration**: with diagnostics enabled, drive MIDI events through
  the existing scoring path and assert one hit line is emitted per event whose
  `class` matches what `scoreHit` produced — proving the log reflects real scoring.
- **No new external dependencies**: hand-write the JSON (all fields are simple
  scalars or small `[step, voice]` arrays), consistent with the repo's avoidance of
  extra libraries.
