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
// snd_sdl.c -- SDL2 sound driver

#include <SDL.h>
#include "quakedef.h"

static SDL_AudioDeviceID audio_device = 0;
static qboolean snd_inited = false;

// DMA buffer
static unsigned char *dma_buffer = NULL;
static int dma_buffer_size = 0;
static volatile int dma_pos = 0;

/*
==============
SNDDMA_AudioCallback

SDL audio callback - fills audio buffer from our DMA buffer
==============
*/
static void SNDDMA_AudioCallback(void *userdata, Uint8 *stream, int len)
{
    int pos, wrapped;

    if (!snd_inited || !dma_buffer) {
        memset(stream, 0, len);
        return;
    }

    pos = dma_pos;
    wrapped = pos + len;

    if (wrapped > dma_buffer_size) {
        // Handle wrap-around
        int first_part = dma_buffer_size - pos;
        memcpy(stream, dma_buffer + pos, first_part);
        memcpy(stream + first_part, dma_buffer, len - first_part);
        dma_pos = len - first_part;
    } else {
        memcpy(stream, dma_buffer + pos, len);
        dma_pos = wrapped;
        if (dma_pos >= dma_buffer_size)
            dma_pos = 0;
    }
}

/*
==============
SNDDMA_Init

Try to find a sound device to mix for.
Returns false if nothing is found.
==============
*/
qboolean SNDDMA_Init(void)
{
    SDL_AudioSpec desired, obtained;
    int samples;

    if (snd_inited)
        return true;

    if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) {
        Con_Printf("Couldn't init SDL audio: %s\n", SDL_GetError());
        return false;
    }

    // Set up desired audio format
    memset(&desired, 0, sizeof(desired));
    desired.freq = 22050;
    desired.format = AUDIO_S16SYS;
    desired.channels = 2;
    desired.samples = 1024; // Buffer size in samples
    desired.callback = SNDDMA_AudioCallback;
    desired.userdata = NULL;

    audio_device = SDL_OpenAudioDevice(NULL, 0, &desired, &obtained, SDL_AUDIO_ALLOW_FREQUENCY_CHANGE);
    if (audio_device == 0) {
        Con_Printf("Couldn't open audio device: %s\n", SDL_GetError());
        return false;
    }

    // Allocate DMA buffer
    // Use power-of-2 sample count (required for mixer's bitmask operations)
    samples = 32768; // ~0.74 seconds at 22050 stereo
    dma_buffer_size = samples * (SDL_AUDIO_BITSIZE(obtained.format) / 8);

    dma_buffer = (unsigned char *)malloc(dma_buffer_size);
    if (!dma_buffer) {
        Con_Printf("Couldn't allocate DMA buffer\n");
        SDL_CloseAudioDevice(audio_device);
        audio_device = 0;
        return false;
    }
    memset(dma_buffer, 0, dma_buffer_size);

    // Fill in shm structure
    shm = &sn;
    memset((void *)shm, 0, sizeof(*shm));

    shm->splitbuffer = 0;
    shm->samplebits = SDL_AUDIO_BITSIZE(obtained.format);
    shm->speed = obtained.freq;
    shm->channels = obtained.channels;
    shm->samples = samples;
    shm->samplepos = 0;
    shm->soundalive = true;
    shm->gamealive = true;
    shm->submission_chunk = 1;
    shm->buffer = dma_buffer;

    Con_Printf("SDL Audio: %d Hz, %d channels, %d bits\n",
               shm->speed, shm->channels, shm->samplebits);

    snd_inited = true;

    // Start playback
    SDL_PauseAudioDevice(audio_device, 0);

    return true;
}

/*
==============
SNDDMA_GetDMAPos

return the current sample position (in mono samples read)
inside the recirculating dma buffer, so the mixing code will know
how many sample are required to fill it up.
==============
*/
int SNDDMA_GetDMAPos(void)
{
    if (!snd_inited)
        return 0;

    return dma_pos / (shm->samplebits / 8);
}

/*
==============
SNDDMA_Shutdown

Reset the sound device for exiting
==============
*/
void SNDDMA_Shutdown(void)
{
    if (!snd_inited)
        return;

    snd_inited = false;

    if (audio_device) {
        SDL_CloseAudioDevice(audio_device);
        audio_device = 0;
    }

    if (dma_buffer) {
        free(dma_buffer);
        dma_buffer = NULL;
    }
}

/*
==============
SNDDMA_Submit

Send sound to device if buffer isn't really the dma buffer
==============
*/
void SNDDMA_Submit(void)
{
    // Not needed for SDL callback-based audio
}

/*
==============
S_BlockSound
==============
*/
void S_BlockSound(void)
{
    if (snd_inited && audio_device)
        SDL_PauseAudioDevice(audio_device, 1);
}

/*
==============
S_UnblockSound
==============
*/
void S_UnblockSound(void)
{
    if (snd_inited && audio_device)
        SDL_PauseAudioDevice(audio_device, 0);
}
