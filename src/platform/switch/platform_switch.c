/*
 * RetroRacer - Nintendo Switch Platform Implementation
 * Hardware: NVIDIA Tegra X1 (4x ARM Cortex-A57 + 4x A53), Maxwell GPU, 4GB RAM
 * Supports docked (1080p) and handheld (720p) modes
 */

#include "platform.h"
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>

#ifdef PLATFORM_SWITCH
#include <switch.h>
#endif

static platform_caps_t g_caps = {
    .name = "Nintendo Switch",
    .screen_width = 1920,
    .screen_height = 1080,
    .color_depth = 32,
    .max_polygons_per_frame = 1000000,
    .has_hardware_transform = 1,
    .has_texture_compression = 1,
    .max_texture_size = 8192,
    .audio_channels = 24,
    .audio_sample_rate = 48000
};

static struct {
    int initialized;
    int is_docked;
    uint64_t start_time;

#ifdef PLATFORM_SWITCH
    NWindow *window;
    Framebuffer fb;
#endif
} g_platform;

void platform_init(void) {
    if (g_platform.initialized) return;

    memset(&g_platform, 0, sizeof(g_platform));

#ifdef PLATFORM_SWITCH
    /* Initialize services */
    appletInitializeGamePlayRecording();

    /* Get window */
    g_platform.window = nwindowGetDefault();

    /* Check if docked */
    AppletOperationMode mode = appletGetOperationMode();
    g_platform.is_docked = (mode == AppletOperationMode_Console);

    /* Set resolution based on mode */
    if (g_platform.is_docked) {
        g_caps.screen_width = 1920;
        g_caps.screen_height = 1080;
    } else {
        g_caps.screen_width = 1280;
        g_caps.screen_height = 720;
    }

    /* Initialize framebuffer */
    framebufferCreate(&g_platform.fb, g_platform.window,
                      g_caps.screen_width, g_caps.screen_height,
                      PIXEL_FORMAT_RGBA_8888, 2);
    framebufferMakeLinear(&g_platform.fb);

    /* Get start time */
    g_platform.start_time = armGetSystemTick();

    /* Initialize socket for debugging (optional) */
    socketInitializeDefault();
    nxlinkStdio();
#endif

    g_platform.initialized = 1;
}

void platform_shutdown(void) {
    if (!g_platform.initialized) return;

#ifdef PLATFORM_SWITCH
    framebufferClose(&g_platform.fb);
    socketExit();
#endif

    g_platform.initialized = 0;
}

const platform_caps_t *platform_get_caps(void) {
    return &g_caps;
}

uint64_t platform_get_time_us(void) {
#ifdef PLATFORM_SWITCH
    uint64_t now = armGetSystemTick();
    return armTicksToNs(now - g_platform.start_time) / 1000;
#else
    return 0;
#endif
}

void platform_sleep_ms(int ms) {
#ifdef PLATFORM_SWITCH
    svcSleepThread((int64_t)ms * 1000000);
#else
    (void)ms;
#endif
}

void *platform_alloc(size_t size) {
#ifdef PLATFORM_SWITCH
    return aligned_alloc(16, size);
#else
    return malloc(size);
#endif
}

void *platform_alloc_aligned(size_t size, size_t alignment) {
#ifdef PLATFORM_SWITCH
    return aligned_alloc(alignment, size);
#else
    void *ptr;
    if (posix_memalign(&ptr, alignment, size) == 0) {
        return ptr;
    }
    return NULL;
#endif
}

void platform_free(void *ptr) {
    free(ptr);
}

void platform_debug_print(const char *fmt, ...) {
    char buffer[256];
    va_list args;

    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

#ifdef PLATFORM_SWITCH
    printf("%s", buffer);
#endif
}

/* Switch-specific functions */
#ifdef PLATFORM_SWITCH
int platform_is_docked(void) {
    AppletOperationMode mode = appletGetOperationMode();
    g_platform.is_docked = (mode == AppletOperationMode_Console);
    return g_platform.is_docked;
}

void platform_update_resolution(void) {
    int was_docked = g_platform.is_docked;
    platform_is_docked();

    if (was_docked != g_platform.is_docked) {
        /* Resolution changed - resize framebuffer */
        framebufferClose(&g_platform.fb);

        if (g_platform.is_docked) {
            g_caps.screen_width = 1920;
            g_caps.screen_height = 1080;
        } else {
            g_caps.screen_width = 1280;
            g_caps.screen_height = 720;
        }

        framebufferCreate(&g_platform.fb, g_platform.window,
                          g_caps.screen_width, g_caps.screen_height,
                          PIXEL_FORMAT_RGBA_8888, 2);
        framebufferMakeLinear(&g_platform.fb);
    }
}

void *platform_get_framebuffer(uint32_t *stride) {
    return framebufferBegin(&g_platform.fb, stride);
}

void platform_present_framebuffer(void) {
    framebufferEnd(&g_platform.fb);
}

int platform_should_quit(void) {
    return !appletMainLoop();
}

void platform_set_cpu_boost(int enable) {
    if (enable) {
        appletSetCpuBoostMode(ApmCpuBoostMode_FastLoad);
    } else {
        appletSetCpuBoostMode(ApmCpuBoostMode_Normal);
    }
}
#endif
