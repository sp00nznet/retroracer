/*
 * RetroRacer - Game Boy Advance Audio Implementation
 * Uses libgba with GBA sound hardware (4 channels + 2 DMA)
 */

#include "audio.h"
#include "platform.h"

#ifdef PLATFORM_GBA

#include <gba_sound.h>
#include <gba_dma.h>
#include <gba_timers.h>
#include <stdlib.h>
#include <string.h>

/* Sound registers */
#define REG_SOUNDCNT_L  (*(volatile u16*)0x04000080)
#define REG_SOUNDCNT_H  (*(volatile u16*)0x04000082)
#define REG_SOUNDCNT_X  (*(volatile u16*)0x04000084)
#define REG_SOUNDBIAS   (*(volatile u16*)0x04000088)

/* DMA sound registers */
#define REG_FIFO_A      (*(volatile u32*)0x040000A0)
#define REG_FIFO_B      (*(volatile u32*)0x040000A4)

#define MAX_SOUNDS 8

typedef struct {
    s8 *data;
    u32 length;
    u32 position;
    int playing;
    int looping;
} sound_slot_t;

static sound_slot_t sounds[MAX_SOUNDS];
static int audio_initialized = 0;

/* Double buffer for DMA */
static s8 mix_buffer_a[304] __attribute__((aligned(4)));
static s8 mix_buffer_b[304] __attribute__((aligned(4)));
static int current_buffer = 0;

void audio_init(void) {
    /* Enable master sound */
    REG_SOUNDCNT_X = 0x80;

    /* Setup Direct Sound A */
    REG_SOUNDCNT_H = 0x0B0F;  /* DMA A 100% volume, timer 0, enable */

    /* Setup Timer 0 for ~18157 Hz sample rate */
    REG_TM0CNT_L = 65536 - 924;  /* 16.78MHz / 924 = ~18157 Hz */
    REG_TM0CNT_H = 0x0080;       /* Enable timer */

    /* Clear sound slots */
    memset(sounds, 0, sizeof(sounds));
    memset(mix_buffer_a, 0, sizeof(mix_buffer_a));
    memset(mix_buffer_b, 0, sizeof(mix_buffer_b));

    /* Start DMA for sound */
    DMA1COPY(mix_buffer_a, &REG_FIFO_A, DMA_DST_FIXED | DMA_SRC_INC |
             DMA_REPEAT | DMA32 | DMA_SPECIAL | DMA_ENABLE);

    audio_initialized = 1;
}

void audio_shutdown(void) {
    /* Disable DMA and timer */
    REG_DMA1CNT = 0;
    REG_TM0CNT_H = 0;
    REG_SOUNDCNT_X = 0;

    /* Free sound data */
    for (int i = 0; i < MAX_SOUNDS; i++) {
        if (sounds[i].data) {
            free(sounds[i].data);
            sounds[i].data = NULL;
        }
    }

    audio_initialized = 0;
}

void audio_update(void) {
    if (!audio_initialized) return;

    /* Mix sounds into buffer */
    s8 *buffer = current_buffer ? mix_buffer_b : mix_buffer_a;

    memset(buffer, 0, 304);

    for (int i = 0; i < MAX_SOUNDS; i++) {
        if (!sounds[i].playing || !sounds[i].data) continue;

        for (int j = 0; j < 304 && sounds[i].position < sounds[i].length; j++) {
            int sample = buffer[j] + sounds[i].data[sounds[i].position++];
            if (sample > 127) sample = 127;
            if (sample < -128) sample = -128;
            buffer[j] = (s8)sample;
        }

        if (sounds[i].position >= sounds[i].length) {
            if (sounds[i].looping) {
                sounds[i].position = 0;
            } else {
                sounds[i].playing = 0;
            }
        }
    }

    current_buffer ^= 1;
}

sound_t audio_load_sound(const char *filename) {
    int slot = -1;
    for (int i = 0; i < MAX_SOUNDS; i++) {
        if (!sounds[i].data) {
            slot = i;
            break;
        }
    }

    if (slot < 0) return -1;

    /* Allocate placeholder sound */
    sounds[slot].data = (s8 *)malloc(18157);  /* 1 second at ~18kHz */
    sounds[slot].length = 18157;
    sounds[slot].position = 0;
    sounds[slot].playing = 0;
    sounds[slot].looping = 0;

    /* Generate placeholder tone */
    for (u32 i = 0; i < sounds[slot].length; i++) {
        float t = (float)i / 18157.0f;
        sounds[slot].data[i] = (s8)(sinf(t * 440.0f * 6.28318f) * 64);
    }

    (void)filename;
    return slot;
}

void audio_play_sound(sound_t sound, int loop) {
    if (sound < 0 || sound >= MAX_SOUNDS) return;
    if (!sounds[sound].data) return;

    sounds[sound].position = 0;
    sounds[sound].playing = 1;
    sounds[sound].looping = loop;
}

void audio_stop_sound(sound_t sound) {
    if (sound < 0 || sound >= MAX_SOUNDS) return;
    sounds[sound].playing = 0;
}

void audio_set_volume(sound_t sound, float volume) {
    /* GBA DMA sound doesn't have per-channel volume */
    (void)sound;
    (void)volume;
}

void audio_free_sound(sound_t sound) {
    if (sound < 0 || sound >= MAX_SOUNDS) return;
    if (sounds[sound].data) {
        free(sounds[sound].data);
        sounds[sound].data = NULL;
    }
    sounds[sound].playing = 0;
}

void audio_play_music(const char *filename) {
    /* Would use a MOD/S3M player like Maxmod */
    (void)filename;
}

void audio_stop_music(void) {
    /* Stop music playback */
}

void audio_set_music_volume(float volume) {
    (void)volume;
}

#endif /* PLATFORM_GBA */
