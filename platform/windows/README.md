# KAVIRO Windows Player

KAVIRO now includes a native Win32 desktop shell in `app/windows_main.cpp`. The Windows CMake build produces `kaviro_windows_player` in addition to the portable core/CLI.

## Current desktop controls

- Open a local media file with the Windows file picker.
- Play/pause and stop.
- Seek using the timeline slider.
- Render video in the main playback area and resize it with the window.
- Use Windows Media Foundation for system-provided decoding and playback.

## Important scope

This is the first functional Windows desktop playback shell, not yet a claim of universal codec coverage or a bundled FFmpeg Windows release. Supported media depends on the Windows Media Foundation codecs installed on the target system. The Windows CI build verifies that the application compiles and that the portable core tests pass; actual Windows playback still requires a Windows runtime test with representative audio and video files.

## Next Windows release checks

1. Run the desktop app on Windows and test open, play/pause, stop, seeking, resize, and close while playing.
2. Verify codec and document-path behavior; record unsupported formats clearly.
3. Add track/subtitle controls and integrate the bundled FFmpeg engine where needed.
4. Package the app and required dependencies and verify a clean-machine installation.
