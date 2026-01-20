/*
 * RetroRacer - Xbox One Platform Implementation
 * Hardware: AMD Jaguar 8-core 1.75GHz, AMD GCN GPU, 8GB DDR3
 * Xbox One X: 2.3GHz CPU, 6 TFLOP GPU, 12GB GDDR5
 */

#include "platform.h"
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>

#ifdef PLATFORM_XBOXONE
#include <windows.h>
#include <xdk.h>
#endif

static platform_caps_t g_caps = {
    .name = "Xbox One",
    .screen_width = 1920,
    .screen_height = 1080,
    .color_depth = 32,
    .max_polygons_per_frame = 2000000,
    .has_hardware_transform = 1,
    .has_texture_compression = 1,
    .max_texture_size = 16384,
    .audio_channels = 32,
    .audio_sample_rate = 48000
};

static struct {
    int initialized;
    int is_xbox_one_x;
    uint64_t start_time;
    uint64_t perf_freq;
} g_platform;

void platform_init(void) {
    if (g_platform.initialized) return;

    memset(&g_platform, 0, sizeof(g_platform));

#ifdef PLATFORM_XBOXONE
    /* Query performance counter frequency */
    LARGE_INTEGER freq, counter;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&counter);

    g_platform.perf_freq = freq.QuadPart;
    g_platform.start_time = counter.QuadPart;

    /* Detect Xbox One variant */
    /* In production: check XSystemGetDeviceType() */
    g_platform.is_xbox_one_x = 0;

    if (g_platform.is_xbox_one_x) {
        /* Xbox One X can do 4K */
        g_caps.screen_width = 3840;
        g_caps.screen_height = 2160;
        g_caps.max_polygons_per_frame = 4000000;
        g_caps.name = "Xbox One X";
    }
#else
    g_platform.start_time = 0;
    g_platform.perf_freq = 1000000;
#endif

    g_platform.initialized = 1;
}

void platform_shutdown(void) {
    if (!g_platform.initialized) return;
    g_platform.initialized = 0;
}

const platform_caps_t *platform_get_caps(void) {
    return &g_caps;
}

uint64_t platform_get_time_us(void) {
#ifdef PLATFORM_XBOXONE
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    uint64_t elapsed = counter.QuadPart - g_platform.start_time;
    return (elapsed * 1000000) / g_platform.perf_freq;
#else
    return 0;
#endif
}

void platform_sleep_ms(int ms) {
#ifdef PLATFORM_XBOXONE
    Sleep(ms);
#else
    (void)ms;
#endif
}

void *platform_alloc(size_t size) {
#ifdef PLATFORM_XBOXONE
    return VirtualAlloc(NULL, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
#else
    return malloc(size);
#endif
}

void *platform_alloc_aligned(size_t size, size_t alignment) {
#ifdef PLATFORM_XBOXONE
    return _aligned_malloc(size, alignment);
#else
    void *ptr;
    if (posix_memalign(&ptr, alignment, size) == 0) {
        return ptr;
    }
    return NULL;
#endif
}

void platform_free(void *ptr) {
#ifdef PLATFORM_XBOXONE
    VirtualFree(ptr, 0, MEM_RELEASE);
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

#ifdef PLATFORM_XBOXONE
    OutputDebugStringA(buffer);
#endif
}

/* Xbox One specific functions */
#ifdef PLATFORM_XBOXONE
int platform_is_xbox_one_x(void) {
    return g_platform.is_xbox_one_x;
}

int platform_get_core_count(void) {
    return 8;
}

void platform_set_thread_affinity(int core) {
    if (core >= 0 && core < 8) {
        SetThreadAffinityMask(GetCurrentThread(), 1ULL << core);
    }
}

uint64_t platform_get_available_memory(void) {
    MEMORYSTATUSEX status;
    status.dwLength = sizeof(status);
    GlobalMemoryStatusEx(&status);
    return status.ullAvailPhys;
}

int platform_is_kinect_connected(void) {
    /* Would check Kinect sensor status */
    return 0;
}
#endif
