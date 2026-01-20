/*
 * RetroRacer - Game Boy Advance Platform Implementation
 * Uses libgba/devkitARM
 */

#include "platform.h"

#ifdef PLATFORM_GBA

#include <gba_systemcalls.h>
#include <gba_interrupt.h>
#include <gba_timers.h>
#include <stdlib.h>

static u32 frame_count = 0;
static u32 last_frame = 0;

/* Timer-based millisecond counter */
static volatile u32 timer_ticks = 0;

static void timer_isr(void) {
    timer_ticks++;
}

void platform_init(void) {
    /* Initialize interrupt system */
    irqInit();

    /* Setup Timer 2 for millisecond timing */
    /* Timer runs at 16.78MHz, prescaler 64 = ~262kHz */
    /* Overflow at 262 = ~1ms */
    REG_TM2CNT_L = 65536 - 262;
    REG_TM2CNT_H = TIMER_IRQ | TIMER_START | 2;  /* Prescaler 64 */

    irqSet(IRQ_TIMER2, timer_isr);
    irqEnable(IRQ_TIMER2);

    frame_count = 0;
    last_frame = 0;
}

void platform_shutdown(void) {
    irqDisable(IRQ_TIMER2);
}

u32 platform_get_ticks(void) {
    return timer_ticks;
}

float platform_get_delta_time(void) {
    u32 current = frame_count;
    u32 delta = current - last_frame;
    last_frame = current;

    /* GBA runs at ~60fps */
    return (float)delta / 60.0f;
}

void platform_sleep(u32 ms) {
    u32 target = timer_ticks + ms;
    while (timer_ticks < target) {
        VBlankIntrWait();
    }
}

void *platform_alloc(size_t size) {
    return malloc(size);
}

void platform_free(void *ptr) {
    free(ptr);
}

const char *platform_get_name(void) {
    return "Game Boy Advance";
}

int platform_get_screen_width(void) {
    return 240;
}

int platform_get_screen_height(void) {
    return 160;
}

void platform_frame_start(void) {
    frame_count++;
}

#endif /* PLATFORM_GBA */
