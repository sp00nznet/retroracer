/*
 * RetroRacer - PlayStation 2 Audio Implementation
 * Uses PS2SDK audsrv for SPU2 audio
 */

#include "audio.h"
#include "platform.h"

#ifdef PLATFORM_PS2

#include <tamtypes.h>
#include <kernel.h>
#include <sifrpc.h>
#include <loadfile.h>
#include <audsrv.h>

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

void audio_init(void) {
    memset(&audio_state, 0, sizeof(audio_state_t));

    audio_state.music_enabled = 1;
    audio_state.sfx_enabled = 1;
    audio_state.music_volume = 80;
    audio_state.sfx_volume = 100;
    audio_state.current_track = TRACK_NEON_RUSH;
    audio_state.is_playing = 0;

    /* Initialize audio server */
    int ret;

    /* Load AUDSRV module */
    ret = SifLoadModule("rom0:LIBSD", 0, NULL);
    if (ret < 0) {
        printf("Failed to load LIBSD\n");
        return;
    }

    ret = SifLoadModule("host:audsrv.irx", 0, NULL);
    if (ret < 0) {
        /* Try from disc */
        ret = SifLoadModule("cdrom0:\\AUDSRV.IRX;1", 0, NULL);
    }

    ret = audsrv_init();
    if (ret != 0) {
        printf("Failed to initialize audsrv: %d\n", ret);
        return;
    }

    /* Set audio format */
    struct audsrv_fmt_t format;
    format.bits = 16;
    format.freq = 48000;
    format.channels = 2;

    audsrv_set_format(&format);
    audsrv_set_volume(MAX_VOLUME);
}

void audio_shutdown(void) {
    audsrv_quit();
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
     * PS2 music playback options:
     *
     * 1. ADPCM streaming from disc:
     *    audsrv_play_audio(buffer, size);
     *
     * 2. Sequenced music (MIDI-like):
     *    Use libsd directly for SPU2 sequencing
     *
     * 3. MP3/OGG via additional IRX modules
     */

    int vol = (audio_state.music_volume * MAX_VOLUME) / 100;
    audsrv_set_volume(vol);
}

void audio_stop_music(void) {
    audio_state.is_playing = 0;
    audsrv_stop_audio();
}

void audio_pause_music(void) {
    if (audio_state.is_playing) {
        audio_state.is_playing = 0;
        /* audsrv doesn't have pause - would need to track position */
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

    int vol = (volume * MAX_VOLUME) / 100;
    audsrv_set_volume(vol);
}

void audio_play_sfx(sound_effect_t sfx) {
    if (!audio_state.sfx_enabled) return;
    if (sfx >= SFX_COUNT) return;

    /*
     * Play sound effect using audsrv ADPCM channels
     *
     * audsrv_adpcm_t sample;
     * audsrv_load_adpcm(&sample, buffer, size);
     * int ch = audsrv_ch_play_adpcm(-1, &sample);
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

#endif /* PLATFORM_PS2 */
