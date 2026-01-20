/*
 * RetroRacer - 3DO Platform Implementation
 * Uses 3DO SDK (Portfolio OS)
 */

#include "platform.h"

#ifdef PLATFORM_3DO

#include <types.h>
#include <mem.h>
#include <time.h>
#include <io.h>
#include <event.h>
#include <string.h>

static Item timer_io;
static uint32 start_time;
static uint32 last_frame_time;

void platform_init(void) {
    /* Create timer for timing */
    timer_io = CreateTimerIOReq();

    /* Get initial time */
    IOInfo io_info;
    memset(&io_info, 0, sizeof(IOInfo));
    io_info.ioi_Command = CMD_READ;
    DoIO(timer_io, &io_info);

    start_time = io_info.ioi_Offset;
    last_frame_time = start_time;
}

void platform_shutdown(void) {
    DeleteItem(timer_io);
}

uint32 platform_get_ticks(void) {
    IOInfo io_info;
    memset(&io_info, 0, sizeof(IOInfo));
    io_info.ioi_Command = CMD_READ;
    DoIO(timer_io, &io_info);

    /* Convert to milliseconds (3DO timer is in microseconds) */
    return (io_info.ioi_Offset - start_time) / 1000;
}

float platform_get_delta_time(void) {
    IOInfo io_info;
    memset(&io_info, 0, sizeof(IOInfo));
    io_info.ioi_Command = CMD_READ;
    DoIO(timer_io, &io_info);

    uint32 current = io_info.ioi_Offset;
    uint32 delta = current - last_frame_time;
    last_frame_time = current;

    /* Convert microseconds to seconds */
    return (float)delta / 1000000.0f;
}

void platform_sleep(uint32 ms) {
    WaitTime(timer_io, 0, ms * 1000);  /* Convert to microseconds */
}

void *platform_alloc(size_t size) {
    return AllocMem(size, MEMTYPE_ANY);
}

void platform_free(void *ptr) {
    FreeMem(ptr, -1);  /* -1 = system figures out size */
}

const char *platform_get_name(void) {
    return "3DO";
}

int platform_get_screen_width(void) {
    return 320;
}

int platform_get_screen_height(void) {
    return 240;
}

#endif /* PLATFORM_3DO */
