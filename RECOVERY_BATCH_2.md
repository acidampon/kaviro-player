# KAVIRO Player — Recovery Batch 2

Native media-engine boundary reconstructed.

Included:
- FfmpegMediaSession contract
- FFmpeg demux/stream-discovery implementation behind KAVIRO_FFMPEG_NATIVE
- Explicit native build option
- Stream metadata boundary
- Track-selection boundary
- Owned decoded-frame data contract
- Recovery outcome contract
- Monotonic playback clock with bounded speed and overflow protection
- Native-boundary regression tests

Verification:
- CMake configure/build with native option OFF: PASS
- CTest: 2/2 PASS
- Native boundary executable: PASS
- Native option ON: correctly fails closed because libavformat/libavcodec/libavutil development packages are absent in this environment

Release truth:
This batch does NOT claim a compiled native FFmpeg engine. The current environment has FFmpeg command-line binaries but not FFmpeg development headers/libraries. The actual decoder/presentation pipeline remains the next native-engine stage.
