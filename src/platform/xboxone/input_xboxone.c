/*
 * RetroRacer - Xbox One Input Implementation
 * Supports Xbox One Controller with impulse triggers
 */

#include "input.h"
#include "platform.h"
#include <string.h>

#ifdef PLATFORM_XBOXONE
#include <XInput.h>
#else
/* Stub definitions */
typedef struct {
    unsigned short wButtons;
    unsigned char bLeftTrigger;
    unsigned char bRightTrigger;
    short sThumbLX;
    short sThumbLY;
    short sThumbRX;
    short sThumbRY;
} XINPUT_GAMEPAD;

typedef struct {
    unsigned long dwPacketNumber;
    XINPUT_GAMEPAD Gamepad;
} XINPUT_STATE;

typedef struct {
    unsigned short wLeftMotorSpeed;
    unsigned short wRightMotorSpeed;
} XINPUT_VIBRATION;

#define XINPUT_GAMEPAD_DPAD_LEFT      0x0004
#define XINPUT_GAMEPAD_DPAD_RIGHT     0x0008
#define XINPUT_GAMEPAD_START          0x0010
#define XINPUT_GAMEPAD_BACK           0x0020
#define XINPUT_GAMEPAD_A              0x1000
#define XINPUT_GAMEPAD_B              0x2000
#define XINPUT_GAMEPAD_X              0x4000
#define XINPUT_GAMEPAD_Y              0x8000
#define XINPUT_GAMEPAD_LEFT_SHOULDER  0x0100
#define XINPUT_GAMEPAD_RIGHT_SHOULDER 0x0200
#define ERROR_SUCCESS 0
#endif

#define MAX_CONTROLLERS 4
#define DEADZONE_LEFT  7849
#define DEADZONE_RIGHT 8689
#define TRIGGER_THRESHOLD 30

/* Input state */
static struct {
    int connected[MAX_CONTROLLERS];
    int active_controller;

    float steering;
    float throttle;
    float brake;
    int button_state;
    int prev_button_state;

    /* Impulse trigger state */
    int left_trigger_rumble;
    int right_trigger_rumble;

    int initialized;
} g_input;

#define BTN_ACCELERATE   0x0001
#define BTN_BRAKE        0x0002
#define BTN_PAUSE        0x0004
#define BTN_SELECT       0x0008
#define BTN_BACK         0x0010
#define BTN_ITEM         0x0020

static float apply_deadzone(short value, short deadzone) {
    if (value > deadzone) {
        return (float)(value - deadzone) / (32767.0f - deadzone);
    } else if (value < -deadzone) {
        return (float)(value + deadzone) / (32767.0f - deadzone);
    }
    return 0.0f;
}

void input_init(void) {
    if (g_input.initialized) return;

    memset(&g_input, 0, sizeof(g_input));
    g_input.active_controller = -1;

#ifdef PLATFORM_XBOXONE
    /* XInput is always available on Xbox One */
#endif

    g_input.initialized = 1;
}

void input_shutdown(void) {
    if (!g_input.initialized) return;

    /* Stop all vibration */
    for (int i = 0; i < MAX_CONTROLLERS; i++) {
        if (g_input.connected[i]) {
#ifdef PLATFORM_XBOXONE
            XINPUT_VIBRATION vibration = {0};
            XInputSetState(i, &vibration);
#endif
        }
    }

    g_input.initialized = 0;
}

void input_update(void) {
    if (!g_input.initialized) return;

    g_input.prev_button_state = g_input.button_state;
    g_input.button_state = 0;
    g_input.steering = 0.0f;
    g_input.throttle = 0.0f;
    g_input.brake = 0.0f;

#ifdef PLATFORM_XBOXONE
    /* Check all controllers */
    for (int i = 0; i < MAX_CONTROLLERS; i++) {
        XINPUT_STATE state;
        if (XInputGetState(i, &state) == ERROR_SUCCESS) {
            g_input.connected[i] = 1;

            /* Use first connected controller if none active */
            if (g_input.active_controller < 0) {
                g_input.active_controller = i;
            }

            if (i == g_input.active_controller) {
                XINPUT_GAMEPAD *pad = &state.Gamepad;

                /* Left stick for steering */
                g_input.steering = apply_deadzone(pad->sThumbLX, DEADZONE_LEFT);

                /* Triggers for throttle/brake */
                if (pad->bRightTrigger > TRIGGER_THRESHOLD) {
                    g_input.throttle = (pad->bRightTrigger - TRIGGER_THRESHOLD) / (255.0f - TRIGGER_THRESHOLD);
                }
                if (pad->bLeftTrigger > TRIGGER_THRESHOLD) {
                    g_input.brake = (pad->bLeftTrigger - TRIGGER_THRESHOLD) / (255.0f - TRIGGER_THRESHOLD);
                }

                /* Buttons */
                if (pad->wButtons & XINPUT_GAMEPAD_A) g_input.button_state |= BTN_ACCELERATE;
                if (pad->wButtons & XINPUT_GAMEPAD_B) g_input.button_state |= BTN_BRAKE;
                if (pad->wButtons & XINPUT_GAMEPAD_X) g_input.button_state |= BTN_ITEM;
                if (pad->wButtons & XINPUT_GAMEPAD_Y) g_input.button_state |= BTN_SELECT;
                if (pad->wButtons & XINPUT_GAMEPAD_START) g_input.button_state |= BTN_PAUSE;
                if (pad->wButtons & XINPUT_GAMEPAD_BACK) g_input.button_state |= BTN_BACK;

                /* Shoulders as alternative */
                if (pad->wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER) g_input.throttle = 1.0f;
                if (pad->wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER) g_input.brake = 1.0f;

                /* D-pad for digital steering */
                if (pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT) g_input.steering = -1.0f;
                if (pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT) g_input.steering = 1.0f;
            }
        } else {
            g_input.connected[i] = 0;
            if (g_input.active_controller == i) {
                g_input.active_controller = -1;
            }
        }
    }
#else
    g_input.connected[0] = 1;
    g_input.active_controller = 0;
#endif

    /* Button to analog conversion */
    if ((g_input.button_state & BTN_ACCELERATE) && g_input.throttle < 0.5f) {
        g_input.throttle = 1.0f;
    }
    if ((g_input.button_state & BTN_BRAKE) && g_input.brake < 0.5f) {
        g_input.brake = 1.0f;
    }
}

int input_is_connected(void) {
    return g_input.active_controller >= 0 && g_input.connected[g_input.active_controller];
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
    return (g_input.button_state & button) && !(g_input.prev_button_state & button);
}

int input_button_released(int button) {
    return !(g_input.button_state & button) && (g_input.prev_button_state & button);
}

void input_set_rumble(int intensity) {
#ifdef PLATFORM_XBOXONE
    if (g_input.active_controller >= 0) {
        XINPUT_VIBRATION vibration;
        vibration.wLeftMotorSpeed = (unsigned short)(intensity * 655);
        vibration.wRightMotorSpeed = (unsigned short)(intensity * 655);
        XInputSetState(g_input.active_controller, &vibration);
    }
#else
    (void)intensity;
#endif
}

/* Xbox One specific - impulse triggers */
void input_set_trigger_rumble(int left_intensity, int right_intensity) {
#ifdef PLATFORM_XBOXONE
    if (g_input.active_controller >= 0) {
        /* Xbox One impulse triggers via extended API */
        XINPUT_VIBRATION_EX vibration;
        vibration.wLeftMotorSpeed = 0;
        vibration.wRightMotorSpeed = 0;
        vibration.wLeftTriggerSpeed = (unsigned short)(left_intensity * 655);
        vibration.wRightTriggerSpeed = (unsigned short)(right_intensity * 655);
        XInputSetStateEx(g_input.active_controller, &vibration);
    }
#else
    (void)left_intensity;
    (void)right_intensity;
#endif
    g_input.left_trigger_rumble = left_intensity;
    g_input.right_trigger_rumble = right_intensity;
}

int input_get_controller_count(void) {
    int count = 0;
    for (int i = 0; i < MAX_CONTROLLERS; i++) {
        if (g_input.connected[i]) count++;
    }
    return count;
}
