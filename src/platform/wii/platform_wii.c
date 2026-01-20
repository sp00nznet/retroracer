/*
 * RetroRacer - Wii Platform Implementation
 * Hardware: Broadway 729MHz CPU, Hollywood GPU, 88MB RAM
 */

#include "platform.h"
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>

#ifdef PLATFORM_WII
#include <gccore.h>
#include <wiiuse/wpad.h>
#include <fat.h>
#include <ogc/lwp_watchdog.h>
#endif

static platform_caps_t g_caps = {
    .name = "Nintendo Wii",
    .screen_width = 640,
    .screen_height = 480,
    .color_depth = 32,
    .max_polygons_per_frame = 100000,
    .has_hardware_transform = 1,
    .has_texture_compression = 0,
    .max_texture_size = 1024,
    .audio_channels = 16,
    .audio_sample_rate = 32000
};

static struct {
    int initialized;
    uint64_t start_time;
#ifdef PLATFORM_WII
    void *xfb;
    GXRModeObj *rmode;
#endif
} g_platform;

void platform_init(void) {
    if (g_platform.initialized) return;

    memset(&g_platform, 0, sizeof(g_platform));

#ifdef PLATFORM_WII
    /* Initialize video */
    VIDEO_Init();

    /* Get preferred video mode */
    g_platform.rmode = VIDEO_GetPreferredMode(NULL);

    /* Allocate framebuffer */
    g_platform.xfb = MEM_K0_TO_K1(SYS_AllocateFramebuffer(g_platform.rmode));

    /* Initialize console for debug output */
    console_init(g_platform.xfb, 20, 20, g_platform.rmode->fbWidth,
                 g_platform.rmode->xfbHeight, g_platform.rmode->fbWidth * VI_DISPLAY_PIX_SZ);

    /* Configure video */
    VIDEO_Configure(g_platform.rmode);
    VIDEO_SetNextFramebuffer(g_platform.xfb);
    VIDEO_SetBlack(FALSE);
    VIDEO_Flush();
    VIDEO_WaitVSync();

    if (g_platform.rmode->viTVMode & VI_NON_INTERLACE) {
        VIDEO_WaitVSync();
    }

    /* Initialize FAT filesystem for SD card access */
    fatInitDefault();

    /* Update caps based on actual video mode */
    g_caps.screen_width = g_platform.rmode->fbWidth;
    g_caps.screen_height = g_platform.rmode->efbHeight;

    /* Store start time */
    g_platform.start_time = gettime();
#endif

    g_platform.initialized = 1;
}

void platform_shutdown(void) {
    if (!g_platform.initialized) return;

#ifdef PLATFORM_WII
    VIDEO_SetBlack(TRUE);
    VIDEO_Flush();
#endif

    g_platform.initialized = 0;
}

const platform_caps_t *platform_get_caps(void) {
    return &g_caps;
}

uint64_t platform_get_time_us(void) {
#ifdef PLATFORM_WII
    return ticks_to_microsecs(gettime() - g_platform.start_time);
#else
    return 0;
#endif
}

void platform_sleep_ms(int ms) {
#ifdef PLATFORM_WII
    usleep(ms * 1000);
#else
    (void)ms;
#endif
}

void *platform_alloc(size_t size) {
#ifdef PLATFORM_WII
    return memalign(32, size);
#else
    return malloc(size);
#endif
}

void *platform_alloc_aligned(size_t size, size_t alignment) {
#ifdef PLATFORM_WII
    return memalign(alignment, size);
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

#ifdef PLATFORM_WII
    printf("%s", buffer);
#endif
}

/* Wii-specific functions */
#ifdef PLATFORM_WII
void *platform_get_framebuffer(void) {
    return g_platform.xfb;
}

GXRModeObj *platform_get_video_mode(void) {
    return g_platform.rmode;
}

int platform_is_widescreen(void) {
    return CONF_GetAspectRatio() == CONF_ASPECT_16_9;
}

int platform_is_progressive(void) {
    return VIDEO_HaveComponentCable();
}
#endif
