/*
 * RetroRacer - PlayStation 2 Input Implementation
 * Uses PS2SDK libpad for DualShock 2 support
 */

#include "input.h"
#include "platform.h"

#ifdef PLATFORM_PS2

#include <tamtypes.h>
#include <kernel.h>
#include <sifrpc.h>
#include <loadfile.h>
#include <libpad.h>

#define MAX_PLAYERS 2
#define DEFAULT_DEADZONE 0.15f

static input_state_t input_states[MAX_PLAYERS];
static uint32_t prev_buttons[MAX_PLAYERS];
static float deadzone = DEFAULT_DEADZONE;

/* Pad DMA buffers - must be 64-byte aligned */
static char pad_buffer[2][256] __attribute__((aligned(64)));
static int pad_connected[MAX_PLAYERS];

void input_init(void) {
    memset(input_states, 0, sizeof(input_states));
    memset(prev_buttons, 0, sizeof(prev_buttons));
    memset(pad_connected, 0, sizeof(pad_connected));
    deadzone = DEFAULT_DEADZONE;

    /* Initialize pad library */
    padInit(0);

    /* Open ports for both players */
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (padPortOpen(i, 0, pad_buffer[i]) == 0) {
            pad_connected[i] = 0;
        } else {
            pad_connected[i] = 1;
        }
    }
}

void input_update(void) {
    struct padButtonStatus buttons;

    for (int i = 0; i < MAX_PLAYERS; i++) {
        prev_buttons[i] = input_states[i].buttons;
        memset(&input_states[i], 0, sizeof(input_state_t));

        if (!pad_connected[i]) continue;

        int state = padGetState(i, 0);
        if (state != PAD_STATE_STABLE && state != PAD_STATE_FINDCTP1) {
            continue;
        }

        int ret = padRead(i, 0, &buttons);
        if (ret != 0) {
            input_states[i].connected = 1;

            /* PS2 buttons are active low */
            uint16_t pad = ~buttons.btns;

            /* Map buttons */
            if (pad & PAD_UP)       input_states[i].buttons |= BTN_DPAD_UP;
            if (pad & PAD_DOWN)     input_states[i].buttons |= BTN_DPAD_DOWN;
            if (pad & PAD_LEFT)     input_states[i].buttons |= BTN_DPAD_LEFT;
            if (pad & PAD_RIGHT)    input_states[i].buttons |= BTN_DPAD_RIGHT;

            if (pad & PAD_TRIANGLE) input_states[i].buttons |= BTN_Y;
            if (pad & PAD_CROSS)    input_states[i].buttons |= BTN_A;
            if (pad & PAD_SQUARE)   input_states[i].buttons |= BTN_X;
            if (pad & PAD_CIRCLE)   input_states[i].buttons |= BTN_B;

            if (pad & PAD_START)    input_states[i].buttons |= BTN_START;
            if (pad & PAD_SELECT)   input_states[i].buttons |= BTN_Z;

            if (pad & PAD_L1)       input_states[i].buttons |= BTN_C;
            if (pad & PAD_R1)       input_states[i].buttons |= BTN_D;
            if (pad & PAD_L2)       input_states[i].ltrig = 255;
            if (pad & PAD_R2)       input_states[i].rtrig = 255;

            /* Analog sticks (0-255, centered at 128) */
            int lx = buttons.ljoy_h - 128;
            int ly = buttons.ljoy_v - 128;
            int rx = buttons.rjoy_h - 128;
            int ry = buttons.rjoy_v - 128;

            input_states[i].stick_x = lx;
            input_states[i].stick_y = ly;

            input_states[i].analog_x = (float)lx / 128.0f;
            input_states[i].analog_y = (float)ly / 128.0f;

            /* Apply deadzone */
            if (input_states[i].analog_x > -deadzone && input_states[i].analog_x < deadzone) {
                input_states[i].analog_x = 0;
            }
            if (input_states[i].analog_y > -deadzone && input_states[i].analog_y < deadzone) {
                input_states[i].analog_y = 0;
            }

            /* Pressure-sensitive triggers (DualShock 2) */
            /* Use L2/R2 pressure values if available */
            int mode = padInfoMode(i, 0, PAD_MODETABLE, -1);
            if (mode != 0) {
                /* Has pressure sensitive buttons */
                input_states[i].trigger_l = (float)buttons.l2_p / 255.0f;
                input_states[i].trigger_r = (float)buttons.r2_p / 255.0f;
                input_states[i].ltrig = buttons.l2_p;
                input_states[i].rtrig = buttons.r2_p;
            } else {
                /* Digital triggers */
                input_states[i].trigger_l = (pad & PAD_L2) ? 1.0f : 0.0f;
                input_states[i].trigger_r = (pad & PAD_R2) ? 1.0f : 0.0f;
            }
        }

        /* Calculate pressed/released */
        input_states[i].pressed = input_states[i].buttons & ~prev_buttons[i];
        input_states[i].released = ~input_states[i].buttons & prev_buttons[i];
    }
}

input_state_t *input_get_state(int player) {
    if (player < 0 || player >= MAX_PLAYERS) {
        return &input_states[0];
    }
    return &input_states[player];
}

int input_button_held(input_state_t *state, uint32_t button) {
    return (state->buttons & button) != 0;
}

int input_button_pressed(input_state_t *state, uint32_t button) {
    return (state->pressed & button) != 0;
}

int input_button_released(input_state_t *state, uint32_t button) {
    return (state->released & button) != 0;
}

float input_get_steering(input_state_t *state) {
    if (state->analog_x != 0) {
        return state->analog_x;
    }

    if (input_button_held(state, BTN_DPAD_LEFT)) {
        return -1.0f;
    }
    if (input_button_held(state, BTN_DPAD_RIGHT)) {
        return 1.0f;
    }

    return 0.0f;
}

float input_get_throttle(input_state_t *state) {
    if (state->trigger_r > 0.1f) {
        return state->trigger_r;
    }

    if (input_button_held(state, BTN_A)) {
        return 1.0f;
    }

    return 0.0f;
}

float input_get_brake(input_state_t *state) {
    if (state->trigger_l > 0.1f) {
        return state->trigger_l;
    }

    if (input_button_held(state, BTN_X)) {
        return 1.0f;
    }

    return 0.0f;
}

void input_set_deadzone(float dz) {
    deadzone = dz;
}

#endif /* PLATFORM_PS2 */
