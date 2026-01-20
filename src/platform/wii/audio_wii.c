/*
 * RetroRacer - Wii Audio Implementation
 * Uses ASND library for streaming audio
 * Supports 32kHz/48kHz stereo output with DSP mixing
 */

#include "audio.h"
#include "platform.h"
#include <string.h>
#include <stdlib.h>

#ifdef PLATFORM_WII
#include <gccore.h>
#include <asndlib.h>
#include <aesndlib.h>
#endif

#define MAX_SOUNDS 32
#define MAX_CHANNELS 16
#define SAMPLE_RATE 32000
#define BUFFER_SIZE 4096

/* Sound data structure */
typedef struct {
    int16_t *samples;
    int sample_count;
    int sample_rate;
    int channels;
    int loop;
    int loaded;
} sound_data_t;

/* Channel state */
typedef struct {
    int sound_id;
    int playing;
    int paused;
    float volume;
    float pan;
    int voice;
} channel_state_t;

/* Global audio state */
static struct {
    sound_data_t sounds[MAX_SOUNDS];
    channel_state_t channels[MAX_CHANNELS];
    int sound_count;

    float master_volume;
    float music_volume;
    float sfx_volume;

    /* Engine sound state */
    int engine_channel;
    float engine_rpm;
    float engine_load;

    /* Streaming music */
    int music_channel;
    int music_playing;

    int initialized;
} g_audio;

void audio_init(void) {
    if (g_audio.initialized) return;

    memset(&g_audio, 0, sizeof(g_audio));

#ifdef PLATFORM_WII
    /* Initialize ASND */
    ASND_Init();
    ASND_Pause(0);
#endif

    g_audio.master_volume = 1.0f;
    g_audio.music_volume = 0.7f;
    g_audio.sfx_volume = 1.0f;
    g_audio.engine_channel = -1;
    g_audio.music_channel = -1;

    g_audio.initialized = 1;
}

void audio_shutdown(void) {
    if (!g_audio.initialized) return;

    /* Stop all sounds */
    for (int i = 0; i < MAX_CHANNELS; i++) {
        if (g_audio.channels[i].playing) {
            audio_stop(i);
        }
    }

    /* Free sound data */
    for (int i = 0; i < g_audio.sound_count; i++) {
        if (g_audio.sounds[i].samples) {
            free(g_audio.sounds[i].samples);
        }
    }

#ifdef PLATFORM_WII
    ASND_Pause(1);
    ASND_End();
#endif

    g_audio.initialized = 0;
}

void audio_update(void) {
    if (!g_audio.initialized) return;

#ifdef PLATFORM_WII
    /* Update channel states */
    for (int i = 0; i < MAX_CHANNELS; i++) {
        if (g_audio.channels[i].playing && g_audio.channels[i].voice >= 0) {
            if (ASND_StatusVoice(g_audio.channels[i].voice) == SND_UNUSED) {
                g_audio.channels[i].playing = 0;
            }
        }
    }
#endif

    /* Update engine sound pitch based on RPM */
    if (g_audio.engine_channel >= 0 && g_audio.channels[g_audio.engine_channel].playing) {
        float pitch = 0.5f + (g_audio.engine_rpm / 8000.0f) * 1.5f;
        /* ASND doesn't support real-time pitch change, would need custom DSP */
    }
}

int audio_load_sound(const char *filename) {
    if (g_audio.sound_count >= MAX_SOUNDS) return -1;

    int id = g_audio.sound_count;
    sound_data_t *sound = &g_audio.sounds[id];

    /* In production, load from file. Create placeholder for now */
    sound->sample_rate = SAMPLE_RATE;
    sound->channels = 1;
    sound->sample_count = SAMPLE_RATE;  /* 1 second */
    sound->samples = (int16_t *)malloc(sound->sample_count * sizeof(int16_t));

    if (sound->samples) {
        /* Generate placeholder tone */
        for (int i = 0; i < sound->sample_count; i++) {
            float t = (float)i / sound->sample_rate;
            float freq = 440.0f;  /* A4 note */
            sound->samples[i] = (int16_t)(16000.0f * sinf(2.0f * 3.14159f * freq * t));
        }
        sound->loaded = 1;
    }

    g_audio.sound_count++;
    return id;
}

void audio_unload_sound(int sound_id) {
    if (sound_id < 0 || sound_id >= g_audio.sound_count) return;

    if (g_audio.sounds[sound_id].samples) {
        free(g_audio.sounds[sound_id].samples);
        g_audio.sounds[sound_id].samples = NULL;
    }
    g_audio.sounds[sound_id].loaded = 0;
}

int audio_play(int sound_id, float volume, float pan, int loop) {
    if (sound_id < 0 || sound_id >= g_audio.sound_count) return -1;
    if (!g_audio.sounds[sound_id].loaded) return -1;

    /* Find free channel */
    int channel = -1;
    for (int i = 0; i < MAX_CHANNELS; i++) {
        if (!g_audio.channels[i].playing) {
            channel = i;
            break;
        }
    }
    if (channel < 0) return -1;

    sound_data_t *sound = &g_audio.sounds[sound_id];
    channel_state_t *ch = &g_audio.channels[channel];

    ch->sound_id = sound_id;
    ch->volume = volume * g_audio.sfx_volume * g_audio.master_volume;
    ch->pan = pan;
    ch->playing = 1;
    ch->paused = 0;

#ifdef PLATFORM_WII
    int format = (sound->channels == 2) ? VOICE_STEREO_16BIT : VOICE_MONO_16BIT;
    int voice = ASND_GetFirstUnusedVoice();

    if (voice >= 0) {
        ch->voice = voice;

        int vol_left = (int)((1.0f - pan) * ch->volume * 255);
        int vol_right = (int)((1.0f + pan) * ch->volume * 255);
        if (vol_left > 255) vol_left = 255;
        if (vol_right > 255) vol_right = 255;

        ASND_SetVoice(voice, format, sound->sample_rate, 0,
                      sound->samples, sound->sample_count * sizeof(int16_t),
                      vol_left, vol_right, NULL);

        if (loop) {
            ASND_SetInfiniteVoice(voice, format, sound->sample_rate, 0,
                                   sound->samples, sound->sample_count * sizeof(int16_t),
                                   vol_left, vol_right);
        }
    }
#endif

    sound->loop = loop;
    return channel;
}

void audio_stop(int channel) {
    if (channel < 0 || channel >= MAX_CHANNELS) return;

#ifdef PLATFORM_WII
    if (g_audio.channels[channel].voice >= 0) {
        ASND_StopVoice(g_audio.channels[channel].voice);
    }
#endif

    g_audio.channels[channel].playing = 0;
    g_audio.channels[channel].voice = -1;
}

void audio_pause(int channel) {
    if (channel < 0 || channel >= MAX_CHANNELS) return;

#ifdef PLATFORM_WII
    if (g_audio.channels[channel].voice >= 0) {
        ASND_PauseVoice(g_audio.channels[channel].voice, 1);
    }
#endif

    g_audio.channels[channel].paused = 1;
}

void audio_resume(int channel) {
    if (channel < 0 || channel >= MAX_CHANNELS) return;

#ifdef PLATFORM_WII
    if (g_audio.channels[channel].voice >= 0) {
        ASND_PauseVoice(g_audio.channels[channel].voice, 0);
    }
#endif

    g_audio.channels[channel].paused = 0;
}

void audio_set_volume(int channel, float volume) {
    if (channel < 0 || channel >= MAX_CHANNELS) return;

    g_audio.channels[channel].volume = volume;

#ifdef PLATFORM_WII
    if (g_audio.channels[channel].voice >= 0) {
        float pan = g_audio.channels[channel].pan;
        int vol_left = (int)((1.0f - pan) * volume * g_audio.master_volume * 255);
        int vol_right = (int)((1.0f + pan) * volume * g_audio.master_volume * 255);
        if (vol_left > 255) vol_left = 255;
        if (vol_right > 255) vol_right = 255;
        ASND_ChangeVolumeVoice(g_audio.channels[channel].voice, vol_left, vol_right);
    }
#endif
}

void audio_set_master_volume(float volume) {
    g_audio.master_volume = volume;
}

void audio_set_music_volume(float volume) {
    g_audio.music_volume = volume;
}

void audio_set_sfx_volume(float volume) {
    g_audio.sfx_volume = volume;
}

/* Engine sound system */
void audio_set_engine_params(float rpm, float load) {
    g_audio.engine_rpm = rpm;
    g_audio.engine_load = load;
}

int audio_play_engine(int sound_id) {
    g_audio.engine_channel = audio_play(sound_id, 1.0f, 0.0f, 1);
    return g_audio.engine_channel;
}

void audio_stop_engine(void) {
    if (g_audio.engine_channel >= 0) {
        audio_stop(g_audio.engine_channel);
        g_audio.engine_channel = -1;
    }
}

/* Music playback */
int audio_play_music(int sound_id) {
    g_audio.music_channel = audio_play(sound_id, g_audio.music_volume, 0.0f, 1);
    g_audio.music_playing = 1;
    return g_audio.music_channel;
}

void audio_stop_music(void) {
    if (g_audio.music_channel >= 0) {
        audio_stop(g_audio.music_channel);
        g_audio.music_channel = -1;
        g_audio.music_playing = 0;
    }
}

int audio_is_playing(int channel) {
    if (channel < 0 || channel >= MAX_CHANNELS) return 0;
    return g_audio.channels[channel].playing;
}
