/*
 * RetroRacer - PlayStation 1 (PSX) Audio Implementation
 * Uses Sony PsyQ SDK libspu / libsnd
 */

#include "audio.h"
#include "platform.h"

#ifdef PLATFORM_PSX

#include <sys/types.h>
#include <libspu.h>
#include <libsnd.h>

static audio_state_t audio_state;

/* SPU voice allocation */
#define MUSIC_VOICE_START 0
#define MUSIC_VOICE_COUNT 8
#define SFX_VOICE_START 8
#define SFX_VOICE_COUNT 16

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

/* SPU transfer buffer */
static char spu_transfer_buffer[SPU_TRANSFER_WAIT];

void audio_init(void) {
    memset(&audio_state, 0, sizeof(audio_state_t));

    audio_state.music_enabled = 1;
    audio_state.sfx_enabled = 1;
    audio_state.music_volume = 80;
    audio_state.sfx_volume = 100;
    audio_state.current_track = TRACK_NEON_RUSH;
    audio_state.is_playing = 0;

    /* Initialize SPU */
    SpuInit();

    /* Set transfer mode */
    SpuSetTransferMode(SPU_TRANSFER_BY_DMA);

    /* Initialize sound library for sequenced audio */
    SsInit();
    SsSetMVol(127, 127);  /* Max master volume */
}

void audio_shutdown(void) {
    SpuQuit();
    SsQuit();
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
     * PSX music would be streamed from CD using XA audio
     * or played using SEQ/VAB files
     *
     * Example XA playback:
     * CdlCB callback;
     * CdlFILE xa_file;
     * CdSearchFile(&xa_file, "\\MUSIC\\TRACK01.XA;1");
     * CdControl(CdlSetloc, &xa_file.pos, 0);
     * CdControl(CdlReadS, 0, 0);
     *
     * Or SEQ/VAB:
     * SsSeqOpen(seq_data, vab_id);
     * SsSeqPlay(seq_handle, SSPLAY_PLAY, 0);
     */

    /* Set music volume */
    int vol = (audio_state.music_volume * 127) / 100;
    SsSetMVol(vol, vol);
}

void audio_stop_music(void) {
    audio_state.is_playing = 0;

    /* Stop any playing sequences */
    /* SsSeqStop(seq_handle); */
}

void audio_pause_music(void) {
    if (audio_state.is_playing) {
        audio_state.is_playing = 0;
        /* SsSeqPause(seq_handle); */
    }
}

void audio_resume_music(void) {
    if (audio_state.music_enabled) {
        audio_state.is_playing = 1;
        /* SsSeqReplay(seq_handle); */
    }
}

void audio_set_music_volume(int volume) {
    if (volume < 0) volume = 0;
    if (volume > 100) volume = 100;
    audio_state.music_volume = volume;

    int vol = (volume * 127) / 100;
    SsSetMVol(vol, vol);
}

void audio_play_sfx(sound_effect_t sfx) {
    if (!audio_state.sfx_enabled) return;
    if (sfx >= SFX_COUNT) return;

    /*
     * Play sound effect from loaded VAB
     *
     * SsVabOpenHead(vab_header, vab_id);
     * SsVabTransBody(vab_body, vab_id);
     * SsVabTransCompleted(SS_WAIT);
     *
     * Then:
     * short voice = SsUtKeyOn(vab_id, program, tone, note, 0, volume, volume);
     */
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

#endif /* PLATFORM_PSX */
