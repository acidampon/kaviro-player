# KAVIRO Player

**Your media. Your device. Your way.**

KAVIRO is an offline-first media player project for Android and Windows. The project is being recovered and hardened in small, verified increments; the Android app is ahead of the Windows UI.

## Current verified checkpoint

Latest verified Android development commit: `747ecc185e76b6a1e6b622fad73a256aaf7d3212`.

The matching GitHub Actions run completed successfully for all four jobs: Android APK, Linux core, Windows core, and Linux FFmpeg native boundary. This verifies compilation and automated tests, not physical-device behavior or release readiness.

### Android app

The current Android app includes:

- Open local media through Android's Storage Access Framework
- Native playback controls: play/pause, stop, seek, and playback state
- Audio and video stream selection where the native engine exposes selectable streams
- Recent media list with saved playback position
- Persistent upcoming queue with add, reorder, remove, clear, and retry-after-open-failure behavior
- Audio-focus and lifecycle pause/resume handling
- External SubRip (`.srt`) subtitles, with on/off toggle and long-press replacement
- Subtitle parser resource limits: 5 MB per file, 10,000 cues, and 4,000 characters per cue
- arm64-v8a and x86_64 Android native library packaging

### Shared C++ core and Windows

The portable core contains media models and identity, scanning/diagnostics, playback state/controller, SQLite-backed resume persistence, queue/history/playlist/settings foundations, engine/factory contracts, and player-session orchestration. CI builds and tests the core on Linux and Windows. A finished Windows graphical player is not yet claimed.

## Standalone design goals

The intended release must not require a user account, cloud storage, Supabase/Firebase, internet access for local playback, or a separately installed FFmpeg/ffplay package. CI fetches and verifies a pinned FFmpeg source release to build the bundled native dependencies.

## Build and verification

- Android project: `android/`
- Shared C++ core: repository root CMake project
- CI workflow: [KAVIRO CI](.github/workflows/kaviro-ci.yml)
- Latest verified CI run: [run #233](https://github.com/acidampon/kaviro-player/actions/runs/37856513131)

A successful CI run does not replace real-device testing. Before release, test real media files and document providers, subtitle visibility and synchronization, seeking, audio focus, lifecycle recovery, and supported codec/container combinations on physical Android devices. Embedded subtitles, subtitle formats other than SRT, and release signing are not claimed complete.

## Next priorities

1. Verify subtitle overlay visibility and synchronization on a physical Android device.
2. Add automated tests around SRT parsing and boundary conditions.
3. Improve media-open failure recovery and test against varied document providers.
4. Add embedded subtitle support and additional external formats only after the current SRT path is stable.
5. Continue Windows playback/UI integration and release hardening.
