# KAVIRO Android Platform Notes

This directory documents the intended platform boundary between Android-specific lifecycle/storage behavior and the portable `ump_core`. The current user-facing Android application lives in `android/`; do not treat this directory as the active app module.

## Current status

The Android application module is implemented and its debug APK builds in CI. The verified development checkpoint is `747ecc185e76b6a1e6b622fad73a256aaf7d3212`, with all four CI jobs passing in [run #233](https://github.com/acidampon/kaviro-player/actions/runs/37856513131).

Implemented in the active `android/` module:

1. Storage Access Framework media picker and persisted media URI access where supported
2. Playback controls and seek bar
3. Audio/video stream selection through the native engine
4. Recent media and saved playback positions
5. Persistent upcoming queue and queue management
6. Audio focus and activity lifecycle handling
7. External SRT subtitle loading, visibility toggle, and resource limits

## Remaining platform work

- Verify the subtitle overlay and timing on physical Android devices.
- Add a foreground playback service and notification controls if background playback is a product requirement.
- Evaluate Picture-in-Picture for video.
- Add embedded subtitle support and more external subtitle formats.
- Test URI access and media-open recovery across document providers and Android versions.

Keep Android APIs out of the portable C++ core. Update this note when platform architecture or implementation status changes.
