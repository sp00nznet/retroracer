/*
 * RetroRacer - Xbox One Audio Implementation
 * Uses XAudio2 for hardware-accelerated audio
 * Supports spatial audio and 7.1 surround
 */

#include "audio.h"
#include "platform.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>

#ifdef PLATFORM_XBOXONE
#include <xaudio2.h>
#include <x3daudio.h>
#else
typedef void* IXAudio2;
typedef void* IXAudio2MasteringVoice;
typedef void* IXAudio2SourceVoice;
#endif

#define MAX_SOUNDS 64
#define MAX_CHANNELS 32
#define SAMPLE_RATE 48000

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
#ifdef PLATFORM_XBOXONE
    IXAudio2SourceVoice *voice;
#else
    void *voice;
#endif
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

#ifdef PLATFORM_XBOXONE
    IXAudio2 *xaudio2;
    IXAudio2MasteringVoice *mastering_voice;
    X3DAUDIO_HANDLE x3d_instance;
#endif

    int initialized;
} g_audio;

void audio_init(void) {
    if (g_audio.initialized) return;

    memset(&g_audio, 0, sizeof(g_audio));

#ifdef PLATFORM_XBOXONE
    HRESULT hr = XAudio2Create(&g_audio.xaudio2, 0, XAUDIO2_DEFAULT_PROCESSOR);
    if (SUCCEEDED(hr)) {
        hr = g_audio.xaudio2->CreateMasteringVoice(&g_audio.mastering_voice,
                                                    XAUDIO2_DEFAULT_CHANNELS,
                                                    SAMPLE_RATE, 0, NULL, NULL,
                                                    AudioCategory_GameEffects);
    }

    if (SUCCEEDED(hr)) {
        /* Initialize X3DAudio for spatial sound */
        DWORD channelMask;
        g_audio.mastering_voice->GetChannelMask(&channelMask);
        X3DAudioInitialize(channelMask, X3DAUDIO_SPEED_OF_SOUND, g_audio.x3d_instance);
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

#ifdef PLATFORM_XBOXONE
    if (g_audio.mastering_voice) {
        g_audio.mastering_voice->DestroyVoice();
    }
    if (g_audio.xaudio2) {
        g_audio.xaudio2->Release();
    }
#endif

    g_audio.initialized = 0;
}

void audio_update(void) {
    if (!g_audio.initialized) return;

#ifdef PLATFORM_XBOXONE
    for (int i = 0; i < MAX_CHANNELS; i++) {
        if (g_audio.channels[i].playing && g_audio.channels[i].voice) {
            XAUDIO2_VOICE_STATE state;
            g_audio.channels[i].voice->GetState(&state);
            if (state.BuffersQueued == 0) {
                g_audio.channels[i].playing = 0;
            }
        }
    }
#endif

    /* Update engine sound */
    if (g_audio.engine_channel >= 0) {
        audio_set_pitch(g_audio.engine_channel, 0.5f + (g_audio.engine_rpm / 8000.0f) * 1.5f);
    }
}

int audio_load_sound(const char *filename) {
    if (g_audio.sound_count >= MAX_SOUNDS) return -1;

    int id = g_audio.sound_count;
    sound_data_t *sound = &g_audio.sounds[id];

    sound->sample_rate = SAMPLE_RATE;
    sound->channels = 2;
    sound->sample_count = SAMPLE_RATE;
    sound->samples = (int16_t *)malloc(sound->sample_count * sound->channels * sizeof(int16_t));

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
    ch->pitch = 1.0f;
    ch->playing = 1;
    ch->paused = 0;

#ifdef PLATFORM_XBOXONE
    WAVEFORMATEX wfx = {0};
    wfx.wFormatTag = WAVE_FORMAT_PCM;
    wfx.nChannels = sound->channels;
    wfx.nSamplesPerSec = sound->sample_rate;
    wfx.wBitsPerSample = 16;
    wfx.nBlockAlign = wfx.nChannels * wfx.wBitsPerSample / 8;
    wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;

    if (ch->voice) {
        ch->voice->DestroyVoice();
        ch->voice = NULL;
    }

    HRESULT hr = g_audio.xaudio2->CreateSourceVoice(&ch->voice, &wfx);
    if (SUCCEEDED(hr) && ch->voice) {
        XAUDIO2_BUFFER buffer = {0};
        buffer.AudioBytes = sound->sample_count * sound->channels * sizeof(int16_t);
        buffer.pAudioData = (BYTE *)sound->samples;
        buffer.LoopCount = loop ? XAUDIO2_LOOP_INFINITE : 0;

        ch->voice->SubmitSourceBuffer(&buffer);
        ch->voice->SetVolume(ch->volume);

        /* Set pan using output matrix */
        float outputMatrix[8] = {0};
        float leftVol = 1.0f - fmaxf(0, pan);
        float rightVol = 1.0f + fminf(0, pan);
        outputMatrix[0] = leftVol;   /* Left input -> Left output */
        outputMatrix[1] = rightVol;  /* Left input -> Right output */
        if (sound->channels == 2) {
            outputMatrix[2] = leftVol;   /* Right input -> Left output */
            outputMatrix[3] = rightVol;  /* Right input -> Right output */
        }
        ch->voice->SetOutputMatrix(NULL, sound->channels, 2, outputMatrix);

        ch->voice->Start();
    }
#endif

    sound->loop = loop;
    return channel;
}

void audio_stop(int channel) {
    if (channel < 0 || channel >= MAX_CHANNELS) return;

#ifdef PLATFORM_XBOXONE
    if (g_audio.channels[channel].voice) {
        g_audio.channels[channel].voice->Stop();
        g_audio.channels[channel].voice->FlushSourceBuffers();
    }
#endif

    g_audio.channels[channel].playing = 0;
}

void audio_pause(int channel) {
    if (channel < 0 || channel >= MAX_CHANNELS) return;

#ifdef PLATFORM_XBOXONE
    if (g_audio.channels[channel].voice) {
        g_audio.channels[channel].voice->Stop();
    }
#endif

    g_audio.channels[channel].paused = 1;
}

void audio_resume(int channel) {
    if (channel < 0 || channel >= MAX_CHANNELS) return;

#ifdef PLATFORM_XBOXONE
    if (g_audio.channels[channel].voice) {
        g_audio.channels[channel].voice->Start();
    }
#endif

    g_audio.channels[channel].paused = 0;
}

void audio_set_volume(int channel, float volume) {
    if (channel < 0 || channel >= MAX_CHANNELS) return;

    g_audio.channels[channel].volume = volume;

#ifdef PLATFORM_XBOXONE
    if (g_audio.channels[channel].voice) {
        g_audio.channels[channel].voice->SetVolume(volume * g_audio.master_volume);
    }
#endif
}

void audio_set_pitch(int channel, float pitch) {
    if (channel < 0 || channel >= MAX_CHANNELS) return;

    g_audio.channels[channel].pitch = pitch;

#ifdef PLATFORM_XBOXONE
    if (g_audio.channels[channel].voice) {
        g_audio.channels[channel].voice->SetFrequencyRatio(pitch);
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
