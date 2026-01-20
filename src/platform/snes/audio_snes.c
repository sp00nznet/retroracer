/*
 * RetroRacer - Super Nintendo (SNES) Audio Implementation
 * Uses SPC700 APU with BRR samples
 */

#include "audio.h"
#include "platform.h"

#ifdef PLATFORM_SNES

#include <snes.h>

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

/* SPC music data pointers */
static uint8_t *music_spc_data = NULL;
static uint16_t music_spc_size = 0;

void audio_init(void) {
    memset(&audio_state, 0, sizeof(audio_state_t));

    audio_state.music_enabled = 1;
    audio_state.sfx_enabled = 1;
    audio_state.music_volume = 80;
    audio_state.sfx_volume = 100;
    audio_state.current_track = TRACK_NEON_RUSH;
    audio_state.is_playing = 0;

    /* Initialize SPC700 audio
     * PVSnesLib uses spcLoad() and spcPlay() for SPC music
     */
}

void audio_shutdown(void) {
    spcStop();
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
     * SNES music is typically:
     * 1. SPC files (complete SPC700 state)
     * 2. Custom driver + music data
     *
     * Example with PVSnesLib:
     * spcLoad(track_spc_data);
     * spcPlay(0);  // Start playing
     *
     * Or use the built-in music system:
     * spcSetBank(&music_bank);
     * spcPlay(track);
     */

    /* Set volume (0-127 for SPC700) */
    uint8_t vol = (audio_state.music_volume * 127) / 100;
    spcSetModuleVolume(vol);
}

void audio_stop_music(void) {
    audio_state.is_playing = 0;
    spcStop();
}

void audio_pause_music(void) {
    if (audio_state.is_playing) {
        audio_state.is_playing = 0;
        /* SPC700 doesn't have pause - would need to track position */
        spcStop();
    }
}

void audio_resume_music(void) {
    if (audio_state.music_enabled) {
        audio_state.is_playing = 1;
        spcPlay(0);
    }
}

void audio_set_music_volume(int volume) {
    if (volume < 0) volume = 0;
    if (volume > 100) volume = 100;
    audio_state.music_volume = volume;

    uint8_t vol = (volume * 127) / 100;
    spcSetModuleVolume(vol);
}

void audio_play_sfx(sound_effect_t sfx) {
    if (!audio_state.sfx_enabled) return;
    if (sfx >= SFX_COUNT) return;

    /*
     * Play BRR sample as sound effect
     * Uses one of the 8 SPC700 voices
     *
     * spcPlaySound(channel, &sfx_brr_data[sfx]);
     *
     * Or use dedicated SFX voices:
     * spcEffect(sfx, vol_left, vol_right, pitch);
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

#endif /* PLATFORM_SNES */
