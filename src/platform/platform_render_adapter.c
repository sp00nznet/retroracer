/*
 * RetroRacer - Platform Render Adapter
 *
 * Bridges platform-specific rendering to the game's render system.
 * Provides unified rendering interface across all consoles.
 */

#include "platform.h"
#include "render.h"
#include "math3d.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

/*============================================================================
 * Render State
 *============================================================================*/

static camera_t *current_camera = NULL;
static mat4_t view_proj_matrix;

/* Near plane for clipping */
#define NEAR_PLANE 0.1f
#define FAR_PLANE 1000.0f

/*============================================================================
 * Platform-Specific Render Functions (External)
 *============================================================================*/

#if defined(PLATFORM_PSX)
extern void psx_draw_tri_flat(int x0, int y0, int x1, int y1, int x2, int y2,
                               unsigned char r, unsigned char g, unsigned char b, int z);
extern void psx_draw_tri_gouraud(int x0, int y0, int x1, int y1, int x2, int y2,
                                  unsigned char r0, unsigned char g0, unsigned char b0,
                                  unsigned char r1, unsigned char g1, unsigned char b1,
                                  unsigned char r2, unsigned char g2, unsigned char b2, int z);
extern void psx_draw_quad_flat(int x0, int y0, int x1, int y1, int x2, int y2, int x3, int y3,
                                unsigned char r, unsigned char g, unsigned char b, int z);
extern void psx_draw_rect(int x, int y, int w, int h, unsigned char r, unsigned char g, unsigned char b);

#elif defined(PLATFORM_PS2)
extern void ps2_draw_tri_flat(float x0, float y0, float z0,
                               float x1, float y1, float z1,
                               float x2, float y2, float z2,
                               unsigned char r, unsigned char g, unsigned char b, unsigned char a);
extern void ps2_draw_tri_gouraud(float x0, float y0, float z0, unsigned char r0, unsigned char g0, unsigned char b0,
                                  float x1, float y1, float z1, unsigned char r1, unsigned char g1, unsigned char b1,
                                  float x2, float y2, float z2, unsigned char r2, unsigned char g2, unsigned char b2,
                                  unsigned char a);

#elif defined(PLATFORM_PS3)
/* PS3 uses shaders, render through RSX commands */

#elif defined(PLATFORM_XBOX)
extern void xbox_draw_tri_flat(float x0, float y0, float z0,
                                float x1, float y1, float z1,
                                float x2, float y2, float z2,
                                uint32_t color);
extern void xbox_draw_tri_gouraud(float x0, float y0, float z0, uint32_t c0,
                                   float x1, float y1, float z1, uint32_t c1,
                                   float x2, float y2, float z2, uint32_t c2);
extern void xbox_draw_rect_2d(int x, int y, int w, int h, uint32_t color);

#elif defined(PLATFORM_XBOX360)
extern void x360_draw_tri_flat(float x0, float y0, float z0,
                                float x1, float y1, float z1,
                                float x2, float y2, float z2,
                                uint32_t color);
extern void x360_draw_tri_gouraud(float x0, float y0, float z0, uint32_t c0,
                                   float x1, float y1, float z1, uint32_t c1,
                                   float x2, float y2, float z2, uint32_t c2);
extern void x360_draw_rect_2d(int x, int y, int w, int h, uint32_t color);
extern void x360_set_transform(float *world_view_proj);

#endif

/*============================================================================
 * Matrix and Transform Helpers
 *============================================================================*/

/* Transform vertex by matrix */
static void transform_vertex(vec3_t *out, const vec3_t *in, const mat4_t *m) {
    float w;
    out->x = in->x * m->m[0] + in->y * m->m[4] + in->z * m->m[8] + m->m[12];
    out->y = in->x * m->m[1] + in->y * m->m[5] + in->z * m->m[9] + m->m[13];
    out->z = in->x * m->m[2] + in->y * m->m[6] + in->z * m->m[10] + m->m[14];
    w = in->x * m->m[3] + in->y * m->m[7] + in->z * m->m[11] + m->m[15];

    if (w != 0.0f && w != 1.0f) {
        out->x /= w;
        out->y /= w;
        out->z /= w;
    }
}

/* Project vertex to screen coordinates */
static void project_vertex(float *sx, float *sy, float *sz, const vec3_t *v) {
    const platform_caps_t *caps = platform_get_caps();
    float hw = caps->screen_width / 2.0f;
    float hh = caps->screen_height / 2.0f;

    *sx = hw + v->x * hw;
    *sy = hh - v->y * hh;  /* Y is flipped in screen space */
    *sz = (v->z + 1.0f) / 2.0f;  /* Map -1..1 to 0..1 */

    if (*sz < 0.0f) *sz = 0.0f;
    if (*sz > 1.0f) *sz = 1.0f;
}

/*============================================================================
 * Render System Implementation
 *============================================================================*/

void render_init(void) {
    platform_gfx_init();
}

void render_begin_frame(void) {
    platform_gfx_begin_frame();
    platform_gfx_set_render_list(RENDER_LIST_OPAQUE);
}

void render_end_frame(void) {
    platform_gfx_end_frame();
}

void render_clear(uint32_t color) {
    platform_gfx_clear(color);
}

void render_set_camera(camera_t *cam) {
    current_camera = cam;

    if (cam) {
        /* Combine view and projection matrices */
        mat4_multiply(&view_proj_matrix, &cam->proj_matrix, &cam->view_matrix);

#if defined(PLATFORM_XBOX360)
        /* Xbox 360 uses shader constants for transform */
        x360_set_transform(view_proj_matrix.m);
#endif
    }
}

void camera_update(camera_t *cam) {
    if (!cam) return;

    /* Calculate view matrix (look-at) */
    mat4_look_at(&cam->view_matrix, &cam->position, &cam->target, &cam->up);

    /* Calculate projection matrix (perspective) */
    mat4_perspective(&cam->proj_matrix, cam->fov, cam->aspect, cam->near_plane, cam->far_plane);
}

/*============================================================================
 * Triangle Rendering with Transform
 *============================================================================*/

void render_draw_triangle(vertex_t *v0, vertex_t *v1, vertex_t *v2) {
    vec3_t transformed[3];
    float sx[3], sy[3], sz[3];

    /* Transform vertices if we have a camera */
    if (current_camera) {
        transform_vertex(&transformed[0], &v0->pos, &view_proj_matrix);
        transform_vertex(&transformed[1], &v1->pos, &view_proj_matrix);
        transform_vertex(&transformed[2], &v2->pos, &view_proj_matrix);
    } else {
        transformed[0] = v0->pos;
        transformed[1] = v1->pos;
        transformed[2] = v2->pos;
    }

    /* Near plane clipping (simple reject) */
    int behind = 0;
    if (transformed[0].z < -1.0f) behind++;
    if (transformed[1].z < -1.0f) behind++;
    if (transformed[2].z < -1.0f) behind++;
    if (behind == 3) return;  /* Fully behind camera */

    /* Project to screen */
    project_vertex(&sx[0], &sy[0], &sz[0], &transformed[0]);
    project_vertex(&sx[1], &sy[1], &sz[1], &transformed[1]);
    project_vertex(&sx[2], &sy[2], &sz[2], &transformed[2]);

    /* Extract colors */
    uint8_t r0 = (v0->color >> 16) & 0xFF;
    uint8_t g0 = (v0->color >> 8) & 0xFF;
    uint8_t b0 = v0->color & 0xFF;
    uint8_t a0 = (v0->color >> 24) & 0xFF;

    uint8_t r1 = (v1->color >> 16) & 0xFF;
    uint8_t g1 = (v1->color >> 8) & 0xFF;
    uint8_t b1 = v1->color & 0xFF;

    uint8_t r2 = (v2->color >> 16) & 0xFF;
    uint8_t g2 = (v2->color >> 8) & 0xFF;
    uint8_t b2 = v2->color & 0xFF;

    /* Check if flat or gouraud shaded */
    int is_flat = (v0->color == v1->color && v1->color == v2->color);

    /* Platform-specific rendering */
#if defined(PLATFORM_PSX)
    int z = (int)((sz[0] + sz[1] + sz[2]) / 3.0f * 4095.0f);  /* OT depth */
    if (is_flat) {
        psx_draw_tri_flat((int)sx[0], (int)sy[0], (int)sx[1], (int)sy[1],
                          (int)sx[2], (int)sy[2], r0, g0, b0, z);
    } else {
        psx_draw_tri_gouraud((int)sx[0], (int)sy[0], (int)sx[1], (int)sy[1],
                             (int)sx[2], (int)sy[2],
                             r0, g0, b0, r1, g1, b1, r2, g2, b2, z);
    }

#elif defined(PLATFORM_PS2)
    if (is_flat) {
        ps2_draw_tri_flat(sx[0], sy[0], sz[0], sx[1], sy[1], sz[1],
                          sx[2], sy[2], sz[2], r0, g0, b0, a0);
    } else {
        ps2_draw_tri_gouraud(sx[0], sy[0], sz[0], r0, g0, b0,
                             sx[1], sy[1], sz[1], r1, g1, b1,
                             sx[2], sy[2], sz[2], r2, g2, b2, a0);
    }

#elif defined(PLATFORM_PS3)
    /* PS3 would use RSX vertex submission */
    (void)is_flat;
    (void)r0; (void)g0; (void)b0; (void)a0;
    (void)r1; (void)g1; (void)b1;
    (void)r2; (void)g2; (void)b2;
    (void)sx; (void)sy; (void)sz;

#elif defined(PLATFORM_XBOX)
    if (is_flat) {
        xbox_draw_tri_flat(sx[0], sy[0], sz[0], sx[1], sy[1], sz[1],
                           sx[2], sy[2], sz[2], v0->color);
    } else {
        xbox_draw_tri_gouraud(sx[0], sy[0], sz[0], v0->color,
                              sx[1], sy[1], sz[1], v1->color,
                              sx[2], sy[2], sz[2], v2->color);
    }

#elif defined(PLATFORM_XBOX360)
    if (is_flat) {
        x360_draw_tri_flat(sx[0], sy[0], sz[0], sx[1], sy[1], sz[1],
                           sx[2], sy[2], sz[2], v0->color);
    } else {
        x360_draw_tri_gouraud(sx[0], sy[0], sz[0], v0->color,
                              sx[1], sy[1], sz[1], v1->color,
                              sx[2], sy[2], sz[2], v2->color);
    }

#else
    /* Native stub - just count triangles */
    (void)is_flat;
    (void)r0; (void)g0; (void)b0; (void)a0;
    (void)r1; (void)g1; (void)b1;
    (void)r2; (void)g2; (void)b2;
    (void)sx; (void)sy; (void)sz;
#endif
}

/*============================================================================
 * Mesh Rendering
 *============================================================================*/

void render_draw_mesh(mesh_t *mesh, mat4_t transform) {
    if (!mesh || !mesh->triangles) return;

    /* Combine with view-projection */
    mat4_t mvp;
    if (current_camera) {
        mat4_multiply(&mvp, &view_proj_matrix, &transform);
    } else {
        mvp = transform;
    }

    for (int i = 0; i < mesh->tri_count; i++) {
        triangle_t *tri = &mesh->triangles[i];

        /* Transform vertices */
        vertex_t v0 = tri->v[0];
        vertex_t v1 = tri->v[1];
        vertex_t v2 = tri->v[2];

        transform_vertex(&v0.pos, &tri->v[0].pos, &mvp);
        transform_vertex(&v1.pos, &tri->v[1].pos, &mvp);
        transform_vertex(&v2.pos, &tri->v[2].pos, &mvp);

        /* Apply mesh base color if vertices don't have color */
        if (v0.color == 0) v0.color = mesh->base_color;
        if (v1.color == 0) v1.color = mesh->base_color;
        if (v2.color == 0) v2.color = mesh->base_color;

        render_draw_triangle(&v0, &v1, &v2);
    }
}

/*============================================================================
 * 2D Rendering (HUD)
 *============================================================================*/

static int in_hud_mode = 0;

void render_begin_hud(void) {
    platform_gfx_set_render_list(RENDER_LIST_TRANSPARENT);
    in_hud_mode = 1;
}

void render_end_hud(void) {
    in_hud_mode = 0;
}

void render_wait_vram_ready(void) {
    /* Compatibility - no-op on most platforms */
}

void render_draw_rect_2d(int x, int y, int w, int h, uint32_t color) {
#if defined(PLATFORM_PSX)
    psx_draw_rect(x, y, w, h,
                  (color >> 16) & 0xFF,
                  (color >> 8) & 0xFF,
                  color & 0xFF);
#elif defined(PLATFORM_XBOX)
    xbox_draw_rect_2d(x, y, w, h, color);
#elif defined(PLATFORM_XBOX360)
    x360_draw_rect_2d(x, y, w, h, color);
#else
    /* Draw as two triangles */
    vertex_t v0 = { .pos = {x, y, 0}, .color = color };
    vertex_t v1 = { .pos = {x + w, y, 0}, .color = color };
    vertex_t v2 = { .pos = {x, y + h, 0}, .color = color };
    vertex_t v3 = { .pos = {x + w, y + h, 0}, .color = color };

    camera_t *saved_cam = current_camera;
    current_camera = NULL;  /* Disable 3D transform for 2D */

    render_draw_triangle(&v0, &v1, &v2);
    render_draw_triangle(&v1, &v3, &v2);

    current_camera = saved_cam;
#endif
}

void render_draw_quad(vec3_t pos, float width, float height, uint32_t color) {
    float hw = width / 2.0f;
    float hh = height / 2.0f;

    vertex_t v0 = { .pos = {pos.x - hw, pos.y, pos.z - hh}, .color = color };
    vertex_t v1 = { .pos = {pos.x + hw, pos.y, pos.z - hh}, .color = color };
    vertex_t v2 = { .pos = {pos.x - hw, pos.y, pos.z + hh}, .color = color };
    vertex_t v3 = { .pos = {pos.x + hw, pos.y, pos.z + hh}, .color = color };

    render_draw_triangle(&v0, &v1, &v2);
    render_draw_triangle(&v1, &v3, &v2);
}

void render_draw_sky_background(uint32_t color) {
    /* Full-screen quad at max depth */
    const platform_caps_t *caps = platform_get_caps();
    render_draw_rect_2d(0, 0, caps->screen_width, caps->screen_height, color);
}

/*============================================================================
 * Text Rendering (Basic block font)
 *============================================================================*/

/* Simple 5x7 block font for basic characters */
static const uint8_t font_data[96][7] = {
    /* Space (32) through tilde (126) */
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00}, /* Space */
    {0x04,0x04,0x04,0x04,0x00,0x04,0x00}, /* ! */
    {0x0A,0x0A,0x00,0x00,0x00,0x00,0x00}, /* " */
    {0x0A,0x1F,0x0A,0x0A,0x1F,0x0A,0x00}, /* # */
    {0x04,0x0F,0x14,0x0E,0x05,0x1E,0x04}, /* $ */
    {0x19,0x19,0x02,0x04,0x08,0x13,0x13}, /* % */
    {0x08,0x14,0x14,0x08,0x15,0x12,0x0D}, /* & */
    {0x04,0x04,0x00,0x00,0x00,0x00,0x00}, /* ' */
    {0x02,0x04,0x08,0x08,0x08,0x04,0x02}, /* ( */
    {0x08,0x04,0x02,0x02,0x02,0x04,0x08}, /* ) */
    {0x00,0x04,0x15,0x0E,0x15,0x04,0x00}, /* * */
    {0x00,0x04,0x04,0x1F,0x04,0x04,0x00}, /* + */
    {0x00,0x00,0x00,0x00,0x04,0x04,0x08}, /* , */
    {0x00,0x00,0x00,0x1F,0x00,0x00,0x00}, /* - */
    {0x00,0x00,0x00,0x00,0x00,0x04,0x00}, /* . */
    {0x01,0x01,0x02,0x04,0x08,0x10,0x10}, /* / */
    {0x0E,0x11,0x13,0x15,0x19,0x11,0x0E}, /* 0 */
    {0x04,0x0C,0x04,0x04,0x04,0x04,0x0E}, /* 1 */
    {0x0E,0x11,0x01,0x06,0x08,0x10,0x1F}, /* 2 */
    {0x0E,0x11,0x01,0x06,0x01,0x11,0x0E}, /* 3 */
    {0x02,0x06,0x0A,0x12,0x1F,0x02,0x02}, /* 4 */
    {0x1F,0x10,0x1E,0x01,0x01,0x11,0x0E}, /* 5 */
    {0x06,0x08,0x10,0x1E,0x11,0x11,0x0E}, /* 6 */
    {0x1F,0x01,0x02,0x04,0x08,0x08,0x08}, /* 7 */
    {0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E}, /* 8 */
    {0x0E,0x11,0x11,0x0F,0x01,0x02,0x0C}, /* 9 */
    {0x00,0x04,0x00,0x00,0x04,0x00,0x00}, /* : */
    {0x00,0x04,0x00,0x00,0x04,0x04,0x08}, /* ; */
    {0x02,0x04,0x08,0x10,0x08,0x04,0x02}, /* < */
    {0x00,0x00,0x1F,0x00,0x1F,0x00,0x00}, /* = */
    {0x08,0x04,0x02,0x01,0x02,0x04,0x08}, /* > */
    {0x0E,0x11,0x01,0x02,0x04,0x00,0x04}, /* ? */
    {0x0E,0x11,0x17,0x15,0x17,0x10,0x0E}, /* @ */
    {0x0E,0x11,0x11,0x1F,0x11,0x11,0x11}, /* A */
    {0x1E,0x11,0x11,0x1E,0x11,0x11,0x1E}, /* B */
    {0x0E,0x11,0x10,0x10,0x10,0x11,0x0E}, /* C */
    {0x1E,0x11,0x11,0x11,0x11,0x11,0x1E}, /* D */
    {0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F}, /* E */
    {0x1F,0x10,0x10,0x1E,0x10,0x10,0x10}, /* F */
    {0x0E,0x11,0x10,0x17,0x11,0x11,0x0E}, /* G */
    {0x11,0x11,0x11,0x1F,0x11,0x11,0x11}, /* H */
    {0x0E,0x04,0x04,0x04,0x04,0x04,0x0E}, /* I */
    {0x07,0x02,0x02,0x02,0x02,0x12,0x0C}, /* J */
    {0x11,0x12,0x14,0x18,0x14,0x12,0x11}, /* K */
    {0x10,0x10,0x10,0x10,0x10,0x10,0x1F}, /* L */
    {0x11,0x1B,0x15,0x15,0x11,0x11,0x11}, /* M */
    {0x11,0x19,0x15,0x13,0x11,0x11,0x11}, /* N */
    {0x0E,0x11,0x11,0x11,0x11,0x11,0x0E}, /* O */
    {0x1E,0x11,0x11,0x1E,0x10,0x10,0x10}, /* P */
    {0x0E,0x11,0x11,0x11,0x15,0x12,0x0D}, /* Q */
    {0x1E,0x11,0x11,0x1E,0x14,0x12,0x11}, /* R */
    {0x0E,0x11,0x10,0x0E,0x01,0x11,0x0E}, /* S */
    {0x1F,0x04,0x04,0x04,0x04,0x04,0x04}, /* T */
    {0x11,0x11,0x11,0x11,0x11,0x11,0x0E}, /* U */
    {0x11,0x11,0x11,0x11,0x11,0x0A,0x04}, /* V */
    {0x11,0x11,0x11,0x15,0x15,0x15,0x0A}, /* W */
    {0x11,0x11,0x0A,0x04,0x0A,0x11,0x11}, /* X */
    {0x11,0x11,0x0A,0x04,0x04,0x04,0x04}, /* Y */
    {0x1F,0x01,0x02,0x04,0x08,0x10,0x1F}, /* Z */
    /* ... more characters ... */
};

void render_draw_text(int x, int y, uint32_t color, const char *text) {
    const int char_width = 6;
    const int char_height = 8;
    int cursor_x = x;

    while (*text) {
        char c = *text++;

        if (c == '\n') {
            cursor_x = x;
            y += char_height;
            continue;
        }

        if (c < 32 || c > 126) c = '?';

        int idx = c - 32;
        if (idx >= 0 && idx < 96) {
            for (int row = 0; row < 7; row++) {
                uint8_t bits = font_data[idx][row];
                for (int col = 0; col < 5; col++) {
                    if (bits & (1 << (4 - col))) {
                        render_draw_rect_2d(cursor_x + col, y + row, 1, 1, color);
                    }
                }
            }
        }

        cursor_x += char_width;
    }
}

/*============================================================================
 * Mesh Creation
 *============================================================================*/

mesh_t *mesh_create_cube(float size, uint32_t color) {
    mesh_t *mesh = platform_malloc(sizeof(mesh_t));
    if (!mesh) return NULL;

    /* 12 triangles for a cube */
    mesh->tri_count = 12;
    mesh->triangles = platform_malloc(sizeof(triangle_t) * mesh->tri_count);
    mesh->base_color = color;

    if (!mesh->triangles) {
        platform_free(mesh);
        return NULL;
    }

    float hs = size / 2.0f;

    /* Define cube vertices */
    vec3_t v[8] = {
        {-hs, -hs, -hs}, { hs, -hs, -hs}, { hs,  hs, -hs}, {-hs,  hs, -hs},
        {-hs, -hs,  hs}, { hs, -hs,  hs}, { hs,  hs,  hs}, {-hs,  hs,  hs}
    };

    /* Face indices (2 triangles per face) */
    int faces[12][3] = {
        {0,1,2}, {0,2,3},  /* Front */
        {4,6,5}, {4,7,6},  /* Back */
        {0,4,5}, {0,5,1},  /* Bottom */
        {2,6,7}, {2,7,3},  /* Top */
        {0,3,7}, {0,7,4},  /* Left */
        {1,5,6}, {1,6,2}   /* Right */
    };

    for (int i = 0; i < 12; i++) {
        mesh->triangles[i].v[0].pos = v[faces[i][0]];
        mesh->triangles[i].v[1].pos = v[faces[i][1]];
        mesh->triangles[i].v[2].pos = v[faces[i][2]];
        mesh->triangles[i].v[0].color = color;
        mesh->triangles[i].v[1].color = color;
        mesh->triangles[i].v[2].color = color;
    }

    return mesh;
}

mesh_t *mesh_create_vehicle(uint32_t color) {
    /* Simple low-poly vehicle mesh */
    mesh_t *mesh = platform_malloc(sizeof(mesh_t));
    if (!mesh) return NULL;

    /* Body + wheels (simplified) */
    mesh->tri_count = 20;
    mesh->triangles = platform_malloc(sizeof(triangle_t) * mesh->tri_count);
    mesh->base_color = color;

    if (!mesh->triangles) {
        platform_free(mesh);
        return NULL;
    }

    /* Simple wedge-shaped body */
    vec3_t body[8] = {
        {-0.5f, 0.0f, -1.0f}, { 0.5f, 0.0f, -1.0f},
        { 0.5f, 0.3f, -0.5f}, {-0.5f, 0.3f, -0.5f},
        {-0.5f, 0.3f,  0.5f}, { 0.5f, 0.3f,  0.5f},
        { 0.5f, 0.0f,  1.0f}, {-0.5f, 0.0f,  1.0f}
    };

    int body_faces[12][3] = {
        {0,1,2}, {0,2,3},  /* Front */
        {4,5,6}, {4,6,7},  /* Back */
        {0,3,4}, {0,4,7},  /* Left */
        {1,5,2}, {1,6,5},  /* Right */
        {3,2,5}, {3,5,4},  /* Top */
        {0,7,6}, {0,6,1}   /* Bottom */
    };

    for (int i = 0; i < 12; i++) {
        mesh->triangles[i].v[0].pos = body[body_faces[i][0]];
        mesh->triangles[i].v[1].pos = body[body_faces[i][1]];
        mesh->triangles[i].v[2].pos = body[body_faces[i][2]];
        mesh->triangles[i].v[0].color = color;
        mesh->triangles[i].v[1].color = color;
        mesh->triangles[i].v[2].color = color;
    }

    /* Add simple wheel representations (quads as triangles) */
    uint32_t wheel_color = PACK_COLOR(255, 32, 32, 32);  /* Dark gray */
    float wheel_positions[4][2] = {
        {-0.5f, -0.7f}, {0.5f, -0.7f},  /* Front wheels */
        {-0.5f,  0.7f}, {0.5f,  0.7f}   /* Rear wheels */
    };

    for (int w = 0; w < 4 && (12 + w*2) < mesh->tri_count; w++) {
        float wx = wheel_positions[w][0];
        float wz = wheel_positions[w][1];
        float wr = 0.15f;

        int ti = 12 + w * 2;
        mesh->triangles[ti].v[0].pos = (vec3_t){wx - wr, 0.0f, wz - wr};
        mesh->triangles[ti].v[1].pos = (vec3_t){wx + wr, 0.0f, wz - wr};
        mesh->triangles[ti].v[2].pos = (vec3_t){wx + wr, 0.0f, wz + wr};
        mesh->triangles[ti].v[0].color = wheel_color;
        mesh->triangles[ti].v[1].color = wheel_color;
        mesh->triangles[ti].v[2].color = wheel_color;

        ti++;
        mesh->triangles[ti].v[0].pos = (vec3_t){wx - wr, 0.0f, wz - wr};
        mesh->triangles[ti].v[1].pos = (vec3_t){wx + wr, 0.0f, wz + wr};
        mesh->triangles[ti].v[2].pos = (vec3_t){wx - wr, 0.0f, wz + wr};
        mesh->triangles[ti].v[0].color = wheel_color;
        mesh->triangles[ti].v[1].color = wheel_color;
        mesh->triangles[ti].v[2].color = wheel_color;
    }

    return mesh;
}

mesh_t *mesh_create_track_segment(float width, float length, uint32_t color) {
    mesh_t *mesh = platform_malloc(sizeof(mesh_t));
    if (!mesh) return NULL;

    mesh->tri_count = 2;
    mesh->triangles = platform_malloc(sizeof(triangle_t) * mesh->tri_count);
    mesh->base_color = color;

    if (!mesh->triangles) {
        platform_free(mesh);
        return NULL;
    }

    float hw = width / 2.0f;

    mesh->triangles[0].v[0].pos = (vec3_t){-hw, 0, 0};
    mesh->triangles[0].v[1].pos = (vec3_t){ hw, 0, 0};
    mesh->triangles[0].v[2].pos = (vec3_t){ hw, 0, length};
    mesh->triangles[0].v[0].color = color;
    mesh->triangles[0].v[1].color = color;
    mesh->triangles[0].v[2].color = color;

    mesh->triangles[1].v[0].pos = (vec3_t){-hw, 0, 0};
    mesh->triangles[1].v[1].pos = (vec3_t){ hw, 0, length};
    mesh->triangles[1].v[2].pos = (vec3_t){-hw, 0, length};
    mesh->triangles[1].v[0].color = color;
    mesh->triangles[1].v[1].color = color;
    mesh->triangles[1].v[2].color = color;

    return mesh;
}

void mesh_destroy(mesh_t *mesh) {
    if (mesh) {
        if (mesh->triangles) platform_free(mesh->triangles);
        platform_free(mesh);
    }
}
