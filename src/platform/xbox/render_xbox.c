/*
 * RetroRacer - Original Xbox Rendering Implementation
 * Uses OpenXDK or nxdk with NV2A GPU (GeForce 3 based)
 */

#include "render.h"
#include "platform.h"

#ifdef PLATFORM_XBOX

#include <hal/video.h>
#include <hal/xbox.h>
#include <pbkit/pbkit.h>
#include <xboxkrnl/xboxkrnl.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* Framebuffer */
static unsigned int *framebuffer = NULL;
static int fb_width = 640;
static int fb_height = 480;
static int current_buffer = 0;

static camera_t *current_camera = NULL;

/* NV2A push buffer for GPU commands */
static uint32_t *pb_begin = NULL;
static uint32_t *pb_end = NULL;

void render_init(void) {
    /* Initialize video */
    XVideoSetMode(fb_width, fb_height, 32, REFRESH_DEFAULT);

    /* Initialize push buffer graphics */
    int status = pb_init();
    if (status != 0) {
        /* Fallback to framebuffer */
        framebuffer = XVideoGetFB();
    }

    /* Setup default state */
    pb_begin = pb_end = NULL;
}

void render_begin_frame(void) {
    /* Wait for previous frame */
    pb_wait_for_vbl();

    /* Begin push buffer */
    pb_reset();
    pb_target_back_buffer();

    /* Clear screen */
    pb_erase_depth_stencil_buffer(0, 0, fb_width, fb_height);
    pb_fill(0, 0, fb_width, fb_height, 0xFF87CEEB);  /* Sky blue */

    /* Setup 3D rendering state */
    pb_set_viewport(0, 0, fb_width, fb_height, 0.0f, 1.0f);
}

void render_end_frame(void) {
    /* Finish push buffer */
    pb_finished();
}

void render_begin_hud(void) {
    /* Disable depth test for HUD */
    /* pb_set_depth_test(0); */
}

void render_end_hud(void) {
    /* Swap buffers */
    while (pb_busy());
    pb_show_front_screen();

    current_buffer ^= 1;
}

void render_clear(uint32_t color) {
    pb_fill(0, 0, fb_width, fb_height, color | 0xFF000000);
}

void render_set_camera(camera_t *cam) {
    current_camera = cam;

    if (!cam) return;

    /* Setup NV2A transformation matrices */
    /* The NV2A uses OpenGL-style matrices */

    /* View matrix */
    float view[16];
    mat4_t vm = cam->view_matrix;
    for (int i = 0; i < 16; i++) {
        view[i] = vm.m[i];
    }

    /* Projection matrix */
    float proj[16];
    mat4_t pm = cam->proj_matrix;
    for (int i = 0; i < 16; i++) {
        proj[i] = pm.m[i];
    }

    /* Load matrices to NV2A */
    /* pb_set_modelview_matrix(view); */
    /* pb_set_projection_matrix(proj); */
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

/* Software transform for framebuffer fallback */
static int transform_vertex(vec3_t pos, float *sx, float *sy, float *sz) {
    if (!current_camera) return 0;

    mat4_t vp = mat4_multiply(current_camera->proj_matrix, current_camera->view_matrix);

    float x = vp.m[0]*pos.x + vp.m[4]*pos.y + vp.m[8]*pos.z + vp.m[12];
    float y = vp.m[1]*pos.x + vp.m[5]*pos.y + vp.m[9]*pos.z + vp.m[13];
    float z = vp.m[2]*pos.x + vp.m[6]*pos.y + vp.m[10]*pos.z + vp.m[14];
    float w = vp.m[3]*pos.x + vp.m[7]*pos.y + vp.m[11]*pos.z + vp.m[15];

    if (w <= 0.0001f) return 0;

    float inv_w = 1.0f / w;
    *sx = (x * inv_w * 0.5f + 0.5f) * fb_width;
    *sy = (1.0f - (y * inv_w * 0.5f + 0.5f)) * fb_height;
    *sz = (z * inv_w * 0.5f + 0.5f);

    if (*sz < 0.0f || *sz > 1.0f) return 0;

    return 1;
}

void render_draw_triangle(vertex_t *v0, vertex_t *v1, vertex_t *v2) {
    if (!current_camera) return;

    /*
     * NV2A GPU rendering would use push buffer commands:
     *
     * pb_push(pb_begin, NV097_SET_BEGIN_END, 1);
     * pb_push(pb_begin, NV097_TRIANGLES, 0);
     *
     * pb_push(pb_begin, NV097_SET_DIFFUSE_COLOR, 3);
     * pb_push(pb_begin, v0->color);
     * pb_push(pb_begin, v0->pos.x);
     * pb_push(pb_begin, v0->pos.y);
     * pb_push(pb_begin, v0->pos.z);
     * ... repeat for v1, v2 ...
     *
     * pb_push(pb_begin, NV097_SET_BEGIN_END, 1);
     * pb_push(pb_begin, NV097_END, 0);
     */

    /* For now, software rasterization fallback */
    float sx0, sy0, sz0;
    float sx1, sy1, sz1;
    float sx2, sy2, sz2;

    if (!transform_vertex(v0->pos, &sx0, &sy0, &sz0)) return;
    if (!transform_vertex(v1->pos, &sx1, &sy1, &sz1)) return;
    if (!transform_vertex(v2->pos, &sx2, &sy2, &sz2)) return;

    /* Simple edge-based rasterizer would go here */
    (void)sx0; (void)sy0; (void)sz0;
    (void)sx1; (void)sy1; (void)sz1;
    (void)sx2; (void)sy2; (void)sz2;
    (void)v0; (void)v1; (void)v2;
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
    while (pb_busy());
}

void render_draw_sky_background(uint32_t color) {
    pb_fill(0, 0, fb_width, fb_height, color | 0xFF000000);
}

void render_draw_rect_2d(int x, int y, int w, int h, uint32_t color) {
    pb_fill(x, y, w, h, color | 0xFF000000);
}

void render_draw_text(int x, int y, uint32_t color, const char *text) {
    int cx = x;
    int char_width = 12;
    int char_height = 24;

    while (*text) {
        char c = *text++;

        if (c == ' ') {
            cx += char_width;
            continue;
        }

        render_draw_rect_2d(cx, y, char_width - 2, char_height, color);
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

#endif /* PLATFORM_XBOX */
