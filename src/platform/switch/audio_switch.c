/*
 * RetroRacer - Nintendo Switch Audio Implementation
 * Uses libnx audren for hardware-accelerated audio
 * Supports 48kHz stereo with multiple voices
 */

#include "audio.h"
#include "platform.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>

#ifdef PLATFORM_SWITCH
#include <switch.h>
#endif

#define MAX_SOUNDS 64
#define MAX_CHANNELS 24
#define SAMPLE_RATE 48000
#define BUFFER_SAMPLES 4096

typedef struct {
    int16_t *samples;
    int sample_count;
    int sample_rate;
    int channels;
    int loop;
    int loaded;
} sound_data_t;

typedef struct {
    int sound_id;
    int playing;
    int paused;
    float volume;
    float pan;
    float pitch;
    int position;
} channel_state_t;

static struct {
    sound_data_t sounds[MAX_SOUNDS];
    channel_state_t channels[MAX_CHANNELS];
    int sound_count;

    float master_volume;
    float music_volume;
    float sfx_volume;

    int engine_channel;
    float engine_rpm;
    float engine_load;

    int music_channel;

#ifdef PLATFORM_SWITCH
    AudioDriver drv;
    int mempool_id;
    void *mempool;
    AudioDriverWaveBuf wavebufs[MAX_CHANNELS];
#endif

    int initialized;
} g_audio;

void audio_init(void) {
    if (g_audio.initialized) return;

    memset(&g_audio, 0, sizeof(g_audio));

#ifdef PLATFORM_SWITCH
    static const AudioRendererConfig arConfig = {
        .output_rate     = AudioRendererOutputRate_48kHz,
        .num_voices      = MAX_CHANNELS,
        .num_effects     = 0,
        .num_sinks       = 1,
        .num_mix_objs    = 1,
        .num_mix_buffers = 2,
    };

    Result rc = audrenInitialize(&arConfig);
    if (R_SUCCEEDED(rc)) {
        audrvCreate(&g_audio.drv, &arConfig, 2);

        /* Allocate memory pool for audio */
        size_t mempool_size = 1024 * 1024;  /* 1MB */
        g_audio.mempool = memalign(0x1000, mempool_size);
        if (g_audio.mempool) {
            g_audio.mempool_id = audrvMemPoolAdd(&g_audio.drv, g_audio.mempool, mempool_size);
            audrvMemPoolAttach(&g_audio.drv, g_audio.mempool_id);
        }

        /* Initialize voices */
        for (int i = 0; i < MAX_CHANNELS; i++) {
            audrvVoiceInit(&g_audio.drv, i, 2, PcmFormat_Int16, SAMPLE_RATE);
            audrvVoiceSetDestinationMix(&g_audio.drv, i, AUDREN_FINAL_MIX_ID);
            audrvVoiceSetMixFactor(&g_audio.drv, i, 1.0f, 0, 0);
            audrvVoiceSetMixFactor(&g_audio.drv, i, 1.0f, 0, 1);
        }

        audrvUpdate(&g_audio.drv);
        audrenStartAudioRenderer();
    }
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

    for (int i = 0; i < MAX_CHANNELS; i++) {
        audio_stop(i);
    }

    for (int i = 0; i < g_audio.sound_count; i++) {
        if (g_audio.sounds[i].samples) {
            free(g_audio.sounds[i].samples);
        }
    }

#ifdef PLATFORM_SWITCH
    audrvClose(&g_audio.drv);
    if (g_audio.mempool) free(g_audio.mempool);
    audrenExit();
#endif

    g_audio.initialized = 0;
}

void audio_update(void) {
    if (!g_audio.initialized) return;

#ifdef PLATFORM_SWITCH
    audrvUpdate(&g_audio.drv);
    audrenWaitFrame();

    for (int i = 0; i < MAX_CHANNELS; i++) {
        if (g_audio.channels[i].playing) {
            if (!audrvVoiceIsPlaying(&g_audio.drv, i)) {
                g_audio.channels[i].playing = 0;
            }
        }
    }
#endif
}

int audio_load_sound(const char *filename) {
    if (g_audio.sound_count >= MAX_SOUNDS) return -1;

    int id = g_audio.sound_count;
    sound_data_t *sound = &g_audio.sounds[id];

    sound->sample_rate = SAMPLE_RATE;
    sound->channels = 2;
    sound->sample_count = SAMPLE_RATE;

#ifdef PLATFORM_SWITCH
    sound->samples = (int16_t *)memalign(0x1000, sound->sample_count * sound->channels * sizeof(int16_t));
#else
    sound->samples = (int16_t *)malloc(sound->sample_count * sound->channels * sizeof(int16_t));
#endif

    if (sound->samples) {
        for (int i = 0; i < sound->sample_count; i++) {
            float t = (float)i / sound->sample_rate;
            float val = sinf(2.0f * 3.14159f * 440.0f * t) * 16000.0f;
            sound->samples[i * 2] = (int16_t)val;
            sound->samples[i * 2 + 1] = (int16_t)val;
        }
#ifdef PLATFORM_SWITCH
        armDCacheFlush(sound->samples, sound->sample_count * sound->channels * sizeof(int16_t));
#endif
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
    ch->pitch = 1.0f;
    ch->playing = 1;
    ch->paused = 0;
    ch->position = 0;

#ifdef PLATFORM_SWITCH
    AudioDriverWaveBuf *wavebuf = &g_audio.wavebufs[channel];
    wavebuf->data_raw = sound->samples;
    wavebuf->size = sound->sample_count * sound->channels * sizeof(int16_t);
    wavebuf->start_sample_offset = 0;
    wavebuf->end_sample_offset = sound->sample_count;

    audrvVoiceSetPitch(&g_audio.drv, channel, 1.0f);

    float vol_l = ch->volume * (1.0f - fmaxf(0, pan));
    float vol_r = ch->volume * (1.0f + fminf(0, pan));
    audrvVoiceSetMixFactor(&g_audio.drv, channel, vol_l, 0, 0);
    audrvVoiceSetMixFactor(&g_audio.drv, channel, vol_r, 0, 1);

    audrvVoiceAddWaveBuf(&g_audio.drv, channel, wavebuf);

    if (loop) {
        wavebuf->state = AudioDriverWaveBufState_Queued;
    }

    audrvVoiceStart(&g_audio.drv, channel);
#endif

    sound->loop = loop;
    return channel;
}

void audio_stop(int channel) {
    if (channel < 0 || channel >= MAX_CHANNELS) return;

#ifdef PLATFORM_SWITCH
    audrvVoiceStop(&g_audio.drv, channel);
#endif

    g_audio.channels[channel].playing = 0;
}

void audio_pause(int channel) {
    if (channel < 0 || channel >= MAX_CHANNELS) return;

#ifdef PLATFORM_SWITCH
    audrvVoiceSetPaused(&g_audio.drv, channel, true);
#endif

    g_audio.channels[channel].paused = 1;
}

void audio_resume(int channel) {
    if (channel < 0 || channel >= MAX_CHANNELS) return;

#ifdef PLATFORM_SWITCH
    audrvVoiceSetPaused(&g_audio.drv, channel, false);
#endif

    g_audio.channels[channel].paused = 0;
}

void audio_set_volume(int channel, float volume) {
    if (channel < 0 || channel >= MAX_CHANNELS) return;

    g_audio.channels[channel].volume = volume;

#ifdef PLATFORM_SWITCH
    float pan = g_audio.channels[channel].pan;
    float vol_l = volume * g_audio.master_volume * (1.0f - fmaxf(0, pan));
    float vol_r = volume * g_audio.master_volume * (1.0f + fminf(0, pan));
    audrvVoiceSetMixFactor(&g_audio.drv, channel, vol_l, 0, 0);
    audrvVoiceSetMixFactor(&g_audio.drv, channel, vol_r, 0, 1);
#endif
}

void audio_set_master_volume(float volume) { g_audio.master_volume = volume; }
void audio_set_music_volume(float volume) { g_audio.music_volume = volume; }
void audio_set_sfx_volume(float volume) { g_audio.sfx_volume = volume; }

void audio_set_engine_params(float rpm, float load) {
    g_audio.engine_rpm = rpm;
    g_audio.engine_load = load;

#ifdef PLATFORM_SWITCH
    if (g_audio.engine_channel >= 0 && g_audio.channels[g_audio.engine_channel].playing) {
        float pitch = 0.5f + (rpm / 8000.0f) * 1.5f;
        audrvVoiceSetPitch(&g_audio.drv, g_audio.engine_channel, pitch);
    }
#endif
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

int audio_play_music(int sound_id) {
    g_audio.music_channel = audio_play(sound_id, g_audio.music_volume, 0.0f, 1);
    return g_audio.music_channel;
}

void audio_stop_music(void) {
    if (g_audio.music_channel >= 0) {
        audio_stop(g_audio.music_channel);
        g_audio.music_channel = -1;
    }
}

int audio_is_playing(int channel) {
    if (channel < 0 || channel >= MAX_CHANNELS) return 0;
    return g_audio.channels[channel].playing;
}
