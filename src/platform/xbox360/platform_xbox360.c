/*
 * RetroRacer - Xbox 360 Platform Implementation
 * Uses libxenon for Xbox 360 homebrew
 */

#include "platform.h"

#ifdef PLATFORM_XBOX360

#include <xenos/xe.h>
#include <console/console.h>
#include <xenon_smc/xenon_smc.h>
#include <ppc/timebase.h>
#include <time/time.h>
#include <stdlib.h>

static uint64_t start_time = 0;
static uint64_t last_frame_time = 0;

void platform_init(void) {
    /* Initialize console for debug output */
    console_init();

    /* Get initial time */
    start_time = mftb();
    last_frame_time = start_time;
}

void platform_shutdown(void) {
    /* Nothing to clean up */
}

uint32_t platform_get_ticks(void) {
    uint64_t current = mftb();
    /* Convert timebase to milliseconds (assuming ~50MHz timebase) */
    return (uint32_t)((current - start_time) / 50000);
}

float platform_get_delta_time(void) {
    uint64_t current = mftb();
    uint64_t delta = current - last_frame_time;
    last_frame_time = current;

    /* Convert to seconds (assuming ~50MHz timebase) */
    return (float)delta / 50000000.0f;
}

void platform_sleep(uint32_t ms) {
    udelay(ms * 1000);
}

void *platform_alloc(size_t size) {
    return malloc(size);
}

void platform_free(void *ptr) {
    free(ptr);
}

const char *platform_get_name(void) {
    return "Xbox 360";
}

int platform_get_screen_width(void) {
    return 1280;
}

int platform_get_screen_height(void) {
    return 720;
}

#endif /* PLATFORM_XBOX360 */
