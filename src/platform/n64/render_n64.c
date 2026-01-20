/*
 * RetroRacer - Nintendo 64 Rendering Implementation
 * Uses libdragon with RDP (Reality Display Processor)
 */

#include "render.h"
#include "platform.h"

#ifdef PLATFORM_N64

#include <libdragon.h>
#include <t3d/t3d.h>
#include <t3d/t3dmath.h>
#include <t3d/t3dmodel.h>
#include <rdpq.h>
#include <rdpq_mode.h>
#include <rdpq_debug.h>

/* Display settings */
#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240

static surface_t *framebuffer = NULL;
static surface_t zbuffer;
static camera_t *current_camera = NULL;

/* T3D matrices for hardware transform */
static T3DMat4 t3d_view_matrix;
static T3DMat4 t3d_proj_matrix;
static T3DMat4 t3d_mvp_matrix;

/* Display context */
static display_context_t disp = 0;

void render_init(void) {
    /* Initialize display - 320x240 16-bit */
    display_init(RESOLUTION_320x240, DEPTH_16_BPP, 2, GAMMA_NONE, FILTERS_RESAMPLE);

    /* Initialize RDP */
    rdpq_init();

    /* Initialize T3D for 3D math and rendering */
    t3d_init((T3DInitParams){});

    /* Create Z-buffer */
    zbuffer = surface_alloc(FMT_RGBA16, SCREEN_WIDTH, SCREEN_HEIGHT);

    /* Set default viewport */
    t3d_viewport_set(T3DViewport{{0, 0, SCREEN_WIDTH, SCREEN_HEIGHT}});
}

void render_begin_frame(void) {
    /* Get display buffer */
    disp = display_get();
    framebuffer = display_get_buffer(disp);

    /* Attach RDP to framebuffer and zbuffer */
    rdpq_attach(framebuffer, &zbuffer);

    /* Clear screen with sky blue */
    rdpq_clear(RGBA32(0x87, 0xCE, 0xEB, 0xFF));
    rdpq_clear_z(0xFFFC);

    /* Set up 3D rendering mode */
    rdpq_mode_begin();
    rdpq_mode_combiner(RDPQ_COMBINER_SHADE);
    rdpq_mode_zbuf(true, true);
    rdpq_mode_antialias(AA_STANDARD);
    rdpq_mode_end();
}

void render_end_frame(void) {
    /* Finish RDP operations */
    rdpq_detach_wait();
}

void render_begin_hud(void) {
    /* Switch to 2D mode for HUD */
    rdpq_mode_begin();
    rdpq_mode_zbuf(false, false);
    rdpq_mode_combiner(RDPQ_COMBINER_FLAT);
    rdpq_mode_end();
}

void render_end_hud(void) {
    /* Show frame */
    display_show(disp);
}

void render_clear(uint32_t color) {
    uint8_t r = (color >> 16) & 0xFF;
    uint8_t g = (color >> 8) & 0xFF;
    uint8_t b = color & 0xFF;
    rdpq_clear(RGBA32(r, g, b, 0xFF));
}

void render_set_camera(camera_t *cam) {
    current_camera = cam;

    if (!cam) return;

    /* Build T3D view matrix */
    T3DVec3 eye = {{cam->position.x, cam->position.y, cam->position.z}};
    T3DVec3 target = {{cam->target.x, cam->target.y, cam->target.z}};
    T3DVec3 up = {{cam->up.x, cam->up.y, cam->up.z}};

    t3d_mat4_look_at(&t3d_view_matrix, &eye, &target, &up);

    /* Build projection matrix */
    float aspect = (float)SCREEN_WIDTH / (float)SCREEN_HEIGHT;
    t3d_mat4_perspective(&t3d_proj_matrix,
        T3D_DEG_TO_RAD(cam->fov),
        aspect,
        cam->near_plane,
        cam->far_plane);

    /* Combine into MVP */
    t3d_mat4_mul(&t3d_mvp_matrix, &t3d_proj_matrix, &t3d_view_matrix);
}

void camera_update(camera_t *cam) {
    cam->view_matrix = mat4_look_at(cam->position, cam->target, cam->up);
    cam->proj_matrix = mat4_perspective(
        deg_to_rad(cam->fov),
        cam->aspect,
        cam->near_plane,
        cam->far_plane
    );
}

/* Transform vertex through MVP matrix */
static int transform_vertex(vec3_t pos, float *sx, float *sy, float *sz) {
    T3DVec3 in = {{pos.x, pos.y, pos.z}};
    T3DVec4 out;

    /* Transform by MVP */
    t3d_mat4_mul_vec3(&out, &t3d_mvp_matrix, &in);

    /* Perspective divide */
    if (out.v[3] <= 0.001f) return 0;

    float inv_w = 1.0f / out.v[3];
    float nx = out.v[0] * inv_w;
    float ny = out.v[1] * inv_w;
    float nz = out.v[2] * inv_w;

    /* Clip check */
    if (nx < -1.0f || nx > 1.0f || ny < -1.0f || ny > 1.0f || nz < 0.0f || nz > 1.0f) {
        return 0;
    }

    /* Screen transform */
    *sx = (nx * 0.5f + 0.5f) * SCREEN_WIDTH;
    *sy = (0.5f - ny * 0.5f) * SCREEN_HEIGHT;
    *sz = nz;

    return 1;
}

void render_draw_triangle(vertex_t *v0, vertex_t *v1, vertex_t *v2) {
    if (!current_camera) return;

    float sx0, sy0, sz0;
    float sx1, sy1, sz1;
    float sx2, sy2, sz2;

    if (!transform_vertex(v0->pos, &sx0, &sy0, &sz0)) return;
    if (!transform_vertex(v1->pos, &sx1, &sy1, &sz1)) return;
    if (!transform_vertex(v2->pos, &sx2, &sy2, &sz2)) return;

    /* Extract colors */
    color_t c0 = color_from_packed32(v0->color | 0xFF000000);
    color_t c1 = color_from_packed32(v1->color | 0xFF000000);
    color_t c2 = color_from_packed32(v2->color | 0xFF000000);

    /* Draw with RDP */
    rdpq_triangle(&TRIFMT_SHADE_ZBUF,
        (float[]){sx0, sy0, sz0, c0.r/255.0f, c0.g/255.0f, c0.b/255.0f, 1.0f},
        (float[]){sx1, sy1, sz1, c1.r/255.0f, c1.g/255.0f, c1.b/255.0f, 1.0f},
        (float[]){sx2, sy2, sz2, c2.r/255.0f, c2.g/255.0f, c2.b/255.0f, 1.0f}
    );
}

void render_draw_mesh(mesh_t *mesh, mat4_t transform) {
    if (!mesh || !current_camera) return;

    for (int i = 0; i < mesh->tri_count; i++) {
        triangle_t *tri = &mesh->triangles[i];

        vertex_t v0 = tri->v[0];
        vertex_t v1 = tri->v[1];
        vertex_t v2 = tri->v[2];

        v0.pos = mat4_transform_vec3(transform, v0.pos);
        v1.pos = mat4_transform_vec3(transform, v1.pos);
        v2.pos = mat4_transform_vec3(transform, v2.pos);

        render_draw_triangle(&v0, &v1, &v2);
    }
}

void render_draw_quad(vec3_t pos, float width, float height, uint32_t color) {
    vertex_t v0, v1, v2, v3;
    float hw = width * 0.5f;
    float hh = height * 0.5f;

    v0.pos = vec3_create(pos.x - hw, pos.y, pos.z - hh);
    v1.pos = vec3_create(pos.x + hw, pos.y, pos.z - hh);
    v2.pos = vec3_create(pos.x + hw, pos.y, pos.z + hh);
    v3.pos = vec3_create(pos.x - hw, pos.y, pos.z + hh);

    v0.color = v1.color = v2.color = v3.color = color;

    render_draw_triangle(&v0, &v1, &v2);
    render_draw_triangle(&v0, &v2, &v3);
}

void render_wait_vram_ready(void) {
    rspq_wait();
}

void render_draw_sky_background(uint32_t color) {
    render_clear(color);
}

void render_draw_rect_2d(int x, int y, int w, int h, uint32_t color) {
    color_t c = color_from_packed32(color | 0xFF000000);
    rdpq_set_prim_color(c);
    rdpq_fill_rectangle(x, y, x + w, y + h);
}

void render_draw_text(int x, int y, uint32_t color, const char *text) {
    /* Use rdpq_text or simple rectangles */
    int cx = x;
    int char_width = 8;
    int char_height = 12;

    color_t c = color_from_packed32(color | 0xFF000000);
    rdpq_set_prim_color(c);

    while (*text) {
        char ch = *text++;

        if (ch == ' ') {
            cx += char_width;
            continue;
        }

        rdpq_fill_rectangle(cx, y, cx + char_width - 1, y + char_height);
        cx += char_width;
    }
}

mesh_t *mesh_create_cube(float size, uint32_t color) {
    mesh_t *mesh = (mesh_t *)malloc(sizeof(mesh_t));
    mesh->tri_count = 12;
    mesh->triangles = (triangle_t *)malloc(sizeof(triangle_t) * mesh->tri_count);
    mesh->base_color = color;

    float hs = size * 0.5f;

    vec3_t corners[8] = {
        {-hs, -hs, -hs}, {hs, -hs, -hs}, {hs, hs, -hs}, {-hs, hs, -hs},
        {-hs, -hs, hs}, {hs, -hs, hs}, {hs, hs, hs}, {-hs, hs, hs}
    };

    int faces[6][4] = {
        {0, 1, 2, 3}, {5, 4, 7, 6}, {4, 0, 3, 7},
        {1, 5, 6, 2}, {3, 2, 6, 7}, {4, 5, 1, 0}
    };

    int ti = 0;
    for (int f = 0; f < 6; f++) {
        mesh->triangles[ti].v[0].pos = corners[faces[f][0]];
        mesh->triangles[ti].v[1].pos = corners[faces[f][1]];
        mesh->triangles[ti].v[2].pos = corners[faces[f][2]];
        mesh->triangles[ti].v[0].color = color;
        mesh->triangles[ti].v[1].color = color;
        mesh->triangles[ti].v[2].color = color;
        ti++;

        mesh->triangles[ti].v[0].pos = corners[faces[f][0]];
        mesh->triangles[ti].v[1].pos = corners[faces[f][2]];
        mesh->triangles[ti].v[2].pos = corners[faces[f][3]];
        mesh->triangles[ti].v[0].color = color;
        mesh->triangles[ti].v[1].color = color;
        mesh->triangles[ti].v[2].color = color;
        ti++;
    }

    return mesh;
}

mesh_t *mesh_create_vehicle(uint32_t color) {
    mesh_t *mesh = (mesh_t *)malloc(sizeof(mesh_t));
    mesh->tri_count = 12;
    mesh->triangles = (triangle_t *)malloc(sizeof(triangle_t) * mesh->tri_count);
    mesh->base_color = color;

    float bw = 0.8f, bh = 0.4f, bl = 1.5f;

    vec3_t body[8] = {
        {-bw/2, 0, -bl/2}, {bw/2, 0, -bl/2},
        {bw/2, bh, -bl/2}, {-bw/2, bh, -bl/2},
        {-bw/2, 0, bl/2}, {bw/2, 0, bl/2},
        {bw/2, bh, bl/2}, {-bw/2, bh, bl/2}
    };

    int faces[6][4] = {
        {0, 1, 2, 3}, {5, 4, 7, 6}, {4, 0, 3, 7},
        {1, 5, 6, 2}, {3, 2, 6, 7}, {4, 5, 1, 0}
    };

    int ti = 0;
    for (int f = 0; f < 6 && ti < 12; f++) {
        mesh->triangles[ti].v[0].pos = body[faces[f][0]];
        mesh->triangles[ti].v[1].pos = body[faces[f][1]];
        mesh->triangles[ti].v[2].pos = body[faces[f][2]];
        mesh->triangles[ti].v[0].color = color;
        mesh->triangles[ti].v[1].color = color;
        mesh->triangles[ti].v[2].color = color;
        ti++;

        mesh->triangles[ti].v[0].pos = body[faces[f][0]];
        mesh->triangles[ti].v[1].pos = body[faces[f][2]];
        mesh->triangles[ti].v[2].pos = body[faces[f][3]];
        mesh->triangles[ti].v[0].color = color;
        mesh->triangles[ti].v[1].color = color;
        mesh->triangles[ti].v[2].color = color;
        ti++;
    }

    mesh->tri_count = ti;
    return mesh;
}

mesh_t *mesh_create_track_segment(float width, float length, uint32_t color) {
    mesh_t *mesh = (mesh_t *)malloc(sizeof(mesh_t));
    mesh->tri_count = 2;
    mesh->triangles = (triangle_t *)malloc(sizeof(triangle_t) * mesh->tri_count);
    mesh->base_color = color;

    float hw = width * 0.5f;

    mesh->triangles[0].v[0].pos = vec3_create(-hw, 0, 0);
    mesh->triangles[0].v[1].pos = vec3_create(hw, 0, 0);
    mesh->triangles[0].v[2].pos = vec3_create(hw, 0, length);
    mesh->triangles[0].v[0].color = color;
    mesh->triangles[0].v[1].color = color;
    mesh->triangles[0].v[2].color = color;

    mesh->triangles[1].v[0].pos = vec3_create(-hw, 0, 0);
    mesh->triangles[1].v[1].pos = vec3_create(hw, 0, length);
    mesh->triangles[1].v[2].pos = vec3_create(-hw, 0, length);
    mesh->triangles[1].v[0].color = color;
    mesh->triangles[1].v[1].color = color;
    mesh->triangles[1].v[2].color = color;

    return mesh;
}

void mesh_destroy(mesh_t *mesh) {
    if (mesh) {
        if (mesh->triangles) free(mesh->triangles);
        free(mesh);
    }
}

#endif /* PLATFORM_N64 */
