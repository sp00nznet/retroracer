/*
 * RetroRacer - Main Game Logic
 * Game state management and main loop
 */

#ifndef GAME_H
#define GAME_H

#include "math3d.h"
#include "render.h"
#include "track.h"
#include "vehicle.h"
#include "ai.h"
#include "menu.h"
#include "input.h"

/* Game states */
typedef enum {
    GAME_STATE_INIT,
    GAME_STATE_MENU,
    GAME_STATE_LOADING,
    GAME_STATE_COUNTDOWN,
    GAME_STATE_RACING,
    GAME_STATE_PAUSED,
    GAME_STATE_FINISHED,
    GAME_STATE_RESULTS
} game_state_t;

/* Grand Prix standings */
typedef struct {
    int points[MAX_VEHICLES];
    int current_race;
    int total_races;
    int finished;
} grand_prix_t;

/* Main game structure */
typedef struct {
    game_state_t state;
    game_mode_t mode;

    /* Track */
    track_t *track;
    track_params_t track_params;

    /* Vehicles */
    vehicle_t *vehicles[MAX_VEHICLES];
    ai_controller_t *ai_controllers[MAX_VEHICLES];
    int vehicle_count;
    int player_vehicle_index;

    /* Camera */
    camera_t camera;
    float camera_distance;
    float camera_height;

    /* Timing */
    float race_time;
    float countdown_timer;
    int countdown_value;

    /* Race settings */
    int num_laps;
    int current_lap;

    /* Grand Prix */
    grand_prix_t grand_prix;

    /* Time trial */
    float best_time;
    float ghost_progress;

    /* Statistics */
    int races_completed;
    float total_play_time;
} game_t;

/* Initialize game */
void game_init(void);

/* Shutdown game */
void game_shutdown(void);

/* Main update function */
void game_update(float dt);

/* Main render function */
void game_render(void);

/* Get game instance */
game_t *game_get_instance(void);

/* State transitions */
void game_start_race(game_mode_t mode, int num_laps, int num_opponents);
void game_end_race(void);
void game_pause(void);
void game_resume(void);
void game_restart_race(void);
void game_return_to_menu(void);

/* Grand Prix */
void game_start_grand_prix(void);
void game_next_grand_prix_race(void);
void game_finish_grand_prix(void);

/* Camera control */
void game_update_camera(float dt);
void game_set_camera_mode(int mode);

/* Utility */
float game_get_delta_time(void);
int game_is_racing(void);

#endif /* GAME_H */
