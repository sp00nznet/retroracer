/*
 * RetroRacer - 3DO Audio Implementation
 * Uses 3DO SDK audio folio with DSP mixer
 */

#include "audio.h"
#include "platform.h"

#ifdef PLATFORM_3DO

#include <audio.h>
#include <music.h>
#include <mem.h>
#include <string.h>

#define MAX_SOUNDS 16
#define SAMPLE_RATE 44100

typedef struct {
    Item sample_item;
    Item attachment;
    int playing;
    float volume;
} sound_slot_t;

static sound_slot_t sounds[MAX_SOUNDS];
static Item mixer_instrument;
static Item output_instrument;
static int audio_initialized = 0;

void audio_init(void) {
    /* Open audio folio */
    OpenAudioFolio();

    /* Create mixer */
    mixer_instrument = LoadInstrument("mixer8x2.dsp", 0, 100);

    /* Create output */
    output_instrument = LoadInstrument("directout.dsp", 0, 100);

    /* Connect mixer to output */
    ConnectInstruments(mixer_instrument, "Output", output_instrument, "Input");

    /* Start instruments */
    StartInstrument(output_instrument, NULL);
    StartInstrument(mixer_instrument, NULL);

    /* Clear sound slots */
    memset(sounds, 0, sizeof(sounds));

    audio_initialized = 1;
}

void audio_shutdown(void) {
    for (int i = 0; i < MAX_SOUNDS; i++) {
        if (sounds[i].sample_item) {
            UnloadSample(sounds[i].sample_item);
        }
    }

    StopInstrument(mixer_instrument, NULL);
    StopInstrument(output_instrument, NULL);
    UnloadInstrument(mixer_instrument);
    UnloadInstrument(output_instrument);

    CloseAudioFolio();
    audio_initialized = 0;
}

void audio_update(void) {
    /* 3DO audio is interrupt-driven, no manual update needed */
}

sound_t audio_load_sound(const char *filename) {
    int slot = -1;
    for (int i = 0; i < MAX_SOUNDS; i++) {
        if (!sounds[i].sample_item) {
            slot = i;
            break;
        }
    }

    if (slot < 0) return -1;

    /* Load AIFF sample */
    sounds[slot].sample_item = LoadSample(filename);
    if (sounds[slot].sample_item < 0) {
        sounds[slot].sample_item = 0;
        return -1;
    }

    sounds[slot].volume = 1.0f;
    sounds[slot].playing = 0;

    return slot;
}

void audio_play_sound(sound_t sound, int loop) {
    if (sound < 0 || sound >= MAX_SOUNDS) return;
    if (!sounds[sound].sample_item) return;

    /* Create sample player instrument */
    Item player = LoadInstrument("sampler.dsp", 0, 100);

    /* Attach sample to player */
    sounds[sound].attachment = AttachSample(player, sounds[sound].sample_item, NULL);

    /* Connect to mixer */
    ConnectInstruments(player, "Output", mixer_instrument, "Input0");

    /* Play */
    StartInstrument(player, NULL);

    sounds[sound].playing = 1;
    (void)loop;
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
    if (sounds[sound].sample_item) {
        UnloadSample(sounds[sound].sample_item);
        sounds[sound].sample_item = 0;
    }
}

void audio_play_music(const char *filename) {
    /* 3DO typically uses streaming audio for music */
    (void)filename;
}

void audio_stop_music(void) {
    /* Stop music stream */
}

void audio_set_music_volume(float volume) {
    (void)volume;
}

#endif /* PLATFORM_3DO */
