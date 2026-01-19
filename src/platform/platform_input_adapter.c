/*
 * RetroRacer - Platform Input Adapter
 *
 * Bridges platform-specific input to the game's input system.
 * Maps console-specific buttons to unified input_state_t.
 */

#include "platform.h"
#include "input.h"
#include <string.h>

/*============================================================================
 * Input State Management
 *============================================================================*/

#define MAX_PLAYERS 4

static input_state_t input_states[MAX_PLAYERS];
static uint32_t prev_buttons[MAX_PLAYERS];
static float deadzone = 0.15f;

/*============================================================================
 * Platform-Specific Input Functions (External)
 *============================================================================*/

#if defined(PLATFORM_PSX)
extern uint32_t psx_get_buttons(int port);
extern void psx_get_analog(int port, int *lx, int *ly, int *rx, int *ry);
#elif defined(PLATFORM_PS2)
extern uint32_t ps2_get_buttons(int port);
extern void ps2_get_analog(int port, int *lx, int *ly, int *rx, int *ry);
extern void ps2_get_pressure(int port, int *l2, int *r2);
#elif defined(PLATFORM_PS3)
extern uint32_t ps3_get_buttons(int port);
extern void ps3_get_analog(int port, int *lx, int *ly, int *rx, int *ry);
extern void ps3_get_triggers(int port, int *l2, int *r2);
#elif defined(PLATFORM_XBOX)
extern uint32_t xbox_get_buttons(int port);
extern void xbox_get_analog(int port, int *lx, int *ly, int *rx, int *ry);
extern void xbox_get_triggers(int port, int *lt, int *rt);
#elif defined(PLATFORM_XBOX360)
extern uint32_t x360_get_buttons(int port);
extern void x360_get_analog(int port, int *lx, int *ly, int *rx, int *ry);
extern void x360_get_triggers(int port, int *lt, int *rt);
#else
/* Native platform */
extern uint32_t native_get_buttons(int port);
extern void native_get_analog(int port, int *lx, int *ly, int *rx, int *ry);
#endif

/*============================================================================
 * Button Mapping: Platform -> Game
 *============================================================================*/

static uint32_t map_platform_to_game_buttons(uint32_t platform_buttons) {
    uint32_t game_buttons = 0;

    /* Map cross-platform buttons to game buttons */
    if (platform_buttons & PLAT_BTN_CROSS)      game_buttons |= BTN_A;
    if (platform_buttons & PLAT_BTN_CIRCLE)     game_buttons |= BTN_B;
    if (platform_buttons & PLAT_BTN_SQUARE)     game_buttons |= BTN_X;
    if (platform_buttons & PLAT_BTN_TRIANGLE)   game_buttons |= BTN_Y;
    if (platform_buttons & PLAT_BTN_START)      game_buttons |= BTN_START;
    if (platform_buttons & PLAT_BTN_L1)         game_buttons |= BTN_Z;      /* L1 -> Z */
    if (platform_buttons & PLAT_BTN_R1)         game_buttons |= BTN_C;      /* R1 -> C */
    if (platform_buttons & PLAT_BTN_DPAD_UP)    game_buttons |= BTN_DPAD_UP;
    if (platform_buttons & PLAT_BTN_DPAD_DOWN)  game_buttons |= BTN_DPAD_DOWN;
    if (platform_buttons & PLAT_BTN_DPAD_LEFT)  game_buttons |= BTN_DPAD_LEFT;
    if (platform_buttons & PLAT_BTN_DPAD_RIGHT) game_buttons |= BTN_DPAD_RIGHT;

    return game_buttons;
}

/*============================================================================
 * Input System Implementation
 *============================================================================*/

void input_init(void) {
    memset(input_states, 0, sizeof(input_states));
    memset(prev_buttons, 0, sizeof(prev_buttons));

    /* Mark player 0 as connected by default */
    input_states[0].connected = 1;
}

void input_update(void) {
    for (int i = 0; i < MAX_PLAYERS; i++) {
        input_state_t *state = &input_states[i];
        uint32_t platform_buttons = 0;
        int lx = 128, ly = 128, rx = 128, ry = 128;
        int lt = 0, rt = 0;

        /* Get platform-specific input */
#if defined(PLATFORM_PSX)
        platform_buttons = psx_get_buttons(i);
        psx_get_analog(i, &lx, &ly, &rx, &ry);
        state->connected = (platform_buttons != 0 || lx != 128 || ly != 128);
#elif defined(PLATFORM_PS2)
        platform_buttons = ps2_get_buttons(i);
        ps2_get_analog(i, &lx, &ly, &rx, &ry);
        ps2_get_pressure(i, &lt, &rt);
        state->connected = 1;  /* PS2 pad detection is complex */
#elif defined(PLATFORM_PS3)
        platform_buttons = ps3_get_buttons(i);
        ps3_get_analog(i, &lx, &ly, &rx, &ry);
        ps3_get_triggers(i, &lt, &rt);
        state->connected = (platform_buttons != 0);
#elif defined(PLATFORM_XBOX)
        platform_buttons = xbox_get_buttons(i);
        xbox_get_analog(i, &lx, &ly, &rx, &ry);
        xbox_get_triggers(i, &lt, &rt);
        state->connected = (platform_buttons != 0);
#elif defined(PLATFORM_XBOX360)
        platform_buttons = x360_get_buttons(i);
        x360_get_analog(i, &lx, &ly, &rx, &ry);
        x360_get_triggers(i, &lt, &rt);
        state->connected = (platform_buttons != 0);
#else
        platform_buttons = native_get_buttons(i);
        native_get_analog(i, &lx, &ly, &rx, &ry);
        state->connected = (i == 0);  /* Only player 0 in native mode */
#endif

        /* Map platform buttons to game buttons */
        uint32_t new_buttons = map_platform_to_game_buttons(platform_buttons);

        /* Edge detection */
        state->pressed = new_buttons & ~prev_buttons[i];
        state->released = ~new_buttons & prev_buttons[i];
        state->buttons = new_buttons;
        prev_buttons[i] = new_buttons;

        /* Store raw analog values */
        state->stick_x = (int8_t)(lx - 128);
        state->stick_y = (int8_t)(ly - 128);

        /* Normalize analog stick with deadzone */
        float ax = (lx - 128) / 127.0f;
        float ay = (ly - 128) / 127.0f;

        if (ax > -deadzone && ax < deadzone) ax = 0.0f;
        if (ay > -deadzone && ay < deadzone) ay = 0.0f;

        /* Clamp to -1.0 to 1.0 */
        if (ax < -1.0f) ax = -1.0f;
        if (ax > 1.0f) ax = 1.0f;
        if (ay < -1.0f) ay = -1.0f;
        if (ay > 1.0f) ay = 1.0f;

        state->analog_x = ax;
        state->analog_y = ay;

        /* Triggers */
        state->ltrig = (uint8_t)lt;
        state->rtrig = (uint8_t)rt;
        state->trigger_l = lt / 255.0f;
        state->trigger_r = rt / 255.0f;
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
    /* Prefer analog stick */
    if (state->analog_x != 0.0f) {
        return state->analog_x;
    }

    /* Fall back to D-pad */
    if (state->buttons & BTN_DPAD_LEFT) {
        return -1.0f;
    }
    if (state->buttons & BTN_DPAD_RIGHT) {
        return 1.0f;
    }

    return 0.0f;
}

float input_get_throttle(input_state_t *state) {
    /* Prefer right trigger */
    if (state->trigger_r > 0.0f) {
        return state->trigger_r;
    }

    /* Fall back to A button */
    if (state->buttons & BTN_A) {
        return 1.0f;
    }

    return 0.0f;
}

float input_get_brake(input_state_t *state) {
    /* Prefer left trigger */
    if (state->trigger_l > 0.0f) {
        return state->trigger_l;
    }

    /* Fall back to B button */
    if (state->buttons & BTN_B) {
        return 1.0f;
    }

    return 0.0f;
}

void input_set_deadzone(float dz) {
    deadzone = dz;
    if (deadzone < 0.0f) deadzone = 0.0f;
    if (deadzone > 0.5f) deadzone = 0.5f;
}
