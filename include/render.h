/*
 * RetroRacer - Rendering System
 * PowerVR Graphics for Dreamcast
 */

#ifndef RENDER_H
#define RENDER_H

#include <stdint.h>
#include "math3d.h"

/* Vertex with position, normal, UV, and color */
typedef struct {
    vec3_t pos;
    vec3_t normal;
    float u, v;
    uint32_t color;
} vertex_t;

/* Triangle primitive */
typedef struct {
    vertex_t v[3];
} triangle_t;

/* Simple mesh structure */
typedef struct {
    triangle_t *triangles;
    int tri_count;
    uint32_t base_color;
} mesh_t;

/* Camera structure */
typedef struct {
    vec3_t position;
    vec3_t target;
    vec3_t up;
    float fov;
    float aspect;
    float near_plane;
    float far_plane;
    mat4_t view_matrix;
    mat4_t proj_matrix;
} camera_t;

/* Initialize rendering system */
void render_init(void);

/* Begin/end frame */
void render_begin_frame(void);
void render_end_frame(void);

/* Clear screen */
void render_clear(uint32_t color);

/* Set camera for rendering */
void render_set_camera(camera_t *cam);

/* Update camera matrices */
void camera_update(camera_t *cam);

/* Draw mesh with transformation */
void render_draw_mesh(mesh_t *mesh, mat4_t transform);

/* Draw a single triangle */
void render_draw_triangle(vertex_t *v0, vertex_t *v1, vertex_t *v2);

/* Draw colored quad (for UI/ground) */
void render_draw_quad(vec3_t pos, float width, float height, uint32_t color);

/* Draw text on screen */
void render_draw_text(int x, int y, uint32_t color, const char *text);

/* Create basic meshes */
mesh_t *mesh_create_cube(float size, uint32_t color);
mesh_t *mesh_create_vehicle(uint32_t color);
mesh_t *mesh_create_track_segment(float width, float length, uint32_t color);

/* Free mesh memory */
void mesh_destroy(mesh_t *mesh);

/* Color helpers */
#define PACK_COLOR(a, r, g, b) (((a) << 24) | ((r) << 16) | ((g) << 8) | (b))
#define COLOR_WHITE   PACK_COLOR(255, 255, 255, 255)
#define COLOR_BLACK   PACK_COLOR(255, 0, 0, 0)
#define COLOR_RED     PACK_COLOR(255, 255, 0, 0)
#define COLOR_GREEN   PACK_COLOR(255, 0, 255, 0)
#define COLOR_BLUE    PACK_COLOR(255, 0, 0, 255)
#define COLOR_YELLOW  PACK_COLOR(255, 255, 255, 0)
#define COLOR_GRAY    PACK_COLOR(255, 128, 128, 128)
#define COLOR_ASPHALT PACK_COLOR(255, 64, 64, 64)
#define COLOR_GRASS   PACK_COLOR(255, 34, 139, 34)

#endif /* RENDER_H */
