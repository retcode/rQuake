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
// in_sdl.c -- SDL2 input driver

#include "quakedef.h"
#include "sdl_local.h"

// Mouse variables
cvar_t m_filter = {"m_filter", "0"};
cvar_t m_raw = {"m_raw", "1", true};  // Raw mouse input (no OS acceleration)

qboolean mouseactive = false;
static qboolean mouseinitialized = false;
static qboolean mouse_grabbed = false;

static int mouse_x, mouse_y;
static int old_mouse_x, old_mouse_y;
static int mouse_oldbuttonstate;
static qboolean mouse_consumed_for_view = false;

/*
===========
IN_ShowMouse
===========
*/
void IN_ShowMouse(void)
{
    SDL_ShowCursor(SDL_ENABLE);
}

/*
===========
IN_HideMouse
===========
*/
void IN_HideMouse(void)
{
    SDL_ShowCursor(SDL_DISABLE);
}

/*
===========
IN_ActivateMouse
===========
*/
void IN_ActivateMouse(void)
{
    if (mouseinitialized && !mouseactive) {
        mouseactive = true;
        mouse_grabbed = true;

        // Set raw mouse hint before enabling relative mode
        if (m_raw.value)
        {
            SDL_SetHint(SDL_HINT_MOUSE_RELATIVE_MODE_WARP, "0");
        }

        SDL_SetRelativeMouseMode(SDL_TRUE);
        IN_HideMouse();
    }
}

/*
===========
IN_DeactivateMouse
===========
*/
void IN_DeactivateMouse(void)
{
    if (mouseinitialized && mouseactive) {
        mouseactive = false;
        mouse_grabbed = false;
        SDL_SetRelativeMouseMode(SDL_FALSE);
        IN_ShowMouse();
    }
}

void IN_UpdateClipCursor(void)
{
    // Handled by SDL relative mouse mode
}

/*
===========
IN_Init
===========
*/
void IN_Init(void)
{
    Cvar_RegisterVariable(&m_filter);
    Cvar_RegisterVariable(&m_raw);

    Cmd_AddCommand("force_centerview", Force_CenterView_f);

    if (COM_CheckParm("-nomouse"))
        return;

    mouseinitialized = true;

    Con_Printf("Mouse initialized\n");
}

/*
===========
IN_Shutdown
===========
*/
void IN_Shutdown(void)
{
    IN_DeactivateMouse();
}

/*
===========
Force_CenterView_f
===========
*/
void Force_CenterView_f(void)
{
    cl.viewangles[PITCH] = 0;
}

/*
===========
MapSDLKeyToQuake

Map SDL scancode to Quake key code
===========
*/
static int MapSDLKeyToQuake(SDL_Scancode scancode)
{
    switch (scancode) {
    case SDL_SCANCODE_TAB:
        return K_TAB;
    case SDL_SCANCODE_RETURN:
        return K_ENTER;
    case SDL_SCANCODE_ESCAPE:
        return K_ESCAPE;
    case SDL_SCANCODE_SPACE:
        return K_SPACE;
    case SDL_SCANCODE_BACKSPACE:
        return K_BACKSPACE;
    case SDL_SCANCODE_UP:
        return K_UPARROW;
    case SDL_SCANCODE_DOWN:
        return K_DOWNARROW;
    case SDL_SCANCODE_LEFT:
        return K_LEFTARROW;
    case SDL_SCANCODE_RIGHT:
        return K_RIGHTARROW;
    case SDL_SCANCODE_LALT:
    case SDL_SCANCODE_RALT:
        return K_ALT;
    case SDL_SCANCODE_LCTRL:
    case SDL_SCANCODE_RCTRL:
        return K_CTRL;
    case SDL_SCANCODE_LSHIFT:
    case SDL_SCANCODE_RSHIFT:
        return K_SHIFT;
    case SDL_SCANCODE_F1:
        return K_F1;
    case SDL_SCANCODE_F2:
        return K_F2;
    case SDL_SCANCODE_F3:
        return K_F3;
    case SDL_SCANCODE_F4:
        return K_F4;
    case SDL_SCANCODE_F5:
        return K_F5;
    case SDL_SCANCODE_F6:
        return K_F6;
    case SDL_SCANCODE_F7:
        return K_F7;
    case SDL_SCANCODE_F8:
        return K_F8;
    case SDL_SCANCODE_F9:
        return K_F9;
    case SDL_SCANCODE_F10:
        return K_F10;
    case SDL_SCANCODE_F11:
        return K_F11;
    case SDL_SCANCODE_F12:
        return K_F12;
    case SDL_SCANCODE_INSERT:
        return K_INS;
    case SDL_SCANCODE_DELETE:
        return K_DEL;
    case SDL_SCANCODE_PAGEDOWN:
        return K_PGDN;
    case SDL_SCANCODE_PAGEUP:
        return K_PGUP;
    case SDL_SCANCODE_HOME:
        return K_HOME;
    case SDL_SCANCODE_END:
        return K_END;
    case SDL_SCANCODE_PAUSE:
        return K_PAUSE;

    // Number row
    case SDL_SCANCODE_1:
        return '1';
    case SDL_SCANCODE_2:
        return '2';
    case SDL_SCANCODE_3:
        return '3';
    case SDL_SCANCODE_4:
        return '4';
    case SDL_SCANCODE_5:
        return '5';
    case SDL_SCANCODE_6:
        return '6';
    case SDL_SCANCODE_7:
        return '7';
    case SDL_SCANCODE_8:
        return '8';
    case SDL_SCANCODE_9:
        return '9';
    case SDL_SCANCODE_0:
        return '0';
    case SDL_SCANCODE_MINUS:
        return '-';
    case SDL_SCANCODE_EQUALS:
        return '=';

    // Letters
    case SDL_SCANCODE_A:
        return 'a';
    case SDL_SCANCODE_B:
        return 'b';
    case SDL_SCANCODE_C:
        return 'c';
    case SDL_SCANCODE_D:
        return 'd';
    case SDL_SCANCODE_E:
        return 'e';
    case SDL_SCANCODE_F:
        return 'f';
    case SDL_SCANCODE_G:
        return 'g';
    case SDL_SCANCODE_H:
        return 'h';
    case SDL_SCANCODE_I:
        return 'i';
    case SDL_SCANCODE_J:
        return 'j';
    case SDL_SCANCODE_K:
        return 'k';
    case SDL_SCANCODE_L:
        return 'l';
    case SDL_SCANCODE_M:
        return 'm';
    case SDL_SCANCODE_N:
        return 'n';
    case SDL_SCANCODE_O:
        return 'o';
    case SDL_SCANCODE_P:
        return 'p';
    case SDL_SCANCODE_Q:
        return 'q';
    case SDL_SCANCODE_R:
        return 'r';
    case SDL_SCANCODE_S:
        return 's';
    case SDL_SCANCODE_T:
        return 't';
    case SDL_SCANCODE_U:
        return 'u';
    case SDL_SCANCODE_V:
        return 'v';
    case SDL_SCANCODE_W:
        return 'w';
    case SDL_SCANCODE_X:
        return 'x';
    case SDL_SCANCODE_Y:
        return 'y';
    case SDL_SCANCODE_Z:
        return 'z';

    // Punctuation
    case SDL_SCANCODE_SEMICOLON:
        return ';';
    case SDL_SCANCODE_APOSTROPHE:
        return '\'';
    case SDL_SCANCODE_GRAVE:
        return '`';
    case SDL_SCANCODE_COMMA:
        return ',';
    case SDL_SCANCODE_PERIOD:
        return '.';
    case SDL_SCANCODE_SLASH:
        return '/';
    case SDL_SCANCODE_BACKSLASH:
        return '\\';
    case SDL_SCANCODE_LEFTBRACKET:
        return '[';
    case SDL_SCANCODE_RIGHTBRACKET:
        return ']';

    default:
        return 0;
    }
}

/*
===========
IN_ProcessEvent

Called from Sys_SendKeyEvents for each SDL event
===========
*/
void IN_ProcessEvent(SDL_Event *event)
{
    int key;

    switch (event->type) {
    case SDL_KEYDOWN:
    case SDL_KEYUP:
        key = MapSDLKeyToQuake(event->key.keysym.scancode);
        if (key)
            Key_Event(key, event->type == SDL_KEYDOWN);
        break;

    case SDL_MOUSEBUTTONDOWN:
    case SDL_MOUSEBUTTONUP:
        switch (event->button.button) {
        case SDL_BUTTON_LEFT:
            Key_Event(K_MOUSE1, event->type == SDL_MOUSEBUTTONDOWN);
            break;
        case SDL_BUTTON_RIGHT:
            Key_Event(K_MOUSE2, event->type == SDL_MOUSEBUTTONDOWN);
            break;
        case SDL_BUTTON_MIDDLE:
            Key_Event(K_MOUSE3, event->type == SDL_MOUSEBUTTONDOWN);
            break;
        }
        break;

    case SDL_MOUSEWHEEL:
        if (event->wheel.y > 0) {
            Key_Event(K_MWHEELUP, true);
            Key_Event(K_MWHEELUP, false);
        } else if (event->wheel.y < 0) {
            Key_Event(K_MWHEELDOWN, true);
            Key_Event(K_MWHEELDOWN, false);
        }
        break;

    case SDL_MOUSEMOTION:
        if (mouseactive) {
            mouse_x += event->motion.xrel;
            mouse_y += event->motion.yrel;
        }
        break;
    }
}

/*
===========
IN_MouseEvent

Called from window messages (not used in SDL build)
===========
*/
void IN_MouseEvent(int mstate)
{
    // Not used in SDL build - we handle mouse in IN_ProcessEvent
}

/*
===========
IN_UpdateViewAngles

Updates view angles from mouse input without building a movement command.
Called every render frame for smooth mouse look.
===========
*/
void IN_UpdateViewAngles(void)
{
    int mx, my;

    if (!mouseactive || !ActiveApp || Minimized)
        return;

    mx = mouse_x;
    my = mouse_y;

    if (m_filter.value) {
        mx = (mx + old_mouse_x) / 2;
        my = (my + old_mouse_y) / 2;
    }

    old_mouse_x = mouse_x;
    old_mouse_y = mouse_y;
    mouse_x = 0;
    mouse_y = 0;

    mx *= sensitivity.value;
    my *= sensitivity.value;

    // Apply to view angles (freelook always enabled in modern mode)
    if (!((in_strafe.state & 1) || lookstrafe.value))
        cl.viewangles[YAW] -= m_yaw.value * mx;

    V_StopPitchDrift();

    if (!(in_strafe.state & 1)) {
        cl.viewangles[PITCH] += m_pitch.value * my;

        // Clamp pitch
        if (cl.viewangles[PITCH] > 80)
            cl.viewangles[PITCH] = 80;
        if (cl.viewangles[PITCH] < -70)
            cl.viewangles[PITCH] = -70;
    }

    mouse_consumed_for_view = true;
}

/*
===========
IN_MouseMove

Apply mouse movement to view (fallback for when IN_UpdateViewAngles not called)
===========
*/
static void IN_MouseMove(usercmd_t *cmd)
{
    int mx, my;
    int freelook;

    if (!mouseactive)
        return;

    // If viewangles already updated this frame, mouse is consumed
    if (mouse_consumed_for_view) {
        mouse_consumed_for_view = false;
        return;  // Skip - viewangles already applied
    }

    mx = mouse_x;
    my = mouse_y;

    if (m_filter.value) {
        mx = (mx + old_mouse_x) / 2;
        my = (my + old_mouse_y) / 2;
    }

    old_mouse_x = mouse_x;
    old_mouse_y = mouse_y;
    mouse_x = 0;
    mouse_y = 0;

    mx *= sensitivity.value;
    my *= sensitivity.value;

    // Modern freelook - always enabled (can still use +mlook for original behavior)
    freelook = 1;  // or use: (in_mlook.state & 1) || freelook_cvar.value

    // Add mouse X/Y movement to cmd
    if ((in_strafe.state & 1) || (lookstrafe.value && freelook))
        cmd->sidemove += m_side.value * mx;
    else
        cl.viewangles[YAW] -= m_yaw.value * mx;

    if (freelook)
        V_StopPitchDrift();

    if (freelook && !(in_strafe.state & 1)) {
        cl.viewangles[PITCH] += m_pitch.value * my;
        if (cl.viewangles[PITCH] > 80)
            cl.viewangles[PITCH] = 80;
        if (cl.viewangles[PITCH] < -70)
            cl.viewangles[PITCH] = -70;
    } else {
        if ((in_strafe.state & 1) && noclip_anglehack)
            cmd->upmove -= m_forward.value * my;
        else
            cmd->forwardmove -= m_forward.value * my;
    }
}

/*
===========
IN_Move
===========
*/
void IN_Move(usercmd_t *cmd)
{
    if (ActiveApp && !Minimized) {
        IN_MouseMove(cmd);
    }
}

/*
===========
IN_Accumulate
===========
*/
void IN_Accumulate(void)
{
    // In SDL, mouse motion is accumulated in IN_ProcessEvent
}

/*
===================
IN_ClearStates
===================
*/
void IN_ClearStates(void)
{
    if (mouseactive) {
        mouse_x = 0;
        mouse_y = 0;
        old_mouse_x = 0;
        old_mouse_y = 0;
        mouse_oldbuttonstate = 0;
    }
}

/*
===========
IN_Commands
===========
*/
void IN_Commands(void)
{
    // No joystick support in this basic SDL port
}
