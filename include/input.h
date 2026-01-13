/*
 * RetroRacer - Input System
 * Dreamcast Controller Handling
 */

#ifndef INPUT_H
#define INPUT_H

#include <stdint.h>

/* Dreamcast controller buttons */
#define BTN_C           (1 << 0)
#define BTN_B           (1 << 1)
#define BTN_A           (1 << 2)
#define BTN_START       (1 << 3)
#define BTN_DPAD_UP     (1 << 4)
#define BTN_DPAD_DOWN   (1 << 5)
#define BTN_DPAD_LEFT   (1 << 6)
#define BTN_DPAD_RIGHT  (1 << 7)
#define BTN_Z           (1 << 8)
#define BTN_Y           (1 << 9)
#define BTN_X           (1 << 10)
#define BTN_D           (1 << 11)

/* Input state structure */
typedef struct {
    /* Current button state */
    uint32_t buttons;

    /* Buttons pressed this frame (edge detection) */
    uint32_t pressed;

    /* Buttons released this frame */
    uint32_t released;

    /* Analog stick (-128 to 127) */
    int8_t stick_x;
    int8_t stick_y;

    /* Normalized analog (-1.0 to 1.0) */
    float analog_x;
    float analog_y;

    /* Triggers (0 to 255) */
    uint8_t ltrig;
    uint8_t rtrig;

    /* Normalized triggers (0.0 to 1.0) */
    float trigger_l;
    float trigger_r;

    /* Controller connected */
    int connected;
} input_state_t;

/* Initialize input system */
void input_init(void);

/* Update input state (call once per frame) */
void input_update(void);

/* Get input state for player */
input_state_t *input_get_state(int player);

/* Check if button is currently held */
int input_button_held(input_state_t *state, uint32_t button);

/* Check if button was just pressed */
int input_button_pressed(input_state_t *state, uint32_t button);

/* Check if button was just released */
int input_button_released(input_state_t *state, uint32_t button);

/* Get steering value (-1.0 to 1.0) from analog or d-pad */
float input_get_steering(input_state_t *state);

/* Get throttle value (0.0 to 1.0) from trigger or button */
float input_get_throttle(input_state_t *state);

/* Get brake value (0.0 to 1.0) from trigger or button */
float input_get_brake(input_state_t *state);

/* Deadzone configuration */
void input_set_deadzone(float deadzone);

#endif /* INPUT_H */
