/*
 * RetroRacer - Vehicle System Implementation
 * Player and AI Vehicle Management
 */

#include "vehicle.h"
#include "physics.h"
#include <stdlib.h>
#include <string.h>

/* Vehicle class stats */
static const struct {
    float max_speed;
    float acceleration;
    float brake_rate;
    float steering;
    float grip;
} vehicle_stats[] = {
    /* VEHICLE_STANDARD */  {80.0f, 25.0f, 40.0f, 2.5f, 0.9f},
    /* VEHICLE_SPEED */     {100.0f, 30.0f, 35.0f, 2.0f, 0.8f},
    /* VEHICLE_HANDLING */  {70.0f, 22.0f, 45.0f, 3.2f, 0.95f},
    /* VEHICLE_BALANCED */  {85.0f, 27.0f, 42.0f, 2.7f, 0.88f}
};

void vehicle_init(void) {
    /* Nothing to initialize */
}

vehicle_t *vehicle_create(vehicle_class_t vclass, uint32_t color, int is_player) {
    vehicle_t *v = (vehicle_t *)malloc(sizeof(vehicle_t));
    memset(v, 0, sizeof(vehicle_t));

    v->vehicle_class = vclass;
    v->color = color;
    v->is_player = is_player;

    /* Set stats from class */
    v->max_speed = vehicle_stats[vclass].max_speed;
    v->acceleration_rate = vehicle_stats[vclass].acceleration;
    v->brake_rate = vehicle_stats[vclass].brake_rate;
    v->steering_rate = vehicle_stats[vclass].steering;
    v->grip = vehicle_stats[vclass].grip;
    v->drag = 0.01f;

    /* Create mesh */
    v->mesh = mesh_create_vehicle(color);

    /* Initialize race state */
    v->current_lap = 0;
    v->total_laps = 3;
    v->current_checkpoint = 0;
    v->best_lap_time = 999999.0f;

    return v;
}

void vehicle_destroy(vehicle_t *vehicle) {
    if (vehicle) {
        mesh_destroy(vehicle->mesh);
        free(vehicle);
    }
}

vec3_t vehicle_get_forward(vehicle_t *vehicle) {
    float angle = vehicle->rotation_y;
    return vec3_create(sinf(angle), 0, cosf(angle));
}

void vehicle_set_throttle(vehicle_t *vehicle, float throttle) {
    vehicle->throttle = clamp(throttle, 0.0f, 1.0f);
}

void vehicle_set_brake(vehicle_t *vehicle, float brake) {
    vehicle->brake = clamp(brake, 0.0f, 1.0f);
}

void vehicle_set_steering(vehicle_t *vehicle, float steering) {
    vehicle->steering = clamp(steering, -1.0f, 1.0f);
}

void vehicle_reset(vehicle_t *vehicle, vec3_t pos, float rotation) {
    vehicle->position = pos;
    vehicle->velocity = vec3_create(0, 0, 0);
    vehicle->acceleration = vec3_create(0, 0, 0);
    vehicle->rotation_y = rotation;
    vehicle->rotation_x = 0;
    vehicle->rotation_z = 0;
    vehicle->speed = 0;
    vehicle->throttle = 0;
    vehicle->brake = 0;
    vehicle->steering = 0;
    vehicle->is_on_track = 1;
    vehicle->is_airborne = 0;
    vehicle->current_checkpoint = 0;
}

void vehicle_update(vehicle_t *vehicle, track_t *track, float dt) {
    vec3_t forward = vehicle_get_forward(vehicle);
    vec3_t right = vec3_cross(forward, vec3_create(0, 1, 0));

    /* Check if on track */
    float ground_height = 0;
    vehicle->is_on_track = track_is_on_surface(track, vehicle->position, &ground_height);

    /* Apply gravity */
    if (vehicle->position.y > ground_height + 0.1f) {
        vehicle->velocity.y -= GRAVITY * dt;
        vehicle->is_airborne = 1;
    } else {
        vehicle->position.y = ground_height;
        vehicle->velocity.y = 0;
        vehicle->is_airborne = 0;
    }

    /* Calculate speed */
    vehicle->speed = vec3_length(vec3_create(vehicle->velocity.x, 0, vehicle->velocity.z));

    /* Apply steering (only when moving) */
    if (vehicle->speed > 1.0f && !vehicle->is_airborne) {
        float steer_amount = vehicle->steering * vehicle->steering_rate * dt;

        /* Reduce steering at high speed */
        float speed_factor = 1.0f - (vehicle->speed / vehicle->max_speed) * 0.5f;
        steer_amount *= speed_factor;

        vehicle->rotation_y += steer_amount;

        /* Update velocity direction based on grip */
        vec3_t desired_dir = vehicle_get_forward(vehicle);
        vec3_t current_dir = vec3_normalize(vec3_create(vehicle->velocity.x, 0, vehicle->velocity.z));

        vec3_t blended = vec3_lerp(current_dir, desired_dir, vehicle->grip);
        blended = vec3_normalize(blended);

        vehicle->velocity.x = blended.x * vehicle->speed;
        vehicle->velocity.z = blended.z * vehicle->speed;
    }

    /* Apply throttle */
    if (vehicle->throttle > 0 && !vehicle->is_airborne) {
        float accel = vehicle->acceleration_rate * vehicle->throttle;

        /* Reduce acceleration when off track */
        if (!vehicle->is_on_track) {
            accel *= 0.5f;
        }

        /* Limit to max speed */
        if (vehicle->speed < vehicle->max_speed) {
            forward = vehicle_get_forward(vehicle);
            vehicle->velocity = vec3_add(vehicle->velocity, vec3_scale(forward, accel * dt));
        }
    }

    /* Apply brakes */
    if (vehicle->brake > 0 && !vehicle->is_airborne) {
        float brake_force = vehicle->brake_rate * vehicle->brake * dt;
        vehicle->speed -= brake_force;
        if (vehicle->speed < 0) vehicle->speed = 0;

        /* Update velocity magnitude */
        if (vehicle->speed > 0.1f) {
            vec3_t dir = vec3_normalize(vec3_create(vehicle->velocity.x, 0, vehicle->velocity.z));
            vehicle->velocity.x = dir.x * vehicle->speed;
            vehicle->velocity.z = dir.z * vehicle->speed;
        } else {
            vehicle->velocity.x = 0;
            vehicle->velocity.z = 0;
        }
    }

    /* Apply drag */
    float drag_factor = vehicle->is_on_track ? 0.99f : 0.95f;
    vehicle->velocity = vec3_scale(vehicle->velocity, drag_factor);

    /* Update position */
    vehicle->position = vec3_add(vehicle->position, vec3_scale(vehicle->velocity, dt));

    /* Update checkpoint */
    int new_cp = track_check_checkpoint(track, vehicle->position, vehicle->current_checkpoint);
    if (new_cp != vehicle->current_checkpoint) {
        vehicle->current_checkpoint = new_cp;

        /* Check for lap completion */
        if (new_cp == 0 && vehicle->current_checkpoint == 0) {
            /* Completed a lap */
            vehicle->current_lap++;

            /* Update best lap time */
            if (vehicle->lap_time < vehicle->best_lap_time && vehicle->lap_time > 1.0f) {
                vehicle->best_lap_time = vehicle->lap_time;
            }

            vehicle->lap_time = 0;
        }
    }

    /* Update timing */
    vehicle->lap_time += dt;
    vehicle->total_time += dt;

    /* Calculate track progress */
    int seg = track_find_segment(track, vehicle->position);
    vehicle->track_progress = track_get_progress(track, vehicle->position, seg);

    /* Check if finished */
    if (vehicle->current_lap >= vehicle->total_laps) {
        vehicle->finished = 1;
    }

    /* Tilt based on steering */
    vehicle->rotation_z = -vehicle->steering * 0.1f;
}

void vehicle_render(vehicle_t *vehicle, camera_t *cam) {
    if (!vehicle || !vehicle->mesh) return;

    render_set_camera(cam);

    /* Build transform matrix */
    mat4_t transform = mat4_identity();

    /* Position */
    mat4_t trans = mat4_translate(
        vehicle->position.x,
        vehicle->position.y + 0.3f,  /* Offset to sit on ground */
        vehicle->position.z
    );

    /* Rotation */
    mat4_t rot_y = mat4_rotate_y(vehicle->rotation_y);
    mat4_t rot_x = mat4_rotate_x(vehicle->rotation_x);
    mat4_t rot_z = mat4_rotate_z(vehicle->rotation_z);

    mat4_t rotation = mat4_multiply(rot_y, mat4_multiply(rot_x, rot_z));
    transform = mat4_multiply(trans, rotation);

    render_draw_mesh(vehicle->mesh, transform);
}

int vehicle_check_collision(vehicle_t *a, vehicle_t *b) {
    float dist = vec3_distance(a->position, b->position);
    float collision_radius = 2.0f;  /* Vehicle radius */
    return dist < collision_radius * 2;
}

void vehicle_resolve_collision(vehicle_t *a, vehicle_t *b) {
    vec3_t delta = vec3_sub(a->position, b->position);
    float dist = vec3_length(delta);

    if (dist < 0.001f) {
        delta = vec3_create(1, 0, 0);
        dist = 1.0f;
    }

    vec3_t normal = vec3_scale(delta, 1.0f / dist);

    /* Separate vehicles */
    float overlap = 4.0f - dist;  /* 2 * collision radius */
    if (overlap > 0) {
        a->position = vec3_add(a->position, vec3_scale(normal, overlap * 0.5f));
        b->position = vec3_sub(b->position, vec3_scale(normal, overlap * 0.5f));
    }

    /* Exchange momentum */
    vec3_t rel_vel = vec3_sub(a->velocity, b->velocity);
    float vel_along_normal = vec3_dot(rel_vel, normal);

    if (vel_along_normal < 0) {
        vec3_t impulse = vec3_scale(normal, vel_along_normal * 0.5f);
        a->velocity = vec3_sub(a->velocity, impulse);
        b->velocity = vec3_add(b->velocity, impulse);
    }
}

const char *vehicle_class_name(vehicle_class_t vclass) {
    switch (vclass) {
        case VEHICLE_STANDARD: return "Standard";
        case VEHICLE_SPEED: return "Speed";
        case VEHICLE_HANDLING: return "Handling";
        case VEHICLE_BALANCED: return "Balanced";
        default: return "Unknown";
    }
}
