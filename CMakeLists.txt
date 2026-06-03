cmake_minimum_required(VERSION 3.20)
project(drumming_training CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── RtMidi (bundled) ──────────────────────────────────────────────────────────
add_library(rtmidi STATIC third_party/rtmidi/RtMidi.cpp)
target_include_directories(rtmidi PUBLIC third_party/rtmidi)
target_compile_definitions(rtmidi PUBLIC __MACOSX_CORE__)
target_link_libraries(rtmidi PUBLIC
    "-framework CoreMIDI"
    "-framework CoreAudio"
    "-framework CoreFoundation"
)

# ── SFML ─────────────────────────────────────────────────────────────────────
set(SFML_DIR "/opt/homebrew/Cellar/sfml/3.0.2/lib/cmake/SFML")
find_package(SFML 3 COMPONENTS Graphics Window Audio System REQUIRED)

# ── drum_viz ─────────────────────────────────────────────────────────────────
add_executable(drum_viz
    src/main.cpp
    src/audio.cpp
    src/draw.cpp
    src/geometry.cpp
    src/midi.cpp
    src/types.cpp
)
target_include_directories(drum_viz PRIVATE include)
target_link_libraries(drum_viz PRIVATE
    rtmidi
    SFML::Graphics
    SFML::Window
    SFML::Audio
    SFML::System
)

# ── drums_in (MIDI debug tool) ────────────────────────────────────────────────
add_executable(drums_in tools/drums_in.cpp)
target_link_libraries(drums_in PRIVATE rtmidi)
