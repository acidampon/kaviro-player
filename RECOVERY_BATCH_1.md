# KAVIRO Player — Recovery Batch 1

This is a reconstruction checkpoint from the surviving GitHub foundation. It is not a byte-for-byte restoration of the lost advanced local source tree.

## Included
- Stable media identity and local media model
- Recursive media scanning with diagnostics/cancellation
- FFprobe development inspection boundary
- Playback controller with bounded speed/volume/seek state
- SQLite playback persistence
- Queue, history, playlists and settings foundations
- Playback-engine contract and development adapter
- Engine factory boundary
- Player session orchestration
- Compatibility wrapper headers preserving historical KAVIRO include names

## Verification
- CMake configure/build: PASS
- Core regression test: PASS (1/1)
- Generated MP4 inspection with ffprobe: PASS
- Generated MP4 recursive scan: PASS

## Release boundary
The external ffplay/ffprobe path remains development-only. It is not the final standalone native engine. Android, Windows x86/x64 packaging and bundled native FFmpeg still require their respective toolchains.
