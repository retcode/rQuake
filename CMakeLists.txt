cmake_minimum_required(VERSION 3.16)
project(GLQuake VERSION 1.09 LANGUAGES C)

# C standard (C99 for compatibility with legacy Quake code)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Option for SDL_mixer music support
option(USE_SDL_MIXER "Enable OGG/MP3 music playback via SDL_mixer" OFF)

# Local SDL2 path
set(SDL2_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../SDL2-2.32.10")
set(SDL2_INCLUDE_DIR "${SDL2_DIR}/include")
# Select correct lib directory based on target architecture
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(SDL2_LIB_DIR "${SDL2_DIR}/lib/x64")
else()
    set(SDL2_LIB_DIR "${SDL2_DIR}/lib/x86")
endif()

# SDL2_mixer paths (if enabled)
if(USE_SDL_MIXER)
    set(SDL2_MIXER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../SDL2_mixer-2.8.0")
    set(SDL2_MIXER_INCLUDE_DIR "${SDL2_MIXER_DIR}/include")
    set(SDL2_MIXER_LIB_DIR "${SDL2_MIXER_DIR}/lib/x64")
endif()

# Find OpenGL
find_package(OpenGL REQUIRED)

# Compiler definitions
add_definitions(-DGLQUAKE -DUSE_SDL -Did386=0)

if(USE_SDL_MIXER)
    add_definitions(-DUSE_SDL_MIXER)
endif()

# Core game logic sources
set(GAME_SOURCES
    src/chase.c
    src/cl_demo.c
    src/cl_input.c
    src/cl_main.c
    src/cl_parse.c
    src/cl_tent.c
    src/cmd.c
    src/common.c
    src/console.c
    src/crc.c
    src/cvar.c
    src/host.c
    src/host_cmd.c
    src/keys.c
    src/mathlib.c
    src/menu.c
    src/pr_cmds.c
    src/pr_edict.c
    src/pr_exec.c
    src/sbar.c
    src/view.c
    src/wad.c
    src/zone.c
    src/world.c
)

# Server sources
set(SERVER_SOURCES
    src/sv_main.c
    src/sv_move.c
    src/sv_phys.c
    src/sv_user.c
)

# OpenGL renderer sources
set(GL_SOURCES
    src/gl_draw.c
    src/gl_mesh.c
    src/gl_model.c
    src/gl_refrag.c
    src/gl_rlight.c
    src/gl_rmain.c
    src/gl_rmisc.c
    src/gl_rsurf.c
    src/gl_screen.c
    src/gl_warp.c
    src/r_part.c
)

# Sound core sources
set(SOUND_SOURCES
    src/snd_dma.c
    src/snd_mem.c
    src/snd_mix.c
)

# Networking sources (cross-platform UDP)
set(NET_SOURCES
    src/net_main.c
    src/net_loop.c
    src/net_dgrm.c
    src/net_vcr.c
    src/net_sdl.c
)

# SDL2 platform layer sources
set(SDL_SOURCES
    src/sys_sdl.c
    src/gl_vid_sdl.c
    src/in_sdl.c
    src/snd_sdl.c
    src/cd_sdl.c
)

# Create the executable
add_executable(glquake
    ${GAME_SOURCES}
    ${SERVER_SOURCES}
    ${GL_SOURCES}
    ${SOUND_SOURCES}
    ${NET_SOURCES}
    ${SDL_SOURCES}
)

# Include directories
target_include_directories(glquake PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${SDL2_INCLUDE_DIR}
    ${OPENGL_INCLUDE_DIR}
)

if(USE_SDL_MIXER)
    target_include_directories(glquake PRIVATE ${SDL2_MIXER_INCLUDE_DIR})
endif()

# Link libraries
target_link_libraries(glquake PRIVATE
    ${SDL2_LIB_DIR}/SDL2.lib
    ${SDL2_LIB_DIR}/SDL2main.lib
    ${OPENGL_LIBRARIES}
)

if(USE_SDL_MIXER)
    target_link_libraries(glquake PRIVATE ${SDL2_MIXER_LIB_DIR}/SDL2_mixer.lib)
endif()

# Platform-specific settings
if(WIN32)
    target_link_libraries(glquake PRIVATE ws2_32)
    target_compile_definitions(glquake PRIVATE _CRT_SECURE_NO_WARNINGS)
    # MSVC-specific: Disable optimizations and use strict floating-point
    if(MSVC)
        target_compile_options(glquake PRIVATE /Od /fp:strict)
    endif()
elseif(UNIX AND NOT APPLE)
    target_link_libraries(glquake PRIVATE m)
endif()

# Copy SDL2.dll to output directory
add_custom_command(TARGET glquake POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${SDL2_LIB_DIR}/SDL2.dll"
    $<TARGET_FILE_DIR:glquake>
)

# Copy SDL2_mixer.dll if enabled
if(USE_SDL_MIXER)
    add_custom_command(TARGET glquake POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${SDL2_MIXER_LIB_DIR}/SDL2_mixer.dll"
        $<TARGET_FILE_DIR:glquake>
    )
endif()

# Create id1 folder for game data
add_custom_command(TARGET glquake POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
    "$<TARGET_FILE_DIR:glquake>/id1"
)

# Copy PAK files if they exist
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../original-quake-files/PAK0.PAK")
    add_custom_command(TARGET glquake POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/../original-quake-files/PAK0.PAK"
        "$<TARGET_FILE_DIR:glquake>/id1/pak0.pak"
    )
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../original-quake-files/PAK1.PAK")
    add_custom_command(TARGET glquake POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/../original-quake-files/PAK1.PAK"
        "$<TARGET_FILE_DIR:glquake>/id1/pak1.pak"
    )
endif()

# Create music folder for SDL_mixer
if(USE_SDL_MIXER)
    add_custom_command(TARGET glquake POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:glquake>/id1/music"
    )
endif()

# Print configuration summary
message(STATUS "")
message(STATUS "=== GLQuake Build Configuration ===")
message(STATUS "Version:        ${PROJECT_VERSION}")
message(STATUS "C Standard:     C${CMAKE_C_STANDARD}")
message(STATUS "Build Type:     ${CMAKE_BUILD_TYPE}")
message(STATUS "SDL2 Path:      ${SDL2_DIR}")
message(STATUS "SDL_mixer:      ${USE_SDL_MIXER}")
message(STATUS "")
