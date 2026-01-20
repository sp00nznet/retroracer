/*
 * RetroRacer - Game Boy Platform Implementation
 * Hardware: Sharp LR35902 4.19MHz, 8KB RAM, 160x144 screen
 * Supports DMG (original) and CGB (Color)
 */

#include "platform.h"
#include <string.h>

#ifdef PLATFORM_GAMEBOY
#include <gb/gb.h>
#include <gb/cgb.h>
#endif

static platform_caps_t g_caps = {
    .name = "Game Boy",
    .screen_width = 160,
    .screen_height = 144,
    .color_depth = 2,  /* 4 shades on DMG */
    .max_polygons_per_frame = 0,  /* No 3D hardware */
    .has_hardware_transform = 0,
    .has_texture_compression = 0,
    .max_texture_size = 8,  /* Sprite size */
    .audio_channels = 4,
    .audio_sample_rate = 0  /* PSG, not sample-based */
};

static struct {
    int initialized;
    int is_cgb;
    int is_sgb;
    uint16_t frame_count;
} g_platform;

void platform_init(void) {
    if (g_platform.initialized) return;

    memset(&g_platform, 0, sizeof(g_platform));

#ifdef PLATFORM_GAMEBOY
    /* Detect hardware type */
    g_platform.is_cgb = (_cpu == CGB_TYPE);
    g_platform.is_sgb = (sgb_check() != 0);

    if (g_platform.is_cgb) {
        g_caps.color_depth = 15;  /* 32768 colors */
        g_caps.name = "Game Boy Color";

        /* Set CGB double speed mode for better performance */
        cpu_fast();
    }

    if (g_platform.is_sgb) {
        g_caps.name = "Super Game Boy";
    }

    /* Initialize display */
    DISPLAY_ON;

    /* Set up default palette for DMG compatibility */
    if (!g_platform.is_cgb) {
        BGP_REG = 0xE4;   /* 11 10 01 00 - standard grayscale */
        OBP0_REG = 0xE4;
        OBP1_REG = 0xE4;
    }
#endif

    g_platform.initialized = 1;
}

void platform_shutdown(void) {
    if (!g_platform.initialized) return;

#ifdef PLATFORM_GAMEBOY
    DISPLAY_OFF;
#endif

    g_platform.initialized = 0;
}

const platform_caps_t *platform_get_caps(void) {
    return &g_caps;
}

uint64_t platform_get_time_us(void) {
    /* Game Boy has no high-resolution timer */
    /* Approximate based on frame count (59.73 Hz) */
    return (uint64_t)g_platform.frame_count * 16742;  /* ~16.742ms per frame */
}

void platform_sleep_ms(int ms) {
#ifdef PLATFORM_GAMEBOY
    /* Sleep by waiting for VBlanks */
    int frames = ms / 17;  /* ~17ms per frame */
    if (frames < 1) frames = 1;

    for (int i = 0; i < frames; i++) {
        wait_vbl_done();
        g_platform.frame_count++;
    }
#else
    (void)ms;
#endif
}

void *platform_alloc(size_t size) {
    /* Game Boy has very limited RAM - no dynamic allocation */
    /* Use static buffers instead */
    (void)size;
    return NULL;
}

void *platform_alloc_aligned(size_t size, size_t alignment) {
    (void)size;
    (void)alignment;
    return NULL;
}

void platform_free(void *ptr) {
    (void)ptr;
}

void platform_debug_print(const char *fmt, ...) {
    /* No debug output on Game Boy */
    (void)fmt;
}

/* Game Boy specific functions */
#ifdef PLATFORM_GAMEBOY
int platform_is_cgb(void) {
    return g_platform.is_cgb;
}

int platform_is_sgb(void) {
    return g_platform.is_sgb;
}

void platform_wait_vblank(void) {
    wait_vbl_done();
    g_platform.frame_count++;
}

void platform_set_bgp(uint8_t palette) {
    BGP_REG = palette;
}

void platform_set_obp0(uint8_t palette) {
    OBP0_REG = palette;
}

void platform_set_obp1(uint8_t palette) {
    OBP1_REG = palette;
}

void platform_enable_lcd(int enable) {
    if (enable) {
        DISPLAY_ON;
    } else {
        DISPLAY_OFF;
    }
}

uint8_t platform_get_joypad(void) {
    return joypad();
}

void platform_delay_frames(int frames) {
    for (int i = 0; i < frames; i++) {
        wait_vbl_done();
        g_platform.frame_count++;
    }
}
#endif
