/*
 * RetroRacer - PlayStation 3 Audio Implementation
 * Uses PSL1GHT SDK with libaudio
 */

#include "audio.h"
#include "platform.h"

#ifdef PLATFORM_PS3

#include <psl1ght/lv2.h>
#include <audio/audio.h>
#include <string.h>

static audio_state_t audio_state;

/* Audio configuration */
static audioPortConfig audio_config;
static u32 audio_port = 0;
static int audio_initialized = 0;

/* Track info */
static const struct {
    const char *name;
    const char *artist;
    int bpm;
} track_info[MUSIC_TRACK_COUNT] = {
    {"Neon Rush",        "Velocity 909",    140},
    {"Digital Highway",  "Circuit Breaker", 138},
    {"Cyber Drift",      "Acid Module",     145},
    {"Pulse Driver",     "Rave Machine",    142},
    {"Rave Circuit",     "Techno Force",    150},
    {"Techno Speedway",  "Future Beat",     135},
    {"Synth Velocity",   "Grid Runner",     148},
    {"Electro Burn",     "Neon Pulse",      143},
    {"Trance Racer",     "Dream State",     140},
    {"Future Groove",    "Digital Echo",    137}
};

void audio_init(void) {
    memset(&audio_state, 0, sizeof(audio_state_t));

    audio_state.music_enabled = 1;
    audio_state.sfx_enabled = 1;
    audio_state.music_volume = 80;
    audio_state.sfx_volume = 100;
    audio_state.current_track = TRACK_NEON_RUSH;
    audio_state.is_playing = 0;

    /* Initialize audio subsystem */
    audioInit();

    /* Configure audio port */
    audioPortParam params;
    memset(&params, 0, sizeof(params));
    params.numChannels = AUDIO_PORT_2CH;
    params.numBlocks = AUDIO_BLOCK_8;
    params.attr = 0;
    params.level = 1.0f;

    int ret = audioPortOpen(&params, &audio_port);
    if (ret != 0) {
        printf("Failed to open audio port: %d\n", ret);
        return;
    }

    ret = audioGetPortConfig(audio_port, &audio_config);
    if (ret != 0) {
        printf("Failed to get audio config: %d\n", ret);
        audioPortClose(audio_port);
        return;
    }

    ret = audioPortStart(audio_port);
    if (ret != 0) {
        printf("Failed to start audio port: %d\n", ret);
        audioPortClose(audio_port);
        return;
    }

    audio_initialized = 1;
}

void audio_shutdown(void) {
    if (audio_initialized) {
        audioPortStop(audio_port);
        audioPortClose(audio_port);
        audioQuit();
    }
}

audio_state_t *audio_get_state(void) {
    return &audio_state;
}

void audio_play_music(music_track_t track) {
    if (!audio_state.music_enabled) return;
    if (!audio_initialized) return;
    if (track >= MUSIC_TRACK_COUNT) return;

    audio_state.current_track = track;
    audio_state.is_playing = 1;

    /*
     * PS3 music playback options:
     *
     * 1. Use libaudio for raw PCM streaming
     * 2. Use libat3 for ATRAC3 compressed audio
     * 3. Use libmp3 for MP3 decoding
     *
     * For streaming, fill audio buffers in a thread:
     *
     * float *buffer = (float*)audio_config.audioDataStart;
     * u64 current_block = *(u64*)(u32)audio_config.readIndex;
     * float *block_buffer = buffer + AUDIO_BLOCK_SAMPLES * current_block;
     * // Fill block_buffer with samples
     */
}

void audio_stop_music(void) {
    audio_state.is_playing = 0;
}

void audio_pause_music(void) {
    if (audio_state.is_playing) {
        audio_state.is_playing = 0;
    }
}

void audio_resume_music(void) {
    if (audio_state.music_enabled) {
        audio_state.is_playing = 1;
    }
}

void audio_set_music_volume(int volume) {
    if (volume < 0) volume = 0;
    if (volume > 100) volume = 100;
    audio_state.music_volume = volume;

    /* Adjust audio port level */
    if (audio_initialized) {
        float level = (float)volume / 100.0f;
        audioPortParam params;
        params.level = level;
        /* audioPortSetParam would be used here */
    }
}

void audio_play_sfx(sound_effect_t sfx) {
    if (!audio_state.sfx_enabled) return;
    if (!audio_initialized) return;
    if (sfx >= SFX_COUNT) return;

    /*
     * Play sound effect by mixing into audio buffer
     * or using a separate audio port for SFX
     */
    (void)sfx;
}

void audio_set_sfx_volume(int volume) {
    if (volume < 0) volume = 0;
    if (volume > 100) volume = 100;
    audio_state.sfx_volume = volume;
}

const char *audio_get_track_name(music_track_t track) {
    if (track >= MUSIC_TRACK_COUNT) return "Unknown";
    return track_info[track].name;
}

const char *audio_get_track_artist(music_track_t track) {
    if (track >= MUSIC_TRACK_COUNT) return "Unknown";
    return track_info[track].artist;
}

int audio_get_track_bpm(music_track_t track) {
    if (track >= MUSIC_TRACK_COUNT) return 0;
    return track_info[track].bpm;
}

void audio_next_track(void) {
    int next = (int)audio_state.current_track + 1;
    if (next >= MUSIC_TRACK_COUNT) {
        next = 0;
    }
    audio_state.current_track = (music_track_t)next;

    if (audio_state.is_playing) {
        audio_play_music(audio_state.current_track);
    }
}

void audio_prev_track(void) {
    int prev = (int)audio_state.current_track - 1;
    if (prev < 0) {
        prev = MUSIC_TRACK_COUNT - 1;
    }
    audio_state.current_track = (music_track_t)prev;

    if (audio_state.is_playing) {
        audio_play_music(audio_state.current_track);
    }
}

void audio_toggle_music(void) {
    audio_state.music_enabled = !audio_state.music_enabled;

    if (!audio_state.music_enabled) {
        audio_stop_music();
    }
}

void audio_toggle_sfx(void) {
    audio_state.sfx_enabled = !audio_state.sfx_enabled;
}

#endif /* PLATFORM_PS3 */
