/*
 * RetroRacer - Game Boy Audio Implementation
 * 4 channels: 2 pulse, 1 wave, 1 noise
 * Uses GBDK sound registers
 */

#include "audio.h"
#include "platform.h"
#include <string.h>

#ifdef PLATFORM_GAMEBOY
#include <gb/gb.h>
#endif

/* Game Boy sound registers */
#define NR10 0xFF10  /* Channel 1 sweep */
#define NR11 0xFF11  /* Channel 1 length/duty */
#define NR12 0xFF12  /* Channel 1 envelope */
#define NR13 0xFF13  /* Channel 1 frequency lo */
#define NR14 0xFF14  /* Channel 1 frequency hi */

#define NR21 0xFF16  /* Channel 2 length/duty */
#define NR22 0xFF17  /* Channel 2 envelope */
#define NR23 0xFF18  /* Channel 2 frequency lo */
#define NR24 0xFF19  /* Channel 2 frequency hi */

#define NR30 0xFF1A  /* Channel 3 on/off */
#define NR31 0xFF1B  /* Channel 3 length */
#define NR32 0xFF1C  /* Channel 3 volume */
#define NR33 0xFF1D  /* Channel 3 frequency lo */
#define NR34 0xFF1E  /* Channel 3 frequency hi */

#define NR41 0xFF20  /* Channel 4 length */
#define NR42 0xFF21  /* Channel 4 envelope */
#define NR43 0xFF22  /* Channel 4 polynomial */
#define NR44 0xFF23  /* Channel 4 trigger */

#define NR50 0xFF24  /* Master volume */
#define NR51 0xFF25  /* Sound panning */
#define NR52 0xFF26  /* Sound on/off */

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
#define CH_ENGINE    1
#define CH_SFX       2
#define CH_MUSIC_1   3
#define CH_MUSIC_2   4

/* Music data structure */
typedef struct {
    const uint8_t *data;
    int length;
    int position;
    int tempo;
    int tick;
} music_track_t;

static struct {
    int engine_playing;
    int engine_pitch;
    int sfx_playing;

    music_track_t music;
    int music_playing;

    float master_volume;
    float music_volume;
    float sfx_volume;

    int initialized;
} g_audio;

#ifdef PLATFORM_GAMEBOY
static void write_reg(uint16_t addr, uint8_t val) {
    *((volatile uint8_t *)addr) = val;
}

static void set_channel1_freq(uint16_t freq) {
    write_reg(NR13, freq & 0xFF);
    write_reg(NR14, 0x80 | ((freq >> 8) & 0x07));  /* Trigger + hi bits */
}

static void set_channel2_freq(uint16_t freq) {
    write_reg(NR23, freq & 0xFF);
    write_reg(NR24, 0x80 | ((freq >> 8) & 0x07));
}

static void set_noise(uint8_t params) {
    write_reg(NR42, 0xF0);  /* Max volume, no envelope */
    write_reg(NR43, params);
    write_reg(NR44, 0x80);  /* Trigger */
}
#endif

void audio_init(void) {
    if (g_audio.initialized) return;

    memset(&g_audio, 0, sizeof(g_audio));

#ifdef PLATFORM_GAMEBOY
    /* Enable sound */
    write_reg(NR52, 0x80);  /* Sound on */
    write_reg(NR50, 0x77);  /* Max volume both channels */
    write_reg(NR51, 0xFF);  /* All channels to both outputs */

    /* Initialize channel 1 (pulse) */
    write_reg(NR10, 0x00);  /* No sweep */
    write_reg(NR11, 0x80);  /* 50% duty, length 0 */
    write_reg(NR12, 0xF0);  /* Max volume, no envelope */

    /* Initialize channel 2 (pulse) */
    write_reg(NR21, 0x80);
    write_reg(NR22, 0xF0);

    /* Initialize channel 3 (wave) */
    write_reg(NR30, 0x80);  /* Enable */
    write_reg(NR32, 0x20);  /* Full volume */

    /* Initialize channel 4 (noise) */
    write_reg(NR41, 0x00);
    write_reg(NR42, 0x00);
#endif

    g_audio.master_volume = 1.0f;
    g_audio.music_volume = 0.7f;
    g_audio.sfx_volume = 1.0f;

    g_audio.initialized = 1;
}

void audio_shutdown(void) {
    if (!g_audio.initialized) return;

#ifdef PLATFORM_GAMEBOY
    write_reg(NR52, 0x00);  /* Sound off */
#endif

    g_audio.initialized = 0;
}

void audio_update(void) {
    if (!g_audio.initialized) return;

    /* Update engine sound - modulate frequency based on pitch */
    if (g_audio.engine_playing) {
#ifdef PLATFORM_GAMEBOY
        /* Convert pitch (0-100) to GB frequency (lower value = higher freq) */
        /* GB freq formula: f = 131072/(2048-x) Hz, x = 2048 - 131072/f */
        int base_freq = 200;  /* Hz */
        int freq = base_freq + (g_audio.engine_pitch * 3);  /* Increase with RPM */
        int gb_freq = 2048 - (131072 / freq);
        if (gb_freq < 0) gb_freq = 0;
        if (gb_freq > 2047) gb_freq = 2047;

        set_channel1_freq(gb_freq);
#endif
    }

    /* Update music playback */
    if (g_audio.music_playing && g_audio.music.data) {
        g_audio.music.tick++;
        if (g_audio.music.tick >= g_audio.music.tempo) {
            g_audio.music.tick = 0;

            /* Read next note */
            if (g_audio.music.position < g_audio.music.length) {
                uint8_t note = g_audio.music.data[g_audio.music.position++];

                if (note == 0) {
                    /* Rest - silence channel */
#ifdef PLATFORM_GAMEBOY
                    write_reg(NR22, 0x00);
#endif
                } else if (note == 0xFF) {
                    /* Loop */
                    g_audio.music.position = 0;
                } else {
                    /* Play note on channel 2 */
#ifdef PLATFORM_GAMEBOY
                    write_reg(NR22, 0xF0);
                    set_channel2_freq(note * 8);
#endif
                }
            }
        }
    }
}

/* Simplified sound system for Game Boy */
int audio_load_sound(const char *filename) {
    /* Sound effects are hardcoded for Game Boy */
    /* Return a pseudo-ID based on filename hash */
    int id = 0;
    if (filename) {
        while (*filename) {
            id = (id * 31) + *filename++;
        }
    }
    return id % 8;  /* 8 built-in sound effects */
}

void audio_unload_sound(int sound_id) {
    /* Nothing to unload - sounds are built-in */
    (void)sound_id;
}

int audio_play(int sound_id, float volume, float pan, int loop) {
    (void)volume;
    (void)pan;
    (void)loop;

#ifdef PLATFORM_GAMEBOY
    switch (sound_id % 8) {
        case SFX_ENGINE:
            /* Start engine drone on channel 1 */
            write_reg(NR11, 0x40);  /* 25% duty */
            write_reg(NR12, 0x72);  /* Medium volume, slight decay */
            set_channel1_freq(400);
            g_audio.engine_playing = 1;
            break;

        case SFX_ACCELERATE:
            /* Rising tone */
            write_reg(NR10, 0x17);  /* Sweep up */
            write_reg(NR11, 0x80);
            write_reg(NR12, 0xF3);
            set_channel1_freq(300);
            break;

        case SFX_BRAKE:
            /* Noise burst */
            set_noise(0x71);
            break;

        case SFX_COLLISION:
            /* Low noise crash */
            set_noise(0x77);
            break;

        case SFX_COUNTDOWN:
            /* Beep */
            write_reg(NR21, 0x80);
            write_reg(NR22, 0xF1);
            set_channel2_freq(600);
            break;

        case SFX_FINISH:
            /* Fanfare */
            write_reg(NR21, 0x80);
            write_reg(NR22, 0xF3);
            set_channel2_freq(800);
            break;

        case SFX_MENU_SELECT:
            write_reg(NR21, 0x40);
            write_reg(NR22, 0xF1);
            set_channel2_freq(700);
            break;

        case SFX_MENU_MOVE:
            write_reg(NR21, 0x40);
            write_reg(NR22, 0x71);
            set_channel2_freq(500);
            break;
    }
#endif

    return sound_id;
}

void audio_stop(int channel) {
    (void)channel;

#ifdef PLATFORM_GAMEBOY
    write_reg(NR12, 0x00);  /* Silence channel 1 */
    write_reg(NR22, 0x00);  /* Silence channel 2 */
    write_reg(NR42, 0x00);  /* Silence noise */
#endif

    g_audio.engine_playing = 0;
    g_audio.sfx_playing = 0;
}

void audio_pause(int channel) {
    (void)channel;
    /* No true pause - just stop */
    audio_stop(channel);
}

void audio_resume(int channel) {
    (void)channel;
    /* Would need to restart sound */
}

void audio_set_volume(int channel, float volume) {
    (void)channel;
    (void)volume;
    /* Game Boy has limited volume control */
}

void audio_set_master_volume(float volume) {
    g_audio.master_volume = volume;
#ifdef PLATFORM_GAMEBOY
    uint8_t vol = (uint8_t)(volume * 7);
    write_reg(NR50, (vol << 4) | vol);
#endif
}

void audio_set_music_volume(float volume) { g_audio.music_volume = volume; }
void audio_set_sfx_volume(float volume) { g_audio.sfx_volume = volume; }

void audio_set_engine_params(float rpm, float load) {
    g_audio.engine_pitch = (int)((rpm / 8000.0f) * 100);
    (void)load;
}

int audio_play_engine(int sound_id) {
    return audio_play(SFX_ENGINE, 1.0f, 0.0f, 1);
}

void audio_stop_engine(void) {
    g_audio.engine_playing = 0;
#ifdef PLATFORM_GAMEBOY
    write_reg(NR12, 0x00);
#endif
}

int audio_play_music(int sound_id) {
    (void)sound_id;
    /* Simple music playback */
    g_audio.music_playing = 1;
    g_audio.music.tempo = 8;
    g_audio.music.position = 0;
    g_audio.music.tick = 0;
    return 0;
}

void audio_stop_music(void) {
    g_audio.music_playing = 0;
#ifdef PLATFORM_GAMEBOY
    write_reg(NR22, 0x00);
#endif
}

int audio_is_playing(int channel) {
    (void)channel;
    return g_audio.engine_playing || g_audio.sfx_playing;
}
