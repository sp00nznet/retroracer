/*
 * RetroRacer - Super Nintendo (SNES) Input Implementation
 * Uses PVSnesLib for controller access
 */

#include "input.h"
#include "platform.h"

#ifdef PLATFORM_SNES

#include <snes.h>

#define MAX_PLAYERS 2  /* SNES has 2 controller ports (up to 4 with multitap) */
#define DEFAULT_DEADZONE 0.15f

static input_state_t input_states[MAX_PLAYERS];
static uint32_t prev_buttons[MAX_PLAYERS];
static float deadzone = DEFAULT_DEADZONE;

void input_init(void) {
    memset(input_states, 0, sizeof(input_states));
    memset(prev_buttons, 0, sizeof(prev_buttons));
    deadzone = DEFAULT_DEADZONE;

    /* PVSnesLib handles controller initialization */
}

void input_update(void) {
    /* Scan controllers */
    scanPads();

    for (int i = 0; i < MAX_PLAYERS; i++) {
        prev_buttons[i] = input_states[i].buttons;
        memset(&input_states[i], 0, sizeof(input_state_t));

        /* Get pad state */
        uint16_t pad = padsCurrent(i);

        /* Check if controller connected (any button state means connected) */
        input_states[i].connected = 1;

        /* Map SNES buttons to generic buttons */
        if (pad & KEY_A)      input_states[i].buttons |= BTN_A;
        if (pad & KEY_B)      input_states[i].buttons |= BTN_B;
        if (pad & KEY_X)      input_states[i].buttons |= BTN_X;
        if (pad & KEY_Y)      input_states[i].buttons |= BTN_Y;

        if (pad & KEY_START)  input_states[i].buttons |= BTN_START;
        if (pad & KEY_SELECT) input_states[i].buttons |= BTN_Z;

        if (pad & KEY_L)      input_states[i].buttons |= BTN_C;
        if (pad & KEY_R)      input_states[i].buttons |= BTN_D;

        /* D-pad */
        if (pad & KEY_UP)     input_states[i].buttons |= BTN_DPAD_UP;
        if (pad & KEY_DOWN)   input_states[i].buttons |= BTN_DPAD_DOWN;
        if (pad & KEY_LEFT)   input_states[i].buttons |= BTN_DPAD_LEFT;
        if (pad & KEY_RIGHT)  input_states[i].buttons |= BTN_DPAD_RIGHT;

        /* SNES has no analog - simulate from D-pad */
        if (pad & KEY_LEFT) {
            input_states[i].analog_x = -1.0f;
            input_states[i].stick_x = -127;
        } else if (pad & KEY_RIGHT) {
            input_states[i].analog_x = 1.0f;
            input_states[i].stick_x = 127;
        }

        if (pad & KEY_UP) {
            input_states[i].analog_y = -1.0f;
            input_states[i].stick_y = -127;
        } else if (pad & KEY_DOWN) {
            input_states[i].analog_y = 1.0f;
            input_states[i].stick_y = 127;
        }

        /* L/R as shoulder "triggers" */
        if (pad & KEY_L) {
            input_states[i].trigger_l = 1.0f;
            input_states[i].ltrig = 255;
        }
        if (pad & KEY_R) {
            input_states[i].trigger_r = 1.0f;
            input_states[i].rtrig = 255;
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
    /* D-pad left/right for steering */
    if (input_button_held(state, BTN_DPAD_LEFT)) {
        return -1.0f;
    }
    if (input_button_held(state, BTN_DPAD_RIGHT)) {
        return 1.0f;
    }

    return 0.0f;
}

float input_get_throttle(input_state_t *state) {
    /* B button for throttle (like F-Zero) */
    if (input_button_held(state, BTN_B)) {
        return 1.0f;
    }

    /* Or Y button */
    if (input_button_held(state, BTN_Y)) {
        return 1.0f;
    }

    return 0.0f;
}

float input_get_brake(input_state_t *state) {
    /* A button for brake */
    if (input_button_held(state, BTN_A)) {
        return 1.0f;
    }

    /* L/R for sharp turns (like F-Zero) */
    if (input_button_held(state, BTN_C) || input_button_held(state, BTN_D)) {
        return 0.5f;
    }

    return 0.0f;
}

void input_set_deadzone(float dz) {
    deadzone = dz;
}

#endif /* PLATFORM_SNES */
