# KAVIRO Player

**Your media. Your device. Your way.**

A standalone, offline-first universal media player for Android and Windows, designed to make local media ownership feel simple while keeping advanced controls underneath.

## Current foundation

- Cross-platform C++ core
- Recursive local media scanning
- Content-aware MPEG transport-stream handling
- Media inspection through a temporary FFprobe development adapter
- Playback controller: play/pause/stop/seek/speed/volume state
- SQLite playback-position persistence
- Playlist model and persistence
- Queue/session/history foundations
- Generated compatibility samples and regression tests

## Standalone promise

Core local playback must not require:
- An account
- A cloud database
- Firebase/Supabase
- Internet access
- A separate FFmpeg installation in the final release

The current development build uses system ffprobe/ffplay only as temporary adapters while the native media-engine integration is developed.

## Roadmap

1. Native/bundled media engine
2. Real playback session with pause/seek/volume/speed
3. Desktop UI
4. Android shell
5. Library/history/favorites UI
6. Subtitles and audio-track selection
7. Background audio and Android media controls
8. Hardware acceleration
9. Online provider adapters
10. Authorized download manager
11. Device-to-device local transfer
12. Compatibility certification and release packaging
