/*
 * RetroRacer - Nintendo Switch Input Implementation
 * Supports Joy-Con (attached/detached), Pro Controller, handheld mode
 */

#include "input.h"
#include "platform.h"
#include <string.h>

#ifdef PLATFORM_SWITCH
#include <switch.h>
#endif

/* Controller types */
#define CTRL_HANDHELD    0
#define CTRL_JOYCON_DUAL 1
#define CTRL_JOYCON_L    2
#define CTRL_JOYCON_R    3
#define CTRL_PRO         4

/* Input state */
static struct {
    int controller_type;
    int connected;
    int player_count;

    float steering;
    float throttle;
    float brake;
    int button_state;
    int prev_button_state;

    /* Motion controls */
    int use_motion;
    float motion_center;

    /* Touch screen (handheld) */
    int touch_count;
    float touch_x[10];
    float touch_y[10];

    int initialized;
} g_input;

#define BTN_ACCELERATE   0x0001
#define BTN_BRAKE        0x0002
#define BTN_PAUSE        0x0004
#define BTN_SELECT       0x0008
#define BTN_BACK         0x0010
#define BTN_ITEM         0x0020

void input_init(void) {
    if (g_input.initialized) return;

    memset(&g_input, 0, sizeof(g_input));

#ifdef PLATFORM_SWITCH
    /* Initialize input */
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);

    /* Initialize touch screen */
    hidInitializeTouchScreen();
#endif

    g_input.initialized = 1;
}

void input_shutdown(void) {
    if (!g_input.initialized) return;

#ifdef PLATFORM_SWITCH
    /* Nothing specific needed */
#endif

    g_input.initialized = 0;
}

void input_update(void) {
    if (!g_input.initialized) return;

    g_input.prev_button_state = g_input.button_state;
    g_input.button_state = 0;
    g_input.steering = 0.0f;
    g_input.throttle = 0.0f;
    g_input.brake = 0.0f;
    g_input.touch_count = 0;

#ifdef PLATFORM_SWITCH
    /* Update pad state */
    PadState pad;
    padInitializeDefault(&pad);
    padUpdate(&pad);

    u64 style = padGetStyleSet(&pad);
    u64 held = padGetButtons(&pad);

    g_input.connected = padIsConnected(&pad);

    /* Determine controller type */
    if (style & HidNpadStyleTag_NpadHandheld) {
        g_input.controller_type = CTRL_HANDHELD;
    } else if (style & HidNpadStyleTag_NpadFullKey) {
        g_input.controller_type = CTRL_PRO;
    } else if (style & HidNpadStyleTag_NpadJoyDual) {
        g_input.controller_type = CTRL_JOYCON_DUAL;
    } else if (style & HidNpadStyleTag_NpadJoyLeft) {
        g_input.controller_type = CTRL_JOYCON_L;
    } else if (style & HidNpadStyleTag_NpadJoyRight) {
        g_input.controller_type = CTRL_JOYCON_R;
    }

    /* Read analog sticks */
    HidAnalogStickState left_stick = padGetStickPos(&pad, 0);
    HidAnalogStickState right_stick = padGetStickPos(&pad, 1);

    /* Left stick for steering */
    g_input.steering = left_stick.x / 32767.0f;

    /* Right stick Y for throttle/brake */
    if (right_stick.y > 5000) {
        g_input.throttle = right_stick.y / 32767.0f;
    } else if (right_stick.y < -5000) {
        g_input.brake = -right_stick.y / 32767.0f;
    }

    /* Motion controls */
    if (g_input.use_motion) {
        HidSixAxisSensorState sixaxis;
        u64 style_set = cycpadGetStyleSet(&pad);

        if (style_set & HidNpadStyleTag_NpadJoyDual ||
            style_set & HidNpadStyleTag_NpadHandheld) {
            /* Read gyro for steering */
            HidSixAxisSensorHandle handles[2];
            hidGetSixAxisSensorHandles(&handles[0], 2, HidNpadIdType_No1, HidNpadStyleTag_NpadJoyDual);
            hidGetSixAxisSensorStates(handles[0], &sixaxis, 1);

            float gyro_steer = sixaxis.angular_velocity.z - g_input.motion_center;
            gyro_steer = gyro_steer / 3.0f;
            if (gyro_steer < -1.0f) gyro_steer = -1.0f;
            if (gyro_steer > 1.0f) gyro_steer = 1.0f;
            g_input.steering = gyro_steer;
        }
    }

    /* Button mapping */
    if (held & HidNpadButton_A) g_input.button_state |= BTN_ACCELERATE;
    if (held & HidNpadButton_B) g_input.button_state |= BTN_BRAKE;
    if (held & HidNpadButton_X) g_input.button_state |= BTN_ITEM;
    if (held & HidNpadButton_Y) g_input.button_state |= BTN_SELECT;
    if (held & HidNpadButton_Plus) g_input.button_state |= BTN_PAUSE;
    if (held & HidNpadButton_Minus) g_input.button_state |= BTN_BACK;

    /* Triggers */
    if (held & HidNpadButton_ZR) {
        g_input.button_state |= BTN_ACCELERATE;
        g_input.throttle = 1.0f;
    }
    if (held & HidNpadButton_ZL) {
        g_input.button_state |= BTN_BRAKE;
        g_input.brake = 1.0f;
    }

    /* Shoulder buttons as alternative */
    if (held & HidNpadButton_R) g_input.throttle = 1.0f;
    if (held & HidNpadButton_L) g_input.brake = 1.0f;

    /* D-pad for digital steering */
    if (held & HidNpadButton_Left) g_input.steering = -1.0f;
    if (held & HidNpadButton_Right) g_input.steering = 1.0f;

    /* Touch screen (handheld mode only) */
    if (g_input.controller_type == CTRL_HANDHELD) {
        HidTouchScreenState touch_state;
        hidGetTouchScreenStates(&touch_state, 1);

        g_input.touch_count = touch_state.count;
        for (int i = 0; i < touch_state.count && i < 10; i++) {
            g_input.touch_x[i] = touch_state.touches[i].x / 1280.0f;
            g_input.touch_y[i] = touch_state.touches[i].y / 720.0f;
        }
    }

#else
    g_input.connected = 1;
    g_input.controller_type = CTRL_PRO;
#endif

    /* Digital button to analog conversion */
    if ((g_input.button_state & BTN_ACCELERATE) && g_input.throttle < 0.5f) {
        g_input.throttle = 1.0f;
    }
    if ((g_input.button_state & BTN_BRAKE) && g_input.brake < 0.5f) {
        g_input.brake = 1.0f;
    }
}

int input_is_connected(void) {
    return g_input.connected;
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
#ifdef PLATFORM_SWITCH
    HidVibrationValue vibration;
    vibration.amp_low = intensity > 0 ? 0.5f : 0.0f;
    vibration.freq_low = 160.0f;
    vibration.amp_high = intensity > 0 ? 0.5f : 0.0f;
    vibration.freq_high = 320.0f;

    HidVibrationDeviceHandle handles[2];
    hidInitializeVibrationDevices(handles, 2, HidNpadIdType_No1, HidNpadStyleTag_NpadJoyDual);
    hidSendVibrationValues(handles, &vibration, 2);
#else
    (void)intensity;
#endif
}

/* Switch specific */
int input_get_touch_count(void) {
    return g_input.touch_count;
}

int input_get_touch(int index, float *x, float *y) {
    if (index >= 0 && index < g_input.touch_count && x && y) {
        *x = g_input.touch_x[index];
        *y = g_input.touch_y[index];
        return 1;
    }
    return 0;
}

void input_enable_motion(int enable) {
#ifdef PLATFORM_SWITCH
    g_input.use_motion = enable;
    if (enable) {
        HidSixAxisSensorHandle handles[2];
        hidGetSixAxisSensorHandles(&handles[0], 2, HidNpadIdType_No1, HidNpadStyleTag_NpadJoyDual);
        hidStartSixAxisSensor(handles[0]);
        hidStartSixAxisSensor(handles[1]);
    }
#else
    (void)enable;
#endif
}

void input_calibrate_motion(void) {
#ifdef PLATFORM_SWITCH
    HidSixAxisSensorState sixaxis;
    HidSixAxisSensorHandle handles[2];
    hidGetSixAxisSensorHandles(&handles[0], 2, HidNpadIdType_No1, HidNpadStyleTag_NpadJoyDual);
    hidGetSixAxisSensorStates(handles[0], &sixaxis, 1);
    g_input.motion_center = sixaxis.angular_velocity.z;
#endif
}
