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
// sys_sdl.c -- SDL2 system driver

#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>

#ifdef _WIN32
#include <direct.h>
#include <io.h>
#include <fcntl.h>
#define mkdir(path, mode) _mkdir(path)
#else
#include <unistd.h>
#include <fcntl.h>
#endif

#include "quakedef.h"
#include "sdl_local.h"

qboolean isDedicated = false;

static int nostdout = 0;

static char *basedir = ".";

// =======================================================================
// General routines
// =======================================================================

void Sys_DebugNumber(int y, int val)
{
}

void Sys_Printf(char *fmt, ...)
{
    va_list argptr;
    char text[1024];

    va_start(argptr, fmt);
    vsnprintf(text, sizeof(text), fmt, argptr);
    va_end(argptr);

    if (nostdout)
        return;

    printf("%s", text);
}

void Sys_Quit(void)
{
    Host_Shutdown();
    SDL_Quit();
    exit(0);
}

void Sys_Init(void)
{
    // Nothing special needed
}

void Sys_Error(char *error, ...)
{
    va_list argptr;
    char string[1024];

    va_start(argptr, error);
    vsnprintf(string, sizeof(string), error, argptr);
    va_end(argptr);

    fprintf(stderr, "Error: %s\n", string);

    // Show error in a message box if possible
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Quake Error", string, NULL);

    Host_Shutdown();
    SDL_Quit();
    exit(1);
}

void Sys_Warn(char *warning, ...)
{
    va_list argptr;
    char string[1024];

    va_start(argptr, warning);
    vsnprintf(string, sizeof(string), warning, argptr);
    va_end(argptr);

    fprintf(stderr, "Warning: %s", string);
}

/*
============
Sys_FileTime

returns -1 if not present
============
*/
int Sys_FileTime(char *path)
{
    struct stat buf;

    if (stat(path, &buf) == -1)
        return -1;

    return buf.st_mtime;
}

void Sys_mkdir(char *path)
{
#ifdef _WIN32
    _mkdir(path);
#else
    mkdir(path, 0777);
#endif
}

int Sys_FileOpenRead(char *path, int *handle)
{
    int h;

#ifdef _WIN32
    struct _stat fileinfo;
    h = _open(path, _O_RDONLY | _O_BINARY);
#else
    struct stat fileinfo;
    h = open(path, O_RDONLY);
#endif
    *handle = h;
    if (h == -1)
        return -1;

#ifdef _WIN32
    if (_fstat(h, &fileinfo) == -1)
#else
    if (fstat(h, &fileinfo) == -1)
#endif
        Sys_Error("Error fstating %s", path);

    return fileinfo.st_size;
}

int Sys_FileOpenWrite(char *path)
{
    int handle;

#ifdef _WIN32
    handle = _open(path, _O_RDWR | _O_CREAT | _O_TRUNC | _O_BINARY, _S_IREAD | _S_IWRITE);
#else
    umask(0);
    handle = open(path, O_RDWR | O_CREAT | O_TRUNC, 0666);
#endif

    if (handle == -1)
        Sys_Error("Error opening %s: %s", path, strerror(errno));

    return handle;
}

int Sys_FileWrite(int handle, void *src, int count)
{
#ifdef _WIN32
    return _write(handle, src, count);
#else
    return write(handle, src, count);
#endif
}

void Sys_FileClose(int handle)
{
#ifdef _WIN32
    _close(handle);
#else
    close(handle);
#endif
}

void Sys_FileSeek(int handle, int position)
{
#ifdef _WIN32
    _lseek(handle, position, SEEK_SET);
#else
    lseek(handle, position, SEEK_SET);
#endif
}

int Sys_FileRead(int handle, void *dest, int count)
{
#ifdef _WIN32
    return _read(handle, dest, count);
#else
    return read(handle, dest, count);
#endif
}

void Sys_DebugLog(char *file, char *fmt, ...)
{
    va_list argptr;
    static char data[1024];
    int fd;

    va_start(argptr, fmt);
    vsnprintf(data, sizeof(data), fmt, argptr);
    va_end(argptr);

#ifdef _WIN32
    fd = _open(file, _O_WRONLY | _O_CREAT | _O_APPEND, _S_IREAD | _S_IWRITE);
    if (fd != -1) {
        _write(fd, data, strlen(data));
        _close(fd);
    }
#else
    fd = open(file, O_WRONLY | O_CREAT | O_APPEND, 0666);
    if (fd != -1) {
        write(fd, data, strlen(data));
        close(fd);
    }
#endif
}

/*
================
Sys_FloatTime
================
*/
double Sys_FloatTime(void)
{
    static Uint64 freq = 0;
    static Uint64 base = 0;
    Uint64 now;

    if (freq == 0) {
        freq = SDL_GetPerformanceFrequency();
        base = SDL_GetPerformanceCounter();
    }

    now = SDL_GetPerformanceCounter();
    return (double)(now - base) / (double)freq;
}

char *Sys_ConsoleInput(void)
{
    // Not supporting console input in SDL build
    return NULL;
}

void Sys_HighFPPrecision(void)
{
}

void Sys_LowFPPrecision(void)
{
}

void Sys_SetFPCW(void)
{
}

void Sys_Sleep(void)
{
    SDL_Delay(1);
}

/*
================
Sys_SendKeyEvents

Process SDL events and send key events to Quake
================
*/
void Sys_SendKeyEvents(void)
{
    SDL_Event event;

    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_QUIT:
            Sys_Quit();
            break;

        case SDL_WINDOWEVENT:
            switch (event.window.event) {
            case SDL_WINDOWEVENT_FOCUS_GAINED:
                ActiveApp = true;
                break;
            case SDL_WINDOWEVENT_FOCUS_LOST:
                ActiveApp = false;
                break;
            case SDL_WINDOWEVENT_MINIMIZED:
                Minimized = true;
                break;
            case SDL_WINDOWEVENT_RESTORED:
                Minimized = false;
                break;
            }
            break;

        default:
            // Pass to input handler
            IN_ProcessEvent(&event);
            break;
        }
    }
}

void Sys_MakeCodeWriteable(unsigned long startaddr, unsigned long length)
{
    // Not needed for SDL build - no assembly
}

/*
================
main
================
*/
int main(int argc, char *argv[])
{
    double time, oldtime, newtime;
    quakeparms_t parms;
    int j;

    memset(&parms, 0, sizeof(parms));

    COM_InitArgv(argc, argv);
    parms.argc = com_argc;
    parms.argv = com_argv;

    parms.memsize = 16 * 1024 * 1024; // 16 MB for GL

    j = COM_CheckParm("-mem");
    if (j)
        parms.memsize = (int)(Q_atof(com_argv[j + 1]) * 1024 * 1024);

    parms.membase = malloc(parms.memsize);
    if (!parms.membase)
        Sys_Error("Not enough memory for heap");

    parms.basedir = basedir;

    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS) < 0)
        Sys_Error("SDL_Init failed: %s", SDL_GetError());

    Host_Init(&parms);
    Sys_Init();

    if (COM_CheckParm("-nostdout"))
        nostdout = 1;
    else
        printf("GLQuake SDL -- Version %0.3f\n", GLQUAKE_VERSION);

    oldtime = Sys_FloatTime() - 0.1;

    // Main game loop
    while (1) {
        // Find time spent rendering last frame
        newtime = Sys_FloatTime();
        time = newtime - oldtime;

        if (time > 0.2)
            time = 0.2; // Cap frame time

        oldtime = newtime;

        Host_Frame(time);
    }

    return 0;
}
