/*
 * RetroRacer - Physics System
 * Simple arcade-style vehicle physics
 */

#ifndef PHYSICS_H
#define PHYSICS_H

#include "math3d.h"
#include "track.h"

/* Physics constants */
#define GRAVITY         9.81f
#define FRICTION        0.98f
#define AIR_RESISTANCE  0.002f
#define GROUND_FRICTION 0.95f
#define OFF_TRACK_DRAG  0.85f

/* Collision shape */
typedef struct {
    vec3_t center;
    vec3_t half_extents;    /* Box collision */
    float radius;           /* Sphere collision for quick checks */
} collision_box_t;

/* Physics body */
typedef struct {
    vec3_t position;
    vec3_t velocity;
    vec3_t acceleration;
    vec3_t angular_velocity;
    float mass;
    float inv_mass;
    float restitution;      /* Bounciness */
    collision_box_t bounds;
    int grounded;
} physics_body_t;

/* Initialize physics system */
void physics_init(void);

/* Create physics body */
physics_body_t physics_body_create(vec3_t pos, float mass, vec3_t half_extents);

/* Apply force to body */
void physics_apply_force(physics_body_t *body, vec3_t force);

/* Apply impulse (instant velocity change) */
void physics_apply_impulse(physics_body_t *body, vec3_t impulse);

/* Update physics body */
void physics_update_body(physics_body_t *body, float dt);

/* Ground collision check */
int physics_ground_check(physics_body_t *body, track_t *track, float *ground_height);

/* Box-box collision detection */
int physics_box_intersect(collision_box_t *a, collision_box_t *b);

/* Sphere-sphere collision (fast) */
int physics_sphere_intersect(vec3_t pos_a, float rad_a, vec3_t pos_b, float rad_b);

/* Resolve collision between two bodies */
void physics_resolve_collision(physics_body_t *a, physics_body_t *b);

/* Simple raycast for track surface */
int physics_raycast_ground(vec3_t origin, vec3_t direction, track_t *track,
                           vec3_t *hit_point, vec3_t *hit_normal);

/* Calculate drag force */
vec3_t physics_calculate_drag(vec3_t velocity, float coefficient);

/* Get friction coefficient based on surface */
float physics_get_surface_friction(track_t *track, vec3_t pos);

#endif /* PHYSICS_H */
