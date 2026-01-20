/*
 * RetroRacer - Wii U Audio Implementation
 * Uses AX library for multi-channel audio
 * Supports TV and GamePad audio output separately
 */

#include "audio.h"
#include "platform.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>

#ifdef PLATFORM_WIIU
#include <sndcore2/core.h>
#include <sndcore2/voice.h>
#include <sndcore2/drcvs.h>
#endif

#define MAX_SOUNDS 64
#define MAX_CHANNELS 24
#define SAMPLE_RATE 48000
#define BUFFER_SIZE 4096

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
    int voice_tv;
    int voice_drc;
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
    int music_playing;

    /* Wii U specific */
    int gamepad_audio_enabled;

    int initialized;
} g_audio;

#ifdef PLATFORM_WIIU
static AXVoice *tv_voices[MAX_CHANNELS];
static AXVoice *drc_voices[MAX_CHANNELS];
#endif

void audio_init(void) {
    if (g_audio.initialized) return;

    memset(&g_audio, 0, sizeof(g_audio));

#ifdef PLATFORM_WIIU
    /* Initialize AX audio system */
    AXInitParams init_params;
    init_params.renderer = AX_INIT_RENDERER_48KHZ;
    init_params.pipeline = AX_INIT_PIPELINE_SINGLE;
    AXInitWithParams(&init_params);

    /* Initialize DRC (GamePad) voice system */
    AXInitDRCVS();

    /* Allocate voices */
    for (int i = 0; i < MAX_CHANNELS; i++) {
        tv_voices[i] = AXAcquireVoice(31, NULL, NULL);
        drc_voices[i] = AXAcquireVoice(31, NULL, NULL);
    }
#endif

    g_audio.master_volume = 1.0f;
    g_audio.music_volume = 0.7f;
    g_audio.sfx_volume = 1.0f;
    g_audio.engine_channel = -1;
    g_audio.music_channel = -1;
    g_audio.gamepad_audio_enabled = 1;

    g_audio.initialized = 1;
}

void audio_shutdown(void) {
    if (!g_audio.initialized) return;

    for (int i = 0; i < MAX_CHANNELS; i++) {
        if (g_audio.channels[i].playing) {
            audio_stop(i);
        }
    }

    for (int i = 0; i < g_audio.sound_count; i++) {
        if (g_audio.sounds[i].samples) {
            free(g_audio.sounds[i].samples);
        }
    }

#ifdef PLATFORM_WIIU
    for (int i = 0; i < MAX_CHANNELS; i++) {
        if (tv_voices[i]) AXFreeVoice(tv_voices[i]);
        if (drc_voices[i]) AXFreeVoice(drc_voices[i]);
    }
    AXQuit();
#endif

    g_audio.initialized = 0;
}

void audio_update(void) {
    if (!g_audio.initialized) return;

#ifdef PLATFORM_WIIU
    for (int i = 0; i < MAX_CHANNELS; i++) {
        if (g_audio.channels[i].playing) {
            if (tv_voices[i] && !AXIsVoiceRunning(tv_voices[i])) {
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
    sound->samples = (int16_t *)memalign(64, sound->sample_count * sound->channels * sizeof(int16_t));

    if (sound->samples) {
        for (int i = 0; i < sound->sample_count; i++) {
            float t = (float)i / sound->sample_rate;
            float val = sinf(2.0f * 3.14159f * 440.0f * t) * 16000.0f;
            sound->samples[i * 2] = (int16_t)val;
            sound->samples[i * 2 + 1] = (int16_t)val;
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

#ifdef PLATFORM_WIIU
    if (tv_voices[channel]) {
        AXVoiceOffsets offsets = {0};
        offsets.data = sound->samples;
        offsets.dataType = AX_VOICE_FORMAT_LPCM16;
        offsets.loopingEnabled = loop ? AX_VOICE_LOOP_ENABLED : AX_VOICE_LOOP_DISABLED;
        offsets.loopOffset = 0;
        offsets.endOffset = sound->sample_count * sound->channels;
        offsets.currentOffset = 0;

        AXSetVoiceOffsets(tv_voices[channel], &offsets);

        AXVoiceVeData ve;
        ve.volume = (uint16_t)(ch->volume * 0x8000);
        ve.delta = 0;
        AXSetVoiceVe(tv_voices[channel], &ve);

        float srcratio = (float)sound->sample_rate / SAMPLE_RATE;
        AXSetVoiceSrcRatio(tv_voices[channel], srcratio);
        AXSetVoiceSrcType(tv_voices[channel], AX_VOICE_SRC_TYPE_LINEAR);

        AXSetVoiceState(tv_voices[channel], AX_VOICE_STATE_PLAYING);
    }

    /* Also play on GamePad if enabled */
    if (g_audio.gamepad_audio_enabled && drc_voices[channel]) {
        AXVoiceOffsets offsets = {0};
        offsets.data = sound->samples;
        offsets.dataType = AX_VOICE_FORMAT_LPCM16;
        offsets.loopingEnabled = loop ? AX_VOICE_LOOP_ENABLED : AX_VOICE_LOOP_DISABLED;
        offsets.loopOffset = 0;
        offsets.endOffset = sound->sample_count * sound->channels;
        offsets.currentOffset = 0;

        AXSetVoiceOffsets(drc_voices[channel], &offsets);

        AXVoiceVeData ve;
        ve.volume = (uint16_t)(ch->volume * 0x8000);
        ve.delta = 0;
        AXSetVoiceVe(drc_voices[channel], &ve);

        float srcratio = (float)sound->sample_rate / SAMPLE_RATE;
        AXSetVoiceSrcRatio(drc_voices[channel], srcratio);

        AXSetVoiceState(drc_voices[channel], AX_VOICE_STATE_PLAYING);
    }
#endif

    sound->loop = loop;
    return channel;
}

void audio_stop(int channel) {
    if (channel < 0 || channel >= MAX_CHANNELS) return;

#ifdef PLATFORM_WIIU
    if (tv_voices[channel]) {
        AXSetVoiceState(tv_voices[channel], AX_VOICE_STATE_STOPPED);
    }
    if (drc_voices[channel]) {
        AXSetVoiceState(drc_voices[channel], AX_VOICE_STATE_STOPPED);
    }
#endif

    g_audio.channels[channel].playing = 0;
}

void audio_pause(int channel) {
    if (channel < 0 || channel >= MAX_CHANNELS) return;

#ifdef PLATFORM_WIIU
    if (tv_voices[channel]) {
        AXSetVoiceState(tv_voices[channel], AX_VOICE_STATE_STOPPED);
    }
    if (drc_voices[channel]) {
        AXSetVoiceState(drc_voices[channel], AX_VOICE_STATE_STOPPED);
    }
#endif

    g_audio.channels[channel].paused = 1;
}

void audio_resume(int channel) {
    if (channel < 0 || channel >= MAX_CHANNELS) return;

#ifdef PLATFORM_WIIU
    if (tv_voices[channel]) {
        AXSetVoiceState(tv_voices[channel], AX_VOICE_STATE_PLAYING);
    }
    if (drc_voices[channel]) {
        AXSetVoiceState(drc_voices[channel], AX_VOICE_STATE_PLAYING);
    }
#endif

    g_audio.channels[channel].paused = 0;
}

void audio_set_volume(int channel, float volume) {
    if (channel < 0 || channel >= MAX_CHANNELS) return;
    g_audio.channels[channel].volume = volume;

#ifdef PLATFORM_WIIU
    if (tv_voices[channel]) {
        AXVoiceVeData ve;
        ve.volume = (uint16_t)(volume * g_audio.master_volume * 0x8000);
        ve.delta = 0;
        AXSetVoiceVe(tv_voices[channel], &ve);
    }
#endif
}

void audio_set_master_volume(float volume) { g_audio.master_volume = volume; }
void audio_set_music_volume(float volume) { g_audio.music_volume = volume; }
void audio_set_sfx_volume(float volume) { g_audio.sfx_volume = volume; }

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

/* Wii U specific */
void audio_set_gamepad_audio(int enable) {
    g_audio.gamepad_audio_enabled = enable;
}
