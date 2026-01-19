/*
 * RetroRacer - PlayStation 3 Platform Implementation
 * Core platform abstraction for PS3 hardware (Cell BE + RSX)
 */

#include "platform.h"

#ifdef PLATFORM_PS3

#include <psl1ght/lv2.h>
#include <sysutil/sysutil.h>
#include <sys/process.h>
#include <sys/timer.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>

static platform_caps_t ps3_caps = {
    .name = "PlayStation 3",
    .screen_width = 1280,
    .screen_height = 720,
    .color_depth = 32,
    .max_polygons_per_frame = 500000,     /* RSX can handle millions with proper batching */
    .has_hardware_transform = 1,           /* RSX vertex shaders */
    .has_texture_compression = 1,          /* DXT/S3TC support */
    .max_texture_size = 4096,              /* 4096x4096 max */
    .audio_channels = 512,                 /* Software mixing */
    .audio_sample_rate = 48000
};

/* System callback for exit handling */
static int running = 1;

static void sys_callback(u64 status, u64 param, void *userdata) {
    (void)param;
    (void)userdata;

    switch (status) {
        case SYSUTIL_EXIT_GAME:
            running = 0;
            break;
        case SYSUTIL_DRAW_BEGIN:
        case SYSUTIL_DRAW_END:
            break;
    }
}

void platform_init(void) {
    /* Register system callback */
    sysUtilRegisterCallback(SYSUTIL_EVENT_SLOT0, sys_callback, NULL);

    printf("RetroRacer - PlayStation 3 Port\n");
    printf("Cell BE: 1 PPE + 6 SPEs available\n");
    printf("RSX: NVIDIA GeForce 7800 GTX-based GPU\n");
    printf("RAM: 256MB System + 256MB Video\n");
}

void platform_shutdown(void) {
    sysUtilUnregisterCallback(SYSUTIL_EVENT_SLOT0);
}

const platform_caps_t *platform_get_caps(void) {
    return &ps3_caps;
}

uint64_t platform_get_time_us(void) {
    u64 time;
    sysGetSystemTime(&time);
    return time;
}

void platform_sleep_ms(int ms) {
    sysUsleep(ms * 1000);
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
    vprintf(fmt, args);
    va_end(args);
}

/*
 * Check if we should keep running
 */
int platform_should_run(void) {
    /* Check for system events */
    sysUtilCheckCallback();
    return running;
}

/*
 * PS3 Main Entry Point
 */
#ifndef NATIVE_BUILD
SYS_PROCESS_PARAM(1001, 0x100000)

int main(int argc, char *argv[]) {
    extern int game_main(int argc, char *argv[]);

    platform_init();

    int result = game_main(argc, argv);

    platform_shutdown();

    return result;
}
#endif

#endif /* PLATFORM_PS3 */
