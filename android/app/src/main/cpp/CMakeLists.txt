cmake_minimum_required(VERSION 3.24)
project(kaviro_android LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT ANDROID)
    message(FATAL_ERROR "KAVIRO Android native target requires the Android NDK toolchain")
endif()

set(KAVIRO_FFMPEG_ROOT "${CMAKE_CURRENT_LIST_DIR}/../jniLibs/${ANDROID_ABI}")
set(KAVIRO_FFMPEG_INCLUDE "${CMAKE_CURRENT_LIST_DIR}/../ffmpeg/include")
if(NOT EXISTS "${KAVIRO_FFMPEG_INCLUDE}/libavformat/avformat.h")
    message(FATAL_ERROR "Pinned FFmpeg headers are missing for Android ABI ${ANDROID_ABI}")
endif()

set(KAVIRO_FFMPEG_LIB_FILES
    "avutil;libavutil.so.61"
    "swresample;libswresample.so.7"
    "swscale;libswscale.so.10"
    "avcodec;libavcodec.so.63"
    "avformat;libavformat.so.63"
)

foreach(entry IN LISTS KAVIRO_FFMPEG_LIB_FILES)
    list(GET entry 0 lib)
    list(GET entry 1 filename)
    add_library(kaviro_ffmpeg_${lib} SHARED IMPORTED)
    set_target_properties(kaviro_ffmpeg_${lib} PROPERTIES
        IMPORTED_LOCATION "${KAVIRO_FFMPEG_ROOT}/${filename}"
        INTERFACE_INCLUDE_DIRECTORIES "${KAVIRO_FFMPEG_INCLUDE}"
    )
endforeach()

add_library(kaviro_android SHARED
    native-lib.cpp
    ../../../../../core/src/FfmpegMediaSession.cpp
    ../../../../../core/src/native/NativeMediaEngine.cpp
)

target_include_directories(kaviro_android PRIVATE
    ../../../../../core/include
    "${KAVIRO_FFMPEG_INCLUDE}"
)

target_compile_definitions(kaviro_android PRIVATE
    KAVIRO_ANDROID_BOUNDARY=1
    KAVIRO_FFMPEG_NATIVE=1
)

find_library(log-lib log)
find_library(android-lib android)

target_link_libraries(kaviro_android PRIVATE
    kaviro_ffmpeg_avformat
    kaviro_ffmpeg_avcodec
    kaviro_ffmpeg_swscale
    kaviro_ffmpeg_swresample
    kaviro_ffmpeg_avutil
    ${android-lib}
    ${log-lib}
)
