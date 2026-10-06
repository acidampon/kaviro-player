# KAVIRO Android Shell

Planned first-class Android application shell.

The shell is intentionally kept separate from `ump_core` so Android lifecycle and storage APIs cannot leak into the portable core.

## Current status

**Contract prepared; native Android build not yet verified in the current environment.**

Required toolchain for the first real build:
- Android SDK
- Android build tools
- Gradle/Android Gradle Plugin
- NDK for the native core/media engine

## First implementation surface

1. Storage Access Framework picker
2. Library screen backed by `LibraryCatalog` + `LibraryDashboardBuilder`
3. Player screen backed by `PlayerSession`
4. Foreground playback service
5. Audio focus + notification controls
6. Picture-in-Picture for video
7. Persistent URI permissions
