/*
 * RetroRacer - Nintendo DS Platform Implementation
 * Uses libnds/devkitARM
 */

#include "platform.h"

#ifdef PLATFORM_NDS

#include <nds.h>
#include <stdlib.h>

static u32 frame_count = 0;
static u32 last_frame = 0;
static u32 start_ticks = 0;

void platform_init(void) {
    /* Initialize timers for timing */
    TIMER0_DATA = 0;
    TIMER0_CR = TIMER_ENABLE | TIMER_DIV_1024;

    TIMER1_DATA = 0;
    TIMER1_CR = TIMER_ENABLE | TIMER_CASCADE;

    start_ticks = 0;
    frame_count = 0;
    last_frame = 0;

    /* Default power settings */
    powerOn(POWER_ALL_2D | POWER_3D_CORE);
}

void platform_shutdown(void) {
    /* Nothing to clean up */
}

u32 platform_get_ticks(void) {
    /* Combine two cascaded timers for millisecond timing */
    /* Timer runs at 33.514MHz / 1024 = ~32.7kHz */
    u32 ticks = TIMER0_DATA | (TIMER1_DATA << 16);
    return (ticks * 1000) / 32728;
}

float platform_get_delta_time(void) {
    u32 current = frame_count;
    u32 delta = current - last_frame;
    last_frame = current;

    /* DS runs at ~60fps */
    return (float)delta / 60.0f;
}

void platform_sleep(u32 ms) {
    u32 frames = (ms * 60) / 1000;
    if (frames < 1) frames = 1;

    for (u32 i = 0; i < frames; i++) {
        swiWaitForVBlank();
    }
}

void *platform_alloc(size_t size) {
    return malloc(size);
}

void platform_free(void *ptr) {
    free(ptr);
}

const char *platform_get_name(void) {
    return "Nintendo DS";
}

int platform_get_screen_width(void) {
    return 256;
}

int platform_get_screen_height(void) {
    return 192;
}

void platform_frame_start(void) {
    frame_count++;
}

#endif /* PLATFORM_NDS */
