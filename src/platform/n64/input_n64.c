/*
 * RetroRacer - Nintendo 64 Input Implementation
 * Uses libdragon joypad for N64 controller
 */

#include "input.h"
#include "platform.h"

#ifdef PLATFORM_N64

#include <libdragon.h>

#define MAX_PLAYERS 4
#define DEFAULT_DEADZONE 0.15f

/* N64 analog stick range is roughly -80 to +80 */
#define N64_STICK_MAX 80.0f

static input_state_t input_states[MAX_PLAYERS];
static uint32_t prev_buttons[MAX_PLAYERS];
static float deadzone = DEFAULT_DEADZONE;

void input_init(void) {
    memset(input_states, 0, sizeof(input_states));
    memset(prev_buttons, 0, sizeof(prev_buttons));
    deadzone = DEFAULT_DEADZONE;

    /* Initialize joypad subsystem */
    joypad_init();
}

void input_update(void) {
    /* Poll controllers */
    joypad_poll();

    for (int i = 0; i < MAX_PLAYERS; i++) {
        prev_buttons[i] = input_states[i].buttons;
        memset(&input_states[i], 0, sizeof(input_state_t));

        /* Check if controller is connected */
        joypad_port_t port = (joypad_port_t)i;
        if (!joypad_is_connected(port)) {
            continue;
        }

        input_states[i].connected = 1;

        /* Get button state */
        joypad_buttons_t btns = joypad_get_buttons(port);
        joypad_buttons_t pressed = joypad_get_buttons_pressed(port);
        joypad_buttons_t released = joypad_get_buttons_released(port);

        /* Map N64 buttons to generic buttons */
        if (btns.a)       input_states[i].buttons |= BTN_A;
        if (btns.b)       input_states[i].buttons |= BTN_B;
        if (btns.start)   input_states[i].buttons |= BTN_START;
        if (btns.z)       input_states[i].buttons |= BTN_Z;

        /* C buttons */
        if (btns.c_up)    input_states[i].buttons |= BTN_Y;
        if (btns.c_down)  input_states[i].buttons |= BTN_X;
        if (btns.c_left)  input_states[i].buttons |= BTN_C;
        if (btns.c_right) input_states[i].buttons |= BTN_D;

        /* D-pad */
        if (btns.d_up)    input_states[i].buttons |= BTN_DPAD_UP;
        if (btns.d_down)  input_states[i].buttons |= BTN_DPAD_DOWN;
        if (btns.d_left)  input_states[i].buttons |= BTN_DPAD_LEFT;
        if (btns.d_right) input_states[i].buttons |= BTN_DPAD_RIGHT;

        /* Shoulder buttons - L and R */
        if (btns.l) {
            input_states[i].ltrig = 255;
            input_states[i].trigger_l = 1.0f;
        }
        if (btns.r) {
            input_states[i].rtrig = 255;
            input_states[i].trigger_r = 1.0f;
        }

        /* Analog stick */
        joypad_inputs_t inputs = joypad_get_inputs(port);

        input_states[i].stick_x = inputs.stick_x;
        input_states[i].stick_y = inputs.stick_y;

        input_states[i].analog_x = (float)inputs.stick_x / N64_STICK_MAX;
        input_states[i].analog_y = (float)inputs.stick_y / N64_STICK_MAX;

        /* Clamp to -1 to 1 */
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

        /* Calculate pressed/released from libdragon's tracking */
        if (pressed.a)       input_states[i].pressed |= BTN_A;
        if (pressed.b)       input_states[i].pressed |= BTN_B;
        if (pressed.start)   input_states[i].pressed |= BTN_START;
        if (pressed.z)       input_states[i].pressed |= BTN_Z;
        if (pressed.d_up)    input_states[i].pressed |= BTN_DPAD_UP;
        if (pressed.d_down)  input_states[i].pressed |= BTN_DPAD_DOWN;
        if (pressed.d_left)  input_states[i].pressed |= BTN_DPAD_LEFT;
        if (pressed.d_right) input_states[i].pressed |= BTN_DPAD_RIGHT;

        if (released.a)       input_states[i].released |= BTN_A;
        if (released.b)       input_states[i].released |= BTN_B;
        if (released.start)   input_states[i].released |= BTN_START;
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

    /* Fall back to D-pad */
    if (input_button_held(state, BTN_DPAD_LEFT)) {
        return -1.0f;
    }
    if (input_button_held(state, BTN_DPAD_RIGHT)) {
        return 1.0f;
    }

    return 0.0f;
}

float input_get_throttle(input_state_t *state) {
    /* A button for throttle (like Mario Kart) */
    if (input_button_held(state, BTN_A)) {
        return 1.0f;
    }

    /* Or use Z trigger */
    if (input_button_held(state, BTN_Z)) {
        return 1.0f;
    }

    return 0.0f;
}

float input_get_brake(input_state_t *state) {
    /* B button for brake */
    if (input_button_held(state, BTN_B)) {
        return 1.0f;
    }

    /* R shoulder for brake/drift */
    if (state->trigger_r > 0.5f) {
        return 1.0f;
    }

    return 0.0f;
}

void input_set_deadzone(float dz) {
    deadzone = dz;
}

#endif /* PLATFORM_N64 */
