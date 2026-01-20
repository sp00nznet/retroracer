/*
 * RetroRacer - Game Boy Advance Input Implementation
 * Uses libgba with GBA button input
 */

#include "input.h"
#include "platform.h"

#ifdef PLATFORM_GBA

#include <gba_input.h>

static u16 key_state = 0;

void input_init(void) {
    /* Nothing special needed for GBA input */
}

void input_shutdown(void) {
    /* Nothing to clean up */
}

void input_update(void) {
    scanKeys();
    key_state = keysHeld();
}

int input_get_button(input_button_t button) {
    switch (button) {
        case INPUT_BUTTON_A:      return (key_state & KEY_A) != 0;
        case INPUT_BUTTON_B:      return (key_state & KEY_B) != 0;
        case INPUT_BUTTON_X:      return 0;  /* GBA has no X button */
        case INPUT_BUTTON_Y:      return 0;  /* GBA has no Y button */
        case INPUT_BUTTON_START:  return (key_state & KEY_START) != 0;
        case INPUT_BUTTON_SELECT: return (key_state & KEY_SELECT) != 0;
        case INPUT_BUTTON_L:      return (key_state & KEY_L) != 0;
        case INPUT_BUTTON_R:      return (key_state & KEY_R) != 0;
        case INPUT_BUTTON_UP:     return (key_state & KEY_UP) != 0;
        case INPUT_BUTTON_DOWN:   return (key_state & KEY_DOWN) != 0;
        case INPUT_BUTTON_LEFT:   return (key_state & KEY_LEFT) != 0;
        case INPUT_BUTTON_RIGHT:  return (key_state & KEY_RIGHT) != 0;
        default: return 0;
    }
}

float input_get_axis(input_axis_t axis) {
    /* GBA has no analog - simulate from D-pad */
    switch (axis) {
        case INPUT_AXIS_LEFT_X:
            if (key_state & KEY_LEFT) return -1.0f;
            if (key_state & KEY_RIGHT) return 1.0f;
            return 0.0f;
        case INPUT_AXIS_LEFT_Y:
            if (key_state & KEY_UP) return -1.0f;
            if (key_state & KEY_DOWN) return 1.0f;
            return 0.0f;
        default:
            return 0.0f;
    }
}

float input_get_steering(void) {
    if (key_state & KEY_LEFT) return -1.0f;
    if (key_state & KEY_RIGHT) return 1.0f;
    return 0.0f;
}

float input_get_acceleration(void) {
    return (key_state & KEY_A) ? 1.0f : 0.0f;
}

float input_get_brake(void) {
    return (key_state & KEY_B) ? 1.0f : 0.0f;
}

#endif /* PLATFORM_GBA */
