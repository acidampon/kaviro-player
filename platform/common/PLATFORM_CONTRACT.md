# KAVIRO Platform Contract

The shared C++ core owns media discovery, library state, playback state, history, playlists and persistence. Platform shells own operating-system concerns only.

## Required shell responsibilities

- Select media/folders using native OS APIs.
- Convert selected locations into `ump::MediaLocation` values.
- Preserve access permissions across app restarts where the OS permits it.
- Resolve locations into readable media streams/paths for the native media engine.
- Forward app foreground/background lifecycle events.
- Expose platform capabilities without pretending unsupported features exist.
- Keep local playback functional when the network is unavailable.

## Android

Use Storage Access Framework (`ACTION_OPEN_DOCUMENT`, `ACTION_OPEN_DOCUMENT_TREE`) and persist URI permissions. Do not assume arbitrary filesystem paths exist for content URIs.

The Android shell will later own:
- foreground playback service
- audio focus
- notification/media controls
- Picture-in-Picture
- scoped-storage/URI access
- hardware decoder selection

## Windows

Use native file/folder picker APIs and normal filesystem paths. The Windows shell will later own:
- window/fullscreen behavior
- system media transport controls
- hardware decoder selection
- file associations
- drag-and-drop

## Non-negotiable boundary

The shell must never make cloud access, an account, GitHub, Supabase, Firebase, or an internet connection a prerequisite for local playback.
