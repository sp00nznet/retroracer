/*
 * RetroRacer - Nintendo Wii U Rendering Implementation
 * Uses wut (Wii U Toolchain) with GX2 graphics
 *
 * GX2 is the Wii U graphics API:
 * - AMD Radeon-based GPU (Latte)
 * - Unified shader architecture
 * - 1920x1080 (TV) / 854x480 (GamePad)
 */

#include "render.h"
#include "platform.h"

#ifdef PLATFORM_WIIU

#include <whb/proc.h>
#include <whb/gfx.h>
#include <gx2/draw.h>
#include <gx2/shaders.h>
#include <gx2/mem.h>
#include <gx2/context.h>
#include <gx2/state.h>
#include <gx2/clear.h>
#include <gx2/display.h>
#include <gx2/event.h>
#include <gx2/registers.h>
#include <gx2r/buffer.h>
#include <gx2r/draw.h>
#include <coreinit/memdefaultheap.h>
#include <coreinit/memexpheap.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

/* Screen dimensions */
#define TV_WIDTH 1920
#define TV_HEIGHT 1080
#define GAMEPAD_WIDTH 854
#define GAMEPAD_HEIGHT 480

/* Max vertices per frame */
#define MAX_VERTICES 16384

/* Vertex structure for GX2 */
typedef struct {
    float position[4];  /* x, y, z, w */
    float color[4];     /* r, g, b, a */
} gx2_vertex_t;

/* State */
static camera_t *current_camera = NULL;
static WHBGfxShaderGroup *shader_group = NULL;

/* Vertex buffer */
static gx2_vertex_t *vertex_buffer = NULL;
static GX2RBuffer vertex_gx2_buffer;
static int vertex_count = 0;

/* Matrices */
static float projection_matrix[16];
static float view_matrix[16];
static float model_matrix[16];

/* Clear color */
static float clear_color[4] = {0.53f, 0.81f, 0.92f, 1.0f};  /* Sky blue */

/* Matrix helper functions */
static void mat4_identity_local(float *m) {
    memset(m, 0, 16 * sizeof(float));
    m[0] = m[5] = m[10] = m[15] = 1.0f;
}

static void mat4_perspective_local(float *m, float fov, float aspect, float near, float far) {
    float f = 1.0f / tanf(fov * 0.5f);
    memset(m, 0, 16 * sizeof(float));
    m[0] = f / aspect;
    m[5] = f;
    m[10] = (far + near) / (near - far);
    m[11] = -1.0f;
    m[14] = (2.0f * far * near) / (near - far);
}

static void mat4_multiply_local(float *out, const float *a, const float *b) {
    float temp[16];
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            temp[i * 4 + j] = 0;
            for (int k = 0; k < 4; k++) {
                temp[i * 4 + j] += a[i * 4 + k] * b[k * 4 + j];
            }
        }
    }
    memcpy(out, temp, 16 * sizeof(float));
}

void render_init(void) {
    /* Initialize WHB graphics helper */
    WHBGfxInit();

    /* Allocate vertex buffer in MEM2 (GPU accessible) */
    vertex_buffer = (gx2_vertex_t *)MEMAllocFromDefaultHeapEx(
        sizeof(gx2_vertex_t) * MAX_VERTICES, GX2_VERTEX_BUFFER_ALIGNMENT);

    /* Initialize GX2R buffer */
    vertex_gx2_buffer.flags = GX2R_RESOURCE_BIND_VERTEX_BUFFER |
                               GX2R_RESOURCE_USAGE_CPU_READ |
                               GX2R_RESOURCE_USAGE_CPU_WRITE |
                               GX2R_RESOURCE_USAGE_GPU_READ;
    vertex_gx2_buffer.elemSize = sizeof(gx2_vertex_t);
    vertex_gx2_buffer.elemCount = MAX_VERTICES;
    GX2RCreateBuffer(&vertex_gx2_buffer);

    /* Initialize matrices */
    mat4_identity_local(view_matrix);
    mat4_identity_local(model_matrix);
    mat4_perspective_local(projection_matrix, 70.0f * 3.14159f / 180.0f,
                          (float)TV_WIDTH / (float)TV_HEIGHT, 0.1f, 300.0f);

    /* Enable depth test */
    GX2SetDepthOnlyControl(GX2_TRUE, GX2_TRUE, GX2_COMPARE_FUNC_LESS);

    /* Set cull mode */
    GX2SetCullOnlyControl(GX2_FRONT_FACE_CCW, GX2_TRUE, GX2_FALSE);
}

void render_shutdown(void) {
    if (vertex_buffer) {
        MEMFreeToDefaultHeap(vertex_buffer);
        vertex_buffer = NULL;
    }

    GX2RDestroyBufferEx(&vertex_gx2_buffer, 0);
    WHBGfxShutdown();
}

void render_begin_frame(void) {
    vertex_count = 0;

    /* Begin rendering to TV */
    WHBGfxBeginRender();

    /* Clear screen */
    WHBGfxBeginRenderTV();
    WHBGfxClearColor(clear_color[0], clear_color[1], clear_color[2], clear_color[3]);

    /* Update view matrix from camera */
    if (current_camera) {
        /* Build look-at matrix */
        vec3_t f = vec3_normalize(vec3_sub(current_camera->target, current_camera->position));
        vec3_t r = vec3_normalize(vec3_cross(f, current_camera->up));
        vec3_t u = vec3_cross(r, f);

        view_matrix[0] = r.x;  view_matrix[4] = r.y;  view_matrix[8] = r.z;
        view_matrix[1] = u.x;  view_matrix[5] = u.y;  view_matrix[9] = u.z;
        view_matrix[2] = -f.x; view_matrix[6] = -f.y; view_matrix[10] = -f.z;
        view_matrix[3] = 0;    view_matrix[7] = 0;    view_matrix[11] = 0;

        view_matrix[12] = -vec3_dot(r, current_camera->position);
        view_matrix[13] = -vec3_dot(u, current_camera->position);
        view_matrix[14] = vec3_dot(f, current_camera->position);
        view_matrix[15] = 1.0f;

        /* Update projection */
        float fov_rad = current_camera->fov * 3.14159f / 180.0f;
        mat4_perspective_local(projection_matrix, fov_rad,
                              (float)TV_WIDTH / (float)TV_HEIGHT,
                              current_camera->near_plane, current_camera->far_plane);
    }
}

void render_end_frame(void) {
    /* Flush remaining vertices */
    if (vertex_count > 0) {
        /* Copy to GX2 buffer */
        void *buffer = GX2RLockBufferEx(&vertex_gx2_buffer, 0);
        memcpy(buffer, vertex_buffer, vertex_count * sizeof(gx2_vertex_t));
        GX2RUnlockBufferEx(&vertex_gx2_buffer, 0);

        /* Draw */
        GX2RSetAttributeBuffer(&vertex_gx2_buffer, 0, sizeof(gx2_vertex_t), 0);
        GX2DrawEx(GX2_PRIMITIVE_MODE_TRIANGLES, vertex_count, 0, 1);
        vertex_count = 0;
    }

    WHBGfxFinishRenderTV();

    /* Also render to GamePad */
    WHBGfxBeginRenderDRC();
    WHBGfxClearColor(clear_color[0], clear_color[1], clear_color[2], clear_color[3]);
    /* Would re-render scene here for GamePad */
    WHBGfxFinishRenderDRC();

    WHBGfxFinishRender();
}

void render_begin_hud(void) {
    /* Flush 3D geometry first */
    if (vertex_count > 0) {
        void *buffer = GX2RLockBufferEx(&vertex_gx2_buffer, 0);
        memcpy(buffer, vertex_buffer, vertex_count * sizeof(gx2_vertex_t));
        GX2RUnlockBufferEx(&vertex_gx2_buffer, 0);
        GX2RSetAttributeBuffer(&vertex_gx2_buffer, 0, sizeof(gx2_vertex_t), 0);
        GX2DrawEx(GX2_PRIMITIVE_MODE_TRIANGLES, vertex_count, 0, 1);
        vertex_count = 0;
    }

    /* Switch to orthographic projection */
    memset(projection_matrix, 0, 16 * sizeof(float));
    projection_matrix[0] = 2.0f / TV_WIDTH;
    projection_matrix[5] = -2.0f / TV_HEIGHT;
    projection_matrix[10] = -1.0f;
    projection_matrix[12] = -1.0f;
    projection_matrix[13] = 1.0f;
    projection_matrix[15] = 1.0f;

    /* Disable depth test for HUD */
    GX2SetDepthOnlyControl(GX2_FALSE, GX2_FALSE, GX2_COMPARE_FUNC_ALWAYS);
}

void render_end_hud(void) {
    /* Restore depth test */
    GX2SetDepthOnlyControl(GX2_TRUE, GX2_TRUE, GX2_COMPARE_FUNC_LESS);

    /* Restore perspective projection */
    if (current_camera) {
        float fov_rad = current_camera->fov * 3.14159f / 180.0f;
        mat4_perspective_local(projection_matrix, fov_rad,
                              (float)TV_WIDTH / (float)TV_HEIGHT,
                              current_camera->near_plane, current_camera->far_plane);
    }
}

void render_clear(u32 color) {
    clear_color[0] = ((color >> 16) & 0xFF) / 255.0f;
    clear_color[1] = ((color >> 8) & 0xFF) / 255.0f;
    clear_color[2] = (color & 0xFF) / 255.0f;
    clear_color[3] = 1.0f;
}

void render_set_camera(camera_t *cam) {
    current_camera = cam;
}

void camera_update(camera_t *cam) {
    cam->view_matrix = mat4_look_at(cam->position, cam->target, cam->up);
    cam->proj_matrix = mat4_perspective(
        deg_to_rad(cam->fov),
        (float)TV_WIDTH / (float)TV_HEIGHT,
        cam->near_plane,
        cam->far_plane
    );
}

static void add_vertex(float x, float y, float z, u32 color) {
    if (vertex_count >= MAX_VERTICES) {
        /* Flush buffer */
        void *buffer = GX2RLockBufferEx(&vertex_gx2_buffer, 0);
        memcpy(buffer, vertex_buffer, vertex_count * sizeof(gx2_vertex_t));
        GX2RUnlockBufferEx(&vertex_gx2_buffer, 0);
        GX2RSetAttributeBuffer(&vertex_gx2_buffer, 0, sizeof(gx2_vertex_t), 0);
        GX2DrawEx(GX2_PRIMITIVE_MODE_TRIANGLES, vertex_count, 0, 1);
        vertex_count = 0;
    }

    gx2_vertex_t *v = &vertex_buffer[vertex_count++];

    /* Transform vertex by MVP matrix */
    float mv[16], mvp[16];
    mat4_multiply_local(mv, view_matrix, model_matrix);
    mat4_multiply_local(mvp, projection_matrix, mv);

    float w = mvp[3]*x + mvp[7]*y + mvp[11]*z + mvp[15];
    v->position[0] = (mvp[0]*x + mvp[4]*y + mvp[8]*z + mvp[12]) / w;
    v->position[1] = (mvp[1]*x + mvp[5]*y + mvp[9]*z + mvp[13]) / w;
    v->position[2] = (mvp[2]*x + mvp[6]*y + mvp[10]*z + mvp[14]) / w;
    v->position[3] = 1.0f;

    v->color[0] = ((color >> 16) & 0xFF) / 255.0f;
    v->color[1] = ((color >> 8) & 0xFF) / 255.0f;
    v->color[2] = (color & 0xFF) / 255.0f;
    v->color[3] = 1.0f;
}

void render_draw_triangle(vertex_t *v0, vertex_t *v1, vertex_t *v2) {
    add_vertex(v0->pos.x, v0->pos.y, v0->pos.z, v0->color);
    add_vertex(v1->pos.x, v1->pos.y, v1->pos.z, v1->color);
    add_vertex(v2->pos.x, v2->pos.y, v2->pos.z, v2->color);
}

void render_draw_mesh(mesh_t *mesh, mat4_t transform) {
    if (!mesh || !current_camera) return;

    /* Store current model matrix */
    float prev_model[16];
    memcpy(prev_model, model_matrix, 16 * sizeof(float));

    /* Set model matrix */
    memcpy(model_matrix, transform.m, 16 * sizeof(float));

    /* Draw triangles */
    for (int i = 0; i < mesh->tri_count; i++) {
        triangle_t *tri = &mesh->triangles[i];
        render_draw_triangle(&tri->v[0], &tri->v[1], &tri->v[2]);
    }

    /* Restore model matrix */
    memcpy(model_matrix, prev_model, 16 * sizeof(float));
}

void render_draw_quad(vec3_t pos, float width, float height, u32 color) {
    float hw = width * 0.5f;
    float hh = height * 0.5f;

    add_vertex(pos.x - hw, pos.y, pos.z - hh, color);
    add_vertex(pos.x + hw, pos.y, pos.z - hh, color);
    add_vertex(pos.x + hw, pos.y, pos.z + hh, color);

    add_vertex(pos.x - hw, pos.y, pos.z - hh, color);
    add_vertex(pos.x + hw, pos.y, pos.z + hh, color);
    add_vertex(pos.x - hw, pos.y, pos.z + hh, color);
}

void render_wait_vram_ready(void) {
    GX2WaitForFlip();
}

void render_draw_sky_background(u32 color) {
    render_clear(color);
}

void render_draw_rect_2d(int x, int y, int w, int h, u32 color) {
    add_vertex(x, y, 0, color);
    add_vertex(x + w, y, 0, color);
    add_vertex(x + w, y + h, 0, color);

    add_vertex(x, y, 0, color);
    add_vertex(x + w, y + h, 0, color);
    add_vertex(x, y + h, 0, color);
}

void render_draw_text(int x, int y, u32 color, const char *text) {
    int cx = x;
    int char_width = 16;
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

mesh_t *mesh_create_cube(float size, u32 color) {
    mesh_t *mesh = (mesh_t *)MEMAllocFromDefaultHeap(sizeof(mesh_t));
    mesh->tri_count = 12;
    mesh->triangles = (triangle_t *)MEMAllocFromDefaultHeap(sizeof(triangle_t) * mesh->tri_count);
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

mesh_t *mesh_create_vehicle(u32 color) {
    mesh_t *mesh = (mesh_t *)MEMAllocFromDefaultHeap(sizeof(mesh_t));
    mesh->tri_count = 12;
    mesh->triangles = (triangle_t *)MEMAllocFromDefaultHeap(sizeof(triangle_t) * mesh->tri_count);
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

mesh_t *mesh_create_track_segment(float width, float length, u32 color) {
    mesh_t *mesh = (mesh_t *)MEMAllocFromDefaultHeap(sizeof(mesh_t));
    mesh->tri_count = 2;
    mesh->triangles = (triangle_t *)MEMAllocFromDefaultHeap(sizeof(triangle_t) * mesh->tri_count);
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
        if (mesh->triangles) MEMFreeToDefaultHeap(mesh->triangles);
        MEMFreeToDefaultHeap(mesh);
    }
}

#endif /* PLATFORM_WIIU */
