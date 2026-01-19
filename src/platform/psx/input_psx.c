/*
 * RetroRacer - PlayStation 1 (PSX) Input Implementation
 * Uses Sony PsyQ SDK libpad
 */

#include "input.h"
#include "platform.h"

#ifdef PLATFORM_PSX

#include <sys/types.h>
#include <libetc.h>
#include <libpad.h>

#define MAX_PLAYERS 2
#define DEFAULT_DEADZONE 0.15f

static input_state_t input_states[MAX_PLAYERS];
static uint32_t prev_buttons[MAX_PLAYERS];
static float deadzone = DEFAULT_DEADZONE;

/* PSX controller buffers */
static u_char pad_buff[2][34];

void input_init(void) {
    memset(input_states, 0, sizeof(input_states));
    memset(prev_buttons, 0, sizeof(prev_buttons));
    deadzone = DEFAULT_DEADZONE;

    /* Initialize pad system */
    PadInitDirect(pad_buff[0], pad_buff[1]);
    PadStartCom();
}

void input_update(void) {
    for (int i = 0; i < MAX_PLAYERS; i++) {
        prev_buttons[i] = input_states[i].buttons;
        memset(&input_states[i], 0, sizeof(input_state_t));

        /* Check controller status */
        if (pad_buff[i][0] == 0) {
            /* Controller disconnected */
            continue;
        }

        input_states[i].connected = 1;

        /* Read button state (active low on PSX) */
        uint16_t pad_data = ~((pad_buff[i][2] << 8) | pad_buff[i][3]);

        /* Map PSX buttons to generic buttons */
        if (pad_data & PADLup)    input_states[i].buttons |= BTN_DPAD_UP;
        if (pad_data & PADLdown)  input_states[i].buttons |= BTN_DPAD_DOWN;
        if (pad_data & PADLleft)  input_states[i].buttons |= BTN_DPAD_LEFT;
        if (pad_data & PADLright) input_states[i].buttons |= BTN_DPAD_RIGHT;

        if (pad_data & PADRup)    input_states[i].buttons |= BTN_Y;    /* Triangle */
        if (pad_data & PADRdown)  input_states[i].buttons |= BTN_A;    /* Cross */
        if (pad_data & PADRleft)  input_states[i].buttons |= BTN_X;    /* Square */
        if (pad_data & PADRright) input_states[i].buttons |= BTN_B;    /* Circle */

        if (pad_data & PADstart)  input_states[i].buttons |= BTN_START;
        if (pad_data & PADselect) input_states[i].buttons |= BTN_Z;    /* Select mapped to Z */

        if (pad_data & PADL1)     input_states[i].buttons |= BTN_C;    /* L1 */
        if (pad_data & PADR1)     input_states[i].buttons |= BTN_D;    /* R1 */

        /* Check for DualShock analog controller */
        if (pad_buff[i][1] == 0x73 || pad_buff[i][1] == 0x53) {
            /* Analog mode - read sticks */
            /* Right stick: bytes 4,5  Left stick: bytes 6,7 */
            int lx = pad_buff[i][6] - 128;
            int ly = pad_buff[i][7] - 128;

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

            /* L2/R2 as triggers (if DualShock 2 connected via adapter) */
            if (pad_data & PADL2) {
                input_states[i].trigger_l = 1.0f;
                input_states[i].ltrig = 255;
            }
            if (pad_data & PADR2) {
                input_states[i].trigger_r = 1.0f;
                input_states[i].rtrig = 255;
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

    /* Cross button (A) for acceleration */
    if (input_button_held(state, BTN_A)) {
        return 1.0f;
    }

    return 0.0f;
}

float input_get_brake(input_state_t *state) {
    if (state->trigger_l > 0.1f) {
        return state->trigger_l;
    }

    /* Square button (X) for brake */
    if (input_button_held(state, BTN_X)) {
        return 1.0f;
    }

    return 0.0f;
}

void input_set_deadzone(float dz) {
    deadzone = dz;
}

#endif /* PLATFORM_PSX */
