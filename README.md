# drumming-training

An application that captures data from the e-drumming kit and visualizes it on the screen. In addition, it allows to create grooves and then checks if I play correctly by comparing data from the kit with the groove.

## Execution

### macOS

Compile:

```bash
g++ -D__MACOSX_CORE__ -o connect connect.cpp RtMidi.cpp -framework CoreMIDI -framework CoreAudio -framework CoreFoundation
```

Run:

```bash
./connect
```

### Test sequence 1

Snare -> Hi tom -> Mid tom -> Low tom