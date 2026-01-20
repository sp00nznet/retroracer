/*
 * RetroRacer - Game Gear Audio Implementation
 * Uses SN76489 PSG (same as SMS)
 * 3 square wave + 1 noise channel
 */

#include "audio.h"
#include "platform.h"
#include <string.h>

#ifdef PLATFORM_GAMEGEAR
#include <sms.h>
#endif

/* PSG port address */
#define PSG_PORT 0x7F

/* PSG channel registers */
#define PSG_LATCH_TONE0 0x80
#define PSG_LATCH_TONE1 0xA0
#define PSG_LATCH_TONE2 0xC0
#define PSG_LATCH_NOISE 0xE0
#define PSG_LATCH_VOL0  0x90
#define PSG_LATCH_VOL1  0xB0
#define PSG_LATCH_VOL2  0xD0
#define PSG_LATCH_VOL3  0xF0

/* Sound effect IDs */
#define SFX_ENGINE      0
#define SFX_ACCELERATE  1
#define SFX_BRAKE       2
#define SFX_COLLISION   3
#define SFX_COUNTDOWN   4
#define SFX_FINISH      5
#define SFX_MENU_SELECT 6
#define SFX_MENU_MOVE   7

/* Channel allocation */
#define CH_ENGINE  0
#define CH_SFX     1
#define CH_MUSIC   2
#define CH_NOISE   3

/* Music state */
typedef struct {
    const uint8_t *data;
    int length;
    int position;
    int tempo;
    int tick;
} music_state_t;

static struct {
    int engine_playing;
    int engine_pitch;

    music_state_t music;
    int music_playing;

    float master_volume;
    float music_volume;
    float sfx_volume;

    int initialized;
} g_audio;

#ifdef PLATFORM_GAMEGEAR
/* Write to PSG port */
static void psg_write(uint8_t value) {
    __asm
        ld a, value
        out (0x7F), a
    __endasm;
}

/* Set tone for a channel (0-2) */
static void psg_set_tone(uint8_t channel, uint16_t frequency) {
    /* PSG frequency: f = 3579545 / (32 * n), so n = 3579545 / (32 * f) */
    if (frequency == 0) frequency = 1;
    uint16_t n = 3579545 / (32 * frequency);
    if (n > 1023) n = 1023;

    uint8_t latch = PSG_LATCH_TONE0 + (channel * 0x20);
    psg_write(latch | (n & 0x0F));         /* Low 4 bits + latch */
    psg_write((n >> 4) & 0x3F);            /* High 6 bits */
}

/* Set volume for a channel (0-3), volume is 0-15 (15 = silent) */
static void psg_set_volume(uint8_t channel, uint8_t volume) {
    uint8_t latch = PSG_LATCH_VOL0 + (channel * 0x20);
    psg_write(latch | (volume & 0x0F));
}

/* Set noise channel parameters */
static void psg_set_noise(uint8_t params) {
    psg_write(PSG_LATCH_NOISE | (params & 0x07));
}
#else
static void psg_write(uint8_t value) { (void)value; }
static void psg_set_tone(uint8_t channel, uint16_t frequency) { (void)channel; (void)frequency; }
static void psg_set_volume(uint8_t channel, uint8_t volume) { (void)channel; (void)volume; }
static void psg_set_noise(uint8_t params) { (void)params; }
#endif

void audio_init(void) {
    if (g_audio.initialized) return;

    memset(&g_audio, 0, sizeof(g_audio));

    /* Initialize PSG - silence all channels */
    psg_set_volume(0, 15);
    psg_set_volume(1, 15);
    psg_set_volume(2, 15);
    psg_set_volume(3, 15);

    g_audio.master_volume = 1.0f;
    g_audio.music_volume = 0.7f;
    g_audio.sfx_volume = 1.0f;

    g_audio.initialized = 1;
}

void audio_shutdown(void) {
    if (!g_audio.initialized) return;

    /* Silence all channels */
    psg_set_volume(0, 15);
    psg_set_volume(1, 15);
    psg_set_volume(2, 15);
    psg_set_volume(3, 15);

    g_audio.initialized = 0;
}

void audio_update(void) {
    if (!g_audio.initialized) return;

    /* Update engine sound frequency */
    if (g_audio.engine_playing) {
        int base_freq = 150;
        int freq = base_freq + (g_audio.engine_pitch * 4);
        psg_set_tone(CH_ENGINE, freq);
    }

    /* Update music playback */
    if (g_audio.music_playing && g_audio.music.data) {
        g_audio.music.tick++;
        if (g_audio.music.tick >= g_audio.music.tempo) {
            g_audio.music.tick = 0;

            if (g_audio.music.position < g_audio.music.length) {
                uint8_t note = g_audio.music.data[g_audio.music.position++];

                if (note == 0) {
                    /* Rest */
                    psg_set_volume(CH_MUSIC, 15);
                } else if (note == 0xFF) {
                    /* Loop */
                    g_audio.music.position = 0;
                } else {
                    /* Play note */
                    int freq = 110 * note / 10;
                    psg_set_tone(CH_MUSIC, freq);
                    psg_set_volume(CH_MUSIC, 4);
                }
            }
        }
    }
}

int audio_load_sound(const char *filename) {
    /* Sounds are built-in - return pseudo-ID */
    int id = 0;
    if (filename) {
        while (*filename) {
            id = (id * 31) + *filename++;
        }
    }
    return id % 8;
}

void audio_unload_sound(int sound_id) {
    (void)sound_id;
}

int audio_play(int sound_id, float volume, float pan, int loop) {
    (void)volume;
    (void)pan;
    (void)loop;

    int vol = 2;  /* Default volume (lower = louder on PSG) */

    switch (sound_id % 8) {
        case SFX_ENGINE:
            psg_set_tone(CH_ENGINE, 200);
            psg_set_volume(CH_ENGINE, 6);
            g_audio.engine_playing = 1;
            break;

        case SFX_ACCELERATE:
            psg_set_tone(CH_SFX, 400);
            psg_set_volume(CH_SFX, vol);
            /* Would add sweep effect in a real implementation */
            break;

        case SFX_BRAKE:
            psg_set_noise(0x04);  /* White noise */
            psg_set_volume(CH_NOISE, vol);
            break;

        case SFX_COLLISION:
            psg_set_noise(0x06);  /* Low noise */
            psg_set_volume(CH_NOISE, 0);  /* Loud */
            break;

        case SFX_COUNTDOWN:
            psg_set_tone(CH_SFX, 880);
            psg_set_volume(CH_SFX, vol);
            break;

        case SFX_FINISH:
            psg_set_tone(CH_SFX, 1200);
            psg_set_volume(CH_SFX, vol);
            break;

        case SFX_MENU_SELECT:
            psg_set_tone(CH_SFX, 1000);
            psg_set_volume(CH_SFX, 4);
            break;

        case SFX_MENU_MOVE:
            psg_set_tone(CH_SFX, 600);
            psg_set_volume(CH_SFX, 6);
            break;
    }

    return sound_id;
}

void audio_stop(int channel) {
    (void)channel;

    psg_set_volume(CH_ENGINE, 15);
    psg_set_volume(CH_SFX, 15);
    psg_set_volume(CH_NOISE, 15);

    g_audio.engine_playing = 0;
}

void audio_pause(int channel) {
    audio_stop(channel);
}

void audio_resume(int channel) {
    (void)channel;
}

void audio_set_volume(int channel, float volume) {
    (void)channel;
    (void)volume;
}

void audio_set_master_volume(float volume) {
    g_audio.master_volume = volume;
}

void audio_set_music_volume(float volume) {
    g_audio.music_volume = volume;
}

void audio_set_sfx_volume(float volume) {
    g_audio.sfx_volume = volume;
}

void audio_set_engine_params(float rpm, float load) {
    g_audio.engine_pitch = (int)((rpm / 8000.0f) * 100);
    (void)load;
}

int audio_play_engine(int sound_id) {
    return audio_play(SFX_ENGINE, 1.0f, 0.0f, 1);
}

void audio_stop_engine(void) {
    g_audio.engine_playing = 0;
    psg_set_volume(CH_ENGINE, 15);
}

int audio_play_music(int sound_id) {
    (void)sound_id;
    g_audio.music_playing = 1;
    g_audio.music.tempo = 6;
    g_audio.music.position = 0;
    g_audio.music.tick = 0;
    return 0;
}

void audio_stop_music(void) {
    g_audio.music_playing = 0;
    psg_set_volume(CH_MUSIC, 15);
}

int audio_is_playing(int channel) {
    (void)channel;
    return g_audio.engine_playing || g_audio.music_playing;
}
