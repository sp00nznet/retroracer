/*
 * RetroRacer - PlayStation 3 Input Implementation
 * Uses PSL1GHT SDK with libpad for SIXAXIS/DualShock 3
 */

#include "input.h"
#include "platform.h"

#ifdef PLATFORM_PS3

#include <psl1ght/lv2.h>
#include <io/pad.h>
#include <string.h>

#define MAX_PLAYERS 4
#define DEFAULT_DEADZONE 0.15f

static input_state_t input_states[MAX_PLAYERS];
static uint32_t prev_buttons[MAX_PLAYERS];
static float deadzone = DEFAULT_DEADZONE;

static padInfo pad_info;
static padData pad_data[MAX_PLAYERS];

void input_init(void) {
    memset(input_states, 0, sizeof(input_states));
    memset(prev_buttons, 0, sizeof(prev_buttons));
    deadzone = DEFAULT_DEADZONE;

    /* Initialize pad library */
    ioPadInit(MAX_PLAYERS);
}

void input_update(void) {
    /* Get pad info */
    ioPadGetInfo(&pad_info);

    for (int i = 0; i < MAX_PLAYERS; i++) {
        prev_buttons[i] = input_states[i].buttons;
        memset(&input_states[i], 0, sizeof(input_state_t));

        if (!pad_info.status[i]) {
            continue;
        }

        /* Read pad data */
        if (ioPadGetData(i, &pad_data[i]) != 0) {
            continue;
        }

        input_states[i].connected = 1;

        padData *pad = &pad_data[i];

        /* Map buttons */
        if (pad->BTN_UP)        input_states[i].buttons |= BTN_DPAD_UP;
        if (pad->BTN_DOWN)      input_states[i].buttons |= BTN_DPAD_DOWN;
        if (pad->BTN_LEFT)      input_states[i].buttons |= BTN_DPAD_LEFT;
        if (pad->BTN_RIGHT)     input_states[i].buttons |= BTN_DPAD_RIGHT;

        if (pad->BTN_TRIANGLE)  input_states[i].buttons |= BTN_Y;
        if (pad->BTN_CROSS)     input_states[i].buttons |= BTN_A;
        if (pad->BTN_SQUARE)    input_states[i].buttons |= BTN_X;
        if (pad->BTN_CIRCLE)    input_states[i].buttons |= BTN_B;

        if (pad->BTN_START)     input_states[i].buttons |= BTN_START;
        if (pad->BTN_SELECT)    input_states[i].buttons |= BTN_Z;

        if (pad->BTN_L1)        input_states[i].buttons |= BTN_C;
        if (pad->BTN_R1)        input_states[i].buttons |= BTN_D;

        /* Analog sticks (0-255, centered at 128) */
        int lx = pad->ANA_L_H - 128;
        int ly = pad->ANA_L_V - 128;
        int rx = pad->ANA_R_H - 128;
        int ry = pad->ANA_R_V - 128;

        input_states[i].stick_x = lx;
        input_states[i].stick_y = ly;

        input_states[i].analog_x = (float)lx / 128.0f;
        input_states[i].analog_y = (float)ly / 128.0f;

        /* Clamp analog values */
        if (input_states[i].analog_x > 1.0f) input_states[i].analog_x = 1.0f;
        if (input_states[i].analog_x < -1.0f) input_states[i].analog_x = -1.0f;
        if (input_states[i].analog_y > 1.0f) input_states[i].analog_y = 1.0f;
        if (input_states[i].analog_y < -1.0f) input_states[i].analog_y = -1.0f;

        /* Apply deadzone */
        if (input_states[i].analog_x > -deadzone && input_states[i].analog_x < deadzone) {
            input_states[i].analog_x = 0;
        }
        if (input_states[i].analog_y > -deadzone && input_states[i].analog_y < deadzone) {
            input_states[i].analog_y = 0;
        }

        /* Pressure-sensitive triggers (0-255) */
        input_states[i].ltrig = pad->PRE_L2;
        input_states[i].rtrig = pad->PRE_R2;
        input_states[i].trigger_l = (float)pad->PRE_L2 / 255.0f;
        input_states[i].trigger_r = (float)pad->PRE_R2 / 255.0f;

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
    /* Use R2 trigger for throttle */
    if (state->trigger_r > 0.1f) {
        return state->trigger_r;
    }

    /* Fall back to X button */
    if (input_button_held(state, BTN_A)) {
        return 1.0f;
    }

    return 0.0f;
}

float input_get_brake(input_state_t *state) {
    /* Use L2 trigger for brake */
    if (state->trigger_l > 0.1f) {
        return state->trigger_l;
    }

    /* Fall back to Square button */
    if (input_button_held(state, BTN_X)) {
        return 1.0f;
    }

    return 0.0f;
}

void input_set_deadzone(float dz) {
    deadzone = dz;
}

#endif /* PLATFORM_PS3 */
