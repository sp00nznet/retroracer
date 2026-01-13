/*
 * RetroRacer - Track System Implementation
 * Procedural Track Generation
 */

#include "track.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

/* Simple PRNG for procedural generation */
static uint32_t track_rand_state = 12345;

static uint32_t track_rand(void) {
    track_rand_state = track_rand_state * 1103515245 + 12345;
    return (track_rand_state >> 16) & 0x7FFF;
}

static float track_rand_float(void) {
    return (float)track_rand() / 32767.0f;
}

static float track_rand_range(float min, float max) {
    return min + track_rand_float() * (max - min);
}

void track_init(void) {
    track_rand_state = (uint32_t)time(NULL);
}

track_params_t track_default_params(void) {
    track_params_t params;
    params.seed = track_random_seed();
    params.num_segments = 32;
    params.track_width = 12.0f;
    params.min_straight_length = 20.0f;
    params.max_straight_length = 60.0f;
    params.max_curve_angle = 45.0f;
    params.max_elevation = 5.0f;
    params.difficulty = 2;
    return params;
}

uint32_t track_random_seed(void) {
    return (uint32_t)time(NULL) ^ (track_rand() << 16);
}

track_t *track_generate(track_params_t *params) {
    track_t *track = (track_t *)malloc(sizeof(track_t));
    memset(track, 0, sizeof(track_t));

    track_rand_state = params->seed;
    track->seed = params->seed;

    sprintf(track->name, "Track %u", params->seed % 1000);

    vec3_t current_pos = vec3_create(0, 0, 0);
    float current_angle = 0;  /* Direction in radians */
    float total_length = 0;

    track->start_position = current_pos;
    track->start_direction = vec3_create(0, 0, 1);

    int num_segments = params->num_segments;
    int checkpoint_interval = num_segments / 8;
    if (checkpoint_interval < 1) checkpoint_interval = 1;

    for (int i = 0; i < num_segments && i < MAX_TRACK_SEGMENTS; i++) {
        track_segment_t *seg = &track->segments[i];

        /* Determine segment type */
        float r = track_rand_float();
        if (r < 0.4f) {
            seg->type = SEGMENT_STRAIGHT;
        } else if (r < 0.6f) {
            seg->type = SEGMENT_CURVE_LEFT;
        } else if (r < 0.8f) {
            seg->type = SEGMENT_CURVE_RIGHT;
        } else if (r < 0.9f) {
            seg->type = SEGMENT_HILL_UP;
        } else {
            seg->type = SEGMENT_HILL_DOWN;
        }

        /* Make first and last segments straight for start/finish */
        if (i == 0 || i == num_segments - 1) {
            seg->type = SEGMENT_STRAIGHT;
        }

        seg->start_pos = current_pos;
        seg->width = params->track_width;

        /* Calculate segment length */
        seg->length = track_rand_range(params->min_straight_length, params->max_straight_length);

        /* Calculate end position and update direction */
        vec3_t dir = vec3_create(sinf(current_angle), 0, cosf(current_angle));

        switch (seg->type) {
            case SEGMENT_STRAIGHT:
                seg->curve_angle = 0;
                seg->elevation_change = 0;
                break;

            case SEGMENT_CURVE_LEFT:
                seg->curve_angle = track_rand_range(15.0f, params->max_curve_angle);
                current_angle -= deg_to_rad(seg->curve_angle);
                seg->elevation_change = 0;
                break;

            case SEGMENT_CURVE_RIGHT:
                seg->curve_angle = track_rand_range(15.0f, params->max_curve_angle);
                current_angle += deg_to_rad(seg->curve_angle);
                seg->elevation_change = 0;
                break;

            case SEGMENT_HILL_UP:
                seg->curve_angle = 0;
                seg->elevation_change = track_rand_range(1.0f, params->max_elevation);
                break;

            case SEGMENT_HILL_DOWN:
                seg->curve_angle = 0;
                seg->elevation_change = -track_rand_range(1.0f, params->max_elevation);
                if (current_pos.y + seg->elevation_change < 0) {
                    seg->elevation_change = -current_pos.y;
                }
                break;
        }

        /* Calculate end position */
        dir = vec3_create(sinf(current_angle), 0, cosf(current_angle));
        seg->direction = dir;
        seg->end_pos = vec3_add(current_pos, vec3_scale(dir, seg->length));
        seg->end_pos.y += seg->elevation_change;

        /* Create segment mesh */
        uint32_t road_color = COLOR_ASPHALT;
        if (i % 2 == 0) {
            /* Slightly different shade for visual variety */
            road_color = PACK_COLOR(255, 70, 70, 70);
        }
        seg->mesh = mesh_create_track_segment(seg->width, seg->length, road_color);

        /* Create border meshes */
        seg->border_left = mesh_create_track_segment(1.0f, seg->length, COLOR_WHITE);
        seg->border_right = mesh_create_track_segment(1.0f, seg->length, COLOR_WHITE);

        /* Add checkpoint */
        if ((i + 1) % checkpoint_interval == 0 && track->checkpoint_count < MAX_CHECKPOINTS) {
            checkpoint_t *cp = &track->checkpoints[track->checkpoint_count];
            cp->position = vec3_lerp(seg->start_pos, seg->end_pos, 0.5f);
            cp->direction = seg->direction;
            cp->width = seg->width;
            cp->segment_index = i;
            cp->passed = 0;
            track->checkpoint_count++;
        }

        total_length += seg->length;
        current_pos = seg->end_pos;
        track->segment_count++;
    }

    /* Add final checkpoint at finish line */
    if (track->checkpoint_count < MAX_CHECKPOINTS) {
        checkpoint_t *cp = &track->checkpoints[track->checkpoint_count];
        cp->position = track->start_position;
        cp->direction = track->start_direction;
        cp->width = params->track_width;
        cp->segment_index = 0;
        cp->passed = 0;
        track->checkpoint_count++;
    }

    track->total_length = total_length;

    return track;
}

void track_destroy(track_t *track) {
    if (!track) return;

    for (int i = 0; i < track->segment_count; i++) {
        mesh_destroy(track->segments[i].mesh);
        mesh_destroy(track->segments[i].border_left);
        mesh_destroy(track->segments[i].border_right);
    }

    free(track);
}

void track_render(track_t *track, camera_t *cam) {
    if (!track) return;

    render_set_camera(cam);

    /* Render ground plane as a grid of tiles around camera */
    /* Uses multiple smaller tiles to avoid depth issues */
    float tile_size = 80.0f;
    int grid_range = 2;  /* Tiles in each direction */

    /* Grid centered on camera position */
    float base_x = floorf(cam->position.x / tile_size) * tile_size;
    float base_z = floorf(cam->position.z / tile_size) * tile_size;

    for (int gx = -grid_range; gx <= grid_range; gx++) {
        for (int gz = -grid_range; gz <= grid_range; gz++) {
            vec3_t tile_pos = vec3_create(
                base_x + gx * tile_size,
                -0.2f,  /* Below track surface */
                base_z + gz * tile_size
            );
            render_draw_quad(tile_pos, tile_size, tile_size, COLOR_GRASS);
        }
    }

    /* Render each segment */
    for (int i = 0; i < track->segment_count; i++) {
        track_segment_t *seg = &track->segments[i];

        /* Calculate segment transform */
        mat4_t transform = mat4_identity();

        /* Position */
        mat4_t trans = mat4_translate(seg->start_pos.x, seg->start_pos.y, seg->start_pos.z);

        /* Rotation to face direction */
        float angle = atan2f(seg->direction.x, seg->direction.z);
        mat4_t rot = mat4_rotate_y(angle);

        transform = mat4_multiply(trans, rot);

        /* Render track surface */
        render_draw_mesh(seg->mesh, transform);

        /* Render borders */
        mat4_t left_offset = mat4_translate(-seg->width / 2 - 0.5f, 0.05f, 0);
        mat4_t right_offset = mat4_translate(seg->width / 2 + 0.5f, 0.05f, 0);

        render_draw_mesh(seg->border_left, mat4_multiply(transform, left_offset));
        render_draw_mesh(seg->border_right, mat4_multiply(transform, right_offset));
    }

    /* Render start/finish line */
    vec3_t start_pos = track->start_position;
    start_pos.y += 0.1f;
    render_draw_quad(start_pos, track->segments[0].width, 2.0f, COLOR_WHITE);
}

void track_get_position(track_t *track, float distance, vec3_t *pos, vec3_t *dir) {
    if (!track || track->segment_count == 0) {
        *pos = vec3_create(0, 0, 0);
        *dir = vec3_create(0, 0, 1);
        return;
    }

    /* Wrap distance for looping track */
    while (distance < 0) distance += track->total_length;
    while (distance >= track->total_length) distance -= track->total_length;

    float accumulated = 0;
    for (int i = 0; i < track->segment_count; i++) {
        track_segment_t *seg = &track->segments[i];

        if (accumulated + seg->length > distance) {
            float t = (distance - accumulated) / seg->length;
            *pos = vec3_lerp(seg->start_pos, seg->end_pos, t);
            *dir = seg->direction;
            return;
        }

        accumulated += seg->length;
    }

    /* Default to start */
    *pos = track->start_position;
    *dir = track->start_direction;
}

int track_find_segment(track_t *track, vec3_t pos) {
    if (!track) return -1;

    float min_dist = 1e10f;
    int closest = -1;

    for (int i = 0; i < track->segment_count; i++) {
        track_segment_t *seg = &track->segments[i];
        vec3_t center = vec3_lerp(seg->start_pos, seg->end_pos, 0.5f);
        float dist = vec3_distance(pos, center);

        if (dist < min_dist) {
            min_dist = dist;
            closest = i;
        }
    }

    return closest;
}

int track_is_on_surface(track_t *track, vec3_t pos, float *height) {
    if (!track) return 0;

    int seg_idx = track_find_segment(track, pos);
    if (seg_idx < 0) return 0;

    track_segment_t *seg = &track->segments[seg_idx];

    /* Check if within track width */
    vec3_t to_pos = vec3_sub(pos, seg->start_pos);
    vec3_t right = vec3_cross(seg->direction, vec3_create(0, 1, 0));

    float lateral_dist = fabsf(vec3_dot(to_pos, right));
    if (lateral_dist > seg->width / 2 + 2.0f) {
        return 0;  /* Off track */
    }

    /* Calculate height at position */
    float along_track = vec3_dot(to_pos, seg->direction);
    float t = clamp(along_track / seg->length, 0, 1);

    *height = lerp(seg->start_pos.y, seg->end_pos.y, t);
    return 1;
}

int track_check_checkpoint(track_t *track, vec3_t pos, int last_checkpoint) {
    if (!track) return last_checkpoint;

    int next_cp = (last_checkpoint + 1) % track->checkpoint_count;
    checkpoint_t *cp = &track->checkpoints[next_cp];

    float dist = vec3_distance(pos, cp->position);
    if (dist < cp->width) {
        return next_cp;
    }

    return last_checkpoint;
}

float track_get_progress(track_t *track, vec3_t pos, int current_segment) {
    if (!track || track->segment_count == 0) return 0;

    float progress = 0;

    /* Sum length of completed segments */
    for (int i = 0; i < current_segment && i < track->segment_count; i++) {
        progress += track->segments[i].length;
    }

    /* Add partial progress in current segment */
    if (current_segment >= 0 && current_segment < track->segment_count) {
        track_segment_t *seg = &track->segments[current_segment];
        vec3_t to_pos = vec3_sub(pos, seg->start_pos);
        float along = vec3_dot(to_pos, seg->direction);
        progress += clamp(along, 0, seg->length);
    }

    return progress / track->total_length;
}
