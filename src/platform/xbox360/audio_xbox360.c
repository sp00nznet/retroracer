/*
 * RetroRacer - Xbox 360 Audio Implementation
 * Uses libxenon audio system with XMA support
 */

#include "audio.h"
#include "platform.h"

#ifdef PLATFORM_XBOX360

#include <xenon_sound/sound.h>
#include <stdlib.h>
#include <string.h>

#define MAX_SOUNDS 32
#define SAMPLE_RATE 48000
#define AUDIO_BUFFER_SIZE 4096

static int audio_initialized = 0;

/* Sound slots */
typedef struct {
    int16_t *data;
    uint32_t length;
    uint32_t position;
    int playing;
    int looping;
    float volume;
} sound_slot_t;

static sound_slot_t sounds[MAX_SOUNDS];

/* Audio mixing buffer */
static int16_t mix_buffer[AUDIO_BUFFER_SIZE];

void audio_init(void) {
    /* Initialize Xenon audio */
    xenon_sound_init();

    /* Clear sound slots */
    memset(sounds, 0, sizeof(sounds));

    audio_initialized = 1;
}

void audio_shutdown(void) {
    /* Free all sounds */
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

    /* Mix all playing sounds */
    memset(mix_buffer, 0, sizeof(mix_buffer));

    for (int i = 0; i < MAX_SOUNDS; i++) {
        if (!sounds[i].playing || !sounds[i].data) continue;

        for (int j = 0; j < AUDIO_BUFFER_SIZE && sounds[i].position < sounds[i].length; j++) {
            int32_t sample = mix_buffer[j];
            sample += (int32_t)(sounds[i].data[sounds[i].position++] * sounds[i].volume);

            /* Clamp */
            if (sample > 32767) sample = 32767;
            if (sample < -32768) sample = -32768;
            mix_buffer[j] = (int16_t)sample;
        }

        /* Handle loop or stop */
        if (sounds[i].position >= sounds[i].length) {
            if (sounds[i].looping) {
                sounds[i].position = 0;
            } else {
                sounds[i].playing = 0;
            }
        }
    }

    /* Submit mixed buffer to hardware */
    xenon_sound_submit(mix_buffer, AUDIO_BUFFER_SIZE * sizeof(int16_t));
}

sound_t audio_load_sound(const char *filename) {
    /* Find free slot */
    int slot = -1;
    for (int i = 0; i < MAX_SOUNDS; i++) {
        if (!sounds[i].data) {
            slot = i;
            break;
        }
    }

    if (slot < 0) return -1;

    /* Load sound file (simplified - would need actual file loading) */
    sounds[slot].data = (int16_t *)malloc(SAMPLE_RATE * 2);  /* 1 second buffer */
    sounds[slot].length = SAMPLE_RATE;
    sounds[slot].position = 0;
    sounds[slot].playing = 0;
    sounds[slot].looping = 0;
    sounds[slot].volume = 1.0f;

    /* Generate placeholder tone */
    for (uint32_t i = 0; i < sounds[slot].length; i++) {
        float t = (float)i / SAMPLE_RATE;
        sounds[slot].data[i] = (int16_t)(sinf(t * 440.0f * 6.28318f) * 8000);
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
    if (sound < 0 || sound >= MAX_SOUNDS) return;
    sounds[sound].volume = volume;
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
    /* Xbox 360 would use XMA for music streaming */
    (void)filename;
}

void audio_stop_music(void) {
    /* Stop music playback */
}

void audio_set_music_volume(float volume) {
    (void)volume;
}

#endif /* PLATFORM_XBOX360 */
