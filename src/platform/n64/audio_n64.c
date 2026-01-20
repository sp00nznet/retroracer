/*
 * RetroRacer - Nintendo 64 Audio Implementation
 * Uses libdragon audio subsystem
 */

#include "audio.h"
#include "platform.h"

#ifdef PLATFORM_N64

#include <libdragon.h>

static audio_state_t audio_state;

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

/* Audio mixer channels */
static mixer_ch_t music_channel = -1;
static wav64_t music_wav;
static bool music_loaded = false;

void audio_init(void) {
    memset(&audio_state, 0, sizeof(audio_state_t));

    audio_state.music_enabled = 1;
    audio_state.sfx_enabled = 1;
    audio_state.music_volume = 80;
    audio_state.sfx_volume = 100;
    audio_state.current_track = TRACK_NEON_RUSH;
    audio_state.is_playing = 0;

    /* Initialize audio subsystem */
    audio_init(44100, 4);  /* 44.1kHz, 4 buffers */

    /* Initialize mixer */
    mixer_init(16);  /* 16 channels */

    /* Reserve channel 0 for music */
    music_channel = 0;
}

void audio_shutdown(void) {
    mixer_close();
    audio_close();
}

audio_state_t *audio_get_state(void) {
    return &audio_state;
}

void audio_play_music(music_track_t track) {
    if (!audio_state.music_enabled) return;
    if (track >= MUSIC_TRACK_COUNT) return;

    audio_state.current_track = track;
    audio_state.is_playing = 1;

    /*
     * N64 music playback with libdragon:
     *
     * Load from ROM filesystem:
     * wav64_open(&music_wav, "rom:/music/track01.wav64");
     * wav64_play(&music_wav, music_channel);
     *
     * Or use XM/MOD tracker music:
     * xm64_open(&xm, "rom:/music/track01.xm64");
     * xm64_play(&xm, 0);
     *
     * YM module support also available for chip-tune style
     */

    /* Set volume */
    float vol = (float)audio_state.music_volume / 100.0f;
    mixer_ch_set_vol(music_channel, vol, vol);
}

void audio_stop_music(void) {
    audio_state.is_playing = 0;

    if (music_loaded) {
        mixer_ch_stop(music_channel);
    }
}

void audio_pause_music(void) {
    if (audio_state.is_playing) {
        audio_state.is_playing = 0;
        mixer_ch_stop(music_channel);
    }
}

void audio_resume_music(void) {
    if (audio_state.music_enabled) {
        audio_state.is_playing = 1;
        /* Would resume playback here */
    }
}

void audio_set_music_volume(int volume) {
    if (volume < 0) volume = 0;
    if (volume > 100) volume = 100;
    audio_state.music_volume = volume;

    float vol = (float)volume / 100.0f;
    mixer_ch_set_vol(music_channel, vol, vol);
}

void audio_play_sfx(sound_effect_t sfx) {
    if (!audio_state.sfx_enabled) return;
    if (sfx >= SFX_COUNT) return;

    /*
     * Play sound effect:
     *
     * static wav64_t sfx_samples[SFX_COUNT];
     * wav64_play(&sfx_samples[sfx], next_free_channel);
     *
     * Or use rdp/audio DMA for one-shot samples
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

#endif /* PLATFORM_N64 */
