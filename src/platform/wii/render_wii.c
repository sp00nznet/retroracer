/*
 * RetroRacer - Nintendo Wii Rendering Implementation
 * Uses libogc with GX (GameCube/Wii graphics)
 *
 * GX is the graphics library for GameCube and Wii
 * - Hardware T&L (Transform & Lighting)
 * - TEV (Texture Environment) for pixel operations
 * - 640x480 (480i/480p)
 */

#include "render.h"
#include "platform.h"

#ifdef PLATFORM_WII

#include <gccore.h>
#include <ogc/gx.h>
#include <malloc.h>
#include <string.h>
#include <math.h>

/* Display settings */
#define SCREEN_WIDTH 640
#define SCREEN_HEIGHT 480
#define FIFO_SIZE (256 * 1024)

/* GX state */
static void *framebuffer[2] = {NULL, NULL};
static int current_fb = 0;
static GXRModeObj *rmode = NULL;
static void *gp_fifo = NULL;

/* Matrices */
static Mtx view_matrix;
static Mtx44 projection_matrix;
static Mtx model_matrix;
static Mtx modelview_matrix;

static camera_t *current_camera = NULL;

/* Vertex format */
static GXColor vertex_colors[3];

void render_init(void) {
    /* Initialize video */
    VIDEO_Init();

    /* Get preferred video mode */
    rmode = VIDEO_GetPreferredMode(NULL);

    /* Allocate framebuffers */
    framebuffer[0] = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
    framebuffer[1] = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));

    /* Configure video */
    VIDEO_Configure(rmode);
    VIDEO_SetNextFramebuffer(framebuffer[current_fb]);
    VIDEO_SetBlack(FALSE);
    VIDEO_Flush();
    VIDEO_WaitVSync();

    if (rmode->viTVMode & VI_NON_INTERLACE) {
        VIDEO_WaitVSync();
    }

    /* Allocate FIFO buffer */
    gp_fifo = memalign(32, FIFO_SIZE);
    memset(gp_fifo, 0, FIFO_SIZE);

    /* Initialize GX */
    GX_Init(gp_fifo, FIFO_SIZE);

    /* Set clear color (sky blue) */
    GXColor background = {0x87, 0xCE, 0xEB, 0xFF};
    GX_SetCopyClear(background, GX_MAX_Z24);

    /* Set viewport */
    GX_SetViewport(0, 0, rmode->fbWidth, rmode->efbHeight, 0, 1);

    /* Set scissor */
    GX_SetScissor(0, 0, rmode->fbWidth, rmode->efbHeight);

    /* Set Y scale */
    f32 yscale = GX_GetYScaleFactor(rmode->efbHeight, rmode->xfbHeight);
    u32 xfbHeight = GX_SetDispCopyYScale(yscale);

    /* Set copy parameters */
    GX_SetDispCopySrc(0, 0, rmode->fbWidth, rmode->efbHeight);
    GX_SetDispCopyDst(rmode->fbWidth, xfbHeight);
    GX_SetCopyFilter(rmode->aa, rmode->sample_pattern, GX_TRUE, rmode->vfilter);
    GX_SetFieldMode(rmode->field_rendering,
                    ((rmode->viHeight == 2 * rmode->xfbHeight) ? GX_ENABLE : GX_DISABLE));

    /* Set pixel format */
    GX_SetPixelFmt(GX_PF_RGB8_Z24, GX_ZC_LINEAR);

    /* Set culling */
    GX_SetCullMode(GX_CULL_BACK);
    GX_CopyDisp(framebuffer[current_fb], GX_TRUE);
    GX_SetDispCopyGamma(GX_GM_1_0);

    /* Setup vertex descriptor - position and color */
    GX_ClearVtxDesc();
    GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
    GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);

    /* Setup vertex attribute table */
    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);

    /* Setup TEV (single color pass) */
    GX_SetNumChans(1);
    GX_SetNumTexGens(0);
    GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORDNULL, GX_TEXMAP_NULL, GX_COLOR0A0);
    GX_SetTevOp(GX_TEVSTAGE0, GX_PASSCLR);

    /* Initialize matrices */
    guMtxIdentity(view_matrix);
    guMtxIdentity(model_matrix);

    /* Set up projection matrix */
    guPerspective(projection_matrix, 70, (f32)rmode->fbWidth / (f32)rmode->efbHeight, 0.1f, 300.0f);
    GX_LoadProjectionMtx(projection_matrix, GX_PERSPECTIVE);
}

void render_shutdown(void) {
    if (gp_fifo) {
        free(gp_fifo);
        gp_fifo = NULL;
    }
}

void render_begin_frame(void) {
    /* Set clear color */
    GXColor background = {0x87, 0xCE, 0xEB, 0xFF};
    GX_SetCopyClear(background, GX_MAX_Z24);

    /* Update camera view matrix */
    if (current_camera) {
        guVector cam_pos = {current_camera->position.x,
                           current_camera->position.y,
                           current_camera->position.z};
        guVector cam_target = {current_camera->target.x,
                              current_camera->target.y,
                              current_camera->target.z};
        guVector cam_up = {current_camera->up.x,
                          current_camera->up.y,
                          current_camera->up.z};

        guLookAt(view_matrix, &cam_pos, &cam_up, &cam_target);

        /* Update projection with camera FOV */
        guPerspective(projection_matrix, current_camera->fov,
                     (f32)rmode->fbWidth / (f32)rmode->efbHeight,
                     current_camera->near_plane, current_camera->far_plane);
        GX_LoadProjectionMtx(projection_matrix, GX_PERSPECTIVE);
    }
}

void render_end_frame(void) {
    /* Draw done */
    GX_DrawDone();

    /* Swap framebuffers */
    current_fb ^= 1;
    GX_SetZMode(GX_TRUE, GX_LEQUAL, GX_TRUE);
    GX_SetColorUpdate(GX_TRUE);
    GX_CopyDisp(framebuffer[current_fb], GX_TRUE);

    VIDEO_SetNextFramebuffer(framebuffer[current_fb]);
    VIDEO_Flush();
    VIDEO_WaitVSync();
}

void render_begin_hud(void) {
    /* Switch to orthographic projection for HUD */
    Mtx44 ortho;
    guOrtho(ortho, 0, rmode->efbHeight, 0, rmode->fbWidth, 0, 1);
    GX_LoadProjectionMtx(ortho, GX_ORTHOGRAPHIC);

    /* Identity modelview */
    Mtx identity;
    guMtxIdentity(identity);
    GX_LoadPosMtxImm(identity, GX_PNMTX0);

    /* Disable depth test for HUD */
    GX_SetZMode(GX_FALSE, GX_ALWAYS, GX_FALSE);
}

void render_end_hud(void) {
    /* Restore perspective projection */
    GX_LoadProjectionMtx(projection_matrix, GX_PERSPECTIVE);
    GX_SetZMode(GX_TRUE, GX_LEQUAL, GX_TRUE);
}

void render_clear(u32 color) {
    u8 r = (color >> 16) & 0xFF;
    u8 g = (color >> 8) & 0xFF;
    u8 b = color & 0xFF;

    GXColor background = {r, g, b, 0xFF};
    GX_SetCopyClear(background, GX_MAX_Z24);
}

void render_set_camera(camera_t *cam) {
    current_camera = cam;
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

void render_draw_triangle(vertex_t *v0, vertex_t *v1, vertex_t *v2) {
    /* Calculate modelview matrix */
    guMtxConcat(view_matrix, model_matrix, modelview_matrix);
    GX_LoadPosMtxImm(modelview_matrix, GX_PNMTX0);

    /* Begin drawing */
    GX_Begin(GX_TRIANGLES, GX_VTXFMT0, 3);

    /* Vertex 0 */
    GX_Position3f32(v0->pos.x, v0->pos.y, v0->pos.z);
    GX_Color4u8((v0->color >> 16) & 0xFF, (v0->color >> 8) & 0xFF,
                v0->color & 0xFF, 0xFF);

    /* Vertex 1 */
    GX_Position3f32(v1->pos.x, v1->pos.y, v1->pos.z);
    GX_Color4u8((v1->color >> 16) & 0xFF, (v1->color >> 8) & 0xFF,
                v1->color & 0xFF, 0xFF);

    /* Vertex 2 */
    GX_Position3f32(v2->pos.x, v2->pos.y, v2->pos.z);
    GX_Color4u8((v2->color >> 16) & 0xFF, (v2->color >> 8) & 0xFF,
                v2->color & 0xFF, 0xFF);

    GX_End();
}

void render_draw_mesh(mesh_t *mesh, mat4_t transform) {
    if (!mesh || !current_camera) return;

    /* Set model matrix from transform */
    model_matrix[0][0] = transform.m[0];
    model_matrix[0][1] = transform.m[1];
    model_matrix[0][2] = transform.m[2];
    model_matrix[0][3] = transform.m[12];
    model_matrix[1][0] = transform.m[4];
    model_matrix[1][1] = transform.m[5];
    model_matrix[1][2] = transform.m[6];
    model_matrix[1][3] = transform.m[13];
    model_matrix[2][0] = transform.m[8];
    model_matrix[2][1] = transform.m[9];
    model_matrix[2][2] = transform.m[10];
    model_matrix[2][3] = transform.m[14];

    /* Calculate modelview */
    guMtxConcat(view_matrix, model_matrix, modelview_matrix);
    GX_LoadPosMtxImm(modelview_matrix, GX_PNMTX0);

    /* Draw all triangles */
    GX_Begin(GX_TRIANGLES, GX_VTXFMT0, mesh->tri_count * 3);

    for (int i = 0; i < mesh->tri_count; i++) {
        triangle_t *tri = &mesh->triangles[i];

        for (int v = 0; v < 3; v++) {
            GX_Position3f32(tri->v[v].pos.x, tri->v[v].pos.y, tri->v[v].pos.z);
            GX_Color4u8((tri->v[v].color >> 16) & 0xFF,
                       (tri->v[v].color >> 8) & 0xFF,
                       tri->v[v].color & 0xFF, 0xFF);
        }
    }

    GX_End();

    /* Reset model matrix */
    guMtxIdentity(model_matrix);
}

void render_draw_quad(vec3_t pos, float width, float height, u32 color) {
    float hw = width * 0.5f;
    float hh = height * 0.5f;

    guMtxConcat(view_matrix, model_matrix, modelview_matrix);
    GX_LoadPosMtxImm(modelview_matrix, GX_PNMTX0);

    u8 r = (color >> 16) & 0xFF;
    u8 g = (color >> 8) & 0xFF;
    u8 b = color & 0xFF;

    GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
    GX_Position3f32(pos.x - hw, pos.y, pos.z - hh);
    GX_Color4u8(r, g, b, 0xFF);
    GX_Position3f32(pos.x + hw, pos.y, pos.z - hh);
    GX_Color4u8(r, g, b, 0xFF);
    GX_Position3f32(pos.x + hw, pos.y, pos.z + hh);
    GX_Color4u8(r, g, b, 0xFF);
    GX_Position3f32(pos.x - hw, pos.y, pos.z + hh);
    GX_Color4u8(r, g, b, 0xFF);
    GX_End();
}

void render_wait_vram_ready(void) {
    VIDEO_WaitVSync();
}

void render_draw_sky_background(u32 color) {
    render_clear(color);
}

void render_draw_rect_2d(int x, int y, int w, int h, u32 color) {
    u8 r = (color >> 16) & 0xFF;
    u8 g = (color >> 8) & 0xFF;
    u8 b = color & 0xFF;

    GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
    GX_Position3f32(x, y, 0);
    GX_Color4u8(r, g, b, 0xFF);
    GX_Position3f32(x + w, y, 0);
    GX_Color4u8(r, g, b, 0xFF);
    GX_Position3f32(x + w, y + h, 0);
    GX_Color4u8(r, g, b, 0xFF);
    GX_Position3f32(x, y + h, 0);
    GX_Color4u8(r, g, b, 0xFF);
    GX_End();
}

void render_draw_text(int x, int y, u32 color, const char *text) {
    int cx = x;
    int char_width = 10;
    int char_height = 16;

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
    mesh_t *mesh = (mesh_t *)memalign(32, sizeof(mesh_t));
    mesh->tri_count = 12;
    mesh->triangles = (triangle_t *)memalign(32, sizeof(triangle_t) * mesh->tri_count);
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
    mesh_t *mesh = (mesh_t *)memalign(32, sizeof(mesh_t));
    mesh->tri_count = 12;
    mesh->triangles = (triangle_t *)memalign(32, sizeof(triangle_t) * mesh->tri_count);
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
    mesh_t *mesh = (mesh_t *)memalign(32, sizeof(mesh_t));
    mesh->tri_count = 2;
    mesh->triangles = (triangle_t *)memalign(32, sizeof(triangle_t) * mesh->tri_count);
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

#endif /* PLATFORM_WII */
