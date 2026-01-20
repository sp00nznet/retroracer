/*
 * RetroRacer - 3DO Rendering Implementation
 * Uses 3DO SDK with CEL Engine for hardware-accelerated graphics
 *
 * The 3DO can render textured/shaded quads (CELs) with perspective
 * This gives it actual 3D capability unlike SNES/Genesis
 */

#include "render.h"
#include "platform.h"

#ifdef PLATFORM_3DO

#include <graphics.h>
#include <celutils.h>
#include <displayutils.h>
#include <mem.h>
#include <operror.h>
#include <types.h>
#include <string.h>
#include <stdlib.h>

/* Screen dimensions - 3DO can do up to 640x480 but 320x240 is common */
#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240

/* Display items */
static Item screenGroup;
static Item screenItems[2];
static Item bitmapItems[2];
static Bitmap *bitmaps[2];
static int currentBuffer = 0;

/* CEL engine */
static Item VRAMIOReq;
static Item timerIO;

static camera_t *current_camera = NULL;

/* CEL pool for rendering */
#define MAX_CELS 512
static CCB celPool[MAX_CELS];
static int celCount = 0;

/* Colors */
static uint32 clearColor = 0x87CE;  /* Sky blue in 3DO RGB555 */

void render_init(void) {
    /* Open graphics folio */
    OpenGraphicsFolio();

    /* Create display */
    screenGroup = CreateScreenGroup(NULL, NULL);
    AddScreenGroup(screenGroup, NULL);

    /* Setup double-buffered display */
    for (int i = 0; i < 2; i++) {
        screenItems[i] = CreateScreenContext(&screenGroup, NULL);
        bitmapItems[i] = CreateBitmap(&screenItems[i], SCREEN_WIDTH, SCREEN_HEIGHT, BMTYPE_16, NULL);
        bitmaps[i] = (Bitmap *)LookupItem(bitmapItems[i]);
    }

    /* Create VRAM I/O request */
    VRAMIOReq = CreateVRAMIOReq();

    /* Create timer for frame timing */
    timerIO = CreateTimerIOReq();

    currentBuffer = 0;
}

void render_shutdown(void) {
    for (int i = 0; i < 2; i++) {
        DeleteItem(bitmapItems[i]);
        DeleteItem(screenItems[i]);
    }
    RemoveScreenGroup(screenGroup);
    DeleteItem(screenGroup);
}

void render_begin_frame(void) {
    /* Clear the current buffer */
    SetVRAMPages(VRAMIOReq, bitmaps[currentBuffer]->bm_Buffer,
                 clearColor, bitmaps[currentBuffer]->bm_Width * bitmaps[currentBuffer]->bm_Height / 2, -1);

    /* Reset CEL count */
    celCount = 0;
}

void render_end_frame(void) {
    /* Draw all CELs */
    if (celCount > 0) {
        /* Link CELs together */
        for (int i = 0; i < celCount - 1; i++) {
            celPool[i].ccb_NextPtr = &celPool[i + 1];
            celPool[i].ccb_Flags &= ~CCB_LAST;
        }
        celPool[celCount - 1].ccb_NextPtr = NULL;
        celPool[celCount - 1].ccb_Flags |= CCB_LAST;

        /* Draw CEL list */
        DrawCels(bitmapItems[currentBuffer], &celPool[0]);
    }
}

void render_begin_hud(void) {
    /* HUD renders on top */
}

void render_end_hud(void) {
    /* Display current buffer */
    DisplayScreen(screenItems[currentBuffer], 0);

    /* Swap buffers */
    currentBuffer ^= 1;

    /* Wait for vsync */
    WaitVBL(VRAMIOReq, 1);
}

void render_clear(uint32_t color) {
    /* Convert to 3DO RGB555 */
    uint8 r = ((color >> 16) & 0xFF) >> 3;
    uint8 g = ((color >> 8) & 0xFF) >> 3;
    uint8 b = (color & 0xFF) >> 3;

    clearColor = (r << 10) | (g << 5) | b;
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

/* Transform vertex through view-projection matrix */
static int transform_vertex(vec3_t pos, int32 *sx, int32 *sy, int32 *sz) {
    if (!current_camera) return 0;

    mat4_t vp = mat4_multiply(current_camera->proj_matrix, current_camera->view_matrix);

    float x = vp.m[0]*pos.x + vp.m[4]*pos.y + vp.m[8]*pos.z + vp.m[12];
    float y = vp.m[1]*pos.x + vp.m[5]*pos.y + vp.m[9]*pos.z + vp.m[13];
    float z = vp.m[2]*pos.x + vp.m[6]*pos.y + vp.m[10]*pos.z + vp.m[14];
    float w = vp.m[3]*pos.x + vp.m[7]*pos.y + vp.m[11]*pos.z + vp.m[15];

    if (w <= 0.0001f) return 0;

    float inv_w = 1.0f / w;
    *sx = (int32)((x * inv_w * 0.5f + 0.5f) * SCREEN_WIDTH * 65536);  /* 16.16 fixed point */
    *sy = (int32)((1.0f - (y * inv_w * 0.5f + 0.5f)) * SCREEN_HEIGHT * 65536);
    *sz = (int32)((z * inv_w * 0.5f + 0.5f) * 65536);

    if (*sz < 0 || *sz > 65536) return 0;

    return 1;
}

/* Convert color to 3DO format */
static uint32 color_to_3do(uint32_t color) {
    uint8 r = ((color >> 16) & 0xFF) >> 3;
    uint8 g = ((color >> 8) & 0xFF) >> 3;
    uint8 b = (color & 0xFF) >> 3;
    return (r << 10) | (g << 5) | b;
}

void render_draw_triangle(vertex_t *v0, vertex_t *v1, vertex_t *v2) {
    if (!current_camera) return;
    if (celCount >= MAX_CELS) return;

    /* Transform vertices */
    int32 sx0, sy0, sz0;
    int32 sx1, sy1, sz1;
    int32 sx2, sy2, sz2;

    if (!transform_vertex(v0->pos, &sx0, &sy0, &sz0)) return;
    if (!transform_vertex(v1->pos, &sx1, &sy1, &sz1)) return;
    if (!transform_vertex(v2->pos, &sx2, &sy2, &sz2)) return;

    /* Create a CEL for this triangle
     * 3DO CELs are quads, so we use a degenerate quad (two points same) */
    CCB *cel = &celPool[celCount++];
    memset(cel, 0, sizeof(CCB));

    /* Setup CEL flags for colored polygon */
    cel->ccb_Flags = CCB_LDSIZE | CCB_LDPRS | CCB_LDPPMP | CCB_CCBPRE |
                     CCB_YOXY | CCB_ACW | CCB_ACCW |
                     CCB_SPABS | CCB_PPABS | CCB_NPABS;

    /* Uncoded CEL (flat color) */
    cel->ccb_PRE0 = PRE0_LITERAL | PRE0_BGND |
                    ((1 - 1) << PRE0_VCNT_SHIFT) |  /* 1 row */
                    PRE0_BPP_16;
    cel->ccb_PRE1 = ((1 - 1) << PRE1_TLHPCNT_SHIFT);  /* 1 pixel */

    /* Use first vertex color */
    cel->ccb_SourcePtr = (CelData *)color_to_3do(v0->color);

    /* Set quad corners (convert from 16.16 to 12.20 format for 3DO) */
    cel->ccb_XPos = sx0 << 4;
    cel->ccb_YPos = sy0 << 4;

    /* Calculate deltas for the quad edges */
    cel->ccb_HDX = ((sx1 - sx0) << 4) / 1;  /* Delta to second corner */
    cel->ccb_HDY = ((sy1 - sy0) << 4) / 1;
    cel->ccb_VDX = ((sx2 - sx0) << 4) / 1;  /* Delta to third corner */
    cel->ccb_VDY = ((sy2 - sy0) << 4) / 1;

    /* Second order deltas (for perspective) */
    cel->ccb_HDDX = 0;
    cel->ccb_HDDY = 0;
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
    WaitIO(VRAMIOReq);
}

void render_draw_sky_background(uint32_t color) {
    render_clear(color);
}

void render_draw_rect_2d(int x, int y, int w, int h, uint32_t color) {
    if (celCount >= MAX_CELS) return;

    CCB *cel = &celPool[celCount++];
    memset(cel, 0, sizeof(CCB));

    cel->ccb_Flags = CCB_LDSIZE | CCB_LDPRS | CCB_LDPPMP | CCB_CCBPRE |
                     CCB_YOXY | CCB_ACW | CCB_ACCW |
                     CCB_SPABS | CCB_PPABS | CCB_NPABS;

    cel->ccb_PRE0 = PRE0_LITERAL | PRE0_BGND |
                    ((1 - 1) << PRE0_VCNT_SHIFT) |
                    PRE0_BPP_16;
    cel->ccb_PRE1 = ((1 - 1) << PRE1_TLHPCNT_SHIFT);

    cel->ccb_SourcePtr = (CelData *)color_to_3do(color);

    /* Position in 16.16 shifted to 12.20 */
    cel->ccb_XPos = x << 20;
    cel->ccb_YPos = y << 20;

    /* Width and height deltas */
    cel->ccb_HDX = w << 20;
    cel->ccb_HDY = 0;
    cel->ccb_VDX = 0;
    cel->ccb_VDY = h << 20;

    cel->ccb_HDDX = 0;
    cel->ccb_HDDY = 0;
}

void render_draw_text(int x, int y, uint32_t color, const char *text) {
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

mesh_t *mesh_create_cube(float size, uint32_t color) {
    mesh_t *mesh = (mesh_t *)AllocMem(sizeof(mesh_t), MEMTYPE_ANY);
    mesh->tri_count = 12;
    mesh->triangles = (triangle_t *)AllocMem(sizeof(triangle_t) * mesh->tri_count, MEMTYPE_ANY);
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
    mesh_t *mesh = (mesh_t *)AllocMem(sizeof(mesh_t), MEMTYPE_ANY);
    mesh->tri_count = 12;
    mesh->triangles = (triangle_t *)AllocMem(sizeof(triangle_t) * mesh->tri_count, MEMTYPE_ANY);
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
    mesh_t *mesh = (mesh_t *)AllocMem(sizeof(mesh_t), MEMTYPE_ANY);
    mesh->tri_count = 2;
    mesh->triangles = (triangle_t *)AllocMem(sizeof(triangle_t) * mesh->tri_count, MEMTYPE_ANY);
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
        if (mesh->triangles) FreeMem(mesh->triangles, sizeof(triangle_t) * mesh->tri_count);
        FreeMem(mesh, sizeof(mesh_t));
    }
}

#endif /* PLATFORM_3DO */
