/*
 * RetroRacer - Wii U Input Implementation
 * Supports GamePad, Pro Controller, Wii Remote, and Wii Classic Controller
 */

#include "input.h"
#include "platform.h"
#include <string.h>

#ifdef PLATFORM_WIIU
#include <vpad/input.h>
#include <padscore/kpad.h>
#include <padscore/wpad.h>
#endif

/* Controller types */
#define CTRL_GAMEPAD     0
#define CTRL_PRO         1
#define CTRL_WIIMOTE     2
#define CTRL_CLASSIC     3

/* Input state */
static struct {
    int controller_type;
    int connected;

    float steering;
    float throttle;
    float brake;
    int button_state;
    int prev_button_state;

    /* GamePad touch state */
    int touch_active;
    float touch_x;
    float touch_y;

    /* Gyro steering */
    int use_gyro;
    float gyro_center;

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

#ifdef PLATFORM_WIIU
    /* Initialize VPAD (GamePad) */
    VPADInit();

    /* Initialize KPAD (Wii controllers) */
    KPADInit();
    WPADEnableURCC(1);  /* Enable USB controllers */
#endif

    g_input.use_gyro = 0;
    g_input.initialized = 1;
}

void input_shutdown(void) {
    if (!g_input.initialized) return;

#ifdef PLATFORM_WIIU
    KPADShutdown();
    VPADShutdown();
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
    g_input.touch_active = 0;

#ifdef PLATFORM_WIIU
    /* Read GamePad */
    VPADStatus vpad;
    VPADReadError vpad_error;
    VPADRead(VPAD_CHAN_0, &vpad, 1, &vpad_error);

    if (vpad_error == VPAD_READ_SUCCESS) {
        g_input.connected = 1;
        g_input.controller_type = CTRL_GAMEPAD;

        /* Left stick for steering */
        g_input.steering = vpad.leftStick.x;

        /* Triggers for throttle/brake */
        if (vpad.hold & VPAD_BUTTON_ZR) {
            g_input.throttle = 1.0f;
        }
        if (vpad.hold & VPAD_BUTTON_ZL) {
            g_input.brake = 1.0f;
        }

        /* Right stick can also control */
        if (vpad.rightStick.y > 0.3f) {
            g_input.throttle = vpad.rightStick.y;
        } else if (vpad.rightStick.y < -0.3f) {
            g_input.brake = -vpad.rightStick.y;
        }

        /* Gyro steering option */
        if (g_input.use_gyro) {
            float gyro_steer = (vpad.gyro.z - g_input.gyro_center) / 2.0f;
            if (gyro_steer < -1.0f) gyro_steer = -1.0f;
            if (gyro_steer > 1.0f) gyro_steer = 1.0f;
            g_input.steering = gyro_steer;
        }

        /* Touch screen */
        VPADTouchData touch;
        VPADGetTPCalibratedPoint(VPAD_CHAN_0, &touch, &vpad.tpNormal);
        if (touch.touched) {
            g_input.touch_active = 1;
            g_input.touch_x = touch.x / 1280.0f;
            g_input.touch_y = touch.y / 720.0f;
        }

        /* Buttons */
        if (vpad.hold & VPAD_BUTTON_A) g_input.button_state |= BTN_ACCELERATE;
        if (vpad.hold & VPAD_BUTTON_B) g_input.button_state |= BTN_BRAKE;
        if (vpad.hold & VPAD_BUTTON_X) g_input.button_state |= BTN_ITEM;
        if (vpad.hold & VPAD_BUTTON_Y) g_input.button_state |= BTN_SELECT;
        if (vpad.hold & VPAD_BUTTON_PLUS) g_input.button_state |= BTN_PAUSE;
        if (vpad.hold & VPAD_BUTTON_HOME) g_input.button_state |= BTN_BACK;

        /* D-pad as alternative steering */
        if (vpad.hold & VPAD_BUTTON_LEFT) g_input.steering = -1.0f;
        if (vpad.hold & VPAD_BUTTON_RIGHT) g_input.steering = 1.0f;
    }

    /* Check for Pro Controller or Wii Remote */
    KPADStatus kpad;
    KPADError kpad_error;

    for (int i = 0; i < 4; i++) {
        if (KPADReadEx((WPADChan)i, &kpad, 1, &kpad_error) > 0 &&
            kpad_error == KPAD_ERROR_OK) {

            if (kpad.extensionType == WPAD_EXT_PRO_CONTROLLER) {
                g_input.connected = 1;
                g_input.controller_type = CTRL_PRO;

                g_input.steering = kpad.pro.leftStick.x;

                if (kpad.pro.hold & WPAD_PRO_BUTTON_ZR) g_input.throttle = 1.0f;
                if (kpad.pro.hold & WPAD_PRO_BUTTON_ZL) g_input.brake = 1.0f;
                if (kpad.pro.hold & WPAD_PRO_BUTTON_A) g_input.button_state |= BTN_ACCELERATE;
                if (kpad.pro.hold & WPAD_PRO_BUTTON_B) g_input.button_state |= BTN_BRAKE;
                if (kpad.pro.hold & WPAD_PRO_BUTTON_X) g_input.button_state |= BTN_ITEM;
                if (kpad.pro.hold & WPAD_PRO_BUTTON_PLUS) g_input.button_state |= BTN_PAUSE;
                if (kpad.pro.hold & WPAD_PRO_BUTTON_HOME) g_input.button_state |= BTN_BACK;

            } else if (kpad.extensionType == WPAD_EXT_CLASSIC ||
                       kpad.extensionType == WPAD_EXT_CLASSIC_PRO) {
                g_input.connected = 1;
                g_input.controller_type = CTRL_CLASSIC;

                g_input.steering = kpad.classic.leftStick.x;
                g_input.throttle = kpad.classic.rightTrigger;
                g_input.brake = kpad.classic.leftTrigger;

                if (kpad.classic.hold & WPAD_CLASSIC_BUTTON_A) g_input.button_state |= BTN_ACCELERATE;
                if (kpad.classic.hold & WPAD_CLASSIC_BUTTON_B) g_input.button_state |= BTN_BRAKE;
                if (kpad.classic.hold & WPAD_CLASSIC_BUTTON_PLUS) g_input.button_state |= BTN_PAUSE;
                if (kpad.classic.hold & WPAD_CLASSIC_BUTTON_HOME) g_input.button_state |= BTN_BACK;
            }
            break;
        }
    }

#else
    g_input.connected = 1;
    g_input.controller_type = CTRL_GAMEPAD;
#endif

    /* Apply button-to-analog conversion */
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
#ifdef PLATFORM_WIIU
    if (intensity > 0) {
        VPADControlMotor(VPAD_CHAN_0, (uint8_t *)"\xFF", 1);
    } else {
        VPADStopMotor(VPAD_CHAN_0);
    }
#else
    (void)intensity;
#endif
}

/* Wii U specific - touch screen access */
int input_get_touch(float *x, float *y) {
    if (g_input.touch_active && x && y) {
        *x = g_input.touch_x;
        *y = g_input.touch_y;
        return 1;
    }
    return 0;
}

void input_enable_gyro(int enable) {
    g_input.use_gyro = enable;
}

void input_calibrate_gyro(void) {
#ifdef PLATFORM_WIIU
    VPADStatus vpad;
    VPADReadError error;
    VPADRead(VPAD_CHAN_0, &vpad, 1, &error);
    if (error == VPAD_READ_SUCCESS) {
        g_input.gyro_center = vpad.gyro.z;
    }
#endif
}
