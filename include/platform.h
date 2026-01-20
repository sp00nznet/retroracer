/*
 * RetroRacer - Platform Abstraction Layer
 * Supports: Dreamcast, PS1, PS2, PS3, Xbox, N64, SNES
 */

#ifndef PLATFORM_H
#define PLATFORM_H

#include <stdint.h>

/* Platform detection */
#if defined(DREAMCAST) || defined(_arch_dreamcast)
    #define PLATFORM_DREAMCAST 1
    #define PLATFORM_NAME "Dreamcast"
#elif defined(PSX) || defined(PLAYSTATION1)
    #define PLATFORM_PSX 1
    #define PLATFORM_NAME "PlayStation"
#elif defined(PS2) || defined(PLAYSTATION2)
    #define PLATFORM_PS2 1
    #define PLATFORM_NAME "PlayStation 2"
#elif defined(PS3) || defined(PLAYSTATION3)
    #define PLATFORM_PS3 1
    #define PLATFORM_NAME "PlayStation 3"
#elif defined(XBOX) || defined(_XBOX)
    #define PLATFORM_XBOX 1
    #define PLATFORM_NAME "Xbox"
#elif defined(N64) || defined(PLATFORM_N64)
    #define PLATFORM_N64 1
    #define PLATFORM_NAME "Nintendo 64"
#elif defined(SNES) || defined(PLATFORM_SNES)
    #define PLATFORM_SNES 1
    #define PLATFORM_NAME "Super Nintendo"
#else
    #define PLATFORM_NATIVE 1
    #define PLATFORM_NAME "Native"
#endif

/* Screen resolution - platform specific defaults */
#if defined(PLATFORM_PSX)
    #define SCREEN_WIDTH  320
    #define SCREEN_HEIGHT 240
    #define SCREEN_BPP    16
#elif defined(PLATFORM_PS2)
    #define SCREEN_WIDTH  640
    #define SCREEN_HEIGHT 448
    #define SCREEN_BPP    32
#elif defined(PLATFORM_PS3)
    #define SCREEN_WIDTH  1280
    #define SCREEN_HEIGHT 720
    #define SCREEN_BPP    32
#elif defined(PLATFORM_XBOX)
    #define SCREEN_WIDTH  640
    #define SCREEN_HEIGHT 480
    #define SCREEN_BPP    32
#elif defined(PLATFORM_DREAMCAST)
    #define SCREEN_WIDTH  640
    #define SCREEN_HEIGHT 480
    #define SCREEN_BPP    16
#elif defined(PLATFORM_N64)
    #define SCREEN_WIDTH  320
    #define SCREEN_HEIGHT 240
    #define SCREEN_BPP    16
#elif defined(PLATFORM_SNES)
    #define SCREEN_WIDTH  256
    #define SCREEN_HEIGHT 224
    #define SCREEN_BPP    15
#else
    #define SCREEN_WIDTH  640
    #define SCREEN_HEIGHT 480
    #define SCREEN_BPP    32
#endif

/* Platform capabilities */
typedef struct {
    const char *name;
    int screen_width;
    int screen_height;
    int color_depth;
    int max_polygons_per_frame;
    int has_hardware_transform;
    int has_texture_compression;
    int max_texture_size;
    int audio_channels;
    int audio_sample_rate;
} platform_caps_t;

/* Platform initialization */
void platform_init(void);
void platform_shutdown(void);
const platform_caps_t *platform_get_caps(void);

/* Timing */
uint64_t platform_get_time_us(void);
void platform_sleep_ms(int ms);

/* Memory management */
void *platform_alloc(size_t size);
void *platform_alloc_aligned(size_t size, size_t alignment);
void platform_free(void *ptr);

/* Debug output */
void platform_debug_print(const char *fmt, ...);

#endif /* PLATFORM_H */
