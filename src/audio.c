/*
 * RetroRacer - Audio System Implementation
 * 90s Techno vibes for Dreamcast
 */

#include "audio.h"
#include <string.h>

#ifdef DREAMCAST
#include <kos.h>
#include <dc/sound/sound.h>
#include <dc/sound/sfxmgr.h>
#endif

static audio_state_t audio_state;

/* Track metadata - 90s techno classics (public domain placeholders) */
typedef struct {
    const char *name;
    const char *artist;
    int bpm;
} track_info_t;

static const track_info_t track_info[MUSIC_TRACK_COUNT] = {
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

#ifdef DREAMCAST
    snd_init();
#endif
}

void audio_shutdown(void) {
#ifdef DREAMCAST
    snd_shutdown();
#endif
}

audio_state_t *audio_get_state(void) {
    return &audio_state;
}

void audio_play_music(music_track_t track) {
    if (!audio_state.music_enabled) return;
    if (track >= MUSIC_TRACK_COUNT) return;

    audio_state.current_track = track;
    audio_state.is_playing = 1;

#ifdef DREAMCAST
    /*
     * In a full implementation, this would load and stream
     * the audio file from CD-ROM or ramdisk.
     * Format: /cd/music/track_XX.adpcm
     *
     * For now, the infrastructure is in place.
     * Add actual audio files and streaming code as needed.
     */
#endif
}

void audio_stop_music(void) {
    audio_state.is_playing = 0;

#ifdef DREAMCAST
    /* Stop streaming audio */
#endif
}

void audio_pause_music(void) {
    if (audio_state.is_playing) {
        audio_state.is_playing = 0;
#ifdef DREAMCAST
        /* Pause stream */
#endif
    }
}

void audio_resume_music(void) {
    if (audio_state.music_enabled) {
        audio_state.is_playing = 1;
#ifdef DREAMCAST
        /* Resume stream */
#endif
    }
}

void audio_set_music_volume(int volume) {
    if (volume < 0) volume = 0;
    if (volume > 100) volume = 100;
    audio_state.music_volume = volume;

#ifdef DREAMCAST
    /* Adjust AICA channel volume */
#endif
}

void audio_play_sfx(sound_effect_t sfx) {
    if (!audio_state.sfx_enabled) return;
    if (sfx >= SFX_COUNT) return;

#ifdef DREAMCAST
    /*
     * Play sound effect from loaded samples
     * sfx_play(sfx_handles[sfx], audio_state.sfx_volume);
     */
#endif
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
