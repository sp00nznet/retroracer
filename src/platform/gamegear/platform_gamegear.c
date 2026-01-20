/*
 * RetroRacer - Game Gear Platform Implementation
 * Hardware: Z80 3.58MHz, 8KB RAM, 160x144 visible (256x192 internal)
 * 32 colors on screen from 4096 color palette
 */

#include "platform.h"
#include <string.h>

#ifdef PLATFORM_GAMEGEAR
#include <sms.h>
#include <gg.h>
#endif

static platform_caps_t g_caps = {
    .name = "Game Gear",
    .screen_width = 160,
    .screen_height = 144,
    .color_depth = 12,  /* 4096 color palette */
    .max_polygons_per_frame = 0,
    .has_hardware_transform = 0,
    .has_texture_compression = 0,
    .max_texture_size = 8,
    .audio_channels = 4,  /* 3 tone + 1 noise */
    .audio_sample_rate = 0
};

static struct {
    int initialized;
    uint16_t frame_count;
    uint8_t vblank_flag;
} g_platform;

/* VBlank interrupt handler */
#ifdef PLATFORM_GAMEGEAR
static void vblank_handler(void) {
    g_platform.vblank_flag = 1;
}
#endif

void platform_init(void) {
    if (g_platform.initialized) return;

    memset(&g_platform, 0, sizeof(g_platform));

#ifdef PLATFORM_GAMEGEAR
    /* Initialize SMS/GG SDK */
    SMS_init();

    /* Set up VDP for Game Gear */
    SMS_VDPturnOnFeature(VDPFEATURE_HIDEFIRSTCOL);

    /* Enable display */
    SMS_displayOn();

    /* Set up VBlank interrupt */
    SMS_setLineInterruptHandler(vblank_handler);
    SMS_enableLineInterrupt();
#endif

    g_platform.initialized = 1;
}

void platform_shutdown(void) {
    if (!g_platform.initialized) return;

#ifdef PLATFORM_GAMEGEAR
    SMS_displayOff();
#endif

    g_platform.initialized = 0;
}

const platform_caps_t *platform_get_caps(void) {
    return &g_caps;
}

uint64_t platform_get_time_us(void) {
    /* Approximate based on frame count (59.92 Hz for NTSC) */
    return (uint64_t)g_platform.frame_count * 16689;
}

void platform_sleep_ms(int ms) {
#ifdef PLATFORM_GAMEGEAR
    int frames = ms / 17;
    if (frames < 1) frames = 1;

    for (int i = 0; i < frames; i++) {
        SMS_waitForVBlank();
        g_platform.frame_count++;
    }
#else
    (void)ms;
#endif
}

void *platform_alloc(size_t size) {
    /* Game Gear has limited RAM - avoid dynamic allocation */
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
    (void)fmt;
}

/* Game Gear specific functions */
#ifdef PLATFORM_GAMEGEAR
void platform_wait_vblank(void) {
    SMS_waitForVBlank();
    g_platform.frame_count++;
}

void platform_set_palette(uint8_t index, uint16_t color) {
    GG_setBGPaletteColor(index, color);
}

void platform_set_sprite_palette(uint8_t index, uint16_t color) {
    GG_setSpritePaletteColor(index, color);
}

void platform_set_tile(uint16_t tile_num, const uint8_t *data) {
    SMS_loadTiles(data, tile_num, 32);  /* 32 bytes per tile */
}

void platform_set_tilemap(uint8_t x, uint8_t y, uint16_t tile) {
    SMS_setTileatXY(x, y, tile);
}

void platform_set_sprite(uint8_t n, uint8_t x, uint8_t y, uint8_t tile) {
    SMS_addSprite(x, y, tile);
}

void platform_update_sprites(void) {
    SMS_copySpritestoSAT();
}

void platform_scroll_bg(int8_t x, int8_t y) {
    SMS_setBGScrollX(x);
    SMS_setBGScrollY(y);
}

uint8_t platform_get_keys(void) {
    return SMS_getKeysStatus();
}

void platform_enable_display(int enable) {
    if (enable) {
        SMS_displayOn();
    } else {
        SMS_displayOff();
    }
}

/* Game Gear has Start button on separate port */
int platform_get_start_button(void) {
    /* Read port 0x00 bit 7 for Start button */
    return (~(*(volatile uint8_t *)0x00) & 0x80) != 0;
}
#endif
