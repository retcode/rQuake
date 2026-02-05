# rQuake

This is a rough coded "Enhanced version of the Quake game". Intended to mainly make it run and have fun with it. 

## Issues

- Frustum culling disabled, due to a workaround. The underlying issue is that the game's `BoxOnPlaneSlide` or the frustrum plane setup does not work correctly, (Will cause performance issues). `R_CullBox` will always return `false` at the moment.
- PVS disabled. Performance impact (all geometry always considered)
- 32-bit build is required, will avoid pointer truncation warnings

## Features

- Can run
- SDL2 support
- OpenGL support
