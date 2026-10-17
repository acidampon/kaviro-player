# KAVIRO Android

The Android application is the most complete user-facing KAVIRO target at this checkpoint. It uses a Java activity for the UI, JNI to the native KAVIRO playback engine, CMake/NDK for native compilation, and bundled FFmpeg shared libraries.

## Current app capabilities

- Open local media through the Storage Access Framework
- Play/pause, stop, seek, and playback-state display
- Select audio/video streams when supported by the opened media
- Recent-media history with playback-position persistence
- Persistent upcoming queue with queue management and failure recovery
- Audio-focus and lifecycle pause/resume handling
- External SubRip (`.srt`) captions with an on/off toggle and long-press replacement
- Subtitle resource limits to reject files over 5 MB, more than 10,000 cues, or a cue over 4,000 characters

## Toolchain

The Android Gradle configuration currently uses:

- Compile SDK 36
- Minimum SDK 26
- Java 17 in CI
- NDK 28.2.13676358
- CMake 3.31.6
- Gradle 9.6.0 in CI
- ABIs: `arm64-v8a` and `x86_64`

The CI workflow verifies the pinned FFmpeg source signature, builds the native libraries for both Android ABIs, assembles a debug APK, and uploads it as the `kaviro-android-debug` artifact.

## Verification status

The latest verified development checkpoint is commit `747ecc185e76b6a1e6b622fad73a256aaf7d3212`; CI run [#233](https://github.com/acidampon/kaviro-player/actions/runs/37856513131) passed all four jobs. This proves CI build/test success only. It does not prove that the subtitle overlay behaves identically on every device, nor does it replace physical-device playback testing.

## Known gaps

- Subtitle formats other than SRT are not supported by the external subtitle loader.
- Embedded subtitle track selection is not implemented in the current UI.
- Physical-device checks for subtitle layering, sync during seek, lifecycle behavior, and diverse document providers remain required.
- A debug APK is not a signed release package.
