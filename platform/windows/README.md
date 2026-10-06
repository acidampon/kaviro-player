# KAVIRO Windows Shell

Planned first-class Windows desktop application shell.

The shell is intentionally kept separate from `ump_core` so Windows APIs cannot leak into the portable core.

## Current status

**Contract prepared; native Windows build not yet verified in the current environment.**

Required build environment for the first real build:
- Windows SDK
- MSVC or a supported Windows C++ toolchain
- Native media engine integration

## First implementation surface

1. File/folder picker
2. Library screen backed by `LibraryCatalog` + `LibraryDashboardBuilder`
3. Player window backed by `PlayerSession`
4. Drag-and-drop media
5. Fullscreen/window controls
6. System media transport controls
7. Hardware acceleration
