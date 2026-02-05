# rQuake

This is a rough coded "Enhanced version of the Quake game". Intended to mainly make it run and have fun with it. 

## Features

- Can run
- SDL2 support
- OpenGL support
- Music support

## Dependencies

- Original Quake game files (required) 
  - `\Quake\id1\pak0.pak`
  - `\Quake\id1\pak1.pak`

## Installation

1. Build the project
2. Copy `pak0.pak` and `pak1.pak` from your Quake installation to `build/Release/id1/`
3. Run `glquake.exe` (or `./glquake` on Linux)

For music you will need to copy the music files to the `build/Release/id1/music` directory.

## Issues

- Frustum culling disabled, due to a workaround. The underlying issue is that the game's `BoxOnPlaneSlide` or the frustrum plane setup does not work correctly, (Will cause performance issues). `R_CullBox` will always return `false` at the moment.
- PVS disabled. Performance impact (all geometry always considered)
- 32-bit build is required, will avoid pointer truncation warnings

## Commands

### Music Commands
Commands available:
  - cd play <track> - play a track
  - cd loop <track> - play looping
  - cd stop - stop music
  - cd pause / cd resume
  - cd info - show status
  - bgmvolume 0.5 - adjust volume (0.0-1.0)

## Credits

- **id Software** - Original Quake engine and game
- **SDL Team** - SDL2 library

Based on the original GLQuake source release
