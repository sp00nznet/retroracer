/*
 * RetroRacer - Nintendo 3DS Audio Implementation
 * Uses NDSP (DSP audio) for sound playback
 *
 * 3DS Audio Capabilities:
 * - 24 NDSP channels
 * - Stereo output
 * - Hardware mixing and effects
 * - Support for PCM, ADPCM
 * - Surround sound support
 */

#include "audio.h"
#include "platform.h"

#ifdef PLATFORM_3DS

#include <3ds.h>
#include <string.h>
#include <stdlib.h>

/* Audio configuration */
#define SAMPLE_RATE 22050
#define NUM_CHANNELS 24
#define BUFFER_SIZE 4096

/* Channel assignments */
#define CHANNEL_ENGINE_BASE 0
#define CHANNEL_ENGINE_COUNT 4
#define CHANNEL_SFX_BASE 4
#define CHANNEL_SFX_COUNT 16
#define CHANNEL_MUSIC_BASE 20
#define CHANNEL_MUSIC_COUNT 4

/* Sound effect structure */
typedef struct {
    s16 *samples;
    u32 sample_count;
    int loaded;
} sound_t;

/* Music track structure */
typedef struct {
    s16 *samples;
    u32 sample_count;
    u32 position;
    int playing;
    int looping;
    float volume;
} music_t;

/* Audio state */
static int audio_initialized = 0;
static float master_volume = 1.0f;
static float sfx_volume = 1.0f;
static float music_volume = 0.7f;

/* Sound effects */
#define MAX_SOUNDS 32
static sound_t sounds[MAX_SOUNDS];
static int sound_count = 0;

/* Current music */
static music_t current_music;

/* NDSP wave buffers */
static ndspWaveBuf wave_buf[NUM_CHANNELS];

void audio_init(void) {
    if (audio_initialized) return;

    /* Initialize NDSP */
    Result res = ndspInit();
    if (R_FAILED(res)) {
        /* Audio initialization failed - continue without sound */
        return;
    }

    /* Set output mode to stereo */
    ndspSetOutputMode(NDSP_OUTPUT_STEREO);

    /* Configure channels */
    for (int i = 0; i < NUM_CHANNELS; i++) {
        ndspChnReset(i);
        ndspChnSetInterp(i, NDSP_INTERP_LINEAR);
        ndspChnSetRate(i, SAMPLE_RATE);
        ndspChnSetFormat(i, NDSP_FORMAT_STEREO_PCM16);

        /* Initialize wave buffer */
        memset(&wave_buf[i], 0, sizeof(ndspWaveBuf));
    }

    /* Clear sound slots */
    memset(sounds, 0, sizeof(sounds));
    sound_count = 0;

    /* Clear music state */
    memset(&current_music, 0, sizeof(music_t));

    audio_initialized = 1;
}

void audio_shutdown(void) {
    if (!audio_initialized) return;

    /* Stop all channels */
    for (int i = 0; i < NUM_CHANNELS; i++) {
        ndspChnWaveBufClear(i);
    }

    /* Free sound data */
    for (int i = 0; i < MAX_SOUNDS; i++) {
        if (sounds[i].samples) {
            linearFree(sounds[i].samples);
            sounds[i].samples = NULL;
        }
    }

    /* Free music data */
    if (current_music.samples) {
        linearFree(current_music.samples);
        current_music.samples = NULL;
    }

    ndspExit();
    audio_initialized = 0;
}

void audio_update(void) {
    if (!audio_initialized) return;

    /* Update music playback if needed */
    if (current_music.playing && current_music.samples) {
        /* Check if music buffer needs refilling */
        /* For streaming music, would handle buffer swapping here */
    }
}

int audio_load_sound(const char *filename) {
    if (!audio_initialized) return -1;
    if (sound_count >= MAX_SOUNDS) return -1;

    int slot = sound_count++;

    /* For now, create a simple sine wave tone as placeholder */
    /* In production, would load from file */
    sounds[slot].sample_count = SAMPLE_RATE / 4;  /* 0.25 second sound */
    sounds[slot].samples = (s16 *)linearAlloc(sounds[slot].sample_count * sizeof(s16) * 2);

    if (!sounds[slot].samples) {
        sound_count--;
        return -1;
    }

    /* Generate a simple tone */
    float freq = 440.0f + (slot * 100.0f);  /* Different pitch per sound */
    for (u32 i = 0; i < sounds[slot].sample_count; i++) {
        float t = (float)i / SAMPLE_RATE;
        float sample = sinf(2.0f * M_PI * freq * t) * 0.5f;

        /* Fade out */
        float fade = 1.0f - ((float)i / sounds[slot].sample_count);
        sample *= fade;

        s16 s = (s16)(sample * 32767.0f);
        sounds[slot].samples[i * 2] = s;      /* Left */
        sounds[slot].samples[i * 2 + 1] = s;  /* Right */
    }

    /* Flush cache for DMA */
    DSP_FlushDataCache(sounds[slot].samples,
                       sounds[slot].sample_count * sizeof(s16) * 2);

    sounds[slot].loaded = 1;

    (void)filename;  /* Would load from file in production */
    return slot;
}

void audio_play_sound(int sound_id, float volume, float pan) {
    if (!audio_initialized) return;
    if (sound_id < 0 || sound_id >= sound_count) return;
    if (!sounds[sound_id].loaded) return;

    /* Find a free SFX channel */
    int channel = -1;
    for (int i = CHANNEL_SFX_BASE; i < CHANNEL_SFX_BASE + CHANNEL_SFX_COUNT; i++) {
        if (!ndspChnIsPlaying(i)) {
            channel = i;
            break;
        }
    }

    if (channel < 0) {
        /* All channels busy, use first one */
        channel = CHANNEL_SFX_BASE;
        ndspChnWaveBufClear(channel);
    }

    /* Configure channel */
    float mix[12] = {0};
    float vol = volume * sfx_volume * master_volume;

    /* Stereo panning */
    float left = (1.0f - pan) * vol;
    float right = (1.0f + pan) * vol * 0.5f + vol * 0.5f;
    if (pan < 0) {
        left = vol;
        right = (1.0f + pan) * vol;
    }

    mix[0] = left;   /* Front left */
    mix[1] = right;  /* Front right */

    ndspChnSetMix(channel, mix);

    /* Set up wave buffer */
    wave_buf[channel].data_vaddr = sounds[sound_id].samples;
    wave_buf[channel].nsamples = sounds[sound_id].sample_count;
    wave_buf[channel].looping = false;
    wave_buf[channel].status = NDSP_WBUF_FREE;

    /* Play */
    ndspChnWaveBufAdd(channel, &wave_buf[channel]);
}

void audio_stop_sound(int sound_id) {
    if (!audio_initialized) return;

    /* Stop all instances of this sound */
    for (int i = CHANNEL_SFX_BASE; i < CHANNEL_SFX_BASE + CHANNEL_SFX_COUNT; i++) {
        /* Would need to track which sound is on which channel */
        /* For now, this is a simplified implementation */
    }

    (void)sound_id;
}

int audio_load_music(const char *filename) {
    if (!audio_initialized) return -1;

    /* Free existing music */
    if (current_music.samples) {
        linearFree(current_music.samples);
    }

    /* For now, generate a simple looping melody as placeholder */
    current_music.sample_count = SAMPLE_RATE * 4;  /* 4 second loop */
    current_music.samples = (s16 *)linearAlloc(current_music.sample_count * sizeof(s16) * 2);

    if (!current_music.samples) {
        return -1;
    }

    /* Generate a simple melody */
    float notes[] = {261.63f, 293.66f, 329.63f, 349.23f, 392.00f, 440.00f, 493.88f, 523.25f};
    int note_count = 8;
    u32 samples_per_note = current_music.sample_count / note_count;

    for (u32 i = 0; i < current_music.sample_count; i++) {
        int note_idx = (i / samples_per_note) % note_count;
        float freq = notes[note_idx];
        float t = (float)i / SAMPLE_RATE;
        float sample = sinf(2.0f * M_PI * freq * t) * 0.3f;

        /* Add some harmonics */
        sample += sinf(2.0f * M_PI * freq * 2.0f * t) * 0.15f;
        sample += sinf(2.0f * M_PI * freq * 3.0f * t) * 0.1f;

        s16 s = (s16)(sample * 32767.0f);
        current_music.samples[i * 2] = s;
        current_music.samples[i * 2 + 1] = s;
    }

    DSP_FlushDataCache(current_music.samples,
                       current_music.sample_count * sizeof(s16) * 2);

    current_music.position = 0;
    current_music.volume = music_volume;

    (void)filename;
    return 0;
}

void audio_play_music(int music_id, int loop) {
    if (!audio_initialized) return;
    if (!current_music.samples) return;

    current_music.looping = loop;
    current_music.playing = 1;
    current_music.position = 0;

    /* Use music channels */
    int channel = CHANNEL_MUSIC_BASE;

    float mix[12] = {0};
    float vol = music_volume * master_volume;
    mix[0] = vol;  /* Left */
    mix[1] = vol;  /* Right */

    ndspChnSetMix(channel, mix);

    wave_buf[channel].data_vaddr = current_music.samples;
    wave_buf[channel].nsamples = current_music.sample_count;
    wave_buf[channel].looping = loop ? true : false;
    wave_buf[channel].status = NDSP_WBUF_FREE;

    ndspChnWaveBufAdd(channel, &wave_buf[channel]);

    (void)music_id;
}

void audio_stop_music(void) {
    if (!audio_initialized) return;

    current_music.playing = 0;

    for (int i = CHANNEL_MUSIC_BASE; i < CHANNEL_MUSIC_BASE + CHANNEL_MUSIC_COUNT; i++) {
        ndspChnWaveBufClear(i);
    }
}

void audio_pause_music(void) {
    if (!audio_initialized) return;

    for (int i = CHANNEL_MUSIC_BASE; i < CHANNEL_MUSIC_BASE + CHANNEL_MUSIC_COUNT; i++) {
        ndspChnSetPaused(i, true);
    }
}

void audio_resume_music(void) {
    if (!audio_initialized) return;

    for (int i = CHANNEL_MUSIC_BASE; i < CHANNEL_MUSIC_BASE + CHANNEL_MUSIC_COUNT; i++) {
        ndspChnSetPaused(i, false);
    }
}

void audio_set_master_volume(float volume) {
    master_volume = volume;
    if (master_volume < 0.0f) master_volume = 0.0f;
    if (master_volume > 1.0f) master_volume = 1.0f;
}

void audio_set_sfx_volume(float volume) {
    sfx_volume = volume;
    if (sfx_volume < 0.0f) sfx_volume = 0.0f;
    if (sfx_volume > 1.0f) sfx_volume = 1.0f;
}

void audio_set_music_volume(float volume) {
    music_volume = volume;
    if (music_volume < 0.0f) music_volume = 0.0f;
    if (music_volume > 1.0f) music_volume = 1.0f;

    /* Update playing music volume */
    if (current_music.playing) {
        float mix[12] = {0};
        float vol = music_volume * master_volume;
        mix[0] = vol;
        mix[1] = vol;

        for (int i = CHANNEL_MUSIC_BASE; i < CHANNEL_MUSIC_BASE + CHANNEL_MUSIC_COUNT; i++) {
            ndspChnSetMix(i, mix);
        }
    }
}

/* Engine sound implementation */
static int engine_sound_id = -1;
static float engine_pitch = 1.0f;

void audio_play_engine_sound(float rpm_normalized) {
    if (!audio_initialized) return;

    /* Load engine sound if not already loaded */
    if (engine_sound_id < 0) {
        engine_sound_id = audio_load_sound("engine.wav");
    }

    /* Adjust pitch based on RPM */
    engine_pitch = 0.5f + rpm_normalized * 1.5f;

    /* Engine channels use looping */
    int channel = CHANNEL_ENGINE_BASE;

    if (!ndspChnIsPlaying(channel) && engine_sound_id >= 0) {
        float mix[12] = {0};
        float vol = sfx_volume * master_volume * 0.5f;
        mix[0] = vol;
        mix[1] = vol;

        ndspChnSetMix(channel, mix);
        ndspChnSetRate(channel, SAMPLE_RATE * engine_pitch);

        wave_buf[channel].data_vaddr = sounds[engine_sound_id].samples;
        wave_buf[channel].nsamples = sounds[engine_sound_id].sample_count;
        wave_buf[channel].looping = true;
        wave_buf[channel].status = NDSP_WBUF_FREE;

        ndspChnWaveBufAdd(channel, &wave_buf[channel]);
    } else if (ndspChnIsPlaying(channel)) {
        /* Update pitch */
        ndspChnSetRate(channel, SAMPLE_RATE * engine_pitch);
    }
}

void audio_stop_engine_sound(void) {
    if (!audio_initialized) return;

    for (int i = CHANNEL_ENGINE_BASE; i < CHANNEL_ENGINE_BASE + CHANNEL_ENGINE_COUNT; i++) {
        ndspChnWaveBufClear(i);
    }
}

#endif /* PLATFORM_3DS */
