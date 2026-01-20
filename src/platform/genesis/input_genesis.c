/*
 * RetroRacer - Sega Genesis Input Implementation
 * Uses SGDK with 3/6 button controller support
 */

#include "input.h"
#include "platform.h"

#ifdef PLATFORM_GENESIS

#include <genesis.h>

static u16 joy_state = 0;

void input_init(void) {
    JOY_init();
}

void input_shutdown(void) {
    /* Nothing to clean up */
}

void input_update(void) {
    joy_state = JOY_readJoypad(JOY_1);
}

int input_get_button(input_button_t button) {
    switch (button) {
        case INPUT_BUTTON_A:      return (joy_state & BUTTON_A) != 0;
        case INPUT_BUTTON_B:      return (joy_state & BUTTON_B) != 0;
        case INPUT_BUTTON_X:      return (joy_state & BUTTON_X) != 0;
        case INPUT_BUTTON_Y:      return (joy_state & BUTTON_Y) != 0;
        case INPUT_BUTTON_START:  return (joy_state & BUTTON_START) != 0;
        case INPUT_BUTTON_SELECT: return (joy_state & BUTTON_MODE) != 0;
        case INPUT_BUTTON_L:      return (joy_state & BUTTON_Z) != 0;
        case INPUT_BUTTON_R:      return (joy_state & BUTTON_C) != 0;
        case INPUT_BUTTON_UP:     return (joy_state & BUTTON_UP) != 0;
        case INPUT_BUTTON_DOWN:   return (joy_state & BUTTON_DOWN) != 0;
        case INPUT_BUTTON_LEFT:   return (joy_state & BUTTON_LEFT) != 0;
        case INPUT_BUTTON_RIGHT:  return (joy_state & BUTTON_RIGHT) != 0;
        default: return 0;
    }
}

float input_get_axis(input_axis_t axis) {
    /* Genesis has no analog sticks - simulate from D-pad */
    switch (axis) {
        case INPUT_AXIS_LEFT_X:
            if (joy_state & BUTTON_LEFT) return -1.0f;
            if (joy_state & BUTTON_RIGHT) return 1.0f;
            return 0.0f;
        case INPUT_AXIS_LEFT_Y:
            if (joy_state & BUTTON_UP) return -1.0f;
            if (joy_state & BUTTON_DOWN) return 1.0f;
            return 0.0f;
        default:
            return 0.0f;
    }
}

float input_get_steering(void) {
    if (joy_state & BUTTON_LEFT) return -1.0f;
    if (joy_state & BUTTON_RIGHT) return 1.0f;
    return 0.0f;
}

float input_get_acceleration(void) {
    return (joy_state & BUTTON_A) ? 1.0f : 0.0f;
}

float input_get_brake(void) {
    return (joy_state & BUTTON_B) ? 1.0f : 0.0f;
}

#endif /* PLATFORM_GENESIS */
