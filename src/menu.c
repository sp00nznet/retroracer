/*
 * RetroRacer - Menu System Implementation
 * Game mode selection and UI
 */

#include "menu.h"
#include "vehicle.h"
#include "ai.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef DREAMCAST
#include <kos.h>
#include <dc/biosfont.h>
#include <dc/video.h>
#endif

static menu_state_t menu_state;
static int menu_active = 1;

/* Menu screen titles */
static const char *screen_titles[] = {
    "RETRORACER",
    "SELECT MODE",
    "SELECT VEHICLE",
    "SELECT DIFFICULTY",
    "PAUSED",
    "RACE RESULTS",
    "GRAND PRIX STANDINGS"
};

void menu_init(void) {
    memset(&menu_state, 0, sizeof(menu_state_t));
    menu_state.current_screen = MENU_MAIN;
    menu_state.selected_index = 0;
    menu_state.selected_mode = MODE_SINGLE_TRACK;
    menu_state.selected_vehicle = 0;
    menu_state.selected_difficulty = 1;  /* Medium */
    menu_state.num_laps = 3;
    menu_state.num_ai_opponents = 3;
    menu_active = 1;

    /* Setup main menu items */
    menu_set_screen(MENU_MAIN);
}

menu_state_t *menu_get_state(void) {
    return &menu_state;
}

static void setup_menu_items(void) {
    menu_state.item_count = 0;

    switch (menu_state.current_screen) {
        case MENU_MAIN:
            menu_state.items[0] = (menu_item_t){"Start Game", 0, 1};
            menu_state.items[1] = (menu_item_t){"Options", 1, 1};
            menu_state.items[2] = (menu_item_t){"Exit", 2, 1};
            menu_state.item_count = 3;
            break;

        case MENU_MODE_SELECT:
            menu_state.items[0] = (menu_item_t){"AI Race (Spectator)", MODE_AI_RACE, 1};
            menu_state.items[1] = (menu_item_t){"Single Track", MODE_SINGLE_TRACK, 1};
            menu_state.items[2] = (menu_item_t){"Time Trial", MODE_TIME_TRIAL, 1};
            menu_state.items[3] = (menu_item_t){"Grand Prix", MODE_GRAND_PRIX, 1};
            menu_state.items[4] = (menu_item_t){"Back", -1, 1};
            menu_state.item_count = 5;
            break;

        case MENU_VEHICLE_SELECT:
            menu_state.items[0] = (menu_item_t){"Standard", VEHICLE_STANDARD, 1};
            menu_state.items[1] = (menu_item_t){"Speed", VEHICLE_SPEED, 1};
            menu_state.items[2] = (menu_item_t){"Handling", VEHICLE_HANDLING, 1};
            menu_state.items[3] = (menu_item_t){"Balanced", VEHICLE_BALANCED, 1};
            menu_state.items[4] = (menu_item_t){"Back", -1, 1};
            menu_state.item_count = 5;
            break;

        case MENU_DIFFICULTY:
            menu_state.items[0] = (menu_item_t){"Easy", AI_EASY, 1};
            menu_state.items[1] = (menu_item_t){"Medium", AI_MEDIUM, 1};
            menu_state.items[2] = (menu_item_t){"Hard", AI_HARD, 1};
            menu_state.items[3] = (menu_item_t){"Expert", AI_EXPERT, 1};
            menu_state.items[4] = (menu_item_t){"Back", -1, 1};
            menu_state.item_count = 5;
            break;

        case MENU_PAUSE:
            menu_state.items[0] = (menu_item_t){"Resume", 0, 1};
            menu_state.items[1] = (menu_item_t){"Restart", 1, 1};
            menu_state.items[2] = (menu_item_t){"Quit to Menu", 2, 1};
            menu_state.item_count = 3;
            break;

        case MENU_RESULTS:
            menu_state.items[0] = (menu_item_t){"Continue", 0, 1};
            menu_state.items[1] = (menu_item_t){"Restart", 1, 1};
            menu_state.items[2] = (menu_item_t){"Quit to Menu", 2, 1};
            menu_state.item_count = 3;
            break;

        case MENU_GRAND_PRIX_STANDINGS:
            menu_state.items[0] = (menu_item_t){"Next Race", 0, 1};
            menu_state.items[1] = (menu_item_t){"Quit to Menu", 1, 1};
            menu_state.item_count = 2;
            break;
    }

    menu_state.selected_index = 0;
}

void menu_set_screen(menu_screen_t screen) {
    menu_state.current_screen = screen;
    menu_state.transitioning = 1;
    menu_state.transition_timer = 0;
    setup_menu_items();
}

void menu_navigate_up(void) {
    if (menu_state.selected_index > 0) {
        menu_state.selected_index--;
    } else {
        menu_state.selected_index = menu_state.item_count - 1;
    }
}

void menu_navigate_down(void) {
    if (menu_state.selected_index < menu_state.item_count - 1) {
        menu_state.selected_index++;
    } else {
        menu_state.selected_index = 0;
    }
}

void menu_select(void) {
    if (menu_state.selected_index >= menu_state.item_count) return;

    menu_item_t *item = &menu_state.items[menu_state.selected_index];
    if (!item->enabled) return;

    switch (menu_state.current_screen) {
        case MENU_MAIN:
            if (item->value == 0) {  /* Start Game */
                menu_set_screen(MENU_MODE_SELECT);
            } else if (item->value == 2) {  /* Exit */
                /* Exit game - handled by main loop */
            }
            break;

        case MENU_MODE_SELECT:
            if (item->value == -1) {  /* Back */
                menu_set_screen(MENU_MAIN);
            } else {
                menu_state.selected_mode = (game_mode_t)item->value;
                if (menu_state.selected_mode == MODE_AI_RACE) {
                    /* AI race goes straight to difficulty */
                    menu_set_screen(MENU_DIFFICULTY);
                } else {
                    menu_set_screen(MENU_VEHICLE_SELECT);
                }
            }
            break;

        case MENU_VEHICLE_SELECT:
            if (item->value == -1) {  /* Back */
                menu_set_screen(MENU_MODE_SELECT);
            } else {
                menu_state.selected_vehicle = item->value;
                menu_set_screen(MENU_DIFFICULTY);
            }
            break;

        case MENU_DIFFICULTY:
            if (item->value == -1) {  /* Back */
                if (menu_state.selected_mode == MODE_AI_RACE) {
                    menu_set_screen(MENU_MODE_SELECT);
                } else {
                    menu_set_screen(MENU_VEHICLE_SELECT);
                }
            } else {
                menu_state.selected_difficulty = item->value;
                /* Start the game - handled by game.c */
                menu_active = 0;
            }
            break;

        case MENU_PAUSE:
            if (item->value == 0) {  /* Resume */
                menu_active = 0;
            } else if (item->value == 1) {  /* Restart */
                menu_active = 0;
                /* Restart handled by game.c */
            } else if (item->value == 2) {  /* Quit */
                menu_set_screen(MENU_MAIN);
                menu_active = 1;
            }
            break;

        case MENU_RESULTS:
            if (item->value == 0) {  /* Continue */
                menu_active = 0;
            } else if (item->value == 1) {  /* Restart */
                menu_active = 0;
            } else if (item->value == 2) {  /* Quit */
                menu_set_screen(MENU_MAIN);
                menu_active = 1;
            }
            break;

        case MENU_GRAND_PRIX_STANDINGS:
            if (item->value == 0) {  /* Next Race */
                menu_active = 0;
            } else {  /* Quit */
                menu_set_screen(MENU_MAIN);
                menu_active = 1;
            }
            break;
    }
}

void menu_back(void) {
    switch (menu_state.current_screen) {
        case MENU_MODE_SELECT:
            menu_set_screen(MENU_MAIN);
            break;
        case MENU_VEHICLE_SELECT:
            menu_set_screen(MENU_MODE_SELECT);
            break;
        case MENU_DIFFICULTY:
            if (menu_state.selected_mode == MODE_AI_RACE) {
                menu_set_screen(MENU_MODE_SELECT);
            } else {
                menu_set_screen(MENU_VEHICLE_SELECT);
            }
            break;
        case MENU_PAUSE:
            menu_active = 0;  /* Resume game */
            break;
        default:
            break;
    }
}

void menu_update(input_state_t *input, float dt) {
    /* Update transition */
    if (menu_state.transitioning) {
        menu_state.transition_timer += dt;
        if (menu_state.transition_timer > 0.3f) {
            menu_state.transitioning = 0;
        }
    }

    /* Handle input */
    if (input_button_pressed(input, BTN_DPAD_UP)) {
        menu_navigate_up();
    }
    if (input_button_pressed(input, BTN_DPAD_DOWN)) {
        menu_navigate_down();
    }
    if (input_button_pressed(input, BTN_A) || input_button_pressed(input, BTN_START)) {
        menu_select();
    }
    if (input_button_pressed(input, BTN_B)) {
        menu_back();
    }
}

/* Draw text directly to VRAM (for menu rendering) */
static void draw_menu_text(int x, int y, uint32_t color, const char *text) {
#ifdef DREAMCAST
    bfont_set_foreground_color(color);
    bfont_draw_str(vram_s + y * 640 + x, 640, 0, text);
#else
    (void)x; (void)y; (void)color;
    printf("%s\n", text);
#endif
}

/* Fill screen with solid color (for menu background) */
static void fill_screen(uint32_t color) {
#ifdef DREAMCAST
    uint16_t col16 = ((color >> 8) & 0xF800) |  /* R */
                     ((color >> 5) & 0x07E0) |  /* G */
                     ((color >> 3) & 0x001F);   /* B */

    uint16_t *vram = vram_s;
    for (int i = 0; i < 640 * 480; i++) {
        vram[i] = col16;
    }
#else
    (void)color;
#endif
}

void menu_render(void) {
#ifdef DREAMCAST
    /* For menu, we bypass PVR and draw directly to VRAM */
    /* Wait for any PVR operations to complete */
    vid_waitvbl();

    /* Fill background with dark blue */
    fill_screen(PACK_COLOR(255, 20, 20, 60));
#endif

    int screen_w = 640;
    int screen_h = 480;
    int center_x = screen_w / 2;

    /* Draw title */
    const char *title = screen_titles[menu_state.current_screen];
    int title_x = center_x - (int)(strlen(title) * 6);
    draw_menu_text(title_x, 60, COLOR_YELLOW, title);

    /* Draw subtitle based on screen */
    if (menu_state.current_screen == MENU_MAIN) {
        draw_menu_text(center_x - 110, 100, COLOR_WHITE, "Dreamcast Racing Game");
    }

    /* Draw menu items */
    int start_y = 180;
    int item_height = 30;

    for (int i = 0; i < menu_state.item_count; i++) {
        menu_item_t *item = &menu_state.items[i];
        int y = start_y + i * item_height;
        int x = center_x - 80;

        uint32_t color = COLOR_WHITE;
        if (i == menu_state.selected_index) {
            color = COLOR_YELLOW;
            /* Draw selection indicator */
            draw_menu_text(x - 20, y, COLOR_YELLOW, ">");
        }
        if (!item->enabled) {
            color = COLOR_GRAY;
        }

        draw_menu_text(x, y, color, item->text);
    }

    /* Draw mode-specific info */
    if (menu_state.current_screen == MENU_MODE_SELECT) {
        const char *desc = "";
        switch (menu_state.selected_index) {
            case 0: desc = "Watch AI cars race each other"; break;
            case 1: desc = "Race one track against AI"; break;
            case 2: desc = "Beat the best lap time"; break;
            case 3: desc = "4-race championship"; break;
        }
        draw_menu_text(center_x - 130, 380, COLOR_GRAY, desc);
    }

    /* Draw controls hint */
    draw_menu_text(20, screen_h - 40, COLOR_GRAY, "A:Select B:Back D-Pad:Navigate");
}

int menu_is_active(void) {
    return menu_active;
}

void menu_show_pause(void) {
    menu_set_screen(MENU_PAUSE);
    menu_active = 1;
}

void menu_show_results(float time, float best_time, int place) {
    (void)time; (void)best_time; (void)place;
    menu_set_screen(MENU_RESULTS);
    menu_active = 1;
}

void menu_show_standings(int *points, int num_racers) {
    (void)points; (void)num_racers;
    menu_set_screen(MENU_GRAND_PRIX_STANDINGS);
    menu_active = 1;
}

game_mode_t menu_get_mode(void) {
    return menu_state.selected_mode;
}

const char *menu_mode_name(game_mode_t mode) {
    switch (mode) {
        case MODE_AI_RACE: return "AI Race";
        case MODE_SINGLE_TRACK: return "Single Track";
        case MODE_TIME_TRIAL: return "Time Trial";
        case MODE_GRAND_PRIX: return "Grand Prix";
        default: return "Unknown";
    }
}
