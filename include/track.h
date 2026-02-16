/*
 * RetroRacer - Track System
 * Procedural Track Generation
 */

#ifndef TRACK_H
#define TRACK_H

#include <stdint.h>
#include "math3d.h"
#include "render.h"
#include "tile_data.h"

/* Maximum track segments */
#define MAX_TRACK_SEGMENTS 256
#define MAX_CHECKPOINTS 32
#define MAX_TILES_PER_SEGMENT 6

/* Track segment type */
typedef enum {
    SEGMENT_STRAIGHT,
    SEGMENT_CURVE_LEFT,
    SEGMENT_CURVE_RIGHT,
    SEGMENT_HILL_UP,
    SEGMENT_HILL_DOWN
} segment_type_t;

/* Tile placement within a segment */
typedef struct {
    tile_id_t type;
    mat4_t transform;
} tile_placement_t;

/* Single track segment */
typedef struct {
    segment_type_t type;
    vec3_t start_pos;
    vec3_t end_pos;
    vec3_t direction;
    float width;
    float length;
    float curve_angle;      /* For curved segments */
    float elevation_change; /* For hills */
    tile_placement_t tiles[MAX_TILES_PER_SEGMENT];
    int tile_count;
} track_segment_t;

/* Checkpoint for lap timing */
typedef struct {
    vec3_t position;
    vec3_t direction;
    float width;
    int segment_index;
    int passed;
} checkpoint_t;

/* Complete track structure */
typedef struct {
    track_segment_t segments[MAX_TRACK_SEGMENTS];
    int segment_count;
    checkpoint_t checkpoints[MAX_CHECKPOINTS];
    int checkpoint_count;
    vec3_t start_position;
    vec3_t start_direction;
    float total_length;
    uint32_t seed;          /* For procedural regeneration */
    char name[32];
} track_t;

/* Track generation parameters */
typedef struct {
    uint32_t seed;
    int num_segments;
    float track_width;
    float min_straight_length;
    float max_straight_length;
    float max_curve_angle;
    float max_elevation;
    int difficulty;         /* 1-5, affects complexity */
} track_params_t;

/* Initialize track system */
void track_init(void);

/* Generate a new procedural track */
track_t *track_generate(track_params_t *params);

/* Get default parameters */
track_params_t track_default_params(void);

/* Free track memory */
void track_destroy(track_t *track);

/* Render entire track */
void track_render(track_t *track, camera_t *cam);

/* Get track position and direction at distance */
void track_get_position(track_t *track, float distance, vec3_t *pos, vec3_t *dir);

/* Find which segment a position is on */
int track_find_segment(track_t *track, vec3_t pos);

/* Check if position is on track */
int track_is_on_surface(track_t *track, vec3_t pos, float *height);

/* Check checkpoint collision */
int track_check_checkpoint(track_t *track, vec3_t pos, int last_checkpoint);

/* Get progress along track (0.0 to 1.0 per lap) */
float track_get_progress(track_t *track, vec3_t pos, int current_segment);

/* Generate random seed */
uint32_t track_random_seed(void);

#endif /* TRACK_H */
