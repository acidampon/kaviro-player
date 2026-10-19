# KAVIRO Player Build Matrix

KAVIRO is built in dedicated CI environments because the development sandbox does not contain the complete Android SDK/NDK, Windows toolchain, or all native media dependencies.

## Current CI lanes

| Lane | Runner | What it verifies | What it does not prove |
| --- | --- | --- | --- |
| Linux core | Ubuntu | Portable C++ core build and regression tests | Actual audio/video playback |
| Linux FFmpeg native boundary | Ubuntu | Pinned FFmpeg source verification, native boundary build/tests, deterministic decode smoke | Android/Windows hardware-specific playback |
| Windows core | Windows Server 2022 | MSVC/CMake portability and core tests | A finished Windows graphical player or Windows native FFmpeg playback |
| Android | Ubuntu + Android SDK/NDK | Java unit tests, native FFmpeg build for arm64-v8a/x86_64, debug APK and unsigned release APK assembly | Physical-device behavior, release signing, store readiness |

## FFmpeg policy

The pinned upstream baseline is **9.0.2**. CI verifies the upstream source signature before compiling the native libraries. The Android job packages the pinned FFmpeg shared libraries for arm64-v8a and x86_64; the Linux native lane separately exercises a generated media fixture.

## Release gates

Do not describe KAVIRO as release-complete until these gates are met:

- **Android:** physical-device checks cover local-file and document-provider opening, audio/video playback, seeking, track selection, SRT subtitle visibility/synchronization, audio focus, background/foreground and surface recreation, and repeated open/close cycles.
- **Android distribution:** produce and verify a signed release APK/AAB using a protected release key. The CI release artifact is explicitly **unsigned** and is for build verification, not direct distribution.
- **Windows:** implement and verify a usable desktop UI, package the required native media dependencies, and test actual playback on Windows. Current Windows CI verifies the portable core only.
- **Media compatibility:** document tested containers/codecs and known limitations; do not promise universal codec support without evidence.
- **Regression:** all CI lanes must pass for the final release commit.

## Next priorities

1. Harden native player lifetime across concurrent JNI calls and release.
2. Build a repeatable unsigned Android release artifact in CI.
3. Implement the Windows desktop player shell and integrate native playback.
4. Perform physical Android and Windows playback validation.
5. Configure release signing and publish verified artifacts only after the device and platform gates pass.
