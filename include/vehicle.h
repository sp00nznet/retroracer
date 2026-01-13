/*
 * RetroRacer - Vehicle System
 * Player and AI Vehicle Management
 */

#ifndef VEHICLE_H
#define VEHICLE_H

#include <stdint.h>
#include "math3d.h"
#include "render.h"
#include "track.h"

/* Maximum vehicles in a race */
#define MAX_VEHICLES 8

/* Vehicle type/class */
typedef enum {
    VEHICLE_STANDARD,
    VEHICLE_SPEED,
    VEHICLE_HANDLING,
    VEHICLE_BALANCED
} vehicle_class_t;

/* Vehicle state */
typedef struct {
    /* Transform */
    vec3_t position;
    vec3_t velocity;
    vec3_t acceleration;
    float rotation_y;       /* Yaw - main steering */
    float rotation_x;       /* Pitch - hills */
    float rotation_z;       /* Roll - banking */

    /* Movement */
    float speed;
    float steering;
    float throttle;
    float brake;

    /* Vehicle properties */
    float max_speed;
    float acceleration_rate;
    float brake_rate;
    float steering_rate;
    float drag;
    float grip;

    /* Race state */
    int current_lap;
    int total_laps;
    int current_checkpoint;
    float lap_time;
    float best_lap_time;
    float total_time;
    float track_progress;
    int finished;
    int place;

    /* Rendering */
    mesh_t *mesh;
    uint32_t color;
    vehicle_class_t vehicle_class;

    /* State flags */
    int is_player;
    int is_on_track;
    int is_airborne;
} vehicle_t;

/* Initialize vehicle system */
void vehicle_init(void);

/* Create a new vehicle */
vehicle_t *vehicle_create(vehicle_class_t vclass, uint32_t color, int is_player);

/* Destroy vehicle */
void vehicle_destroy(vehicle_t *vehicle);

/* Update vehicle physics and state */
void vehicle_update(vehicle_t *vehicle, track_t *track, float dt);

/* Apply controls to vehicle */
void vehicle_set_throttle(vehicle_t *vehicle, float throttle);
void vehicle_set_brake(vehicle_t *vehicle, float brake);
void vehicle_set_steering(vehicle_t *vehicle, float steering);

/* Reset vehicle to position */
void vehicle_reset(vehicle_t *vehicle, vec3_t pos, float rotation);

/* Render vehicle */
void vehicle_render(vehicle_t *vehicle, camera_t *cam);

/* Get forward direction vector */
vec3_t vehicle_get_forward(vehicle_t *vehicle);

/* Check collision between vehicles */
int vehicle_check_collision(vehicle_t *a, vehicle_t *b);

/* Handle collision response */
void vehicle_resolve_collision(vehicle_t *a, vehicle_t *b);

/* Get vehicle stats string */
const char *vehicle_class_name(vehicle_class_t vclass);

#endif /* VEHICLE_H */
