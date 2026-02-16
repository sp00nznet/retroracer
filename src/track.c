/*
 * RetroRacer - Track System Implementation
 * Procedural Track Generation
 */

#include "track.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <math.h>

/* Tile road length after 12x scale: tile Z range is [-6,6] = 12 units */
#define TILE_ROAD_LENGTH 12.0f
#define PI_F 3.14159265f

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

        /* Place road tiles along segment */
        {
            int n_tiles = (int)(seg->length / TILE_ROAD_LENGTH + 0.99f);
            if (n_tiles < 1) n_tiles = 1;
            if (n_tiles > MAX_TILES_PER_SEGMENT) n_tiles = MAX_TILES_PER_SEGMENT;
            float tile_len = seg->length / (float)n_tiles;
            float scale_z = tile_len / TILE_ROAD_LENGTH;
            float seg_angle = atan2f(seg->direction.x, seg->direction.z);

            /* Tile data is pre-rotated: road runs along Z, curbs along X.
             * Transform: translate(tile_center) * rotate_y(seg_angle) * scale(1,1,sz)
             * This matches the old mesh_create_track_segment convention exactly. */
            mat4_t scale_m = mat4_scale(1.0f, 1.0f, scale_z);
            mat4_t seg_rot = mat4_rotate_y(seg_angle);
            mat4_t rot_scale = mat4_multiply(seg_rot, scale_m);

            seg->tile_count = n_tiles;
            for (int t = 0; t < n_tiles; t++) {
                float frac = ((float)t + 0.5f) / (float)n_tiles;
                vec3_t tile_pos = vec3_lerp(seg->start_pos, seg->end_pos, frac);

                mat4_t trans = mat4_translate(tile_pos.x, tile_pos.y, tile_pos.z);
                mat4_t m = mat4_multiply(trans, rot_scale);

                /* Alternate tile types for visual variety */
                seg->tiles[t].type = (t % 3 == 2) ? TILE_DAMAGED : TILE_STRAIGHT;
                seg->tiles[t].transform = m;
            }
        }

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
    /* Tile meshes are shared (owned by tile_data.c), nothing per-segment to free */
    free(track);
}

/* Render grass ground plane around camera position */
static void render_grass(camera_t *cam) {
    /* Grass quad centered on camera, below track level */
    /* Keep size modest to avoid depth precision issues at far edges */
    float grass_size = 200.0f;
    float grass_y = -0.5f;  /* Below track surface at Y=0 */

    vec3_t center = cam->position;

    vertex_t v0, v1, v2, v3;
    v0.pos = vec3_create(center.x - grass_size, grass_y, center.z - grass_size);
    v1.pos = vec3_create(center.x + grass_size, grass_y, center.z - grass_size);
    v2.pos = vec3_create(center.x + grass_size, grass_y, center.z + grass_size);
    v3.pos = vec3_create(center.x - grass_size, grass_y, center.z + grass_size);

    v0.color = v1.color = v2.color = v3.color = COLOR_GRASS;

    render_draw_triangle(&v0, &v1, &v2);
    render_draw_triangle(&v0, &v2, &v3);
}

void track_render(track_t *track, camera_t *cam) {
    if (!track) return;

    render_set_camera(cam);

    /* Render grass ground plane first (below track, above sky background) */
    render_grass(cam);

    /* Render tile-based road for each segment */
    for (int i = 0; i < track->segment_count; i++) {
        track_segment_t *seg = &track->segments[i];

        /* Distance cull: skip segments far from camera */
        vec3_t seg_center = vec3_lerp(seg->start_pos, seg->end_pos, 0.5f);
        float dist = vec3_distance(cam->position, seg_center);
        if (dist > 300.0f) continue;

        /* Render each tile placement */
        for (int t = 0; t < seg->tile_count; t++) {
            mesh_t *tile_mesh = tile_get_mesh(seg->tiles[t].type);
            if (tile_mesh) {
                render_draw_mesh(tile_mesh, seg->tiles[t].transform);
            }
        }
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
