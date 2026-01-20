/*
 * RetroRacer - Nintendo 3DS Input Implementation
 * Uses libctru for input handling
 *
 * 3DS Input:
 * - Circle Pad (analog stick)
 * - D-Pad
 * - A, B, X, Y buttons
 * - L, R shoulder buttons
 * - ZL, ZR (New 3DS only)
 * - Start, Select
 * - Touchscreen
 * - C-Stick (New 3DS only)
 */

#include "input.h"
#include "platform.h"

#ifdef PLATFORM_3DS

#include <3ds.h>
#include <string.h>

/* Deadzone for analog inputs */
#define ANALOG_DEADZONE 15

/* Previous frame state for edge detection */
static u32 keys_down_prev = 0;
static input_state_t current_state;

void input_init(void) {
    memset(&current_state, 0, sizeof(input_state_t));
    keys_down_prev = 0;
}

void input_shutdown(void) {
    /* Nothing to clean up */
}

void input_update(void) {
    /* Scan all input */
    hidScanInput();

    u32 keys_held = hidKeysHeld();
    u32 keys_down = hidKeysDown();
    u32 keys_up = hidKeysUp();

    /* Read circle pad */
    circlePosition circle;
    hidCircleRead(&circle);

    /* Read C-stick (New 3DS) */
    circlePosition cstick;
    hidCstickRead(&cstick);

    /* Read touchscreen */
    touchPosition touch;
    hidTouchRead(&touch);

    /* Clear previous state */
    memset(&current_state, 0, sizeof(input_state_t));

    /* Map buttons */
    /* A = Accelerate */
    if (keys_held & KEY_A) {
        current_state.buttons |= INPUT_BUTTON_A;
    }

    /* B = Brake */
    if (keys_held & KEY_B) {
        current_state.buttons |= INPUT_BUTTON_B;
    }

    /* X = Nitro/Boost */
    if (keys_held & KEY_X) {
        current_state.buttons |= INPUT_BUTTON_X;
    }

    /* Y = Look back */
    if (keys_held & KEY_Y) {
        current_state.buttons |= INPUT_BUTTON_Y;
    }

    /* L = Drift left */
    if (keys_held & KEY_L) {
        current_state.buttons |= INPUT_BUTTON_L;
    }

    /* R = Drift right */
    if (keys_held & KEY_R) {
        current_state.buttons |= INPUT_BUTTON_R;
    }

    /* ZL/ZR (New 3DS) */
    if (keys_held & KEY_ZL) {
        current_state.buttons |= INPUT_BUTTON_L2;
    }
    if (keys_held & KEY_ZR) {
        current_state.buttons |= INPUT_BUTTON_R2;
    }

    /* Start = Pause */
    if (keys_held & KEY_START) {
        current_state.buttons |= INPUT_BUTTON_START;
    }

    /* Select = Map/Options */
    if (keys_held & KEY_SELECT) {
        current_state.buttons |= INPUT_BUTTON_SELECT;
    }

    /* D-Pad for menu navigation and steering fallback */
    if (keys_held & KEY_DUP) {
        current_state.buttons |= INPUT_DPAD_UP;
    }
    if (keys_held & KEY_DDOWN) {
        current_state.buttons |= INPUT_DPAD_DOWN;
    }
    if (keys_held & KEY_DLEFT) {
        current_state.buttons |= INPUT_DPAD_LEFT;
    }
    if (keys_held & KEY_DRIGHT) {
        current_state.buttons |= INPUT_DPAD_RIGHT;
    }

    /* Circle Pad - steering and acceleration */
    /* Range is approximately -156 to +156 */
    float circle_x = (float)circle.dx / 156.0f;
    float circle_y = (float)circle.dy / 156.0f;

    /* Apply deadzone */
    if (abs(circle.dx) < ANALOG_DEADZONE) circle_x = 0.0f;
    if (abs(circle.dy) < ANALOG_DEADZONE) circle_y = 0.0f;

    /* Clamp to -1.0 to 1.0 */
    if (circle_x > 1.0f) circle_x = 1.0f;
    if (circle_x < -1.0f) circle_x = -1.0f;
    if (circle_y > 1.0f) circle_y = 1.0f;
    if (circle_y < -1.0f) circle_y = -1.0f;

    current_state.analog_lx = circle_x;
    current_state.analog_ly = circle_y;

    /* C-Stick (camera control on New 3DS) */
    float cstick_x = (float)cstick.dx / 156.0f;
    float cstick_y = (float)cstick.dy / 156.0f;

    if (abs(cstick.dx) < ANALOG_DEADZONE) cstick_x = 0.0f;
    if (abs(cstick.dy) < ANALOG_DEADZONE) cstick_y = 0.0f;

    if (cstick_x > 1.0f) cstick_x = 1.0f;
    if (cstick_x < -1.0f) cstick_x = -1.0f;
    if (cstick_y > 1.0f) cstick_y = 1.0f;
    if (cstick_y < -1.0f) cstick_y = -1.0f;

    current_state.analog_rx = cstick_x;
    current_state.analog_ry = cstick_y;

    /* L/R analog triggers (simulated from digital buttons) */
    current_state.trigger_l = (keys_held & KEY_L) ? 1.0f : 0.0f;
    current_state.trigger_r = (keys_held & KEY_R) ? 1.0f : 0.0f;

    /* Touch screen - could be used for map/menu interaction */
    if (keys_held & KEY_TOUCH) {
        current_state.touch_x = touch.px;
        current_state.touch_y = touch.py;
        current_state.touch_active = 1;
    } else {
        current_state.touch_active = 0;
    }

    /* Store for next frame */
    keys_down_prev = keys_held;
}

input_state_t *input_get_state(void) {
    return &current_state;
}

int input_button_pressed(u32 button) {
    return (current_state.buttons & button) != 0;
}

int input_button_just_pressed(u32 button) {
    u32 keys_down = hidKeysDown();

    switch (button) {
        case INPUT_BUTTON_A: return (keys_down & KEY_A) != 0;
        case INPUT_BUTTON_B: return (keys_down & KEY_B) != 0;
        case INPUT_BUTTON_X: return (keys_down & KEY_X) != 0;
        case INPUT_BUTTON_Y: return (keys_down & KEY_Y) != 0;
        case INPUT_BUTTON_START: return (keys_down & KEY_START) != 0;
        case INPUT_BUTTON_SELECT: return (keys_down & KEY_SELECT) != 0;
        case INPUT_BUTTON_L: return (keys_down & KEY_L) != 0;
        case INPUT_BUTTON_R: return (keys_down & KEY_R) != 0;
        default: return 0;
    }
}

float input_get_steering(void) {
    /* Use circle pad X for steering, fallback to D-pad */
    if (current_state.analog_lx != 0.0f) {
        return current_state.analog_lx;
    }

    if (current_state.buttons & INPUT_DPAD_LEFT) return -1.0f;
    if (current_state.buttons & INPUT_DPAD_RIGHT) return 1.0f;

    return 0.0f;
}

float input_get_throttle(void) {
    /* A button for acceleration */
    if (current_state.buttons & INPUT_BUTTON_A) {
        return 1.0f;
    }

    /* Circle pad Y forward for acceleration */
    if (current_state.analog_ly > 0.1f) {
        return current_state.analog_ly;
    }

    return 0.0f;
}

float input_get_brake(void) {
    /* B button for braking */
    if (current_state.buttons & INPUT_BUTTON_B) {
        return 1.0f;
    }

    /* Circle pad Y backward for braking */
    if (current_state.analog_ly < -0.1f) {
        return -current_state.analog_ly;
    }

    return 0.0f;
}

int input_is_connected(void) {
    /* 3DS always has built-in controls */
    return 1;
}

#endif /* PLATFORM_3DS */
