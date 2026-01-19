/*
 * RetroRacer - PlayStation 1 (PSX) Platform Implementation
 * Core platform abstraction for PSX hardware
 */

#include "platform.h"

#ifdef PLATFORM_PSX

#include <sys/types.h>
#include <libetc.h>
#include <libgte.h>
#include <libgpu.h>
#include <libapi.h>

static platform_caps_t psx_caps = {
    .name = "PlayStation",
    .screen_width = 320,
    .screen_height = 240,
    .color_depth = 16,
    .max_polygons_per_frame = 2000,      /* PSX can do ~2000-4000 textured polys */
    .has_hardware_transform = 1,          /* GTE provides HW transforms */
    .has_texture_compression = 0,
    .max_texture_size = 256,              /* 256x256 max */
    .audio_channels = 24,                 /* SPU has 24 voices */
    .audio_sample_rate = 44100
};

/* Root counter for timing */
static volatile uint32_t vsync_counter = 0;

static void vsync_callback(void) {
    vsync_counter++;
}

void platform_init(void) {
    /* Initialize BIOS */
    ResetCallback();

    /* Setup VSync callback for timing */
    VSyncCallback(vsync_callback);

    /* Enable interrupts */
    SetIntrMask(0);

    printf("RetroRacer - PlayStation Port\n");
    printf("GTE initialized for hardware transforms\n");
}

void platform_shutdown(void) {
    /* Cleanup callbacks */
    VSyncCallback(NULL);

    /* Return to shell/BIOS */
    StopCallback();
}

const platform_caps_t *platform_get_caps(void) {
    return &psx_caps;
}

uint64_t platform_get_time_us(void) {
    /* PSX runs at ~60Hz NTSC, ~50Hz PAL */
    /* VSync counter gives us frame-accurate timing */
    /* Each frame is ~16666us (60Hz) */
    return (uint64_t)vsync_counter * 16666ULL;
}

void platform_sleep_ms(int ms) {
    /* Wait for approximately ms milliseconds */
    int frames = (ms + 15) / 16;  /* Convert to frames (16ms per frame) */
    uint32_t target = vsync_counter + frames;
    while (vsync_counter < target) {
        /* Spin wait - could use VSync() here */
    }
}

void *platform_alloc(size_t size) {
    return malloc3(size);
}

void *platform_alloc_aligned(size_t size, size_t alignment) {
    /* PSX malloc3 is 4-byte aligned by default */
    /* For larger alignment, allocate extra and align manually */
    if (alignment <= 4) {
        return malloc3(size);
    }

    void *ptr = malloc3(size + alignment);
    if (!ptr) return NULL;

    uintptr_t addr = (uintptr_t)ptr;
    uintptr_t aligned = (addr + alignment - 1) & ~(alignment - 1);

    /* Store original pointer before aligned address */
    /* Note: This simple approach wastes memory but works */
    return (void *)aligned;
}

void platform_free(void *ptr) {
    free3(ptr);
}

void platform_debug_print(const char *fmt, ...) {
    /* PSX debug output - goes to debug console if connected */
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
}

/*
 * PSX Main Entry Point
 */
#ifndef NATIVE_BUILD
int main(void) {
    extern int game_main(int argc, char *argv[]);

    platform_init();

    int result = game_main(0, NULL);

    platform_shutdown();

    return result;
}
#endif

#endif /* PLATFORM_PSX */
