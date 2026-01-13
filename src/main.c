/*
 * RetroRacer - Main Entry Point
 * A Dreamcast 3D Racing Game
 *
 * Features:
 * - 4 game modes: AI Race, Single Track, Time Trial, Grand Prix
 * - Procedurally generated tracks
 * - N64-style low-poly graphics
 * - AI opponents with difficulty levels
 */

#include <stdio.h>
#include <stdlib.h>

#ifdef DREAMCAST
#include <kos.h>

/* KOS_INIT_FLAGS - enable romdisk if needed */
KOS_INIT_FLAGS(INIT_DEFAULT);
#else
/* For non-Dreamcast builds (testing/development) */
#include <time.h>
#endif

#include "game.h"
#include "render.h"
#include "input.h"

/* Target frame rate */
#define TARGET_FPS 60
#define FRAME_TIME (1.0f / TARGET_FPS)

/* Game running flag */
static int running = 1;

#ifdef DREAMCAST
/* Dreamcast timing using TMU */
static uint64_t get_time_us(void) {
    return timer_us_gettime64();
}
#else
/* POSIX timing for development */
#include <sys/time.h>

static uint64_t get_time_us(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000000 + tv.tv_usec;
}
#endif

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    printf("RetroRacer - Dreamcast Racing Game\n");
    printf("===================================\n");
    printf("Starting up...\n");

#ifdef DREAMCAST
    /* Initialize Dreamcast hardware */
    pvr_init_defaults();
    printf("PVR initialized\n");
#endif

    /* Initialize game systems */
    printf("Initializing game...\n");
    game_init();
    printf("Game initialized!\n");

    printf("\nGame Modes:\n");
    printf("  1. AI Race     - Watch AI race themselves\n");
    printf("  2. Single Track - Race one track\n");
    printf("  3. Time Trial  - Beat the best time\n");
    printf("  4. Grand Prix  - 4-race championship\n\n");

    /* Main game loop */
    uint64_t last_time = get_time_us();
    uint64_t accumulator = 0;
    uint64_t frame_time_us = (uint64_t)(FRAME_TIME * 1000000);

    printf("Entering main loop...\n");

    while (running) {
        uint64_t current_time = get_time_us();
        uint64_t elapsed = current_time - last_time;
        last_time = current_time;

        /* Clamp elapsed time to avoid spiral of death */
        if (elapsed > 100000) {
            elapsed = 100000;  /* Max 100ms */
        }

        accumulator += elapsed;

        /* Fixed timestep updates */
        while (accumulator >= frame_time_us) {
            game_update(FRAME_TIME);
            accumulator -= frame_time_us;
        }

        /* Render */
        game_render();

#ifdef DREAMCAST
        /* Check for exit button combination (A + B + X + Y + Start) */
        maple_device_t *cont = maple_enum_type(0, MAPLE_FUNC_CONTROLLER);
        if (cont) {
            cont_state_t *state = (cont_state_t *)maple_dev_status(cont);
            if (state) {
                if ((state->buttons & CONT_A) &&
                    (state->buttons & CONT_B) &&
                    (state->buttons & CONT_X) &&
                    (state->buttons & CONT_Y) &&
                    (state->buttons & CONT_START)) {
                    running = 0;
                }
            }
        }
#else
        /* For non-DC builds, run for a short time then exit */
        static int frame_count = 0;
        frame_count++;
        if (frame_count > 60 * 5) {  /* 5 seconds */
            /* Just for testing - would be removed for actual builds */
            /* running = 0; */
        }
#endif
    }

    /* Cleanup */
    printf("Shutting down...\n");
    game_shutdown();

#ifdef DREAMCAST
    /* Return to Dreamcast BIOS */
    arch_exit();
#endif

    return 0;
}

/*
 * RetroRacer Controls:
 *
 * Menu:
 *   D-Pad Up/Down - Navigate
 *   A / Start     - Select
 *   B             - Back
 *
 * Racing:
 *   Analog Stick / D-Pad Left/Right - Steering
 *   A / Right Trigger               - Accelerate
 *   B / Left Trigger                - Brake
 *   Start                           - Pause
 *
 * Exit:
 *   Hold A + B + X + Y + Start simultaneously
 */
