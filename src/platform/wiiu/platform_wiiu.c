/*
 * RetroRacer - Wii U Platform Implementation
 * Hardware: Espresso 1.24GHz tri-core CPU, Latte GPU, 2GB RAM
 * Dual screen: TV (up to 1080p) + GamePad (854x480)
 */

#include "platform.h"
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>

#ifdef PLATFORM_WIIU
#include <coreinit/core.h>
#include <coreinit/thread.h>
#include <coreinit/time.h>
#include <coreinit/memory.h>
#include <coreinit/memheap.h>
#include <coreinit/memfrmheap.h>
#include <coreinit/foreground.h>
#include <coreinit/screen.h>
#include <proc_ui/procui.h>
#include <whb/log.h>
#include <whb/log_console.h>
#include <whb/proc.h>
#endif

static platform_caps_t g_caps = {
    .name = "Nintendo Wii U",
    .screen_width = 1920,
    .screen_height = 1080,
    .color_depth = 32,
    .max_polygons_per_frame = 500000,
    .has_hardware_transform = 1,
    .has_texture_compression = 1,
    .max_texture_size = 4096,
    .audio_channels = 24,
    .audio_sample_rate = 48000
};

static struct {
    int initialized;
    uint64_t start_time;

#ifdef PLATFORM_WIIU
    void *tv_buffer;
    void *drc_buffer;
    uint32_t tv_buffer_size;
    uint32_t drc_buffer_size;
#endif
} g_platform;

void platform_init(void) {
    if (g_platform.initialized) return;

    memset(&g_platform, 0, sizeof(g_platform));

#ifdef PLATFORM_WIIU
    /* Initialize WHB process handling */
    WHBProcInit();

    /* Initialize logging */
    WHBLogConsoleInit();

    /* Initialize OSScreen for both displays */
    OSScreenInit();

    /* Get required buffer sizes */
    g_platform.tv_buffer_size = OSScreenGetBufferSizeEx(SCREEN_TV);
    g_platform.drc_buffer_size = OSScreenGetBufferSizeEx(SCREEN_DRC);

    /* Allocate screen buffers from MEM1 heap */
    g_platform.tv_buffer = MEMAllocFromDefaultHeapEx(g_platform.tv_buffer_size, 0x100);
    g_platform.drc_buffer = MEMAllocFromDefaultHeapEx(g_platform.drc_buffer_size, 0x100);

    if (g_platform.tv_buffer && g_platform.drc_buffer) {
        OSScreenSetBufferEx(SCREEN_TV, g_platform.tv_buffer);
        OSScreenSetBufferEx(SCREEN_DRC, g_platform.drc_buffer);

        OSScreenEnableEx(SCREEN_TV, TRUE);
        OSScreenEnableEx(SCREEN_DRC, TRUE);
    }

    /* Store start time */
    g_platform.start_time = OSGetSystemTime();
#endif

    g_platform.initialized = 1;
}

void platform_shutdown(void) {
    if (!g_platform.initialized) return;

#ifdef PLATFORM_WIIU
    /* Disable screens */
    OSScreenEnableEx(SCREEN_TV, FALSE);
    OSScreenEnableEx(SCREEN_DRC, FALSE);

    /* Free buffers */
    if (g_platform.tv_buffer) {
        MEMFreeToDefaultHeap(g_platform.tv_buffer);
    }
    if (g_platform.drc_buffer) {
        MEMFreeToDefaultHeap(g_platform.drc_buffer);
    }

    OSScreenShutdown();
    WHBLogConsoleFree();
    WHBProcShutdown();
#endif

    g_platform.initialized = 0;
}

const platform_caps_t *platform_get_caps(void) {
    return &g_caps;
}

uint64_t platform_get_time_us(void) {
#ifdef PLATFORM_WIIU
    OSTime now = OSGetSystemTime();
    return OSTicksToMicroseconds(now - g_platform.start_time);
#else
    return 0;
#endif
}

void platform_sleep_ms(int ms) {
#ifdef PLATFORM_WIIU
    OSSleepTicks(OSMillisecondsToTicks(ms));
#else
    (void)ms;
#endif
}

void *platform_alloc(size_t size) {
#ifdef PLATFORM_WIIU
    return MEMAllocFromDefaultHeap(size);
#else
    return malloc(size);
#endif
}

void *platform_alloc_aligned(size_t size, size_t alignment) {
#ifdef PLATFORM_WIIU
    return MEMAllocFromDefaultHeapEx(size, alignment);
#else
    void *ptr;
    if (posix_memalign(&ptr, alignment, size) == 0) {
        return ptr;
    }
    return NULL;
#endif
}

void platform_free(void *ptr) {
#ifdef PLATFORM_WIIU
    MEMFreeToDefaultHeap(ptr);
#else
    free(ptr);
#endif
}

void platform_debug_print(const char *fmt, ...) {
    char buffer[256];
    va_list args;

    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

#ifdef PLATFORM_WIIU
    WHBLogPrintf("%s", buffer);
#endif
}

/* Wii U specific functions */
#ifdef PLATFORM_WIIU
int platform_should_quit(void) {
    return !WHBProcIsRunning();
}

void *platform_get_tv_buffer(void) {
    return g_platform.tv_buffer;
}

void *platform_get_drc_buffer(void) {
    return g_platform.drc_buffer;
}

void platform_flip_buffers(void) {
    DCFlushRange(g_platform.tv_buffer, g_platform.tv_buffer_size);
    DCFlushRange(g_platform.drc_buffer, g_platform.drc_buffer_size);
    OSScreenFlipBuffersEx(SCREEN_TV);
    OSScreenFlipBuffersEx(SCREEN_DRC);
}

void platform_clear_screen(int screen, uint32_t color) {
    OSScreenClearBufferEx(screen, color);
}

int platform_get_core_count(void) {
    return 3;  /* Wii U has 3 cores */
}
#endif
