/*
 * RetroRacer - Nintendo DS Audio Implementation
 * Uses libnds with DS sound hardware (16 channels)
 */

#include "audio.h"
#include "platform.h"

#ifdef PLATFORM_NDS

#include <nds.h>
#include <maxmod9.h>
#include <stdlib.h>
#include <string.h>

#define MAX_SOUNDS 16

typedef struct {
    mm_sound_effect effect;
    mm_sfxhand handle;
    int loaded;
    int playing;
    float volume;
} sound_slot_t;

static sound_slot_t sounds[MAX_SOUNDS];
static int audio_initialized = 0;

void audio_init(void) {
    /* Initialize Maxmod audio library */
    mmInitDefaultMem((mm_addr)NULL);  /* Soundbank in memory */

    /* Clear sound slots */
    memset(sounds, 0, sizeof(sounds));

    audio_initialized = 1;
}

void audio_shutdown(void) {
    for (int i = 0; i < MAX_SOUNDS; i++) {
        if (sounds[i].playing) {
            mmEffectCancel(sounds[i].handle);
        }
    }
    audio_initialized = 0;
}

void audio_update(void) {
    /* Maxmod handles updates automatically */
}

sound_t audio_load_sound(const char *filename) {
    int slot = -1;
    for (int i = 0; i < MAX_SOUNDS; i++) {
        if (!sounds[i].loaded) {
            slot = i;
            break;
        }
    }

    if (slot < 0) return -1;

    /* Setup sound effect structure */
    sounds[slot].effect.id = slot;  /* Would be actual sample ID */
    sounds[slot].effect.rate = 1024;  /* Normal rate */
    sounds[slot].effect.handle = 0;
    sounds[slot].effect.volume = 255;
    sounds[slot].effect.panning = 128;

    sounds[slot].loaded = 1;
    sounds[slot].playing = 0;
    sounds[slot].volume = 1.0f;

    (void)filename;
    return slot;
}

void audio_play_sound(sound_t sound, int loop) {
    if (sound < 0 || sound >= MAX_SOUNDS) return;
    if (!sounds[sound].loaded) return;

    /* Play using Maxmod */
    sounds[sound].handle = mmEffectEx(&sounds[sound].effect);
    sounds[sound].playing = 1;

    (void)loop;
}

void audio_stop_sound(sound_t sound) {
    if (sound < 0 || sound >= MAX_SOUNDS) return;
    if (sounds[sound].playing) {
        mmEffectCancel(sounds[sound].handle);
        sounds[sound].playing = 0;
    }
}

void audio_set_volume(sound_t sound, float volume) {
    if (sound < 0 || sound >= MAX_SOUNDS) return;
    sounds[sound].volume = volume;
    sounds[sound].effect.volume = (int)(volume * 255);

    if (sounds[sound].playing) {
        mmEffectVolume(sounds[sound].handle, (int)(volume * 255));
    }
}

void audio_free_sound(sound_t sound) {
    if (sound < 0 || sound >= MAX_SOUNDS) return;
    audio_stop_sound(sound);
    sounds[sound].loaded = 0;
}

void audio_play_music(const char *filename) {
    /* Load and play MOD/IT/S3M music */
    /* mmLoad(MOD_MUSIC); */
    /* mmStart(MOD_MUSIC, MM_PLAY_LOOP); */
    (void)filename;
}

void audio_stop_music(void) {
    mmStop();
}

void audio_set_music_volume(float volume) {
    mmSetModuleVolume((int)(volume * 1024));
}

#endif /* PLATFORM_NDS */
