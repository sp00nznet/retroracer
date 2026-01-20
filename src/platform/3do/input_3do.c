/*
 * RetroRacer - 3DO Input Implementation
 * Uses 3DO SDK with controller support
 */

#include "input.h"
#include "platform.h"

#ifdef PLATFORM_3DO

#include <event.h>
#include <controlpad.h>

static ControlPadEventData pad_data;
static uint32 pad_state = 0;

void input_init(void) {
    /* Initialize event broker for controller input */
    InitEventUtility(1, 0, LC_Observer);
}

void input_shutdown(void) {
    KillEventUtility();
}

void input_update(void) {
    /* Poll controller */
    GetControlPad(1, FALSE, &pad_data);
    pad_state = pad_data.cped_ButtonBits;
}

int input_get_button(input_button_t button) {
    switch (button) {
        case INPUT_BUTTON_A:      return (pad_state & ControlA) != 0;
        case INPUT_BUTTON_B:      return (pad_state & ControlB) != 0;
        case INPUT_BUTTON_X:      return (pad_state & ControlC) != 0;
        case INPUT_BUTTON_Y:      return 0;  /* 3DO has only A/B/C */
        case INPUT_BUTTON_START:  return (pad_state & ControlStart) != 0;
        case INPUT_BUTTON_SELECT: return (pad_state & ControlX) != 0;
        case INPUT_BUTTON_L:      return (pad_state & ControlLeftShift) != 0;
        case INPUT_BUTTON_R:      return (pad_state & ControlRightShift) != 0;
        case INPUT_BUTTON_UP:     return (pad_state & ControlUp) != 0;
        case INPUT_BUTTON_DOWN:   return (pad_state & ControlDown) != 0;
        case INPUT_BUTTON_LEFT:   return (pad_state & ControlLeft) != 0;
        case INPUT_BUTTON_RIGHT:  return (pad_state & ControlRight) != 0;
        default: return 0;
    }
}

float input_get_axis(input_axis_t axis) {
    /* 3DO standard controller has no analog - simulate from D-pad */
    switch (axis) {
        case INPUT_AXIS_LEFT_X:
            if (pad_state & ControlLeft) return -1.0f;
            if (pad_state & ControlRight) return 1.0f;
            return 0.0f;
        case INPUT_AXIS_LEFT_Y:
            if (pad_state & ControlUp) return -1.0f;
            if (pad_state & ControlDown) return 1.0f;
            return 0.0f;
        default:
            return 0.0f;
    }
}

float input_get_steering(void) {
    if (pad_state & ControlLeft) return -1.0f;
    if (pad_state & ControlRight) return 1.0f;
    return 0.0f;
}

float input_get_acceleration(void) {
    return (pad_state & ControlA) ? 1.0f : 0.0f;
}

float input_get_brake(void) {
    return (pad_state & ControlB) ? 1.0f : 0.0f;
}

#endif /* PLATFORM_3DO */
