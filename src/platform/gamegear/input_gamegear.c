/*
 * RetroRacer - Game Gear Input Implementation
 * D-pad, 1, 2, Start buttons
 */

#include "input.h"
#include "platform.h"
#include <string.h>

#ifdef PLATFORM_GAMEGEAR
#include <sms.h>
#endif

/* Input state */
static struct {
    unsigned int current_keys;
    unsigned int prev_keys;

    float steering;
    float throttle;
    float brake;
    int button_state;

    int initialized;
} g_input;

/* Game Gear button masks (directly from I/O port) */
#define GG_UP      0x01
#define GG_DOWN    0x02
#define GG_LEFT    0x04
#define GG_RIGHT   0x08
#define GG_BUTTON1 0x10  /* Button 1 */
#define GG_BUTTON2 0x20  /* Button 2 */
#define GG_START   0x80  /* Start button (active low on port 0x00) */

/* Game button mappings */
#define BTN_ACCELERATE   0x0001
#define BTN_BRAKE        0x0002
#define BTN_PAUSE        0x0004
#define BTN_SELECT       0x0008
#define BTN_BACK         0x0010
#define BTN_ITEM         0x0020

#ifdef PLATFORM_GAMEGEAR
static unsigned int read_joypad(void) {
    /* Read port 0xDC for D-pad and buttons 1, 2 */
    unsigned int keys = SMS_getKeysStatus();
    /* Read port 0x00 for Start button on Game Gear */
    unsigned int start = ~(*(volatile unsigned char *)0x00) & 0x80;
    return keys | start;
}
#endif

void input_init(void) {
    if (g_input.initialized) return;

    memset(&g_input, 0, sizeof(g_input));

#ifdef PLATFORM_GAMEGEAR
    /* Initialize SMS/GG port reading */
    SMS_init();
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

#ifdef PLATFORM_GAMEGEAR
    g_input.current_keys = read_joypad();
#else
    g_input.current_keys = 0;
#endif

    /* D-pad for steering - active low, so invert */
    unsigned int keys = ~g_input.current_keys;

    if (keys & GG_LEFT) {
        g_input.steering = -1.0f;
    } else if (keys & GG_RIGHT) {
        g_input.steering = 1.0f;
    }

    /* Button 2 = accelerate (typically right button) */
    if (keys & GG_BUTTON2) {
        g_input.button_state |= BTN_ACCELERATE;
        g_input.throttle = 1.0f;
    }

    /* Button 1 = brake (typically left button) */
    if (keys & GG_BUTTON1) {
        g_input.button_state |= BTN_BRAKE;
        g_input.brake = 1.0f;
    }

    /* Start = pause */
    if (keys & GG_START) {
        g_input.button_state |= BTN_PAUSE;
    }

    /* Up = item use */
    if (keys & GG_UP) {
        g_input.button_state |= BTN_ITEM;
    }

    /* Down = select/back */
    if (keys & GG_DOWN) {
        g_input.button_state |= BTN_BACK;
    }
}

int input_is_connected(void) {
    /* Game Gear always has buttons */
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
    unsigned int prev_keys = ~g_input.prev_keys;
    int prev_button_state = 0;

    if (prev_keys & GG_BUTTON2) prev_button_state |= BTN_ACCELERATE;
    if (prev_keys & GG_BUTTON1) prev_button_state |= BTN_BRAKE;
    if (prev_keys & GG_START) prev_button_state |= BTN_PAUSE;
    if (prev_keys & GG_UP) prev_button_state |= BTN_ITEM;
    if (prev_keys & GG_DOWN) prev_button_state |= BTN_BACK;

    return (g_input.button_state & button) && !(prev_button_state & button);
}

int input_button_released(int button) {
    unsigned int prev_keys = ~g_input.prev_keys;
    int prev_button_state = 0;

    if (prev_keys & GG_BUTTON2) prev_button_state |= BTN_ACCELERATE;
    if (prev_keys & GG_BUTTON1) prev_button_state |= BTN_BRAKE;
    if (prev_keys & GG_START) prev_button_state |= BTN_PAUSE;
    if (prev_keys & GG_UP) prev_button_state |= BTN_ITEM;
    if (prev_keys & GG_DOWN) prev_button_state |= BTN_BACK;

    return !(g_input.button_state & button) && (prev_button_state & button);
}

void input_set_rumble(int intensity) {
    /* Game Gear has no rumble */
    (void)intensity;
}

/* Game Gear specific - raw key access */
unsigned int input_get_raw_keys(void) {
    return g_input.current_keys;
}
