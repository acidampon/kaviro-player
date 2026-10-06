# KAVIRO Player

**Your media. Your device. Your way.**

Standalone, offline-first media player recovery project for Android and Windows.

## Recovery status

Recovery Batch 1 reconstructs the shared C++ core from the surviving GitHub checkpoint.

The current development build provides:
- Local media model and stable media identity
- Recursive media scanning and diagnostics
- FFprobe development inspection
- Playback state/controller
- SQLite resume persistence
- Queue/history/playlists/settings foundations
- Playback engine and factory contracts
- Player session orchestration

## Standalone boundary

Final KAVIRO releases must not require an account, cloud storage, Supabase/Firebase, internet access for local playback, or a separately installed FFmpeg/ffplay package.

The current ffprobe/ffplay adapter is explicitly development-only.

## Next recovery stages

1. Native/bundled media engine
2. Real decoded audio/video output
3. Damaged-media recovery layer
4. A/V clock and synchronization
5. Android and Windows platform adapters
6. UI and installable packages
7. Compatibility certification and release hardening
