/*
 * RetroRacer - Wii Input Implementation
 * Supports Wii Remote, Nunchuk, Classic Controller, and GameCube Controller
 */

#include "input.h"
#include "platform.h"
#include <string.h>

#ifdef PLATFORM_WII
#include <gccore.h>
#include <wiiuse/wpad.h>
#include <ogc/pad.h>
#endif

/* Controller types */
#define CTRL_WIIMOTE     0
#define CTRL_NUNCHUK     1
#define CTRL_CLASSIC     2
#define CTRL_GAMECUBE    3

/* Input state */
static struct {
    int controller_type;
    int connected;

    /* Current state */
    float steering;       /* -1.0 to 1.0 */
    float throttle;       /* 0.0 to 1.0 */
    float brake;          /* 0.0 to 1.0 */
    int button_state;

    /* Previous frame for edge detection */
    int prev_button_state;

    /* Wii Remote tilt calibration */
    float tilt_center;
    float tilt_range;

    int initialized;
} g_input;

/* Button mappings */
#define BTN_ACCELERATE   0x0001
#define BTN_BRAKE        0x0002
#define BTN_PAUSE        0x0004
#define BTN_SELECT       0x0008
#define BTN_BACK         0x0010
#define BTN_ITEM         0x0020

void input_init(void) {
    if (g_input.initialized) return;

    memset(&g_input, 0, sizeof(g_input));

#ifdef PLATFORM_WII
    /* Initialize Wii Remote */
    WPAD_Init();
    WPAD_SetDataFormat(WPAD_CHAN_ALL, WPAD_FMT_BTNS_ACC_IR);
    WPAD_SetVRes(WPAD_CHAN_ALL, 640, 480);

    /* Initialize GameCube controllers */
    PAD_Init();
#endif

    /* Default calibration */
    g_input.tilt_center = 0.0f;
    g_input.tilt_range = 90.0f;

    g_input.initialized = 1;
}

void input_shutdown(void) {
    if (!g_input.initialized) return;

#ifdef PLATFORM_WII
    WPAD_Shutdown();
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

#ifdef PLATFORM_WII
    /* Scan for input */
    WPAD_ScanPads();
    PAD_ScanPads();

    /* Check Wii Remote */
    u32 wpad_type;
    if (WPAD_Probe(WPAD_CHAN_0, &wpad_type) == WPAD_ERR_NONE) {
        g_input.connected = 1;

        WPADData *wpad = WPAD_Data(WPAD_CHAN_0);
        u32 held = WPAD_ButtonsHeld(WPAD_CHAN_0);
        u32 exp_type = wpad->exp.type;

        if (exp_type == WPAD_EXP_NUNCHUK) {
            /* Nunchuk + Wiimote */
            g_input.controller_type = CTRL_NUNCHUK;

            /* Analog stick for steering */
            float stick_x = (wpad->exp.nunchuk.js.pos.x - 128) / 128.0f;
            g_input.steering = stick_x;

            /* Buttons */
            if (held & WPAD_BUTTON_A) g_input.button_state |= BTN_ACCELERATE;
            if (held & WPAD_BUTTON_B) g_input.button_state |= BTN_BRAKE;
            if (held & WPAD_NUNCHUK_BUTTON_Z) g_input.button_state |= BTN_ITEM;
            if (held & WPAD_BUTTON_PLUS) g_input.button_state |= BTN_PAUSE;
            if (held & WPAD_BUTTON_HOME) g_input.button_state |= BTN_BACK;

        } else if (exp_type == WPAD_EXP_CLASSIC) {
            /* Classic Controller */
            g_input.controller_type = CTRL_CLASSIC;

            /* Left stick for steering */
            float stick_x = (wpad->exp.classic.ljs.pos.x - 32) / 32.0f;
            g_input.steering = stick_x;

            /* Analog triggers */
            g_input.throttle = wpad->exp.classic.r_shoulder / 31.0f;
            g_input.brake = wpad->exp.classic.l_shoulder / 31.0f;

            /* Buttons */
            if (held & WPAD_CLASSIC_BUTTON_A) g_input.button_state |= BTN_ACCELERATE;
            if (held & WPAD_CLASSIC_BUTTON_B) g_input.button_state |= BTN_BRAKE;
            if (held & WPAD_CLASSIC_BUTTON_X) g_input.button_state |= BTN_ITEM;
            if (held & WPAD_CLASSIC_BUTTON_PLUS) g_input.button_state |= BTN_PAUSE;
            if (held & WPAD_CLASSIC_BUTTON_HOME) g_input.button_state |= BTN_BACK;

        } else {
            /* Wiimote only - tilt steering */
            g_input.controller_type = CTRL_WIIMOTE;

            /* Accelerometer tilt for steering (held sideways) */
            if (wpad->ir.valid) {
                /* Use IR for more precise control if available */
                g_input.steering = (wpad->ir.x - 320) / 320.0f;
            } else {
                /* Fall back to tilt */
                orient_t orient;
                WPAD_Orientation(WPAD_CHAN_0, &orient);
                g_input.steering = (orient.roll - g_input.tilt_center) / g_input.tilt_range;
            }

            /* Clamp steering */
            if (g_input.steering < -1.0f) g_input.steering = -1.0f;
            if (g_input.steering > 1.0f) g_input.steering = 1.0f;

            /* Buttons (held sideways: 2 = accelerate, 1 = brake) */
            if (held & WPAD_BUTTON_2) g_input.button_state |= BTN_ACCELERATE;
            if (held & WPAD_BUTTON_1) g_input.button_state |= BTN_BRAKE;
            if (held & WPAD_BUTTON_A) g_input.button_state |= BTN_ITEM;
            if (held & WPAD_BUTTON_PLUS) g_input.button_state |= BTN_PAUSE;
            if (held & WPAD_BUTTON_HOME) g_input.button_state |= BTN_BACK;
        }
    }

    /* Check GameCube Controller */
    u16 gc_held = PAD_ButtonsHeld(PAD_CHAN0);
    if (gc_held || PAD_StickX(PAD_CHAN0) || PAD_StickY(PAD_CHAN0)) {
        g_input.connected = 1;
        g_input.controller_type = CTRL_GAMECUBE;

        /* Analog stick */
        g_input.steering = PAD_StickX(PAD_CHAN0) / 128.0f;

        /* Analog triggers */
        g_input.throttle = PAD_TriggerR(PAD_CHAN0) / 255.0f;
        g_input.brake = PAD_TriggerL(PAD_CHAN0) / 255.0f;

        /* Buttons */
        if (gc_held & PAD_BUTTON_A) g_input.button_state |= BTN_ACCELERATE;
        if (gc_held & PAD_BUTTON_B) g_input.button_state |= BTN_BRAKE;
        if (gc_held & PAD_BUTTON_X) g_input.button_state |= BTN_ITEM;
        if (gc_held & PAD_BUTTON_START) g_input.button_state |= BTN_PAUSE;
    }

#else
    /* Stub - set reasonable defaults */
    g_input.connected = 1;
    g_input.controller_type = CTRL_GAMECUBE;
#endif

    /* Convert button accelerate/brake to analog if not using triggers */
    if (g_input.button_state & BTN_ACCELERATE && g_input.throttle < 0.5f) {
        g_input.throttle = 1.0f;
    }
    if (g_input.button_state & BTN_BRAKE && g_input.brake < 0.5f) {
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
#ifdef PLATFORM_WII
    if (intensity > 0) {
        WPAD_Rumble(WPAD_CHAN_0, 1);
        PAD_ControlMotor(PAD_CHAN0, PAD_MOTOR_RUMBLE);
    } else {
        WPAD_Rumble(WPAD_CHAN_0, 0);
        PAD_ControlMotor(PAD_CHAN0, PAD_MOTOR_STOP);
    }
#else
    (void)intensity;
#endif
}

void input_calibrate_tilt(void) {
#ifdef PLATFORM_WII
    /* Calibrate Wiimote tilt center */
    WPAD_ScanPads();
    orient_t orient;
    WPAD_Orientation(WPAD_CHAN_0, &orient);
    g_input.tilt_center = orient.roll;
#endif
}
