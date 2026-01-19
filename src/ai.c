/*
 * RetroRacer - AI Racing System Implementation
 * Computer-controlled vehicle behavior
 */

#include "ai.h"
#include <stdlib.h>
#include <string.h>

/* Simple PRNG for AI variation */
static uint32_t ai_rand_state = 54321;

static float ai_rand_float(void) {
    ai_rand_state = ai_rand_state * 1103515245 + 12345;
    return (float)((ai_rand_state >> 16) & 0x7FFF) / 32767.0f;
}

/* Difficulty parameters */
static const struct {
    float skill;
    float aggression;
    float error_rate;
    float speed_factor;
    float look_ahead;
} difficulty_params[] = {
    /* AI_EASY */   {0.6f, 0.2f, 0.15f, 0.85f, 15.0f},
    /* AI_MEDIUM */ {0.75f, 0.4f, 0.08f, 0.92f, 20.0f},
    /* AI_HARD */   {0.9f, 0.6f, 0.03f, 0.97f, 25.0f},
    /* AI_EXPERT */ {0.98f, 0.8f, 0.01f, 1.0f, 30.0f}
};

void ai_init(void) {
    ai_rand_state = 54321;
}

ai_controller_t *ai_create(vehicle_t *vehicle, ai_difficulty_t difficulty) {
    ai_controller_t *ai = (ai_controller_t *)malloc(sizeof(ai_controller_t));
    memset(ai, 0, sizeof(ai_controller_t));

    ai->vehicle = vehicle;
    ai->difficulty = difficulty;
    ai->state = AI_STATE_RACING;

    /* Set parameters from difficulty */
    ai->skill = difficulty_params[difficulty].skill;
    ai->aggression = difficulty_params[difficulty].aggression;
    ai->error_rate = difficulty_params[difficulty].error_rate;
    ai->speed_factor = difficulty_params[difficulty].speed_factor;
    ai->look_ahead = difficulty_params[difficulty].look_ahead;

    ai->random_seed = (uint32_t)(ai_rand_float() * 65535);
    ai->wander = 0;
    ai->reaction_delay = 0;

    return ai;
}

void ai_destroy(ai_controller_t *ai) {
    if (ai) {
        free(ai);
    }
}

void ai_set_difficulty(ai_controller_t *ai, ai_difficulty_t difficulty) {
    ai->difficulty = difficulty;
    ai->skill = difficulty_params[difficulty].skill;
    ai->aggression = difficulty_params[difficulty].aggression;
    ai->error_rate = difficulty_params[difficulty].error_rate;
    ai->speed_factor = difficulty_params[difficulty].speed_factor;
    ai->look_ahead = difficulty_params[difficulty].look_ahead;
}

vec3_t ai_calculate_racing_line(track_t *track, float distance) {
    vec3_t pos, dir;
    track_get_position(track, distance, &pos, &dir);
    return pos;
}

int ai_path_clear(ai_controller_t *ai, track_t *track, vehicle_t *vehicles[], int count, float distance) {
    vec3_t ahead_pos, dir;
    track_get_position(track, distance, &ahead_pos, &dir);

    for (int i = 0; i < count; i++) {
        if (vehicles[i] == ai->vehicle) continue;

        float dist = vec3_distance(ahead_pos, vehicles[i]->position);
        if (dist < 5.0f) {
            return 0;  /* Path blocked */
        }
    }

    return 1;
}

void ai_update(ai_controller_t *ai, track_t *track, vehicle_t *vehicles[], int vehicle_count, float dt) {
    if (!ai || !ai->vehicle || !track) return;

    vehicle_t *v = ai->vehicle;

    /* Update wander (random steering variation) */
    if (ai_rand_float() < ai->error_rate) {
        ai->wander = (ai_rand_float() - 0.5f) * 0.3f;
    }
    ai->wander *= 0.95f;  /* Decay wander */

    /* Calculate target position (look ahead on track) */
    float current_progress = v->track_progress * track->total_length;
    float target_distance = current_progress + ai->look_ahead + v->speed * 0.5f;

    vec3_t target_pos, target_dir;
    track_get_position(track, target_distance, &target_pos, &target_dir);

    ai->target_pos = target_pos;
    ai->target_distance = target_distance;

    /* Calculate steering to reach target */
    vec3_t to_target = vec3_sub(target_pos, v->position);
    vec3_t forward = vehicle_get_forward(v);

    /* Calculate angle to target */
    float target_angle = atan2f(to_target.x, to_target.z);
    float current_angle = v->rotation_y;

    float angle_diff = target_angle - current_angle;

    /* Normalize angle difference to -PI to PI */
    while (angle_diff > 3.14159f) angle_diff -= 6.28318f;
    while (angle_diff < -3.14159f) angle_diff += 6.28318f;

    /* Apply steering with skill factor */
    float steering = clamp(angle_diff * 2.0f * ai->skill, -1.0f, 1.0f);
    steering += ai->wander;
    steering = clamp(steering, -1.0f, 1.0f);

    /* Check for nearby vehicles (avoidance) */
    for (int i = 0; i < vehicle_count; i++) {
        if (vehicles[i] == v) continue;

        vec3_t to_other = vec3_sub(vehicles[i]->position, v->position);
        float dist = vec3_length(to_other);

        if (dist < 8.0f) {
            /* Other vehicle is close */
            float lateral = vec3_dot(to_other, vec3_cross(forward, vec3_create(0, 1, 0)));

            /* Steer away from other vehicle */
            if (lateral > 0) {
                steering -= 0.3f * ai->aggression * (1.0f - dist / 8.0f);
            } else {
                steering += 0.3f * ai->aggression * (1.0f - dist / 8.0f);
            }

            /* Determine if we should try to overtake */
            float ahead = vec3_dot(to_other, forward);
            if (ahead > 0 && ahead < 10.0f && dist < 5.0f) {
                ai->state = AI_STATE_OVERTAKING;
            }
        }
    }

    /* Calculate throttle */
    float throttle = 1.0f;

    /* Slow down for sharp turns */
    if (fabsf(steering) > 0.5f) {
        throttle = 0.7f - fabsf(steering) * 0.3f;
    }

    /* Apply speed factor from difficulty */
    float max_speed = v->max_speed * ai->speed_factor;
    if (v->speed > max_speed) {
        throttle = 0;
    }

    /* Brake if going too fast into a turn */
    float brake = 0;
    if (v->speed > 50.0f && fabsf(steering) > 0.7f) {
        brake = 0.5f;
        throttle = 0;
    }

    /* Recovery state - if off track, try to get back */
    if (!v->is_on_track) {
        ai->state = AI_STATE_RECOVERING;
        /* Steer more aggressively toward track */
        steering = clamp(angle_diff * 3.0f, -1.0f, 1.0f);
        throttle = 0.5f;
    }

    /* Apply controls */
    vehicle_set_steering(v, steering);
    vehicle_set_throttle(v, throttle);
    vehicle_set_brake(v, brake);

    /* Update state timer */
    ai->state_timer += dt;
    if (ai->state_timer > 2.0f) {
        ai->state = AI_STATE_RACING;
        ai->state_timer = 0;
    }
}

const char *ai_difficulty_name(ai_difficulty_t difficulty) {
    switch (difficulty) {
        case AI_EASY: return "Easy";
        case AI_MEDIUM: return "Medium";
        case AI_HARD: return "Hard";
        case AI_EXPERT: return "Expert";
        default: return "Unknown";
    }
}
