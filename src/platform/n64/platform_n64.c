/*
 * RetroRacer - Nintendo 64 Platform Implementation
 * Uses libdragon SDK for N64 homebrew
 */

#include "platform.h"

#ifdef PLATFORM_N64

#include <libdragon.h>
#include <stdlib.h>
#include <string.h>

static platform_caps_t n64_caps = {
    .name = "Nintendo 64",
    .screen_width = 320,
    .screen_height = 240,
    .color_depth = 16,
    .max_polygons_per_frame = 5000,       /* RCP can handle ~5000-10000 tris */
    .has_hardware_transform = 1,           /* RSP microcode for transforms */
    .has_texture_compression = 0,
    .max_texture_size = 64,                /* 64x64 with TMEM limits */
    .audio_channels = 16,                  /* Software mixer */
    .audio_sample_rate = 44100
};

/* Timer for frame timing */
static volatile uint32_t tick_counter = 0;

void platform_init(void) {
    /* Initialize debug console (for development) */
    debug_init_isviewer();
    debug_init_usblog();

    /* Initialize filesystem for ROM access */
    dfs_init(DFS_DEFAULT_LOCATION);

    /* Initialize timer */
    timer_init();

    debugf("RetroRacer - Nintendo 64 Port\n");
    debugf("CPU: VR4300 (93.75 MHz)\n");
    debugf("RCP: Reality Coprocessor (RSP + RDP)\n");
    debugf("RAM: 4MB (8MB with Expansion Pak)\n");
}

void platform_shutdown(void) {
    timer_close();
}

const platform_caps_t *platform_get_caps(void) {
    return &n64_caps;
}

uint64_t platform_get_time_us(void) {
    /* N64 timer ticks at 46.875 MHz (CPU clock / 2) */
    /* Convert to microseconds */
    return (uint64_t)timer_ticks() * 1000000ULL / 46875000ULL;
}

void platform_sleep_ms(int ms) {
    /* Use timer-based delay */
    uint64_t start = platform_get_time_us();
    uint64_t target = start + (ms * 1000);

    while (platform_get_time_us() < target) {
        /* Spin - could yield to RSP here */
    }
}

void *platform_alloc(size_t size) {
    return malloc(size);
}

void *platform_alloc_aligned(size_t size, size_t alignment) {
    return memalign(alignment, size);
}

void platform_free(void *ptr) {
    free(ptr);
}

void platform_debug_print(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vdebugf(fmt, args);
    va_end(args);
}

/*
 * N64 Main Entry Point
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

#endif /* PLATFORM_N64 */
