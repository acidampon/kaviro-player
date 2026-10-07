cmake_minimum_required(VERSION 3.24)
project(KAVIROPlayer VERSION 0.7.3 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(UMP_BUILD_TESTS "Build core tests" ON)
option(KAVIRO_FFMPEG_NATIVE "Build against native FFmpeg development libraries" OFF)
option(KAVIRO_REQUIRE_NATIVE_FFMPEG "Fail release configuration when native FFmpeg is unavailable" ON)

# A release build must never silently ship the non-decoding boundary stub.
if(CMAKE_BUILD_TYPE STREQUAL "Release" AND KAVIRO_REQUIRE_NATIVE_FFMPEG)
  set(KAVIRO_FFMPEG_NATIVE ON CACHE BOOL "" FORCE)
endif()

add_library(ump_core STATIC
    core/src/MediaItem.cpp
    core/src/MediaLibrary.cpp
    core/src/LibraryView.cpp
    core/src/LibraryRootsStore.cpp
    core/src/LibraryCatalog.cpp
    core/src/LibraryDashboard.cpp
    core/src/PlaybackState.cpp
    core/src/MediaProbe.cpp
    core/src/PlaybackEngine.cpp
    core/src/MediaEngineFactory.cpp
    core/src/PlaybackController.cpp
    core/src/PlaybackStore.cpp
    core/src/Playlist.cpp
    core/src/MediaQueue.cpp
    core/src/HistoryStore.cpp
    core/src/PlayerSession.cpp
    core/src/PlaylistStore.cpp
    core/src/SettingsStore.cpp
)

target_include_directories(ump_core PUBLIC core/include)
target_compile_features(ump_core PUBLIC cxx_std_20)

find_package(SQLite3 REQUIRED)
target_link_libraries(ump_core PUBLIC SQLite::SQLite3)

if(KAVIRO_FFMPEG_NATIVE)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET libavformat libavcodec libavutil)
  target_compile_definitions(ump_core PUBLIC KAVIRO_FFMPEG_NATIVE=1)
  target_link_libraries(ump_core PUBLIC PkgConfig::FFMPEG)
elseif(CMAKE_BUILD_TYPE STREQUAL "Release" AND KAVIRO_REQUIRE_NATIVE_FFMPEG)
  message(FATAL_ERROR "KAVIRO Release builds require native FFmpeg development libraries")
endif()

add_executable(kaviro_player app/main.cpp)
target_link_libraries(kaviro_player PRIVATE ump_core)

if(UMP_BUILD_TESTS)
  enable_testing()
  add_executable(kaviro_core_tests tests/core_tests.cpp)
  target_link_libraries(kaviro_core_tests PRIVATE ump_core)
  add_test(NAME kaviro_core_tests COMMAND kaviro_core_tests)
endif()
