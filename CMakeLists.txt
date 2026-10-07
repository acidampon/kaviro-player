cmake_minimum_required(VERSION 3.24)
project(KAVIROPlayer VERSION 0.7.3 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(UMP_BUILD_TESTS "Build core tests" ON)
option(KAVIRO_FFMPEG_NATIVE "Build against native FFmpeg development libraries" OFF)

add_library(ump_core STATIC
  core/src/KaviroCore.cpp
  core/src/FfmpegMediaSession.cpp
  core/src/native/NativeMediaEngine.cpp
  core/src/native/NativeMediaEnginePackage.cpp
  core/src/native/ReleaseGate.cpp
  platform/common/PLATFORM_SERVICES_FACTORY.cpp
  platform/android/AndroidPlatformServices.cpp
  platform/windows/WindowsPlatformServices.cpp
)
target_include_directories(ump_core PUBLIC core/include platform/common platform/android platform/windows)
find_package(SQLite3 REQUIRED)
target_link_libraries(ump_core PUBLIC SQLite::SQLite3)

if(KAVIRO_FFMPEG_NATIVE)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET libavformat libavcodec libavutil)
  target_compile_definitions(ump_core PUBLIC KAVIRO_FFMPEG_NATIVE=1)
  target_link_libraries(ump_core PUBLIC PkgConfig::FFMPEG)
endif()

add_executable(kaviro_player app/main.cpp)
target_link_libraries(kaviro_player PRIVATE ump_core)

if(UMP_BUILD_TESTS)
  enable_testing()
  add_executable(kaviro_core_tests tests/core_tests.cpp)
  target_link_libraries(kaviro_core_tests PRIVATE ump_core)
  add_test(NAME kaviro_core_tests COMMAND kaviro_core_tests)

  add_executable(kaviro_native_boundary_tests tests/native_tests.cpp)
  target_link_libraries(kaviro_native_boundary_tests PRIVATE ump_core)
  add_test(NAME kaviro_native_boundary_tests COMMAND kaviro_native_boundary_tests)

  add_executable(kaviro_native_engine_contract_tests tests/native_engine_contract_tests.cpp)
  target_link_libraries(kaviro_native_engine_contract_tests PRIVATE ump_core)
  add_test(NAME kaviro_native_engine_contract_tests COMMAND kaviro_native_engine_contract_tests)

  if(KAVIRO_FFMPEG_NATIVE)
    add_executable(kaviro_native_decode_smoke tests/native_decode_smoke.cpp)
    target_link_libraries(kaviro_native_decode_smoke PRIVATE ump_core)
  endif()
endif()
