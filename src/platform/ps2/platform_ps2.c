/*
 * RetroRacer - PlayStation 2 Platform Implementation
 * Core platform abstraction for PS2 hardware
 */

#include "platform.h"

#ifdef PLATFORM_PS2

#include <tamtypes.h>
#include <kernel.h>
#include <sifrpc.h>
#include <loadfile.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <timer.h>

static platform_caps_t ps2_caps = {
    .name = "PlayStation 2",
    .screen_width = 640,
    .screen_height = 448,
    .color_depth = 32,
    .max_polygons_per_frame = 75000,      /* PS2 can push massive polycounts */
    .has_hardware_transform = 1,           /* VU0/VU1 for transforms */
    .has_texture_compression = 0,
    .max_texture_size = 1024,              /* 1024x1024 max */
    .audio_channels = 48,                  /* SPU2 has 48 voices */
    .audio_sample_rate = 48000
};

/* Timer for high-resolution timing */
static int timer_id = -1;
static volatile uint64_t timer_ticks = 0;

static int timer_handler(int ca) {
    (void)ca;
    timer_ticks++;
    return -1;  /* Reschedule */
}

void platform_init(void) {
    /* Initialize SIF RPC */
    SifInitRpc(0);

    /* Load I/O modules */
    int ret;

    ret = SifLoadModule("rom0:SIO2MAN", 0, NULL);
    if (ret < 0) {
        printf("Failed to load SIO2MAN\n");
    }

    ret = SifLoadModule("rom0:PADMAN", 0, NULL);
    if (ret < 0) {
        printf("Failed to load PADMAN\n");
    }

    ret = SifLoadModule("rom0:MCMAN", 0, NULL);
    if (ret < 0) {
        printf("Failed to load MCMAN\n");
    }

    ret = SifLoadModule("rom0:MCSERV", 0, NULL);
    if (ret < 0) {
        printf("Failed to load MCSERV\n");
    }

    /* Setup high-resolution timer */
    /* Bus clock is 147.456 MHz, timer prescaler of 256 gives ~576 KHz */
    timer_id = AllocHardTimer(TC_HLINE, 32, 0);
    if (timer_id >= 0) {
        SetupHardTimer(timer_id, TC_HLINE, 0, timer_handler);
        StartHardTimer(timer_id);
    }

    printf("RetroRacer - PlayStation 2 Port\n");
    printf("VU0/VU1 available for transforms\n");
    printf("GS initialized for rendering\n");
}

void platform_shutdown(void) {
    if (timer_id >= 0) {
        StopHardTimer(timer_id);
        FreeHardTimer(timer_id);
    }

    /* Reset IOP */
    SifExitRpc();
}

const platform_caps_t *platform_get_caps(void) {
    return &ps2_caps;
}

uint64_t platform_get_time_us(void) {
    /* Use EE timer for microsecond precision */
    /* Timer runs at bus clock / 256 = ~576 KHz */
    /* Each tick is ~1.74 us */
    return (timer_ticks * 1000000ULL) / 576000ULL;
}

void platform_sleep_ms(int ms) {
    /* Use kernel delay */
    /* Note: This is approximate, better to use timer-based delay */
    int i;
    for (i = 0; i < ms * 1000; i++) {
        asm volatile("nop");
    }
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
 * PS2 Main Entry Point
 */
#ifndef NATIVE_BUILD
int main(int argc, char *argv[]) {
    extern int game_main(int argc, char *argv[]);

    platform_init();

    int result = game_main(argc, argv);

    platform_shutdown();

    return result;
}
#endif

#endif /* PLATFORM_PS2 */
