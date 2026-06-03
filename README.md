# drumming-training

An application that captures data from the e-drumming kit and visualizes it on the screen. In addition, it allows to create grooves and then checks if I play correctly by comparing data from the kit with the groove.

## Execution

### macOS

Build:

```bash
cmake -S . -B build
cmake --build build
```

Run:

```bash
./build/drum_viz
```

### Controls

| Key | Action |
|-----|--------|
| Click on staff | Add / remove a note |
| Space | Toggle Edit / Play mode |
| `+` / `-` | BPM up / down (5 BPM steps) |
| `M` / `N` | Add / remove a measure |
| `C` | Clear groove |

In **Play** mode a metronome click fires on each beat (high pitch on beat 1 of each measure). The playhead sweeps the groove; incoming MIDI hits are scored green (correct, within ±80 ms) or red (wrong pad or mistimed).
