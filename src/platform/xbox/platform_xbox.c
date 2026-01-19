/*
 * RetroRacer - Original Xbox Platform Implementation
 * Core platform abstraction for Xbox hardware (733MHz Intel Celeron + NV2A)
 */

#include "platform.h"

#ifdef PLATFORM_XBOX

#include <hal/xbox.h>
#include <hal/video.h>
#include <xboxkrnl/xboxkrnl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static platform_caps_t xbox_caps = {
    .name = "Xbox",
    .screen_width = 640,
    .screen_height = 480,
    .color_depth = 32,
    .max_polygons_per_frame = 100000,     /* NV2A can push good poly counts */
    .has_hardware_transform = 1,           /* NV2A vertex shaders */
    .has_texture_compression = 1,          /* DXT compression support */
    .max_texture_size = 4096,              /* 4096x4096 max */
    .audio_channels = 256,                 /* MCPX hardware voices */
    .audio_sample_rate = 48000
};

/* Performance counter frequency */
static LARGE_INTEGER perf_freq;

void platform_init(void) {
    /* Initialize Xbox hardware */
    XInitDevices(0, NULL);

    /* Get performance counter frequency */
    KeQueryPerformanceCounter(&perf_freq);

    /* Print system info */
    DbgPrint("RetroRacer - Xbox Port\n");
    DbgPrint("CPU: 733MHz Intel Celeron (Coppermine)\n");
    DbgPrint("GPU: NV2A (GeForce 3 based)\n");
    DbgPrint("RAM: 64MB unified\n");
}

void platform_shutdown(void) {
    /* Return to dashboard */
    XLaunchXBE(NULL);
}

const platform_caps_t *platform_get_caps(void) {
    return &xbox_caps;
}

uint64_t platform_get_time_us(void) {
    LARGE_INTEGER counter;
    KeQueryPerformanceCounter(&counter);

    /* Convert to microseconds */
    return (uint64_t)(counter.QuadPart * 1000000ULL / perf_freq.QuadPart);
}

void platform_sleep_ms(int ms) {
    /* Use kernel delay */
    LARGE_INTEGER delay;
    delay.QuadPart = -(ms * 10000LL);  /* 100ns units, negative for relative */
    KeDelayExecutionThread(KernelMode, FALSE, &delay);
}

void *platform_alloc(size_t size) {
    return MmAllocateContiguousMemory(size);
}

void *platform_alloc_aligned(size_t size, size_t alignment) {
    /* MmAllocateContiguousMemoryEx supports alignment */
    return MmAllocateContiguousMemoryEx(size, 0, 0xFFFFFFFF, alignment, PAGE_READWRITE);
}

void platform_free(void *ptr) {
    if (ptr) {
        MmFreeContiguousMemory(ptr);
    }
}

void platform_debug_print(const char *fmt, ...) {
    char buffer[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    DbgPrint("%s", buffer);
}

/*
 * Xbox Main Entry Point
 */
#ifndef NATIVE_BUILD
void XBoxStartup(void) {
    extern int game_main(int argc, char *argv[]);

    platform_init();

    /* Xbox doesn't pass argc/argv */
    int result = game_main(0, NULL);

    (void)result;

    platform_shutdown();
}
#endif

#endif /* PLATFORM_XBOX */
