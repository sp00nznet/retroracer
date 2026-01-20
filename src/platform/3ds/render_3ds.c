/*
 * RetroRacer - Nintendo 3DS Rendering Implementation
 * Uses citro3d for hardware-accelerated 3D on the PICA200 GPU
 *
 * The 3DS has a proper 3D GPU (PICA200) with:
 * - Hardware vertex/geometry processing
 * - Per-pixel lighting
 * - Texture mapping with filtering
 * - Stereoscopic 3D on top screen
 *
 * Top screen: 400x240 (stereoscopic 3D capable)
 * Bottom screen: 320x240 (touchscreen, used for HUD)
 */

#include "render.h"
#include "platform.h"

#ifdef PLATFORM_3DS

#include <3ds.h>
#include <citro3d.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

/* Screen dimensions */
#define TOP_SCREEN_WIDTH  400
#define TOP_SCREEN_HEIGHT 240
#define BOT_SCREEN_WIDTH  320
#define BOT_SCREEN_HEIGHT 240

/* Vertex buffer size */
#define MAX_VERTICES 8192

/* Vertex format for citro3d */
typedef struct {
    float position[3];
    float color[4];
} c3d_vertex_t;

/* Citro3D state */
static C3D_RenderTarget *target_top_left = NULL;
static C3D_RenderTarget *target_top_right = NULL;  /* For stereoscopic 3D */
static C3D_RenderTarget *target_bottom = NULL;

static C3D_Mtx projection;
static C3D_Mtx modelview;

/* Vertex buffer */
static c3d_vertex_t *vertex_buffer = NULL;
static int vertex_count = 0;

/* Shader program */
static DVLB_s *vshader_dvlb = NULL;
static shaderProgram_s shader_program;
static int uLoc_projection;
static int uLoc_modelview;

static camera_t *current_camera = NULL;
static bool in_hud_mode = false;

/* Simple vertex shader binary (pre-compiled PICA200 shader) */
/* This is a minimal passthrough shader */
static const u8 vshader_shbin[] = {
    /* DVLE header and shader binary would go here */
    /* For now we'll use the default shader setup */
};

/* Attribute info for vertices */
static C3D_AttrInfo attr_info;
static C3D_BufInfo buf_info;

void render_init(void) {
    /* Initialize citro3d */
    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);

    /* Create render targets */
    target_top_left = C3D_RenderTargetCreate(TOP_SCREEN_HEIGHT, TOP_SCREEN_WIDTH,
                                              GPU_RB_RGBA8, GPU_RB_DEPTH24_STENCIL8);
    C3D_RenderTargetSetOutput(target_top_left, GFX_TOP, GFX_LEFT,
                               DISPLAY_TRANSFER_FLAGS);

    target_top_right = C3D_RenderTargetCreate(TOP_SCREEN_HEIGHT, TOP_SCREEN_WIDTH,
                                               GPU_RB_RGBA8, GPU_RB_DEPTH24_STENCIL8);
    C3D_RenderTargetSetOutput(target_top_right, GFX_TOP, GFX_RIGHT,
                               DISPLAY_TRANSFER_FLAGS);

    target_bottom = C3D_RenderTargetCreate(BOT_SCREEN_HEIGHT, BOT_SCREEN_WIDTH,
                                            GPU_RB_RGBA8, GPU_RB_DEPTH24_STENCIL8);
    C3D_RenderTargetSetOutput(target_bottom, GFX_BOTTOM, GFX_LEFT,
                               DISPLAY_TRANSFER_FLAGS);

    /* Allocate vertex buffer in linear memory (required for GPU access) */
    vertex_buffer = (c3d_vertex_t *)linearAlloc(sizeof(c3d_vertex_t) * MAX_VERTICES);

    /* Configure attribute info */
    AttrInfo_Init(&attr_info);
    AttrInfo_AddLoader(&attr_info, 0, GPU_FLOAT, 3);  /* Position */
    AttrInfo_AddLoader(&attr_info, 1, GPU_FLOAT, 4);  /* Color */

    /* Configure buffer info */
    BufInfo_Init(&buf_info);
    BufInfo_Add(&buf_info, vertex_buffer, sizeof(c3d_vertex_t), 2, 0x10);

    /* Configure the default rendering state */
    C3D_CullFace(GPU_CULL_BACK_CCW);
    C3D_DepthTest(true, GPU_GREATER, GPU_WRITE_ALL);

    /* Set up default environment (no textures, vertex colors) */
    C3D_TexEnv *env = C3D_GetTexEnv(0);
    C3D_TexEnvInit(env);
    C3D_TexEnvSrc(env, C3D_Both, GPU_PRIMARY_COLOR, 0, 0);
    C3D_TexEnvFunc(env, C3D_Both, GPU_REPLACE);
}

void render_shutdown(void) {
    /* Free vertex buffer */
    if (vertex_buffer) {
        linearFree(vertex_buffer);
        vertex_buffer = NULL;
    }

    /* Free render targets */
    if (target_top_left) C3D_RenderTargetDelete(target_top_left);
    if (target_top_right) C3D_RenderTargetDelete(target_top_right);
    if (target_bottom) C3D_RenderTargetDelete(target_bottom);

    /* Deinitialize citro3d */
    C3D_Fini();
}

void render_begin_frame(void) {
    vertex_count = 0;
    in_hud_mode = false;

    /* Begin frame */
    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);

    /* Set up 3D rendering on top screen (left eye) */
    C3D_RenderTargetClear(target_top_left, C3D_CLEAR_ALL, 0x87CEEBFF, 0);
    C3D_FrameDrawOn(target_top_left);

    /* Apply camera matrices */
    if (current_camera) {
        /* Set up projection matrix */
        Mtx_PerspStereoTilt(&projection,
                            C3D_AngleFromDegrees(current_camera->fov),
                            (float)TOP_SCREEN_WIDTH / (float)TOP_SCREEN_HEIGHT,
                            current_camera->near_plane,
                            current_camera->far_plane,
                            -osGet3DSliderState() * 0.08f,  /* Inter-ocular distance */
                            2.0f,  /* Focal length */
                            false);  /* Left eye

        /* Set up view matrix */
        Mtx_Identity(&modelview);
        Mtx_LookAt(&modelview,
                   FVec3_New(current_camera->position.x,
                            current_camera->position.y,
                            current_camera->position.z),
                   FVec3_New(current_camera->target.x,
                            current_camera->target.y,
                            current_camera->target.z),
                   FVec3_New(current_camera->up.x,
                            current_camera->up.y,
                            current_camera->up.z),
                   false);
    } else {
        /* Default matrices */
        Mtx_PerspTilt(&projection, C3D_AngleFromDegrees(70.0f),
                      (float)TOP_SCREEN_WIDTH / (float)TOP_SCREEN_HEIGHT,
                      0.1f, 100.0f, false);
        Mtx_Identity(&modelview);
    }

    /* Upload uniforms */
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLoc_projection, &projection);
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLoc_modelview, &modelview);

    /* Set attribute and buffer info */
    C3D_SetAttrInfo(&attr_info);
    C3D_SetBufInfo(&buf_info);
}

void render_end_frame(void) {
    /* Flush any remaining vertices */
    if (vertex_count > 0) {
        C3D_DrawArrays(GPU_TRIANGLES, 0, vertex_count);
        vertex_count = 0;
    }

    /* If stereoscopic 3D is enabled, render right eye */
    if (osGet3DSliderState() > 0.0f && current_camera) {
        C3D_RenderTargetClear(target_top_right, C3D_CLEAR_ALL, 0x87CEEBFF, 0);
        C3D_FrameDrawOn(target_top_right);

        /* Right eye projection */
        Mtx_PerspStereoTilt(&projection,
                            C3D_AngleFromDegrees(current_camera->fov),
                            (float)TOP_SCREEN_WIDTH / (float)TOP_SCREEN_HEIGHT,
                            current_camera->near_plane,
                            current_camera->far_plane,
                            osGet3DSliderState() * 0.08f,  /* Right eye offset */
                            2.0f,
                            false);

        C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLoc_projection, &projection);

        /* Re-render scene for right eye - would need to batch draw calls */
        /* For now, right eye mirrors left */
    }

    /* End frame */
    C3D_FrameEnd(0);
}

void render_begin_hud(void) {
    in_hud_mode = true;

    /* Flush 3D geometry */
    if (vertex_count > 0) {
        C3D_DrawArrays(GPU_TRIANGLES, 0, vertex_count);
        vertex_count = 0;
    }

    /* Switch to bottom screen for HUD */
    C3D_RenderTargetClear(target_bottom, C3D_CLEAR_ALL, 0x000000FF, 0);
    C3D_FrameDrawOn(target_bottom);

    /* Set up orthographic projection for 2D HUD */
    Mtx_OrthoTilt(&projection, 0.0f, BOT_SCREEN_WIDTH, BOT_SCREEN_HEIGHT, 0.0f,
                  -1.0f, 1.0f, false);
    Mtx_Identity(&modelview);

    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLoc_projection, &projection);
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLoc_modelview, &modelview);

    /* Disable depth test for HUD */
    C3D_DepthTest(false, GPU_ALWAYS, GPU_WRITE_ALL);
}

void render_end_hud(void) {
    /* Flush HUD geometry */
    if (vertex_count > 0) {
        C3D_DrawArrays(GPU_TRIANGLES, 0, vertex_count);
        vertex_count = 0;
    }

    /* Re-enable depth test */
    C3D_DepthTest(true, GPU_GREATER, GPU_WRITE_ALL);
    in_hud_mode = false;
}

void render_clear(u32 color) {
    u8 r = (color >> 16) & 0xFF;
    u8 g = (color >> 8) & 0xFF;
    u8 b = color & 0xFF;
    u32 clear_color = (r << 24) | (g << 16) | (b << 8) | 0xFF;

    C3D_RenderTargetClear(target_top_left, C3D_CLEAR_ALL, clear_color, 0);
    if (osGet3DSliderState() > 0.0f) {
        C3D_RenderTargetClear(target_top_right, C3D_CLEAR_ALL, clear_color, 0);
    }
}

void render_set_camera(camera_t *cam) {
    current_camera = cam;
}

void camera_update(camera_t *cam) {
    cam->view_matrix = mat4_look_at(cam->position, cam->target, cam->up);
    cam->proj_matrix = mat4_perspective(
        deg_to_rad(cam->fov),
        (float)TOP_SCREEN_WIDTH / (float)TOP_SCREEN_HEIGHT,
        cam->near_plane,
        cam->far_plane
    );
}

/* Add a vertex to the buffer */
static void add_vertex(float x, float y, float z, u32 color) {
    if (vertex_count >= MAX_VERTICES) {
        /* Flush buffer */
        C3D_DrawArrays(GPU_TRIANGLES, 0, vertex_count);
        vertex_count = 0;
    }

    c3d_vertex_t *v = &vertex_buffer[vertex_count++];
    v->position[0] = x;
    v->position[1] = y;
    v->position[2] = z;
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

    /* Set model matrix */
    C3D_Mtx model;
    Mtx_Identity(&model);

    /* Copy our matrix to citro3d format */
    model.r[0].x = transform.m[0];
    model.r[0].y = transform.m[1];
    model.r[0].z = transform.m[2];
    model.r[0].w = transform.m[3];
    model.r[1].x = transform.m[4];
    model.r[1].y = transform.m[5];
    model.r[1].z = transform.m[6];
    model.r[1].w = transform.m[7];
    model.r[2].x = transform.m[8];
    model.r[2].y = transform.m[9];
    model.r[2].z = transform.m[10];
    model.r[2].w = transform.m[11];
    model.r[3].x = transform.m[12];
    model.r[3].y = transform.m[13];
    model.r[3].z = transform.m[14];
    model.r[3].w = transform.m[15];

    /* Combine with view matrix */
    C3D_Mtx mv;
    Mtx_Multiply(&mv, &modelview, &model);
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLoc_modelview, &mv);

    /* Draw all triangles */
    for (int i = 0; i < mesh->tri_count; i++) {
        triangle_t *tri = &mesh->triangles[i];
        render_draw_triangle(&tri->v[0], &tri->v[1], &tri->v[2]);
    }

    /* Restore view matrix */
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLoc_modelview, &modelview);
}

void render_draw_quad(vec3_t pos, float width, float height, u32 color) {
    float hw = width * 0.5f;
    float hh = height * 0.5f;

    /* First triangle */
    add_vertex(pos.x - hw, pos.y, pos.z - hh, color);
    add_vertex(pos.x + hw, pos.y, pos.z - hh, color);
    add_vertex(pos.x + hw, pos.y, pos.z + hh, color);

    /* Second triangle */
    add_vertex(pos.x - hw, pos.y, pos.z - hh, color);
    add_vertex(pos.x + hw, pos.y, pos.z + hh, color);
    add_vertex(pos.x - hw, pos.y, pos.z + hh, color);
}

void render_wait_vram_ready(void) {
    gspWaitForVBlank();
}

void render_draw_sky_background(u32 color) {
    render_clear(color);
}

void render_draw_rect_2d(int x, int y, int w, int h, u32 color) {
    /* Draw 2D rectangle (for HUD) */
    add_vertex(x, y, 0, color);
    add_vertex(x + w, y, 0, color);
    add_vertex(x + w, y + h, 0, color);

    add_vertex(x, y, 0, color);
    add_vertex(x + w, y + h, 0, color);
    add_vertex(x, y + h, 0, color);
}

void render_draw_text(int x, int y, u32 color, const char *text) {
    /* Simple block-based text rendering */
    int cx = x;
    int char_width = 8;
    int char_height = 12;

    while (*text) {
        char c = *text++;

        if (c == ' ') {
            cx += char_width;
            continue;
        }

        render_draw_rect_2d(cx, y, char_width - 1, char_height, color);
        cx += char_width;
    }
}

mesh_t *mesh_create_cube(float size, u32 color) {
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

mesh_t *mesh_create_vehicle(u32 color) {
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

mesh_t *mesh_create_track_segment(float width, float length, u32 color) {
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

#endif /* PLATFORM_3DS */
