/*
 * RetroRacer - Sega Genesis Audio Implementation
 * Uses SGDK with YM2612 (FM) and SN76489 (PSG)
 */

#include "audio.h"
#include "platform.h"

#ifdef PLATFORM_GENESIS

#include <genesis.h>

#define MAX_SOUNDS 16

typedef struct {
    const u8 *data;
    u32 length;
    u8 channel;
    u8 playing;
} sound_slot_t;

static sound_slot_t sounds[MAX_SOUNDS];
static int audio_initialized = 0;

void audio_init(void) {
    /* Initialize sound drivers */
    SND_init();

    /* Initialize Z80 for PCM playback */
    Z80_init();

    /* Clear sound slots */
    for (int i = 0; i < MAX_SOUNDS; i++) {
        sounds[i].data = NULL;
        sounds[i].playing = 0;
    }

    audio_initialized = 1;
}

void audio_shutdown(void) {
    SND_stopPlay_PCM();
    audio_initialized = 0;
}

void audio_update(void) {
    /* SGDK handles audio updates automatically via VBlank */
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

    /* In real implementation, load from ROM or filesystem */
    /* For now, just mark as allocated */
    sounds[slot].data = (const u8 *)1;  /* Placeholder */
    sounds[slot].length = 0;
    sounds[slot].playing = 0;
    sounds[slot].channel = SOUND_PCM_CH1;

    (void)filename;
    return slot;
}

void audio_play_sound(sound_t sound, int loop) {
    if (sound < 0 || sound >= MAX_SOUNDS) return;
    if (!sounds[sound].data) return;

    /* Play sound using PCM driver */
    /* SND_startPlay_PCM(sounds[sound].data, sounds[sound].length,
                        SOUND_PCM_CH1, loop); */

    sounds[sound].playing = 1;
    (void)loop;
}

void audio_stop_sound(sound_t sound) {
    if (sound < 0 || sound >= MAX_SOUNDS) return;
    sounds[sound].playing = 0;
}

void audio_set_volume(sound_t sound, float volume) {
    /* Genesis PCM doesn't have per-sound volume easily */
    (void)sound;
    (void)volume;
}

void audio_free_sound(sound_t sound) {
    if (sound < 0 || sound >= MAX_SOUNDS) return;
    sounds[sound].data = NULL;
    sounds[sound].playing = 0;
}

void audio_play_music(const char *filename) {
    /* Load and play XGM/VGM music */
    /* XGM_startPlay(music_data); */
    (void)filename;
}

void audio_stop_music(void) {
    XGM_stopPlay();
}

void audio_set_music_volume(float volume) {
    /* XGM doesn't support volume control directly */
    (void)volume;
}

#endif /* PLATFORM_GENESIS */
