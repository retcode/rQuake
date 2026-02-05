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
// gl_vid_sdl.c -- SDL2 OpenGL video driver

#include "quakedef.h"
#include "sdl_local.h"

// GL_MULTISAMPLE may not be defined on older systems
#ifndef GL_MULTISAMPLE
#define GL_MULTISAMPLE 0x809D
#endif

#define WARP_WIDTH 320
#define WARP_HEIGHT 200

// SDL globals
SDL_Window *sdl_window = NULL;
SDL_GLContext gl_context = NULL;

// Forward declaration from in_sdl.c
extern void IN_ActivateMouse(void);

// Window state
int window_center_x, window_center_y;
int window_x, window_y;
int window_width, window_height;

// App state
qboolean ActiveApp = true;
qboolean Minimized = false;
qboolean scr_skipupdate = false;

// Video state
static qboolean vid_initialized = false;
static float vid_gamma = 1.0;

// Mode state (windowed/fullscreen)
modestate_t modestate = MS_UNINIT;

// GL state
const char *gl_vendor;
const char *gl_renderer;
const char *gl_version;
const char *gl_extensions;

float gldepthmin, gldepthmax;

glvert_t glv;

cvar_t gl_ztrick = {"gl_ztrick", "0"};  // Disabled - causes rendering artifacts on modern GPUs
cvar_t vid_vsync = {"vid_vsync", "0", true};  // 0=off, 1=on, 2=adaptive
cvar_t gl_msaa = {"gl_msaa", "4"};  // MSAA samples (requires restart)

viddef_t vid; // global video state

unsigned short d_8to16table[256];
unsigned d_8to24table[256];
unsigned char d_15to8table[65536];

int texture_mode = GL_LINEAR;
int texture_extension_number = 1;

qboolean is8bit = false;
qboolean isPermedia = false;
qboolean gl_mtexable = false;

// Multitexture support (defined in gl_rsurf.c)
extern lpMTexFUNC qglMTexCoord2fSGIS;
extern lpSelTexFUNC qglSelectTextureSGIS;

// ARB multitexture function pointers
static void (APIENTRY *qglActiveTextureARB)(GLenum texture) = NULL;
static void (APIENTRY *qglMultiTexCoord2fARB)(GLenum texture, GLfloat s, GLfloat t) = NULL;

// Wrapper functions to translate SGIS constants to ARB constants
// TEXTURE0_SGIS = 0x835E, GL_TEXTURE0 = 0x84C0
static void APIENTRY wrapper_SelectTexture(GLenum target)
{
    // Translate SGIS constant to ARB constant
    GLenum arb_target = GL_TEXTURE0 + (target - 0x835E);
    if (qglActiveTextureARB)
        qglActiveTextureARB(arb_target);
}

static void APIENTRY wrapper_MTexCoord2f(GLenum target, GLfloat s, GLfloat t)
{
    // Translate SGIS constant to ARB constant
    GLenum arb_target = GL_TEXTURE0 + (target - 0x835E);
    if (qglMultiTexCoord2fARB)
        qglMultiTexCoord2fARB(arb_target, s, t);
}

cvar_t vid_mode = {"vid_mode", "0", false};
cvar_t _vid_default_mode = {"_vid_default_mode", "0", true};
cvar_t _vid_default_mode_win = {"_vid_default_mode_win", "0", true};
cvar_t vid_wait = {"vid_wait", "0"};
cvar_t vid_nopageflip = {"vid_nopageflip", "0", true};
cvar_t _vid_wait_override = {"_vid_wait_override", "0", true};
cvar_t vid_config_x = {"vid_config_x", "800", true};
cvar_t vid_config_y = {"vid_config_y", "600", true};
cvar_t vid_stretch_by_2 = {"vid_stretch_by_2", "1", true};
cvar_t _windowed_mouse = {"_windowed_mouse", "1", true};

// Function pointers for menu
void (*vid_menudrawfn)(void) = NULL;
void (*vid_menukeyfn)(int key) = NULL;

// Forward declarations
void GL_Init(void);
void VID_ApplyVsync(void);

static float old_vsync = 0;

/*
================
VID_ApplyVsync

Apply vsync setting from cvar
================
*/
void VID_ApplyVsync(void)
{
	if (vid_vsync.value != old_vsync)
	{
		int vsync_mode = (int)vid_vsync.value;
		int interval;

		old_vsync = vid_vsync.value;

		// 0 = off, 1 = on, 2 = adaptive
		if (vsync_mode == 2)
			interval = -1;  // Adaptive vsync
		else if (vsync_mode == 1)
			interval = 1;   // Regular vsync
		else
			interval = 0;   // Off

		if (SDL_GL_SetSwapInterval(interval) < 0)
		{
			// Adaptive not supported, fall back to regular vsync
			if (interval == -1)
			{
				Con_Printf("Adaptive VSync not supported, using regular VSync\n");
				SDL_GL_SetSwapInterval(1);
			}
			else
			{
				Con_Printf("Warning: Unable to set VSync: %s\n", SDL_GetError());
			}
		}
		else
		{
			if (vsync_mode == 2)
				Con_Printf("VSync: adaptive\n");
			else if (vsync_mode == 1)
				Con_Printf("VSync: enabled\n");
			else
				Con_Printf("VSync: disabled\n");
		}
	}
}

/*
================
VID_HandlePause
================
*/
void VID_HandlePause(qboolean pause)
{
}

void VID_ForceLockState(int lk)
{
}

int VID_ForceUnlockedAndReturnState(void)
{
    return 0;
}

void D_BeginDirectRect(int x, int y, byte *pbitmap, int width, int height)
{
}

void D_EndDirectRect(int x, int y, int width, int height)
{
}

/*
================
CheckMultiTextureExtensions

Note: Multitexture is DISABLED by default because the old SGIS-style
texture blending doesn't work correctly on modern GPUs.
Use -mtex to enable if you want to test it.
================
*/
static void CheckMultiTextureExtensions(void)
{
    // Multitexture disabled by default - causes rendering artifacts on modern GPUs
    // The old GL_BLEND texture environment mode doesn't work the same way with ARB multitexture
    if (gl_extensions && strstr(gl_extensions, "GL_ARB_multitexture") && COM_CheckParm("-mtex")) {
        Con_Printf("Multitexture extensions found.\n");
        qglMultiTexCoord2fARB = (void (APIENTRY *)(GLenum, GLfloat, GLfloat))SDL_GL_GetProcAddress("glMultiTexCoord2fARB");
        qglActiveTextureARB = (void (APIENTRY *)(GLenum))SDL_GL_GetProcAddress("glActiveTextureARB");
        if (qglMultiTexCoord2fARB && qglActiveTextureARB) {
            // Use wrapper functions that translate SGIS constants to ARB constants
            qglMTexCoord2fSGIS = wrapper_MTexCoord2f;
            qglSelectTextureSGIS = wrapper_SelectTexture;
            gl_mtexable = true;
        }
    }
}

/*
===============
GL_Init
===============
*/
void GL_Init(void)
{
    gl_vendor = (const char *)glGetString(GL_VENDOR);
    Con_Printf("GL_VENDOR: %s\n", gl_vendor);

    gl_renderer = (const char *)glGetString(GL_RENDERER);
    Con_Printf("GL_RENDERER: %s\n", gl_renderer);

    gl_version = (const char *)glGetString(GL_VERSION);
    Con_Printf("GL_VERSION: %s\n", gl_version);

    gl_extensions = (const char *)glGetString(GL_EXTENSIONS);
    if (gl_extensions)
        Con_Printf("GL_EXTENSIONS: %s\n", gl_extensions);

    if (gl_renderer) {
        if (strstr(gl_renderer, "PowerVR") || strstr(gl_renderer, "Permedia"))
            isPermedia = true;
    }

    CheckMultiTextureExtensions();

    // Enable MSAA if configured
    if (gl_msaa.value > 0)
    {
        glEnable(GL_MULTISAMPLE);
        Con_Printf("MSAA: %dx (requires restart to change)\n", (int)gl_msaa.value);
    }

    glClearColor(0.1f, 0.0f, 0.0f, 0.0f);
    glClearDepth(1.0f);
    glCullFace(GL_FRONT);
    glEnable(GL_TEXTURE_2D);

    // Depth buffer setup
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_TRUE);
    glDepthRange(0.0f, 1.0f);

    glEnable(GL_ALPHA_TEST);
    glAlphaFunc(GL_GREATER, 0.666f);

    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glShadeModel(GL_FLAT);

    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
}

/*
=================
GL_BeginRendering
=================
*/
void GL_BeginRendering(int *x, int *y, int *width, int *height)
{
    *x = *y = 0;
    *width = window_width;
    *height = window_height;
}

/*
=================
GL_EndRendering
=================
*/
void GL_EndRendering(void)
{
    VID_ApplyVsync();
    SDL_GL_SwapWindow(sdl_window);
}

/*
================
VID_SetPalette
================
*/
void VID_SetPalette(unsigned char *palette)
{
    byte *pal;
    unsigned r, g, b;
    unsigned v;
    int r1, g1, b1;
    int j, k, l;
    unsigned short i;
    unsigned *table;

    // 8 8 8 encoding
    pal = palette;
    table = d_8to24table;
    for (i = 0; i < 256; i++) {
        r = pal[0];
        g = pal[1];
        b = pal[2];
        pal += 3;

        v = (255 << 24) + (r << 0) + (g << 8) + (b << 16);
        *table++ = v;
    }
    d_8to24table[255] &= 0xffffff; // 255 is transparent

    // Build 15-to-8 table
    for (i = 0; i < (1 << 15); i++) {
        r = ((i & 0x1F) << 3) + 4;
        g = ((i & 0x03E0) >> 2) + 4;
        b = ((i & 0x7C00) >> 7) + 4;
        pal = (unsigned char *)d_8to24table;
        for (v = 0, k = 0, l = 10000 * 10000; v < 256; v++, pal += 4) {
            r1 = r - pal[0];
            g1 = g - pal[1];
            b1 = b - pal[2];
            j = (r1 * r1) + (g1 * g1) + (b1 * b1);
            if (j < l) {
                k = v;
                l = j;
            }
        }
        d_15to8table[i] = k;
    }
}

void VID_ShiftPalette(unsigned char *palette)
{
    // Gamma handled differently in SDL
}

void VID_SetDefaultMode(void)
{
}

/*
================
VID_Shutdown
================
*/
void VID_Shutdown(void)
{
    if (vid_initialized) {
        vid_initialized = false;

        if (gl_context) {
            SDL_GL_DeleteContext(gl_context);
            gl_context = NULL;
        }

        if (sdl_window) {
            SDL_DestroyWindow(sdl_window);
            sdl_window = NULL;
        }
    }
}

static void Check_Gamma(unsigned char *pal)
{
    float f, inf;
    unsigned char palette[768];
    int i;

    if ((i = COM_CheckParm("-gamma")) != 0)
        vid_gamma = Q_atof(com_argv[i + 1]);
    else
        vid_gamma = 0.7f; // Default gamma

    for (i = 0; i < 768; i++) {
        f = pow((pal[i] + 1) / 256.0, vid_gamma);
        inf = f * 255 + 0.5;
        if (inf < 0)
            inf = 0;
        if (inf > 255)
            inf = 255;
        palette[i] = (unsigned char)inf;
    }

    memcpy(pal, palette, sizeof(palette));
}

/*
===================
VID_Init
===================
*/
void VID_Init(unsigned char *palette)
{
    int i;
    char gldir[MAX_OSPATH];
    int flags;

    Cvar_RegisterVariable(&vid_mode);
    Cvar_RegisterVariable(&vid_wait);
    Cvar_RegisterVariable(&vid_nopageflip);
    Cvar_RegisterVariable(&_vid_wait_override);
    Cvar_RegisterVariable(&_vid_default_mode);
    Cvar_RegisterVariable(&_vid_default_mode_win);
    Cvar_RegisterVariable(&vid_config_x);
    Cvar_RegisterVariable(&vid_config_y);
    Cvar_RegisterVariable(&vid_stretch_by_2);
    Cvar_RegisterVariable(&_windowed_mouse);
    Cvar_RegisterVariable(&gl_ztrick);
    Cvar_RegisterVariable(&vid_vsync);
    Cvar_RegisterVariable(&gl_msaa);

    // Get window size from command line or use defaults
    if (COM_CheckParm("-width"))
        window_width = Q_atoi(com_argv[COM_CheckParm("-width") + 1]);
    else
        window_width = 800;

    if (COM_CheckParm("-height"))
        window_height = Q_atoi(com_argv[COM_CheckParm("-height") + 1]);
    else
        window_height = 600;

    if (window_width < 320)
        window_width = 320;
    if (window_height < 200)
        window_height = 200;

    // Set up OpenGL attributes
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);

    // Set up MSAA if requested
    {
        int msaa = (int)gl_msaa.value;
        if (msaa > 0)
        {
            SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
            SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, msaa);
        }
    }

    // Create window
    flags = SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN;

    if (COM_CheckParm("-fullscreen") || !COM_CheckParm("-window"))
        flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;

    sdl_window = SDL_CreateWindow(
        "GLQuake",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        window_width,
        window_height,
        flags);

    if (!sdl_window)
        Sys_Error("Couldn't create SDL window: %s", SDL_GetError());

    // Get actual window size (may differ with fullscreen)
    SDL_GetWindowSize(sdl_window, &window_width, &window_height);

    // Set mode state
    if (flags & (SDL_WINDOW_FULLSCREEN | SDL_WINDOW_FULLSCREEN_DESKTOP))
        modestate = MS_FULLDIB;
    else
        modestate = MS_WINDOWED;

    // Create OpenGL context
    gl_context = SDL_GL_CreateContext(sdl_window);
    if (!gl_context)
        Sys_Error("Couldn't create OpenGL context: %s", SDL_GetError());

    // Make context current
    SDL_GL_MakeCurrent(sdl_window, gl_context);

    // Try to disable vsync for now (can be enabled via cvar)
    SDL_GL_SetSwapInterval(0);

    // Set up console dimensions
    if ((i = COM_CheckParm("-conwidth")) != 0)
        vid.conwidth = Q_atoi(com_argv[i + 1]);
    else
        vid.conwidth = 640;

    vid.conwidth &= 0xfff8; // make it a multiple of eight

    if (vid.conwidth < 320)
        vid.conwidth = 320;

    // pick a conheight that matches with correct aspect
    vid.conheight = vid.conwidth * 3 / 4;

    if ((i = COM_CheckParm("-conheight")) != 0)
        vid.conheight = Q_atoi(com_argv[i + 1]);
    if (vid.conheight < 200)
        vid.conheight = 200;

    vid.width = vid.conwidth;
    vid.height = vid.conheight;

    vid.maxwarpwidth = WARP_WIDTH;
    vid.maxwarpheight = WARP_HEIGHT;
    vid.colormap = host_colormap;
    vid.fullbright = 256 - LittleLong(*((int *)vid.colormap + 2048));
    vid.numpages = 2;
    vid.aspect = ((float)vid.height / (float)vid.width) * (320.0 / 240.0);

    Check_Gamma(palette);
    VID_SetPalette(palette);

    GL_Init();

    sprintf(gldir, "%s/glquake", com_gamedir);
    Sys_mkdir(gldir);

    vid_initialized = true;

    // Update window center for mouse
    window_center_x = window_width / 2;
    window_center_y = window_height / 2;

    // Activate mouse input
    IN_ActivateMouse();

    Con_Printf("Video mode: %dx%d\n", window_width, window_height);
}

/*
================
VID_NumModes
================
*/
int VID_NumModes(void)
{
    return 1;
}

char *VID_GetModeDescription(int mode)
{
    static char desc[64];
    sprintf(desc, "%dx%d", window_width, window_height);
    return desc;
}

int VID_SetMode(int modenum, unsigned char *palette)
{
    // Mode switching not implemented - would need to recreate window
    return 1;
}

qboolean VID_Is8bit(void)
{
    return is8bit;
}

