/*
Copyright (C) 1996-1997 Id Software, Inc.
Copyright (C) 2024 SDL2 Port

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
// sdl_local.h -- SDL2 shared definitions
// NOTE: Must be included AFTER quakedef.h

#ifndef SDL_LOCAL_H
#define SDL_LOCAL_H

#include <SDL.h>
#include <SDL_opengl.h>

// Video mode states (equivalent to winquake.h)
typedef enum {MS_WINDOWED, MS_FULLSCREEN, MS_FULLDIB, MS_UNINIT} modestate_t;
extern modestate_t modestate;

// Global SDL objects shared between sys_sdl.c, gl_vid_sdl.c, in_sdl.c
extern SDL_Window *sdl_window;
extern SDL_GLContext gl_context;

// Window state
extern int window_center_x, window_center_y;
extern int window_x, window_y;
extern int window_width, window_height;

// App state
extern qboolean ActiveApp;
extern qboolean Minimized;

// Mouse state
extern qboolean mouseactive;

// Input functions
void IN_ProcessEvent(SDL_Event *event);
void IN_ShowMouse(void);
void IN_HideMouse(void);
void IN_ActivateMouse(void);
void IN_DeactivateMouse(void);
void IN_UpdateClipCursor(void);
void IN_MouseEvent(int mstate);

// Sound functions for blocking/unblocking
void S_BlockSound(void);
void S_UnblockSound(void);

#endif // SDL_LOCAL_H
