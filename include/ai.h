/*
 * RetroRacer - AI Racing System
 * Computer-controlled vehicle behavior
 */

#ifndef AI_H
#define AI_H

#include <stdint.h>
#include "vehicle.h"
#include "track.h"

/* AI difficulty levels */
typedef enum {
    AI_EASY,
    AI_MEDIUM,
    AI_HARD,
    AI_EXPERT
} ai_difficulty_t;

/* AI behavior state */
typedef enum {
    AI_STATE_RACING,
    AI_STATE_OVERTAKING,
    AI_STATE_DEFENDING,
    AI_STATE_RECOVERING
} ai_state_t;

/* AI controller for a vehicle */
typedef struct {
    vehicle_t *vehicle;
    ai_difficulty_t difficulty;
    ai_state_t state;

    /* Target following */
    float target_distance;      /* Distance along track to aim for */
    vec3_t target_pos;
    float look_ahead;           /* How far ahead to look */

    /* Behavior parameters */
    float aggression;           /* 0-1, how aggressive in overtaking */
    float skill;                /* 0-1, affects reaction time and precision */
    float error_rate;           /* How often AI makes mistakes */
    float speed_factor;         /* Multiplier for max speed */

    /* State timers */
    float state_timer;
    float reaction_delay;

    /* Randomness */
    float wander;               /* Random steering variation */
    uint32_t random_seed;
} ai_controller_t;

/* Initialize AI system */
void ai_init(void);

/* Create AI controller for vehicle */
ai_controller_t *ai_create(vehicle_t *vehicle, ai_difficulty_t difficulty);

/* Destroy AI controller */
void ai_destroy(ai_controller_t *ai);

/* Update AI decision making */
void ai_update(ai_controller_t *ai, track_t *track, vehicle_t *vehicles[], int vehicle_count, float dt);

/* Set difficulty level */
void ai_set_difficulty(ai_controller_t *ai, ai_difficulty_t difficulty);

/* Get difficulty name string */
const char *ai_difficulty_name(ai_difficulty_t difficulty);

/* Utility: Calculate optimal racing line */
vec3_t ai_calculate_racing_line(track_t *track, float distance);

/* Utility: Check if path is clear */
int ai_path_clear(ai_controller_t *ai, track_t *track, vehicle_t *vehicles[], int count, float distance);

#endif /* AI_H */
