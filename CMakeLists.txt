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

# ── Qt 6 ─────────────────────────────────────────────────────────────────────
# Replaces SFML: Widgets (UI), Gui (QPainter staff/grid), Multimedia (metronome).
# macOS bundles Qt frameworks via macdeployqt (see README), so no static-link
# of fetched deps is needed.
find_package(Qt6 REQUIRED COMPONENTS Widgets Gui Multimedia)
set(CMAKE_AUTOMOC ON)

# ── Catch2 ───────────────────────────────────────────────────────────────────
include(FetchContent)
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
# Stays Qt-free so drumming_tests builds without a GUI toolkit.
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
    SQLite3::SQLite3
)

# ── drum_viz ─────────────────────────────────────────────────────────────────
# On macOS this produces a double-clickable Drumming.app bundle; elsewhere a
# plain executable. The Qt UI sources land across migration steps 3–5; the
# target is defined once they are all present so the project keeps configuring
# (and drumming_tests keeps building) mid-migration.
set(DRUM_VIZ_SOURCES
    src/main.cpp
    src/audio.cpp
    src/controller.cpp
)
file(GLOB DRUM_VIZ_UI_SOURCES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/src/ui/*.cpp")
list(APPEND DRUM_VIZ_SOURCES ${DRUM_VIZ_UI_SOURCES})

set(DRUM_VIZ_READY TRUE)
foreach(_src ${DRUM_VIZ_SOURCES})
    if(NOT EXISTS "${_src}" AND NOT IS_ABSOLUTE "${_src}")
        if(NOT EXISTS "${CMAKE_SOURCE_DIR}/${_src}")
            set(DRUM_VIZ_READY FALSE)
        endif()
    endif()
endforeach()
if(NOT DRUM_VIZ_UI_SOURCES)
    set(DRUM_VIZ_READY FALSE)
endif()

if(DRUM_VIZ_READY)
    if(APPLE)
        set(DRUM_VIZ_ICON "${CMAKE_SOURCE_DIR}/resources/AppIcon.icns")
        list(APPEND DRUM_VIZ_SOURCES "${DRUM_VIZ_ICON}")
    endif()
    add_executable(drum_viz MACOSX_BUNDLE ${DRUM_VIZ_SOURCES})
    target_include_directories(drum_viz PRIVATE include)
    target_link_libraries(drum_viz PRIVATE
        drumming_core
        Qt6::Widgets
        Qt6::Gui
        Qt6::Multimedia
    )

    if(APPLE)
        set_target_properties(drum_viz PROPERTIES
            OUTPUT_NAME "Drumming"
            MACOSX_BUNDLE_BUNDLE_NAME        "Drumming"
            MACOSX_BUNDLE_GUI_IDENTIFIER     "com.drumming.training"
            MACOSX_BUNDLE_BUNDLE_VERSION     "1.0"
            MACOSX_BUNDLE_SHORT_VERSION_STRING "1.0"
            MACOSX_BUNDLE_ICON_FILE          "AppIcon"
        )
        set_source_files_properties("${DRUM_VIZ_ICON}" PROPERTIES
            MACOSX_PACKAGE_LOCATION "Resources"
        )
    endif()
else()
    message(STATUS
        "drum_viz target skipped: Qt UI sources not present yet "
        "(expected during Qt migration steps 3–6).")
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
