/*
 * RetroRacer - Nintendo DS Input Implementation
 * Uses libnds with DS button and touchscreen support
 */

#include "input.h"
#include "platform.h"

#ifdef PLATFORM_NDS

#include <nds.h>

static u16 key_state = 0;
static touchPosition touch;
static int touch_held = 0;

void input_init(void) {
    /* Nothing special needed */
}

void input_shutdown(void) {
    /* Nothing to clean up */
}

void input_update(void) {
    scanKeys();
    key_state = keysHeld();

    /* Read touchscreen */
    touch_held = (key_state & KEY_TOUCH) != 0;
    if (touch_held) {
        touchRead(&touch);
    }
}

int input_get_button(input_button_t button) {
    switch (button) {
        case INPUT_BUTTON_A:      return (key_state & KEY_A) != 0;
        case INPUT_BUTTON_B:      return (key_state & KEY_B) != 0;
        case INPUT_BUTTON_X:      return (key_state & KEY_X) != 0;
        case INPUT_BUTTON_Y:      return (key_state & KEY_Y) != 0;
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
    /* DS has no analog stick - use D-pad or touchscreen */
    switch (axis) {
        case INPUT_AXIS_LEFT_X:
            if (key_state & KEY_LEFT) return -1.0f;
            if (key_state & KEY_RIGHT) return 1.0f;
            /* Use touchscreen X as analog if held */
            if (touch_held) {
                return ((float)touch.px / 128.0f) - 1.0f;
            }
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
    /* D-pad steering */
    if (key_state & KEY_LEFT) return -1.0f;
    if (key_state & KEY_RIGHT) return 1.0f;

    /* Touchscreen analog steering on bottom screen */
    if (touch_held) {
        /* Map touch X (0-255) to steering (-1 to 1) */
        return ((float)touch.px / 128.0f) - 1.0f;
    }

    return 0.0f;
}

float input_get_acceleration(void) {
    /* A button or touch top half of screen */
    if (key_state & KEY_A) return 1.0f;
    if (touch_held && touch.py < 96) return 1.0f;
    return 0.0f;
}

float input_get_brake(void) {
    /* B button or touch bottom half of screen */
    if (key_state & KEY_B) return 1.0f;
    if (touch_held && touch.py >= 96) return 1.0f;
    return 0.0f;
}

/* Get touch position for UI interaction */
int input_get_touch(int *x, int *y) {
    if (!touch_held) return 0;
    *x = touch.px;
    *y = touch.py;
    return 1;
}

#endif /* PLATFORM_NDS */
