/*
 * RetroRacer - Menu System
 * Game mode selection and UI
 */

#ifndef MENU_H
#define MENU_H

#include "render.h"
#include "input.h"

/* Game modes as specified in README */
typedef enum {
    MODE_AI_RACE,       /* AI races itself - spectator mode */
    MODE_SINGLE_TRACK,  /* Single race mode */
    MODE_TIME_TRIAL,    /* Beat the best time */
    MODE_GRAND_PRIX     /* 4-race championship */
} game_mode_t;

/* Menu screens */
typedef enum {
    MENU_MAIN,
    MENU_MODE_SELECT,
    MENU_VEHICLE_SELECT,
    MENU_DIFFICULTY,
    MENU_OPTIONS,
    MENU_MUSIC_SELECT,
    MENU_PAUSE,
    MENU_RESULTS,
    MENU_GRAND_PRIX_STANDINGS
} menu_screen_t;

/* Menu item */
typedef struct {
    const char *text;
    int value;
    int enabled;
} menu_item_t;

/* Menu state */
typedef struct {
    menu_screen_t current_screen;
    int selected_index;
    int item_count;
    menu_item_t items[10];

    /* Selections */
    game_mode_t selected_mode;
    int selected_vehicle;
    int selected_difficulty;
    int num_laps;
    int num_ai_opponents;

    /* Animation */
    float transition_timer;
    int transitioning;
} menu_state_t;

/* Initialize menu system */
void menu_init(void);

/* Get menu state */
menu_state_t *menu_get_state(void);

/* Update menu with input */
void menu_update(input_state_t *input, float dt);

/* Render current menu screen */
void menu_render(void);

/* Navigation */
void menu_navigate_up(void);
void menu_navigate_down(void);
void menu_select(void);
void menu_back(void);

/* Set specific screen */
void menu_set_screen(menu_screen_t screen);

/* Check if menu is active */
int menu_is_active(void);

/* Show pause menu */
void menu_show_pause(void);

/* Show results screen */
void menu_show_results(float time, float best_time, int place);

/* Show Grand Prix standings */
void menu_show_standings(int *points, int num_racers);

/* Get selected game mode */
game_mode_t menu_get_mode(void);

/* Get mode name string */
const char *menu_mode_name(game_mode_t mode);

#endif /* MENU_H */
