/*
 * RetroRacer - Nintendo 3DS Platform Implementation
 * Uses libctru for system functions
 *
 * 3DS System Info:
 * - ARM11 MPCore dual-core (Old 3DS) or quad-core (New 3DS)
 * - ARM9 for security/IO
 * - 128MB RAM (Old) or 256MB RAM (New 3DS)
 * - PICA200 GPU
 */

#include "platform.h"

#ifdef PLATFORM_3DS

#include <3ds.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

static u64 start_ticks = 0;
static u64 last_ticks = 0;
static u32 frame_count = 0;
static bool is_new_3ds = false;

/* Platform capabilities */
static platform_caps_t caps = {
    .name = "Nintendo 3DS",
    .screen_width = 400,
    .screen_height = 240,
    .color_depth = 24,
    .max_polygons_per_frame = 6144,
    .has_hardware_transform = 1,
    .has_texture_compression = 1,
    .max_texture_size = 1024,
    .audio_channels = 24,
    .audio_sample_rate = 32728
};

void platform_init(void) {
    /* Initialize services */
    gfxInitDefault();

    /* Check if running on New 3DS */
    APT_CheckNew3DS(&is_new_3ds);

    if (is_new_3ds) {
        /* Enable New 3DS clock speed boost */
        osSetSpeedupEnable(true);
        caps.name = "New Nintendo 3DS";
    }

    /* Enable stereoscopic 3D */
    gfxSet3D(true);

    /* Initialize console for debug output on bottom screen */
    consoleInit(GFX_BOTTOM, NULL);

    /* Get initial tick count */
    start_ticks = svcGetSystemTick();
    last_ticks = start_ticks;
    frame_count = 0;

    printf("RetroRacer - %s\n", caps.name);
    printf("Press START to exit\n\n");
}

void platform_shutdown(void) {
    /* Clean up graphics */
    gfxExit();
}

const platform_caps_t *platform_get_caps(void) {
    return &caps;
}

u64 platform_get_time_us(void) {
    u64 ticks = svcGetSystemTick();
    /* System ticks are at 268MHz */
    return (ticks * 1000000ULL) / SYSCLOCK_ARM11;
}

u32 platform_get_ticks(void) {
    u64 ticks = svcGetSystemTick() - start_ticks;
    /* Convert to milliseconds */
    return (u32)((ticks * 1000ULL) / SYSCLOCK_ARM11);
}

float platform_get_delta_time(void) {
    u64 current = svcGetSystemTick();
    u64 delta = current - last_ticks;
    last_ticks = current;

    /* Convert to seconds */
    return (float)delta / (float)SYSCLOCK_ARM11;
}

void platform_sleep_ms(int ms) {
    svcSleepThread((s64)ms * 1000000LL);
}

void platform_sleep(u32 ms) {
    svcSleepThread((s64)ms * 1000000LL);
}

void *platform_alloc(size_t size) {
    return malloc(size);
}

void *platform_alloc_aligned(size_t size, size_t alignment) {
    return memalign(alignment, size);
}

void *platform_alloc_linear(size_t size) {
    /* Allocate from linear heap (required for GPU/DMA access) */
    return linearAlloc(size);
}

void platform_free(void *ptr) {
    free(ptr);
}

void platform_free_linear(void *ptr) {
    linearFree(ptr);
}

void platform_debug_print(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
}

const char *platform_get_name(void) {
    return caps.name;
}

int platform_get_screen_width(void) {
    return caps.screen_width;
}

int platform_get_screen_height(void) {
    return caps.screen_height;
}

int platform_is_new_3ds(void) {
    return is_new_3ds ? 1 : 0;
}

float platform_get_3d_slider(void) {
    return osGet3DSliderState();
}

int platform_should_quit(void) {
    /* Check if user pressed HOME or closed the lid */
    return !aptMainLoop();
}

void platform_frame_start(void) {
    frame_count++;
}

void platform_frame_end(void) {
    /* Swap buffers */
    gfxFlushBuffers();
    gfxSwapBuffers();
    gspWaitForVBlank();
}

u32 platform_get_frame_count(void) {
    return frame_count;
}

/* Battery and system info */
u8 platform_get_battery_level(void) {
    u8 level = 0;
    MCUHWC_GetBatteryLevel(&level);
    return level;
}

int platform_is_charging(void) {
    u8 charging = 0;
    PTMU_GetBatteryChargeState(&charging);
    return charging ? 1 : 0;
}

/* WiFi status */
int platform_wifi_connected(void) {
    u32 status = 0;
    ACU_GetWifiStatus(&status);
    return status ? 1 : 0;
}

/* SD card access */
int platform_sd_mounted(void) {
    /* Check if SD is accessible */
    Handle handle;
    Result res = FSUSER_OpenArchive(&handle, ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, ""));
    if (R_SUCCEEDED(res)) {
        FSUSER_CloseArchive(handle);
        return 1;
    }
    return 0;
}

/* Performance monitoring */
static u64 cpu_time_start = 0;
static u64 gpu_time_start = 0;

void platform_perf_begin_cpu(void) {
    cpu_time_start = svcGetSystemTick();
}

float platform_perf_end_cpu(void) {
    u64 end = svcGetSystemTick();
    return (float)(end - cpu_time_start) / (float)SYSCLOCK_ARM11 * 1000.0f;
}

void platform_perf_begin_gpu(void) {
    gpu_time_start = svcGetSystemTick();
}

float platform_perf_end_gpu(void) {
    /* Note: This doesn't actually measure GPU time properly */
    /* Would need GX commands to properly profile GPU */
    u64 end = svcGetSystemTick();
    return (float)(end - gpu_time_start) / (float)SYSCLOCK_ARM11 * 1000.0f;
}

#endif /* PLATFORM_3DS */
