cmake_minimum_required(VERSION 3.24)
project(KAVIROPlayer VERSION 0.7.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(UMP_BUILD_TESTS "Build core tests" ON)

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

add_executable(kaviro_player app/main.cpp)
target_link_libraries(kaviro_player PRIVATE ump_core)

if(UMP_BUILD_TESTS)
  enable_testing()
  add_executable(kaviro_core_tests tests/core_tests.cpp)
  target_link_libraries(kaviro_core_tests PRIVATE ump_core)
  add_test(NAME kaviro_core_tests COMMAND kaviro_core_tests)
endif()
