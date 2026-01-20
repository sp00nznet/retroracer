/*
 * RetroRacer - Xbox 360 Rendering Implementation
 * Uses libxenon with Xenos GPU (ATI-based unified shader architecture)
 */

#include "render.h"
#include "platform.h"

#ifdef PLATFORM_XBOX360

#include <xenos/xe.h>
#include <xenos/xenos.h>
#include <xenos/edram.h>
#include <console/console.h>
#include <ppc/timebase.h>
#include <xenon_smc/xenon_smc.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* Display settings */
#define SCREEN_WIDTH 1280
#define SCREEN_HEIGHT 720

/* Xenos GPU context */
static struct XenosDevice *xe = NULL;
static struct XenosShader *sh_vs = NULL;
static struct XenosShader *sh_ps = NULL;
static struct XenosVertexBuffer *vb = NULL;

/* Framebuffer */
static struct XenosSurface *fb = NULL;

static camera_t *current_camera = NULL;

/* Vertex format for Xenos */
typedef struct {
    float x, y, z;
    float r, g, b, a;
} xenos_vertex_t;

/* Vertex buffer for batching triangles */
#define MAX_VERTICES 16384
static xenos_vertex_t vertex_buffer[MAX_VERTICES];
static int vertex_count = 0;

/* Simple vertex shader (pre-compiled) */
static const unsigned char vs_main[] = {
    /* Xenos vertex shader binary - passes through position and color */
    0x00, 0x00, 0x00, 0x00  /* Placeholder - actual shader would be compiled */
};

/* Simple pixel shader (pre-compiled) */
static const unsigned char ps_main[] = {
    /* Xenos pixel shader binary - outputs interpolated color */
    0x00, 0x00, 0x00, 0x00  /* Placeholder */
};

void render_init(void) {
    /* Initialize Xenos GPU */
    xe = Xe_Init();

    Xe_SetRenderTarget(xe, Xe_GetFramebufferSurface(xe));

    /* Create shaders */
    sh_vs = Xe_LoadShaderFromMemory(xe, (void*)vs_main);
    sh_ps = Xe_LoadShaderFromMemory(xe, (void*)ps_main);

    Xe_InstantiateShader(xe, sh_vs, 0);
    Xe_InstantiateShader(xe, sh_ps, 0);

    /* Create vertex buffer */
    vb = Xe_CreateVertexBuffer(xe, MAX_VERTICES * sizeof(xenos_vertex_t));

    /* Set default state */
    Xe_SetCullMode(xe, XE_CULL_NONE);
    Xe_SetClearColor(xe, 0xFF87CEEB);  /* Sky blue */
}

void render_shutdown(void) {
    if (vb) Xe_DestroyVertexBuffer(xe, vb);
    if (xe) Xe_Shutdown(xe);
}

void render_begin_frame(void) {
    /* Begin new frame */
    Xe_ResolveInto(xe, Xe_GetFramebufferSurface(xe), XE_SOURCE_COLOR,
                   0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);

    /* Clear buffers */
    Xe_Clear(xe, XE_CLEAR_COLOR | XE_CLEAR_DS);

    /* Reset vertex buffer */
    vertex_count = 0;

    /* Set shaders */
    Xe_SetShader(xe, SHADER_TYPE_VERTEX, sh_vs, 0);
    Xe_SetShader(xe, SHADER_TYPE_PIXEL, sh_ps, 0);
}

void render_end_frame(void) {
    /* Flush any remaining triangles */
    if (vertex_count > 0) {
        void *vb_data = Xe_VB_Lock(xe, vb, 0, vertex_count * sizeof(xenos_vertex_t), XE_LOCK_WRITE);
        memcpy(vb_data, vertex_buffer, vertex_count * sizeof(xenos_vertex_t));
        Xe_VB_Unlock(xe, vb);

        Xe_SetStreamSource(xe, 0, vb, 0, sizeof(xenos_vertex_t));
        Xe_DrawPrimitive(xe, XE_PRIMTYPE_TRIANGLELIST, 0, vertex_count / 3);
    }

    /* Resolve and present */
    Xe_Resolve(xe);
    Xe_Sync(xe);
}

void render_begin_hud(void) {
    /* Flush 3D geometry before HUD */
    if (vertex_count > 0) {
        void *vb_data = Xe_VB_Lock(xe, vb, 0, vertex_count * sizeof(xenos_vertex_t), XE_LOCK_WRITE);
        memcpy(vb_data, vertex_buffer, vertex_count * sizeof(xenos_vertex_t));
        Xe_VB_Unlock(xe, vb);

        Xe_SetStreamSource(xe, 0, vb, 0, sizeof(xenos_vertex_t));
        Xe_DrawPrimitive(xe, XE_PRIMTYPE_TRIANGLELIST, 0, vertex_count / 3);
        vertex_count = 0;
    }

    /* Disable depth test for HUD */
    Xe_SetZEnable(xe, 0);
}

void render_end_hud(void) {
    /* Re-enable depth test */
    Xe_SetZEnable(xe, 1);
}

void render_clear(uint32_t color) {
    Xe_SetClearColor(xe, color | 0xFF000000);
    Xe_Clear(xe, XE_CLEAR_COLOR);
}

void render_set_camera(camera_t *cam) {
    current_camera = cam;

    if (!cam) return;

    /* Set view-projection matrix as shader constant */
    mat4_t vp = mat4_multiply(cam->proj_matrix, cam->view_matrix);
    Xe_SetVertexShaderConstantF(xe, 0, (float*)&vp, 4);
}

void camera_update(camera_t *cam) {
    cam->view_matrix = mat4_look_at(cam->position, cam->target, cam->up);
    cam->proj_matrix = mat4_perspective(
        deg_to_rad(cam->fov),
        (float)SCREEN_WIDTH / (float)SCREEN_HEIGHT,
        cam->near_plane,
        cam->far_plane
    );
}

/* Transform and project vertex */
static int transform_vertex(vec3_t pos, float *sx, float *sy, float *sz) {
    if (!current_camera) return 0;

    mat4_t vp = mat4_multiply(current_camera->proj_matrix, current_camera->view_matrix);

    float x = vp.m[0]*pos.x + vp.m[4]*pos.y + vp.m[8]*pos.z + vp.m[12];
    float y = vp.m[1]*pos.x + vp.m[5]*pos.y + vp.m[9]*pos.z + vp.m[13];
    float z = vp.m[2]*pos.x + vp.m[6]*pos.y + vp.m[10]*pos.z + vp.m[14];
    float w = vp.m[3]*pos.x + vp.m[7]*pos.y + vp.m[11]*pos.z + vp.m[15];

    if (w <= 0.0001f) return 0;

    float inv_w = 1.0f / w;
    *sx = (x * inv_w * 0.5f + 0.5f) * SCREEN_WIDTH;
    *sy = (1.0f - (y * inv_w * 0.5f + 0.5f)) * SCREEN_HEIGHT;
    *sz = z * inv_w;

    return 1;
}

void render_draw_triangle(vertex_t *v0, vertex_t *v1, vertex_t *v2) {
    if (!current_camera) return;
    if (vertex_count + 3 > MAX_VERTICES) return;

    float sx0, sy0, sz0;
    float sx1, sy1, sz1;
    float sx2, sy2, sz2;

    if (!transform_vertex(v0->pos, &sx0, &sy0, &sz0)) return;
    if (!transform_vertex(v1->pos, &sx1, &sy1, &sz1)) return;
    if (!transform_vertex(v2->pos, &sx2, &sy2, &sz2)) return;

    /* Convert to normalized device coordinates for GPU */
    vertex_buffer[vertex_count].x = (sx0 / SCREEN_WIDTH) * 2.0f - 1.0f;
    vertex_buffer[vertex_count].y = 1.0f - (sy0 / SCREEN_HEIGHT) * 2.0f;
    vertex_buffer[vertex_count].z = sz0;
    vertex_buffer[vertex_count].r = ((v0->color >> 16) & 0xFF) / 255.0f;
    vertex_buffer[vertex_count].g = ((v0->color >> 8) & 0xFF) / 255.0f;
    vertex_buffer[vertex_count].b = (v0->color & 0xFF) / 255.0f;
    vertex_buffer[vertex_count].a = 1.0f;
    vertex_count++;

    vertex_buffer[vertex_count].x = (sx1 / SCREEN_WIDTH) * 2.0f - 1.0f;
    vertex_buffer[vertex_count].y = 1.0f - (sy1 / SCREEN_HEIGHT) * 2.0f;
    vertex_buffer[vertex_count].z = sz1;
    vertex_buffer[vertex_count].r = ((v1->color >> 16) & 0xFF) / 255.0f;
    vertex_buffer[vertex_count].g = ((v1->color >> 8) & 0xFF) / 255.0f;
    vertex_buffer[vertex_count].b = (v1->color & 0xFF) / 255.0f;
    vertex_buffer[vertex_count].a = 1.0f;
    vertex_count++;

    vertex_buffer[vertex_count].x = (sx2 / SCREEN_WIDTH) * 2.0f - 1.0f;
    vertex_buffer[vertex_count].y = 1.0f - (sy2 / SCREEN_HEIGHT) * 2.0f;
    vertex_buffer[vertex_count].z = sz2;
    vertex_buffer[vertex_count].r = ((v2->color >> 16) & 0xFF) / 255.0f;
    vertex_buffer[vertex_count].g = ((v2->color >> 8) & 0xFF) / 255.0f;
    vertex_buffer[vertex_count].b = (v2->color & 0xFF) / 255.0f;
    vertex_buffer[vertex_count].a = 1.0f;
    vertex_count++;
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
    Xe_Sync(xe);
}

void render_draw_sky_background(uint32_t color) {
    render_clear(color);
}

void render_draw_rect_2d(int x, int y, int w, int h, uint32_t color) {
    /* 2D rectangle as two triangles */
    float x0 = (float)x / SCREEN_WIDTH * 2.0f - 1.0f;
    float y0 = 1.0f - (float)y / SCREEN_HEIGHT * 2.0f;
    float x1 = (float)(x + w) / SCREEN_WIDTH * 2.0f - 1.0f;
    float y1 = 1.0f - (float)(y + h) / SCREEN_HEIGHT * 2.0f;

    float r = ((color >> 16) & 0xFF) / 255.0f;
    float g = ((color >> 8) & 0xFF) / 255.0f;
    float b = (color & 0xFF) / 255.0f;

    if (vertex_count + 6 > MAX_VERTICES) return;

    /* First triangle */
    vertex_buffer[vertex_count++] = (xenos_vertex_t){x0, y0, 0, r, g, b, 1.0f};
    vertex_buffer[vertex_count++] = (xenos_vertex_t){x1, y0, 0, r, g, b, 1.0f};
    vertex_buffer[vertex_count++] = (xenos_vertex_t){x1, y1, 0, r, g, b, 1.0f};

    /* Second triangle */
    vertex_buffer[vertex_count++] = (xenos_vertex_t){x0, y0, 0, r, g, b, 1.0f};
    vertex_buffer[vertex_count++] = (xenos_vertex_t){x1, y1, 0, r, g, b, 1.0f};
    vertex_buffer[vertex_count++] = (xenos_vertex_t){x0, y1, 0, r, g, b, 1.0f};
}

void render_draw_text(int x, int y, uint32_t color, const char *text) {
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

#endif /* PLATFORM_XBOX360 */
