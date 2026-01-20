/*
 * RetroRacer - Game Boy Input Implementation
 * 8 buttons: D-pad, A, B, Start, Select
 */

#include "input.h"
#include "platform.h"
#include <string.h>

#ifdef PLATFORM_GAMEBOY
#include <gb/gb.h>
#endif

/* Input state */
static struct {
    int current_keys;
    int prev_keys;

    float steering;
    float throttle;
    float brake;
    int button_state;

    int initialized;
} g_input;

/* GB button masks */
#define GB_UP      0x04
#define GB_DOWN    0x08
#define GB_LEFT    0x02
#define GB_RIGHT   0x01
#define GB_A       0x10
#define GB_B       0x20
#define GB_START   0x80
#define GB_SELECT  0x40

/* Game button mappings */
#define BTN_ACCELERATE   0x0001
#define BTN_BRAKE        0x0002
#define BTN_PAUSE        0x0004
#define BTN_SELECT       0x0008
#define BTN_BACK         0x0010
#define BTN_ITEM         0x0020

void input_init(void) {
    if (g_input.initialized) return;

    memset(&g_input, 0, sizeof(g_input));

#ifdef PLATFORM_GAMEBOY
    /* Nothing specific needed for GB input init */
#endif

    g_input.initialized = 1;
}

void input_shutdown(void) {
    if (!g_input.initialized) return;
    g_input.initialized = 0;
}

void input_update(void) {
    if (!g_input.initialized) return;

    g_input.prev_keys = g_input.current_keys;
    g_input.button_state = 0;
    g_input.steering = 0.0f;
    g_input.throttle = 0.0f;
    g_input.brake = 0.0f;

#ifdef PLATFORM_GAMEBOY
    /* Read joypad state */
    g_input.current_keys = joypad();
#else
    g_input.current_keys = 0;
#endif

    /* D-pad for steering */
    if (g_input.current_keys & GB_LEFT) {
        g_input.steering = -1.0f;
    } else if (g_input.current_keys & GB_RIGHT) {
        g_input.steering = 1.0f;
    }

    /* A button = accelerate */
    if (g_input.current_keys & GB_A) {
        g_input.button_state |= BTN_ACCELERATE;
        g_input.throttle = 1.0f;
    }

    /* B button = brake */
    if (g_input.current_keys & GB_B) {
        g_input.button_state |= BTN_BRAKE;
        g_input.brake = 1.0f;
    }

    /* Start = pause */
    if (g_input.current_keys & GB_START) {
        g_input.button_state |= BTN_PAUSE;
    }

    /* Select = menu/back */
    if (g_input.current_keys & GB_SELECT) {
        g_input.button_state |= BTN_BACK;
    }

    /* Up/Down could be used for gear shifting or item use */
    if (g_input.current_keys & GB_UP) {
        g_input.button_state |= BTN_ITEM;
    }
}

int input_is_connected(void) {
    /* Game Boy always has a joypad */
    return 1;
}

float input_get_steering(void) {
    return g_input.steering;
}

float input_get_throttle(void) {
    return g_input.throttle;
}

float input_get_brake(void) {
    return g_input.brake;
}

int input_button_held(int button) {
    return (g_input.button_state & button) != 0;
}

int input_button_pressed(int button) {
    /* Check if button is pressed this frame but wasn't last frame */
    int prev_button_state = 0;

    if (g_input.prev_keys & GB_A) prev_button_state |= BTN_ACCELERATE;
    if (g_input.prev_keys & GB_B) prev_button_state |= BTN_BRAKE;
    if (g_input.prev_keys & GB_START) prev_button_state |= BTN_PAUSE;
    if (g_input.prev_keys & GB_SELECT) prev_button_state |= BTN_BACK;
    if (g_input.prev_keys & GB_UP) prev_button_state |= BTN_ITEM;

    return (g_input.button_state & button) && !(prev_button_state & button);
}

int input_button_released(int button) {
    int prev_button_state = 0;

    if (g_input.prev_keys & GB_A) prev_button_state |= BTN_ACCELERATE;
    if (g_input.prev_keys & GB_B) prev_button_state |= BTN_BRAKE;
    if (g_input.prev_keys & GB_START) prev_button_state |= BTN_PAUSE;
    if (g_input.prev_keys & GB_SELECT) prev_button_state |= BTN_BACK;
    if (g_input.prev_keys & GB_UP) prev_button_state |= BTN_ITEM;

    return !(g_input.button_state & button) && (prev_button_state & button);
}

void input_set_rumble(int intensity) {
    /* Game Boy has no rumble */
    (void)intensity;
}

/* Game Boy specific - check raw key state */
int input_get_raw_keys(void) {
    return g_input.current_keys;
}
