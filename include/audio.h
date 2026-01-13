/*
 * RetroRacer - Audio System
 * Music and sound effects for Dreamcast
 */

#ifndef AUDIO_H
#define AUDIO_H

#include <stdint.h>

/* Number of available music tracks */
#define MUSIC_TRACK_COUNT 10

/* Music track IDs */
typedef enum {
    TRACK_NEON_RUSH,
    TRACK_DIGITAL_HIGHWAY,
    TRACK_CYBER_DRIFT,
    TRACK_PULSE_DRIVER,
    TRACK_RAVE_CIRCUIT,
    TRACK_TECHNO_SPEEDWAY,
    TRACK_SYNTH_VELOCITY,
    TRACK_ELECTRO_BURN,
    TRACK_TRANCE_RACER,
    TRACK_FUTURE_GROOVE
} music_track_t;

/* Sound effect IDs */
typedef enum {
    SFX_ENGINE_LOOP,
    SFX_ENGINE_REV,
    SFX_SKID,
    SFX_COLLISION,
    SFX_CHECKPOINT,
    SFX_LAP_COMPLETE,
    SFX_RACE_START,
    SFX_COUNTDOWN_BEEP,
    SFX_MENU_SELECT,
    SFX_MENU_MOVE,
    SFX_COUNT
} sound_effect_t;

/* Audio state */
typedef struct {
    int music_enabled;
    int sfx_enabled;
    int music_volume;      /* 0-100 */
    int sfx_volume;        /* 0-100 */
    music_track_t current_track;
    int is_playing;
} audio_state_t;

/* Initialize audio system */
void audio_init(void);

/* Shutdown audio system */
void audio_shutdown(void);

/* Get audio state */
audio_state_t *audio_get_state(void);

/* Music control */
void audio_play_music(music_track_t track);
void audio_stop_music(void);
void audio_pause_music(void);
void audio_resume_music(void);
void audio_set_music_volume(int volume);

/* Sound effects */
void audio_play_sfx(sound_effect_t sfx);
void audio_set_sfx_volume(int volume);

/* Track information */
const char *audio_get_track_name(music_track_t track);
const char *audio_get_track_artist(music_track_t track);
int audio_get_track_bpm(music_track_t track);

/* Next/previous track */
void audio_next_track(void);
void audio_prev_track(void);

/* Toggle functions */
void audio_toggle_music(void);
void audio_toggle_sfx(void);

#endif /* AUDIO_H */
