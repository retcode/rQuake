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
// cd_sdl.c -- SDL_mixer music playback for OGG/MP3 files

#include "quakedef.h"

#ifdef USE_SDL_MIXER

#include <SDL_mixer.h>

static qboolean cdValid = false;
static qboolean playing = false;
static qboolean wasPlaying = false;
static qboolean enabled = true;
static qboolean playLooping = false;
static byte playTrack;
static byte maxTrack;

static Mix_Music *music = NULL;

// bgmvolume is defined in snd_dma.c and declared extern in sound.h

static void CDAudio_Eject(void)
{
}

static void CDAudio_CloseDoor(void)
{
}

static int CDAudio_GetAudioDiskInfo(void)
{
    cdValid = true;
    maxTrack = 99;  // Assume up to 99 tracks
    return 0;
}

static char *CDAudio_GetTrackPath(byte track, char *buffer, int bufsize)
{
    // Try multiple locations and formats
    static const char *extensions[] = { "ogg", "mp3", "wav", NULL };
    static const char *paths[] = {
        "%s/music/track%02d.%s",      // id1/music/track02.ogg
        "%s/music/track%d.%s",        // id1/music/track2.ogg
        "%s/../music/track%02d.%s",   // music/track02.ogg (relative to game dir)
        NULL
    };
    int i, j;
    FILE *f;

    for (i = 0; paths[i]; i++)
    {
        for (j = 0; extensions[j]; j++)
        {
            snprintf(buffer, bufsize, paths[i], com_gamedir, track, extensions[j]);
            f = fopen(buffer, "rb");
            if (f)
            {
                fclose(f);
                return buffer;
            }
        }
    }

    return NULL;
}

void CDAudio_Play(byte track, qboolean looping)
{
    char trackpath[MAX_OSPATH];

    if (!enabled)
        return;

    if (!cdValid)
    {
        CDAudio_GetAudioDiskInfo();
        if (!cdValid)
            return;
    }

    if (track < 1 || track > maxTrack)
    {
        Con_DPrintf("CDAudio: Bad track number %u.\n", track);
        return;
    }

    // Stop any currently playing music
    if (music)
    {
        Mix_HaltMusic();
        Mix_FreeMusic(music);
        music = NULL;
    }

    // Find the track file
    if (!CDAudio_GetTrackPath(track, trackpath, sizeof(trackpath)))
    {
        Con_DPrintf("CDAudio: Could not find track %u\n", track);
        return;
    }

    // Load and play the music
    music = Mix_LoadMUS(trackpath);
    if (!music)
    {
        Con_DPrintf("CDAudio: Could not load %s: %s\n", trackpath, Mix_GetError());
        return;
    }

    if (Mix_PlayMusic(music, looping ? -1 : 1) == -1)
    {
        Con_DPrintf("CDAudio: Could not play %s: %s\n", trackpath, Mix_GetError());
        Mix_FreeMusic(music);
        music = NULL;
        return;
    }

    Mix_VolumeMusic((int)(bgmvolume.value * MIX_MAX_VOLUME));

    playLooping = looping;
    playTrack = track;
    playing = true;

    Con_DPrintf("CDAudio: Playing track %u (%s)\n", track, trackpath);
}

void CDAudio_Stop(void)
{
    if (!enabled)
        return;

    if (!playing)
        return;

    if (music)
    {
        Mix_HaltMusic();
        Mix_FreeMusic(music);
        music = NULL;
    }

    wasPlaying = false;
    playing = false;
}

void CDAudio_Pause(void)
{
    if (!enabled)
        return;

    if (!playing)
        return;

    Mix_PauseMusic();
    wasPlaying = playing;
    playing = false;
}

void CDAudio_Resume(void)
{
    if (!enabled)
        return;

    if (!wasPlaying)
        return;

    Mix_ResumeMusic();
    playing = true;
}

static void CD_f(void)
{
    char *command;

    if (Cmd_Argc() < 2)
        return;

    command = Cmd_Argv(1);

    if (Q_strcasecmp(command, "on") == 0)
    {
        enabled = true;
        return;
    }

    if (Q_strcasecmp(command, "off") == 0)
    {
        if (playing)
            CDAudio_Stop();
        enabled = false;
        return;
    }

    if (Q_strcasecmp(command, "reset") == 0)
    {
        enabled = true;
        if (playing)
            CDAudio_Stop();
        CDAudio_GetAudioDiskInfo();
        return;
    }

    if (Q_strcasecmp(command, "play") == 0)
    {
        CDAudio_Play((byte)Q_atoi(Cmd_Argv(2)), false);
        return;
    }

    if (Q_strcasecmp(command, "loop") == 0)
    {
        CDAudio_Play((byte)Q_atoi(Cmd_Argv(2)), true);
        return;
    }

    if (Q_strcasecmp(command, "stop") == 0)
    {
        CDAudio_Stop();
        return;
    }

    if (Q_strcasecmp(command, "pause") == 0)
    {
        CDAudio_Pause();
        return;
    }

    if (Q_strcasecmp(command, "resume") == 0)
    {
        CDAudio_Resume();
        return;
    }

    if (Q_strcasecmp(command, "info") == 0)
    {
        Con_Printf("Music is %s\n", enabled ? "enabled" : "disabled");
        if (playing)
            Con_Printf("Currently playing track %u\n", playTrack);
        else if (wasPlaying)
            Con_Printf("Paused on track %u\n", playTrack);
        Con_Printf("Volume is %f\n", bgmvolume.value);
        return;
    }
}

static float old_bgmvolume = -1;

void CDAudio_Update(void)
{
    if (!enabled)
        return;

    // Update volume if changed
    if (bgmvolume.value != old_bgmvolume)
    {
        old_bgmvolume = bgmvolume.value;
        if (bgmvolume.value < 0)
            Cvar_SetValue("bgmvolume", 0);
        else if (bgmvolume.value > 1)
            Cvar_SetValue("bgmvolume", 1);
        Mix_VolumeMusic((int)(bgmvolume.value * MIX_MAX_VOLUME));
    }

    // Check if music stopped (for non-looping tracks)
    if (playing && !Mix_PlayingMusic())
    {
        playing = false;
        if (music)
        {
            Mix_FreeMusic(music);
            music = NULL;
        }
    }
}

int CDAudio_Init(void)
{
    // Register command (bgmvolume cvar is registered by S_Init in snd_dma.c)
    Cmd_AddCommand("cd", CD_f);

    // Try to initialize SDL_mixer
    if (Mix_Init(MIX_INIT_OGG) == 0)
    {
        Con_Printf("CD Audio: OGG support not available\n");
    }

    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 4096) == -1)
    {
        Con_Printf("CD Audio: Could not open audio: %s\n", Mix_GetError());
        enabled = false;
        return 0;  // Return success so game continues
    }

    Con_Printf("CD Audio Initialized (SDL_mixer)\n");
    enabled = true;
    cdValid = true;
    maxTrack = 99;

    return 0;
}

void CDAudio_Shutdown(void)
{
    if (music)
    {
        Mix_HaltMusic();
        Mix_FreeMusic(music);
        music = NULL;
    }

    Mix_CloseAudio();
    Mix_Quit();
}

#else /* !USE_SDL_MIXER */

// Stub implementation when SDL_mixer is not available

void CDAudio_Play(byte track, qboolean looping)
{
}

void CDAudio_Stop(void)
{
}

void CDAudio_Pause(void)
{
}

void CDAudio_Resume(void)
{
}

void CDAudio_Update(void)
{
}

int CDAudio_Init(void)
{
    return 0;
}

void CDAudio_Shutdown(void)
{
}

#endif /* USE_SDL_MIXER */
