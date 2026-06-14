cmake_minimum_required(VERSION 3.15)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
project(drumming_training CXX)
enable_testing()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── RtMidi (bundled) ──────────────────────────────────────────────────────────
add_library(rtmidi STATIC third_party/rtmidi/RtMidi.cpp)
target_include_directories(rtmidi PUBLIC third_party/rtmidi)

if(APPLE)
    target_compile_definitions(rtmidi PUBLIC __MACOSX_CORE__)
    target_link_libraries(rtmidi PUBLIC
        "-framework CoreMIDI"
        "-framework CoreAudio"
        "-framework CoreFoundation"
    )
elseif(UNIX)
    target_compile_definitions(rtmidi PUBLIC __LINUX_ALSA__)
    target_link_libraries(rtmidi PUBLIC asound pthread)
endif()

# ── SFML ─────────────────────────────────────────────────────────────────────
# Build SFML (and other fetched deps) statically so the .app bundle is
# self-contained and can be launched from Finder without dangling dylibs.
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
include(FetchContent)
FetchContent_Declare(
    SFML
    GIT_REPOSITORY https://github.com/SFML/SFML.git
    GIT_TAG        3.0.0
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(SFML)

# ── Catch2 ───────────────────────────────────────────────────────────────────
FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.7.1
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(Catch2)

# ── SQLite3 (system) ─────────────────────────────────────────────────────────
find_package(SQLite3 REQUIRED)

# ── drumming_core (shared between drum_viz and tests) ─────────────────────────
add_library(drumming_core STATIC
    src/midi.cpp
    src/types.cpp
    src/geometry.cpp
    src/scoring.cpp
    src/persistence.cpp
)
target_include_directories(drumming_core PUBLIC include)
target_link_libraries(drumming_core PUBLIC
    rtmidi
    SFML::Graphics
    SFML::System
    SQLite::SQLite3
)

# ── drum_viz ─────────────────────────────────────────────────────────────────
# On macOS this produces a double-clickable Drumming.app bundle; elsewhere a
# plain executable.
add_executable(drum_viz MACOSX_BUNDLE
    src/main.cpp
    src/audio.cpp
    src/draw.cpp
)
target_include_directories(drum_viz PRIVATE include)
target_link_libraries(drum_viz PRIVATE
    drumming_core
    SFML::Graphics
    SFML::Window
    SFML::Audio
    SFML::System
)

if(APPLE)
    set_target_properties(drum_viz PROPERTIES
        OUTPUT_NAME "Drumming"
        MACOSX_BUNDLE_BUNDLE_NAME        "Drumming"
        MACOSX_BUNDLE_GUI_IDENTIFIER     "com.drumming.training"
        MACOSX_BUNDLE_BUNDLE_VERSION     "1.0"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "1.0"
    )
endif()

# ── drums_in (MIDI debug tool) ────────────────────────────────────────────────
add_executable(drums_in tools/drums_in.cpp)
target_link_libraries(drums_in PRIVATE rtmidi)

# ── tests ─────────────────────────────────────────────────────────────────────
add_executable(drumming_tests
    tests/test_midi.cpp
    tests/test_geometry.cpp
    tests/test_scoring.cpp
    tests/test_types.cpp
    tests/test_persistence.cpp
)
target_link_libraries(drumming_tests PRIVATE drumming_core Catch2::Catch2WithMain)

include(Catch)
catch_discover_tests(drumming_tests)
