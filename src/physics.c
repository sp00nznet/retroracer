/*
 * RetroRacer - Physics System Implementation
 * Simple arcade-style vehicle physics
 */

#include "physics.h"
#include <stdlib.h>

void physics_init(void) {
    /* Nothing to initialize */
}

physics_body_t physics_body_create(vec3_t pos, float mass, vec3_t half_extents) {
    physics_body_t body;

    body.position = pos;
    body.velocity = vec3_create(0, 0, 0);
    body.acceleration = vec3_create(0, 0, 0);
    body.angular_velocity = vec3_create(0, 0, 0);
    body.mass = mass;
    body.inv_mass = (mass > 0) ? 1.0f / mass : 0;
    body.restitution = 0.3f;
    body.bounds.center = pos;
    body.bounds.half_extents = half_extents;
    body.bounds.radius = vec3_length(half_extents);
    body.grounded = 0;

    return body;
}

void physics_apply_force(physics_body_t *body, vec3_t force) {
    if (body->inv_mass > 0) {
        body->acceleration = vec3_add(body->acceleration, vec3_scale(force, body->inv_mass));
    }
}

void physics_apply_impulse(physics_body_t *body, vec3_t impulse) {
    if (body->inv_mass > 0) {
        body->velocity = vec3_add(body->velocity, vec3_scale(impulse, body->inv_mass));
    }
}

void physics_update_body(physics_body_t *body, float dt) {
    /* Apply gravity if not grounded */
    if (!body->grounded) {
        body->acceleration.y -= GRAVITY;
    }

    /* Integrate velocity */
    body->velocity = vec3_add(body->velocity, vec3_scale(body->acceleration, dt));

    /* Apply drag */
    body->velocity = vec3_scale(body->velocity, FRICTION);

    /* Integrate position */
    body->position = vec3_add(body->position, vec3_scale(body->velocity, dt));

    /* Update bounds center */
    body->bounds.center = body->position;

    /* Reset acceleration for next frame */
    body->acceleration = vec3_create(0, 0, 0);
}

int physics_ground_check(physics_body_t *body, track_t *track, float *ground_height) {
    float height = 0;

    if (track_is_on_surface(track, body->position, &height)) {
        *ground_height = height;

        if (body->position.y <= height + 0.1f) {
            body->position.y = height;
            body->grounded = 1;

            /* Stop downward velocity */
            if (body->velocity.y < 0) {
                body->velocity.y = 0;
            }
            return 1;
        }
    }

    body->grounded = 0;
    *ground_height = 0;
    return 0;
}

int physics_box_intersect(collision_box_t *a, collision_box_t *b) {
    vec3_t d = vec3_sub(a->center, b->center);

    float overlap_x = a->half_extents.x + b->half_extents.x - fabsf(d.x);
    float overlap_y = a->half_extents.y + b->half_extents.y - fabsf(d.y);
    float overlap_z = a->half_extents.z + b->half_extents.z - fabsf(d.z);

    return (overlap_x > 0 && overlap_y > 0 && overlap_z > 0);
}

int physics_sphere_intersect(vec3_t pos_a, float rad_a, vec3_t pos_b, float rad_b) {
    float dist_sq = vec3_dot(vec3_sub(pos_a, pos_b), vec3_sub(pos_a, pos_b));
    float rad_sum = rad_a + rad_b;
    return dist_sq < rad_sum * rad_sum;
}

void physics_resolve_collision(physics_body_t *a, physics_body_t *b) {
    vec3_t delta = vec3_sub(a->position, b->position);
    float dist = vec3_length(delta);

    if (dist < 0.001f) {
        delta = vec3_create(1, 0, 0);
        dist = 1.0f;
    }

    vec3_t normal = vec3_scale(delta, 1.0f / dist);

    /* Calculate overlap */
    float overlap = (a->bounds.radius + b->bounds.radius) - dist;
    if (overlap < 0) return;

    /* Separate objects */
    float total_mass = a->mass + b->mass;
    float a_ratio = b->mass / total_mass;
    float b_ratio = a->mass / total_mass;

    a->position = vec3_add(a->position, vec3_scale(normal, overlap * a_ratio));
    b->position = vec3_sub(b->position, vec3_scale(normal, overlap * b_ratio));

    /* Calculate relative velocity */
    vec3_t rel_vel = vec3_sub(a->velocity, b->velocity);
    float vel_along_normal = vec3_dot(rel_vel, normal);

    /* Only resolve if objects are moving toward each other */
    if (vel_along_normal > 0) return;

    /* Calculate restitution */
    float e = (a->restitution + b->restitution) * 0.5f;

    /* Calculate impulse scalar */
    float j = -(1 + e) * vel_along_normal;
    j /= a->inv_mass + b->inv_mass;

    /* Apply impulse */
    vec3_t impulse = vec3_scale(normal, j);
    a->velocity = vec3_add(a->velocity, vec3_scale(impulse, a->inv_mass));
    b->velocity = vec3_sub(b->velocity, vec3_scale(impulse, b->inv_mass));
}

int physics_raycast_ground(vec3_t origin, vec3_t direction, track_t *track,
                           vec3_t *hit_point, vec3_t *hit_normal) {
    /* Simple downward raycast */
    float height;
    if (track_is_on_surface(track, origin, &height)) {
        if (direction.y < 0 && origin.y > height) {
            hit_point->x = origin.x;
            hit_point->y = height;
            hit_point->z = origin.z;
            hit_normal->x = 0;
            hit_normal->y = 1;
            hit_normal->z = 0;
            return 1;
        }
    }
    return 0;
}

vec3_t physics_calculate_drag(vec3_t velocity, float coefficient) {
    float speed_sq = vec3_dot(velocity, velocity);
    if (speed_sq < 0.001f) {
        return vec3_create(0, 0, 0);
    }

    float speed = sqrtf(speed_sq);
    vec3_t drag_dir = vec3_scale(velocity, -1.0f / speed);
    float drag_magnitude = coefficient * speed_sq;

    return vec3_scale(drag_dir, drag_magnitude);
}

float physics_get_surface_friction(track_t *track, vec3_t pos) {
    float height;
    if (track_is_on_surface(track, pos, &height)) {
        return GROUND_FRICTION;
    }
    return OFF_TRACK_DRAG;
}
