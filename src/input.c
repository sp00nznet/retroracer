/*
 * RetroRacer - Input System Implementation
 * Dreamcast Controller Handling
 */

#include "input.h"
#include <string.h>

#ifdef DREAMCAST
#include <kos.h>
#else
/* Stub for non-Dreamcast builds */
#endif

#define MAX_PLAYERS 4
#define DEFAULT_DEADZONE 0.15f

static input_state_t input_states[MAX_PLAYERS];
static uint32_t prev_buttons[MAX_PLAYERS];
static float deadzone = DEFAULT_DEADZONE;

void input_init(void) {
    memset(input_states, 0, sizeof(input_states));
    memset(prev_buttons, 0, sizeof(prev_buttons));
    deadzone = DEFAULT_DEADZONE;

#ifdef DREAMCAST
    /* Initialize maple (controller) bus */
    maple_init();
#endif
}

void input_update(void) {
    for (int i = 0; i < MAX_PLAYERS; i++) {
        prev_buttons[i] = input_states[i].buttons;
        memset(&input_states[i], 0, sizeof(input_state_t));

#ifdef DREAMCAST
        maple_device_t *cont;
        cont_state_t *state;

        cont = maple_enum_type(i, MAPLE_FUNC_CONTROLLER);
        if (cont) {
            input_states[i].connected = 1;
            state = (cont_state_t *)maple_dev_status(cont);
            if (state) {
                /* Map buttons */
                if (state->buttons & CONT_C) input_states[i].buttons |= BTN_C;
                if (state->buttons & CONT_B) input_states[i].buttons |= BTN_B;
                if (state->buttons & CONT_A) input_states[i].buttons |= BTN_A;
                if (state->buttons & CONT_START) input_states[i].buttons |= BTN_START;
                if (state->buttons & CONT_DPAD_UP) input_states[i].buttons |= BTN_DPAD_UP;
                if (state->buttons & CONT_DPAD_DOWN) input_states[i].buttons |= BTN_DPAD_DOWN;
                if (state->buttons & CONT_DPAD_LEFT) input_states[i].buttons |= BTN_DPAD_LEFT;
                if (state->buttons & CONT_DPAD_RIGHT) input_states[i].buttons |= BTN_DPAD_RIGHT;
                if (state->buttons & CONT_Z) input_states[i].buttons |= BTN_Z;
                if (state->buttons & CONT_Y) input_states[i].buttons |= BTN_Y;
                if (state->buttons & CONT_X) input_states[i].buttons |= BTN_X;
                if (state->buttons & CONT_D) input_states[i].buttons |= BTN_D;

                /* Analog stick */
                input_states[i].stick_x = state->joyx;
                input_states[i].stick_y = state->joyy;

                /* Normalize analog */
                input_states[i].analog_x = (float)state->joyx / 127.0f;
                input_states[i].analog_y = (float)state->joyy / 127.0f;

                /* Apply deadzone */
                if (input_states[i].analog_x > -deadzone && input_states[i].analog_x < deadzone) {
                    input_states[i].analog_x = 0;
                }
                if (input_states[i].analog_y > -deadzone && input_states[i].analog_y < deadzone) {
                    input_states[i].analog_y = 0;
                }

                /* Triggers */
                input_states[i].ltrig = state->ltrig;
                input_states[i].rtrig = state->rtrig;
                input_states[i].trigger_l = (float)state->ltrig / 255.0f;
                input_states[i].trigger_r = (float)state->rtrig / 255.0f;
            }
        }
#else
        /* Simulation for testing - player 1 always connected */
        if (i == 0) {
            input_states[i].connected = 1;
        }
#endif

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
    /* Prefer analog stick */
    if (state->analog_x != 0) {
        return state->analog_x;
    }

    /* Fall back to d-pad */
    if (input_button_held(state, BTN_DPAD_LEFT)) {
        return -1.0f;
    }
    if (input_button_held(state, BTN_DPAD_RIGHT)) {
        return 1.0f;
    }

    return 0.0f;
}

float input_get_throttle(input_state_t *state) {
    /* Prefer right trigger */
    if (state->trigger_r > 0.1f) {
        return state->trigger_r;
    }

    /* Fall back to A button */
    if (input_button_held(state, BTN_A)) {
        return 1.0f;
    }

    return 0.0f;
}

float input_get_brake(input_state_t *state) {
    /* Prefer left trigger */
    if (state->trigger_l > 0.1f) {
        return state->trigger_l;
    }

    /* Fall back to B button */
    if (input_button_held(state, BTN_B)) {
        return 1.0f;
    }

    return 0.0f;
}

void input_set_deadzone(float dz) {
    deadzone = dz;
}
