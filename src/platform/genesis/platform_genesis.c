/*
 * RetroRacer - Sega Genesis Platform Implementation
 * Uses SGDK (Sega Genesis Development Kit)
 */

#include "platform.h"

#ifdef PLATFORM_GENESIS

#include <genesis.h>

static u32 frame_count = 0;
static u32 last_frame = 0;

void platform_init(void) {
    /* Main initialization handled by SGDK's boot code */
    frame_count = 0;
    last_frame = 0;
}

void platform_shutdown(void) {
    /* Nothing to clean up */
}

u32 platform_get_ticks(void) {
    /* Return time in milliseconds based on frame count
     * NTSC: 60fps, PAL: 50fps */
    return (frame_count * 1000) / (IS_PAL_SYSTEM ? 50 : 60);
}

float platform_get_delta_time(void) {
    u32 current = frame_count;
    u32 delta = current - last_frame;
    last_frame = current;

    /* Convert frames to seconds */
    return (float)delta / (IS_PAL_SYSTEM ? 50.0f : 60.0f);
}

void platform_sleep(u32 ms) {
    /* Convert ms to frames and wait */
    u32 frames = (ms * (IS_PAL_SYSTEM ? 50 : 60)) / 1000;
    if (frames < 1) frames = 1;

    for (u32 i = 0; i < frames; i++) {
        SYS_doVBlankProcess();
        frame_count++;
    }
}

void *platform_alloc(size_t size) {
    return MEM_alloc(size);
}

void platform_free(void *ptr) {
    MEM_free(ptr);
}

const char *platform_get_name(void) {
    return "Sega Genesis";
}

int platform_get_screen_width(void) {
    return 320;
}

int platform_get_screen_height(void) {
    return 224;
}

/* Called by main loop */
void platform_frame_start(void) {
    frame_count++;
}

#endif /* PLATFORM_GENESIS */
