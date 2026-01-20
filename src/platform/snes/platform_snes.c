/*
 * RetroRacer - Super Nintendo (SNES) Platform Implementation
 * Uses PVSnesLib for SNES homebrew development
 *
 * Note: SNES version uses Mode 7 for pseudo-3D racing
 * similar to F-Zero and Super Mario Kart
 */

#include "platform.h"

#ifdef PLATFORM_SNES

#include <snes.h>

static platform_caps_t snes_caps = {
    .name = "Super Nintendo",
    .screen_width = 256,
    .screen_height = 224,
    .color_depth = 15,                     /* 15-bit color (32768 colors) */
    .max_polygons_per_frame = 0,           /* No 3D - Mode 7 affine transform only */
    .has_hardware_transform = 0,           /* Mode 7 is 2D affine, not 3D */
    .has_texture_compression = 0,
    .max_texture_size = 128,               /* Tilemap based */
    .audio_channels = 8,                   /* SPC700 has 8 voices */
    .audio_sample_rate = 32000             /* BRR compressed samples */
};

void platform_init(void) {
    /* Initialize console */
    consoleInit();

    /* Set text layer */
    consoleSetTextVramBGAdr(0x6800);
    consoleSetTextVramAdr(0x3000);
    consoleSetTextOffset(0x0100);

    /* Initialize sprites */
    oamInitGfxSet(&sprite_tiles, (&sprite_tiles_end - &sprite_tiles),
                  &sprite_pal, (&sprite_pal_end - &sprite_pal),
                  0, 0x4000, OBJ_SIZE16_L32);

    /* Set brightness */
    setBrightness(0xF);
}

void platform_shutdown(void) {
    /* Return to nothing - SNES has no OS */
    while (1) {
        WaitForVBlank();
    }
}

const platform_caps_t *platform_get_caps(void) {
    return &snes_caps;
}

uint64_t platform_get_time_us(void) {
    /* SNES runs at ~60Hz NTSC, ~50Hz PAL */
    /* No high-resolution timer available */
    /* Return frame count * frame time */
    static uint32_t frame_count = 0;
    frame_count++;
    return (uint64_t)frame_count * 16666ULL;  /* ~60fps = 16.666ms per frame */
}

void platform_sleep_ms(int ms) {
    /* Wait for frames */
    int frames = (ms + 15) / 16;
    for (int i = 0; i < frames; i++) {
        WaitForVBlank();
    }
}

void *platform_alloc(size_t size) {
    /* SNES has very limited RAM (128KB) */
    /* Most allocation is static */
    /* PVSnesLib may have a simple allocator */
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
    /* Print to text layer for debugging */
    /* Note: Very limited, uses tiles */
    (void)fmt;
}

/*
 * SNES Main Entry Point
 */
#ifndef NATIVE_BUILD

/* External sprite data (would be in separate .asm file) */
extern char sprite_tiles, sprite_tiles_end;
extern char sprite_pal, sprite_pal_end;
extern char track_tiles, track_tiles_end;
extern char track_pal, track_pal_end;
extern char track_map, track_map_end;

int main(void) {
    extern int game_main(int argc, char *argv[]);

    platform_init();

    /* Load track tilemap for Mode 7 */
    bgInitMapSet(0, &track_map, (&track_map_end - &track_map),
                 SC_32x32, 0x0000);
    bgInitTileSet(0, &track_tiles, &track_pal,
                  0, (&track_tiles_end - &track_tiles),
                  (&track_pal_end - &track_pal),
                  BG_256COLORS, 0x2000);

    int result = game_main(0, NULL);

    platform_shutdown();

    return result;
}
#endif

#endif /* PLATFORM_SNES */
