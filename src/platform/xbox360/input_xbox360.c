/*
 * RetroRacer - Xbox 360 Input Implementation
 * Uses libxenon with Xbox 360 controller support
 */

#include "input.h"
#include "platform.h"

#ifdef PLATFORM_XBOX360

#include <input/input.h>
#include <usb/usbmain.h>

static struct controller_data_s controller;

void input_init(void) {
    /* USB/controller init is handled by libxenon */
    usb_init();
    usb_do_poll();
}

void input_shutdown(void) {
    /* Nothing to clean up */
}

void input_update(void) {
    /* Poll controllers */
    usb_do_poll();
    get_controller_data(&controller, 0);
}

int input_get_button(input_button_t button) {
    switch (button) {
        case INPUT_BUTTON_A:      return controller.a;
        case INPUT_BUTTON_B:      return controller.b;
        case INPUT_BUTTON_X:      return controller.x;
        case INPUT_BUTTON_Y:      return controller.y;
        case INPUT_BUTTON_START:  return controller.start;
        case INPUT_BUTTON_SELECT: return controller.back;
        case INPUT_BUTTON_L:      return controller.lb;
        case INPUT_BUTTON_R:      return controller.rb;
        case INPUT_BUTTON_UP:     return controller.up;
        case INPUT_BUTTON_DOWN:   return controller.down;
        case INPUT_BUTTON_LEFT:   return controller.left;
        case INPUT_BUTTON_RIGHT:  return controller.right;
        default: return 0;
    }
}

float input_get_axis(input_axis_t axis) {
    switch (axis) {
        case INPUT_AXIS_LEFT_X:
            return controller.s1_x / 32768.0f;
        case INPUT_AXIS_LEFT_Y:
            return controller.s1_y / 32768.0f;
        case INPUT_AXIS_RIGHT_X:
            return controller.s2_x / 32768.0f;
        case INPUT_AXIS_RIGHT_Y:
            return controller.s2_y / 32768.0f;
        case INPUT_AXIS_TRIGGER_L:
            return controller.lt / 255.0f;
        case INPUT_AXIS_TRIGGER_R:
            return controller.rt / 255.0f;
        default:
            return 0.0f;
    }
}

float input_get_steering(void) {
    float stick = controller.s1_x / 32768.0f;
    if (controller.left) stick = -1.0f;
    if (controller.right) stick = 1.0f;
    return stick;
}

float input_get_acceleration(void) {
    float trigger = controller.rt / 255.0f;
    if (controller.a) trigger = 1.0f;
    return trigger;
}

float input_get_brake(void) {
    float trigger = controller.lt / 255.0f;
    if (controller.b) trigger = 1.0f;
    return trigger;
}

#endif /* PLATFORM_XBOX360 */
