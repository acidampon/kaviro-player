# KAVIRO Native Engine Boundary

Portable contract around the bundled FFmpeg engine. The final release must ship a verified native package and pass the package/release gate before being advertised as standalone.

The current recovery environment does not include FFmpeg development libraries, so native compilation remains gated by KAVIRO_FFMPEG_NATIVE.
