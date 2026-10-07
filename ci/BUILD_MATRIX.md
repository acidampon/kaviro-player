# KAVIRO Player Build Matrix

KAVIRO is built in dedicated environments because the development sandbox does not contain the Android SDK/NDK, Windows toolchain, or FFmpeg development headers.

## Current CI lanes

| Lane | Runner | Native FFmpeg | Purpose |
| --- | --- | --- | --- |
| Linux core | Ubuntu | Off | Core regression and portable CMake build |
| Linux FFmpeg boundary | Ubuntu | System development packages | Proves the native FFmpeg headers/libs compile and link |
| Windows core | Windows | Off | MSVC/CMake portability gate |
| Android | Android/Ubuntu | Not enabled yet | Will be enabled when the Android Gradle/NDK project is present |

## FFmpeg policy

The pinned upstream baseline is **9.0.2**. FFmpeg publishes source releases and release signatures; KAVIRO should verify the selected source artifact before producing distributable native packages.

The Linux FFmpeg CI lane currently uses the runner's development packages only as a compile/link boundary check. It is **not** the final bundled FFmpeg distribution.

## Release gates

A release must not claim:

- Android APK support until an Android Gradle + NDK build completes.
- Windows native playback until the Windows native engine and FFmpeg dependency are built and tested.
- standalone native playback until the decoder/packet/frame pipeline is implemented and tested.
- universal codec support without compatibility-matrix evidence.

## Next build-environment milestones

1. Add the Android application module and JNI bridge.
2. Add a pinned, verified FFmpeg source build for Linux/Windows/Android.
3. Implement persistent AVFormatContext/AVCodecContext session state.
4. Implement packet demux, decode, seek, audio/video output, and recovery.
5. Run a media compatibility matrix on real CI/device runners.
