/*
 * RetroRacer - Original Xbox Input Implementation
 * Uses OpenXDK/nxdk for Duke/Controller S support
 */

#include "input.h"
#include "platform.h"

#ifdef PLATFORM_XBOX

#include <hal/xbox.h>
#include <hal/input.h>
#include <xboxkrnl/xboxkrnl.h>
#include <string.h>

#define MAX_PLAYERS 4
#define DEFAULT_DEADZONE 0.15f

static input_state_t input_states[MAX_PLAYERS];
static uint32_t prev_buttons[MAX_PLAYERS];
static float deadzone = DEFAULT_DEADZONE;

/* Xbox gamepad state */
static XPAD_INPUT g_Pads[MAX_PLAYERS];

void input_init(void) {
    memset(input_states, 0, sizeof(input_states));
    memset(prev_buttons, 0, sizeof(prev_buttons));
    memset(g_Pads, 0, sizeof(g_Pads));
    deadzone = DEFAULT_DEADZONE;

    /* Initialize input subsystem */
    XInitInput();
}

void input_update(void) {
    /* Poll all controllers */
    XGetPadInput();

    for (int i = 0; i < MAX_PLAYERS; i++) {
        prev_buttons[i] = input_states[i].buttons;
        memset(&input_states[i], 0, sizeof(input_state_t));

        /* Check if controller is connected */
        if (!XInputGetState(i, &g_Pads[i])) {
            continue;
        }

        input_states[i].connected = 1;

        XPAD_INPUT *pad = &g_Pads[i];

        /* Map D-pad (hat switch on Xbox) */
        if (pad->wButtons & XPAD_DPAD_UP)    input_states[i].buttons |= BTN_DPAD_UP;
        if (pad->wButtons & XPAD_DPAD_DOWN)  input_states[i].buttons |= BTN_DPAD_DOWN;
        if (pad->wButtons & XPAD_DPAD_LEFT)  input_states[i].buttons |= BTN_DPAD_LEFT;
        if (pad->wButtons & XPAD_DPAD_RIGHT) input_states[i].buttons |= BTN_DPAD_RIGHT;

        /* Face buttons */
        if (pad->bAnalogButtons[XPAD_A] > 32) input_states[i].buttons |= BTN_A;
        if (pad->bAnalogButtons[XPAD_B] > 32) input_states[i].buttons |= BTN_B;
        if (pad->bAnalogButtons[XPAD_X] > 32) input_states[i].buttons |= BTN_X;
        if (pad->bAnalogButtons[XPAD_Y] > 32) input_states[i].buttons |= BTN_Y;

        /* Start and Back */
        if (pad->wButtons & XPAD_START) input_states[i].buttons |= BTN_START;
        if (pad->wButtons & XPAD_BACK)  input_states[i].buttons |= BTN_Z;

        /* Shoulder buttons (White and Black on Duke) */
        if (pad->bAnalogButtons[XPAD_WHITE] > 32) input_states[i].buttons |= BTN_C;
        if (pad->bAnalogButtons[XPAD_BLACK] > 32) input_states[i].buttons |= BTN_D;

        /* Analog sticks (signed 16-bit, -32768 to 32767) */
        int lx = pad->sThumbLX;
        int ly = pad->sThumbLY;
        int rx = pad->sThumbRX;
        int ry = pad->sThumbRY;

        input_states[i].stick_x = lx / 256;  /* Convert to 8-bit range */
        input_states[i].stick_y = ly / 256;

        input_states[i].analog_x = (float)lx / 32767.0f;
        input_states[i].analog_y = (float)ly / 32767.0f;

        /* Clamp */
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

        /* Xbox triggers (0-255 analog) */
        input_states[i].ltrig = pad->bAnalogButtons[XPAD_LEFT_TRIGGER];
        input_states[i].rtrig = pad->bAnalogButtons[XPAD_RIGHT_TRIGGER];
        input_states[i].trigger_l = (float)pad->bAnalogButtons[XPAD_LEFT_TRIGGER] / 255.0f;
        input_states[i].trigger_r = (float)pad->bAnalogButtons[XPAD_RIGHT_TRIGGER] / 255.0f;

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
    /* Right trigger for throttle */
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
    /* Left trigger for brake */
    if (state->trigger_l > 0.1f) {
        return state->trigger_l;
    }

    /* Fall back to X button */
    if (input_button_held(state, BTN_X)) {
        return 1.0f;
    }

    return 0.0f;
}

void input_set_deadzone(float dz) {
    deadzone = dz;
}

#endif /* PLATFORM_XBOX */
