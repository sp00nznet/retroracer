/*
 * RetroRacer - Platform Abstraction Layer
 * Unified API for multi-console support
 *
 * Supported Platforms:
 * - DREAMCAST  - SEGA Dreamcast (PowerVR)
 * - PSX        - Sony PlayStation 1 (PSX GPU)
 * - PS2        - Sony PlayStation 2 (GS)
 * - PS3        - Sony PlayStation 3 (RSX)
 * - XBOX       - Microsoft Xbox (NV2A/DirectX 8)
 * - XBOX360    - Microsoft Xbox 360 (Xenos/DirectX 9)
 */

#ifndef PLATFORM_H
#define PLATFORM_H

#include <stdint.h>
#include <stddef.h>

/*============================================================================
 * Platform Detection
 *============================================================================*/

/* Auto-detect platform from compiler defines */
#if defined(_XBOX360) || defined(XBOX360)
    #define PLATFORM_XBOX360    1
    #define PLATFORM_NAME       "Xbox 360"
    #define PLATFORM_ENDIAN_BIG 1
#elif defined(_XBOX) || defined(XBOX)
    #define PLATFORM_XBOX       1
    #define PLATFORM_NAME       "Xbox"
    #define PLATFORM_ENDIAN_LITTLE 1
#elif defined(__CELLOS_LV2__) || defined(PS3)
    #define PLATFORM_PS3        1
    #define PLATFORM_NAME       "PlayStation 3"
    #define PLATFORM_ENDIAN_BIG 1
#elif defined(PS2) || defined(_EE) || defined(__PS2__)
    #define PLATFORM_PS2        1
    #define PLATFORM_NAME       "PlayStation 2"
    #define PLATFORM_ENDIAN_LITTLE 1
#elif defined(PSX) || defined(__PSX__) || defined(_PSX)
    #define PLATFORM_PSX        1
    #define PLATFORM_NAME       "PlayStation"
    #define PLATFORM_ENDIAN_LITTLE 1
#elif defined(DREAMCAST) || defined(_arch_dreamcast)
    #define PLATFORM_DREAMCAST  1
    #define PLATFORM_NAME       "Dreamcast"
    #define PLATFORM_ENDIAN_LITTLE 1
#else
    #define PLATFORM_NATIVE     1
    #define PLATFORM_NAME       "Native"
    #define PLATFORM_ENDIAN_LITTLE 1
#endif

/*============================================================================
 * Platform Capabilities
 *============================================================================*/

typedef struct {
    const char *name;
    uint32_t ram_size;          /* Total RAM in bytes */
    uint32_t vram_size;         /* Video RAM in bytes */
    uint32_t max_texture_size;  /* Max texture dimension */
    uint32_t max_vertices;      /* Max vertices per frame */
    uint32_t screen_width;
    uint32_t screen_height;
    int has_analog_sticks;
    int has_triggers;
    int has_rumble;
    int color_depth;            /* 16 or 32 bits */
} platform_caps_t;

/* Get platform capabilities */
const platform_caps_t *platform_get_caps(void);

/*============================================================================
 * Platform Initialization
 *============================================================================*/

/* Initialize platform hardware */
int platform_init(void);

/* Shutdown platform */
void platform_shutdown(void);

/* Get high-resolution timer in microseconds */
uint64_t platform_get_time_us(void);

/* Sleep for specified milliseconds */
void platform_sleep_ms(uint32_t ms);

/* Exit to system/dashboard */
void platform_exit(void);

/*============================================================================
 * Memory Management
 *============================================================================*/

/* Platform-optimized memory allocation */
void *platform_malloc(size_t size);
void *platform_malloc_aligned(size_t size, size_t alignment);
void *platform_realloc(void *ptr, size_t size);
void platform_free(void *ptr);

/* Video memory allocation (for textures) */
void *platform_vram_alloc(size_t size);
void platform_vram_free(void *ptr);

/*============================================================================
 * Controller Button Mappings (Unified)
 *============================================================================*/

/* Cross-platform button constants */
#define PLAT_BTN_CROSS      (1 << 0)   /* A/Cross/A */
#define PLAT_BTN_CIRCLE     (1 << 1)   /* B/Circle/B */
#define PLAT_BTN_SQUARE     (1 << 2)   /* X/Square/X */
#define PLAT_BTN_TRIANGLE   (1 << 3)   /* Y/Triangle/Y */
#define PLAT_BTN_START      (1 << 4)   /* Start */
#define PLAT_BTN_SELECT     (1 << 5)   /* Back/Select */
#define PLAT_BTN_L1         (1 << 6)   /* L1/LB/L */
#define PLAT_BTN_R1         (1 << 7)   /* R1/RB/R */
#define PLAT_BTN_L2         (1 << 8)   /* L2/LT (digital) */
#define PLAT_BTN_R2         (1 << 9)   /* R2/RT (digital) */
#define PLAT_BTN_L3         (1 << 10)  /* L3/LS (stick click) */
#define PLAT_BTN_R3         (1 << 11)  /* R3/RS (stick click) */
#define PLAT_BTN_DPAD_UP    (1 << 12)
#define PLAT_BTN_DPAD_DOWN  (1 << 13)
#define PLAT_BTN_DPAD_LEFT  (1 << 14)
#define PLAT_BTN_DPAD_RIGHT (1 << 15)

/* Platform-specific button aliases */
#if defined(PLATFORM_XBOX) || defined(PLATFORM_XBOX360)
    #define PLAT_BTN_A      PLAT_BTN_CROSS
    #define PLAT_BTN_B      PLAT_BTN_CIRCLE
    #define PLAT_BTN_X      PLAT_BTN_SQUARE
    #define PLAT_BTN_Y      PLAT_BTN_TRIANGLE
    #define PLAT_BTN_BACK   PLAT_BTN_SELECT
    #define PLAT_BTN_LB     PLAT_BTN_L1
    #define PLAT_BTN_RB     PLAT_BTN_R1
    #define PLAT_BTN_LT     PLAT_BTN_L2
    #define PLAT_BTN_RT     PLAT_BTN_R2
    #define PLAT_BTN_LS     PLAT_BTN_L3
    #define PLAT_BTN_RS     PLAT_BTN_R3
#elif defined(PLATFORM_DREAMCAST)
    #define PLAT_BTN_A      PLAT_BTN_CROSS
    #define PLAT_BTN_B      PLAT_BTN_CIRCLE
    #define PLAT_BTN_X      PLAT_BTN_SQUARE
    #define PLAT_BTN_Y      PLAT_BTN_TRIANGLE
    #define PLAT_BTN_C      (1 << 16)
    #define PLAT_BTN_D      (1 << 17)
    #define PLAT_BTN_Z      (1 << 18)
#endif

/*============================================================================
 * Graphics API Abstraction
 *============================================================================*/

/* Render list types */
typedef enum {
    RENDER_LIST_OPAQUE,
    RENDER_LIST_TRANSPARENT,
    RENDER_LIST_PUNCHTHROUGH
} render_list_t;

/* Texture format */
typedef enum {
    TEX_FMT_RGB565,
    TEX_FMT_ARGB1555,
    TEX_FMT_ARGB4444,
    TEX_FMT_ARGB8888,
    TEX_FMT_PALETTED_4BPP,
    TEX_FMT_PALETTED_8BPP
} tex_format_t;

/* Platform texture handle */
typedef struct platform_texture {
    void *data;
    uint16_t width;
    uint16_t height;
    tex_format_t format;
    uint32_t platform_handle;
} platform_texture_t;

/* Low-level graphics operations */
void platform_gfx_init(void);
void platform_gfx_shutdown(void);
void platform_gfx_begin_frame(void);
void platform_gfx_end_frame(void);
void platform_gfx_set_render_list(render_list_t list);
void platform_gfx_clear(uint32_t color);

/* Texture operations */
platform_texture_t *platform_texture_create(uint16_t w, uint16_t h, tex_format_t fmt);
void platform_texture_upload(platform_texture_t *tex, const void *data);
void platform_texture_destroy(platform_texture_t *tex);
void platform_texture_bind(platform_texture_t *tex);

/*============================================================================
 * Audio API Abstraction
 *============================================================================*/

/* Audio format */
typedef enum {
    AUDIO_FMT_PCM16,
    AUDIO_FMT_ADPCM,
    AUDIO_FMT_VAG,      /* PlayStation ADPCM */
    AUDIO_FMT_XMA       /* Xbox compressed */
} audio_format_t;

/* Platform audio handle */
typedef struct platform_audio {
    void *data;
    uint32_t size;
    uint32_t sample_rate;
    audio_format_t format;
    int channels;
    uint32_t platform_handle;
} platform_audio_t;

void platform_audio_init(void);
void platform_audio_shutdown(void);
void platform_audio_update(void);

platform_audio_t *platform_audio_load(const char *path);
void platform_audio_unload(platform_audio_t *audio);
void platform_audio_play(platform_audio_t *audio, int loop);
void platform_audio_stop(platform_audio_t *audio);
void platform_audio_set_volume(int volume); /* 0-100 */

/*============================================================================
 * File I/O Abstraction
 *============================================================================*/

/* File handle */
typedef struct platform_file platform_file_t;

platform_file_t *platform_file_open(const char *path, const char *mode);
void platform_file_close(platform_file_t *file);
size_t platform_file_read(platform_file_t *file, void *buf, size_t size);
size_t platform_file_write(platform_file_t *file, const void *buf, size_t size);
int platform_file_seek(platform_file_t *file, long offset, int whence);
long platform_file_tell(platform_file_t *file);
int platform_file_exists(const char *path);

/* Media paths */
const char *platform_get_data_path(void);   /* CD/DVD/HDD data path */
const char *platform_get_save_path(void);   /* Memory card/HDD save path */

#endif /* PLATFORM_H */
