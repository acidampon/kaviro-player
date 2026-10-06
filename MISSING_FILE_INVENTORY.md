# KAVIRO Missing-File Inventory — 2026-10-06

## Historical advanced source not found
The exact source for the historical FFmpeg decode pipeline, owned frame output, A/V clock/sync, media/release hardening, and session/persistence hardening was not found in surviving branches, reachable commits, workflow artifacts, or the local filesystem.

Known lost historical commits:
- 5072540 — Add FFmpeg packet decode pipeline
- 03614a4 — Add owned FFmpeg frame output boundary
- d2702a1 — Add monotonic playback clock and A/V sync policy
- f3c5bec — Harden KAVIRO media and release boundaries
- 91930f6 — Harden session and persistence state boundaries

## Recreated
The recovery workspace now contains reconstructed FFmpeg session decoding/seek/recovery logic, a native-engine contract, package/release validation, platform-service boundaries, and contract tests.

## Still requiring real platform/toolchain work
- Bundled FFmpeg binaries and verified package checksums
- Android Gradle/NDK project and native shell
- Windows MSVC/Windows SDK project and native shell
- Hardware-decoder implementations
- Production audio/video device sinks
- Final UI/installers/APKs

Recreated source is not represented as recovered source. Native FFmpeg compilation remains unverified because the current environment lacks FFmpeg development headers/libraries.
