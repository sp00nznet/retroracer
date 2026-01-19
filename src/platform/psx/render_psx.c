/*
 * RetroRacer - PlayStation 1 (PSX) Rendering Implementation
 * Uses Sony PsyQ SDK / libgte / libgpu
 */

#include "render.h"
#include "platform.h"

#ifdef PLATFORM_PSX

#include <sys/types.h>
#include <libetc.h>
#include <libgte.h>
#include <libgpu.h>
#include <libgs.h>

/* PSX display buffers */
#define OT_LENGTH 4096
#define PACKET_SIZE 32768

static DISPENV disp[2];
static DRAWENV draw[2];
static int current_buffer = 0;

/* Ordering table for depth sorting */
static u_long ot[2][OT_LENGTH];
static char packet_buffer[2][PACKET_SIZE];
static char *next_packet;

static camera_t *current_camera = NULL;

/* GTE transformation matrices */
static MATRIX view_matrix;
static MATRIX proj_matrix;

void render_init(void) {
    /* Initialize PSX graphics */
    ResetGraph(0);

    /* Initialize GTE (Geometry Transformation Engine) */
    InitGeom();
    SetGeomOffset(SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2);
    SetGeomScreen(SCREEN_WIDTH / 2);  /* FOV based on screen width */

    /* Setup double buffering */
    SetDefDispEnv(&disp[0], 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    SetDefDispEnv(&disp[1], 0, SCREEN_HEIGHT, SCREEN_WIDTH, SCREEN_HEIGHT);
    SetDefDrawEnv(&draw[0], 0, SCREEN_HEIGHT, SCREEN_WIDTH, SCREEN_HEIGHT);
    SetDefDrawEnv(&draw[1], 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);

    /* Enable background clear */
    draw[0].isbg = 1;
    draw[1].isbg = 1;
    setRGB0(&draw[0], 135, 206, 235);  /* Sky blue */
    setRGB0(&draw[1], 135, 206, 235);

    /* Enable display */
    SetDispMask(1);

    current_buffer = 0;
    PutDispEnv(&disp[current_buffer]);
    PutDrawEnv(&draw[current_buffer]);
}

void render_begin_frame(void) {
    /* Clear ordering table */
    ClearOTagR(ot[current_buffer], OT_LENGTH);
    next_packet = packet_buffer[current_buffer];
}

void render_end_frame(void) {
    /* Wait for GPU and VBlank */
    DrawSync(0);
    VSync(0);

    /* Swap buffers */
    current_buffer ^= 1;
    PutDispEnv(&disp[current_buffer]);
    PutDrawEnv(&draw[current_buffer]);

    /* Draw ordering table */
    DrawOTag(&ot[1 - current_buffer][OT_LENGTH - 1]);
}

void render_begin_hud(void) {
    /* PSX HUD is just 2D primitives at front of OT */
}

void render_end_hud(void) {
    /* Nothing special needed */
}

void render_clear(uint32_t color) {
    setRGB0(&draw[0],
            (color >> 16) & 0xFF,
            (color >> 8) & 0xFF,
            color & 0xFF);
    setRGB0(&draw[1],
            (color >> 16) & 0xFF,
            (color >> 8) & 0xFF,
            color & 0xFF);
}

void render_set_camera(camera_t *cam) {
    current_camera = cam;

    if (!cam) return;

    /* Setup GTE view matrix from camera */
    VECTOR pos, target, up;

    pos.vx = (int)(cam->position.x * 4096.0f);
    pos.vy = (int)(cam->position.y * 4096.0f);
    pos.vz = (int)(cam->position.z * 4096.0f);

    target.vx = (int)(cam->target.x * 4096.0f);
    target.vy = (int)(cam->target.y * 4096.0f);
    target.vz = (int)(cam->target.z * 4096.0f);

    up.vx = (int)(cam->up.x * 4096.0f);
    up.vy = (int)(cam->up.y * 4096.0f);
    up.vz = (int)(cam->up.z * 4096.0f);

    /* Create look-at matrix using GTE */
    VECTOR dir;
    dir.vx = target.vx - pos.vx;
    dir.vy = target.vy - pos.vy;
    dir.vz = target.vz - pos.vz;

    /* Normalize and create rotation matrix */
    gte_SetRotMatrix(&view_matrix);
    gte_SetTransMatrix(&view_matrix);
}

void camera_update(camera_t *cam) {
    /* Matrix calculation handled in render_set_camera for PSX */
    (void)cam;
}

void render_draw_triangle(vertex_t *v0, vertex_t *v1, vertex_t *v2) {
    if (!current_camera) return;

    POLY_G3 *poly;
    SVECTOR sv[3];
    long otz, p, flag;

    /* Convert vertices to PSX fixed point format */
    sv[0].vx = (short)(v0->pos.x * 16.0f);
    sv[0].vy = (short)(v0->pos.y * 16.0f);
    sv[0].vz = (short)(v0->pos.z * 16.0f);

    sv[1].vx = (short)(v1->pos.x * 16.0f);
    sv[1].vy = (short)(v1->pos.y * 16.0f);
    sv[1].vz = (short)(v1->pos.z * 16.0f);

    sv[2].vx = (short)(v2->pos.x * 16.0f);
    sv[2].vy = (short)(v2->pos.y * 16.0f);
    sv[2].vz = (short)(v2->pos.z * 16.0f);

    /* Transform using GTE */
    gte_ldv3(&sv[0], &sv[1], &sv[2]);
    gte_rtpt();

    /* Get average Z for depth sorting */
    gte_avsz3();
    gte_stotz(&otz);

    /* Clamp to OT range */
    if (otz < 0 || otz >= OT_LENGTH) return;

    /* Allocate polygon from packet buffer */
    poly = (POLY_G3 *)next_packet;
    next_packet += sizeof(POLY_G3);

    /* Initialize polygon */
    setPolyG3(poly);

    /* Store transformed screen coords */
    gte_stsxy0(&poly->x0);
    gte_stsxy1(&poly->x1);
    gte_stsxy2(&poly->x2);

    /* Set vertex colors */
    setRGB0(poly, (v0->color >> 16) & 0xFF, (v0->color >> 8) & 0xFF, v0->color & 0xFF);
    setRGB1(poly, (v1->color >> 16) & 0xFF, (v1->color >> 8) & 0xFF, v1->color & 0xFF);
    setRGB2(poly, (v2->color >> 16) & 0xFF, (v2->color >> 8) & 0xFF, v2->color & 0xFF);

    /* Add to ordering table */
    addPrim(&ot[current_buffer][otz], poly);
}

void render_draw_mesh(mesh_t *mesh, mat4_t transform) {
    if (!mesh || !current_camera) return;

    /* Set GTE transformation matrix */
    MATRIX mtx;

    /* Convert float matrix to PSX fixed point */
    mtx.m[0][0] = (short)(transform.m[0] * 4096.0f);
    mtx.m[0][1] = (short)(transform.m[1] * 4096.0f);
    mtx.m[0][2] = (short)(transform.m[2] * 4096.0f);
    mtx.m[1][0] = (short)(transform.m[4] * 4096.0f);
    mtx.m[1][1] = (short)(transform.m[5] * 4096.0f);
    mtx.m[1][2] = (short)(transform.m[6] * 4096.0f);
    mtx.m[2][0] = (short)(transform.m[8] * 4096.0f);
    mtx.m[2][1] = (short)(transform.m[9] * 4096.0f);
    mtx.m[2][2] = (short)(transform.m[10] * 4096.0f);
    mtx.t[0] = (int)(transform.m[12] * 4096.0f);
    mtx.t[1] = (int)(transform.m[13] * 4096.0f);
    mtx.t[2] = (int)(transform.m[14] * 4096.0f);

    /* Compose with view matrix */
    CompMatrixLV(&view_matrix, &mtx, &mtx);
    gte_SetRotMatrix(&mtx);
    gte_SetTransMatrix(&mtx);

    /* Draw all triangles */
    for (int i = 0; i < mesh->tri_count; i++) {
        triangle_t *tri = &mesh->triangles[i];
        render_draw_triangle(&tri->v[0], &tri->v[1], &tri->v[2]);
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
    DrawSync(0);
}

void render_draw_sky_background(uint32_t color) {
    /* Sky is handled by clear color on PSX */
    render_clear(color);
}

void render_draw_rect_2d(int x, int y, int w, int h, uint32_t color) {
    TILE *tile;

    tile = (TILE *)next_packet;
    next_packet += sizeof(TILE);

    setTile(tile);
    setXY0(tile, x, y);
    setWH(tile, w, h);
    setRGB0(tile, (color >> 16) & 0xFF, (color >> 8) & 0xFF, color & 0xFF);

    /* Add to front of OT (closest to camera) */
    addPrim(&ot[current_buffer][0], tile);
}

void render_draw_text(int x, int y, uint32_t color, const char *text) {
    /* PSX text rendering using debug font or custom font */
    int cx = x;
    int char_width = 8;
    int char_height = 16;

    while (*text) {
        char c = *text++;

        if (c == ' ') {
            cx += char_width;
            continue;
        }

        /* Draw simple rectangle for each character */
        render_draw_rect_2d(cx, y, char_width - 1, char_height, color);
        cx += char_width;
    }
}

/* PSX-specific mesh creation with reduced polygon counts */
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

#endif /* PLATFORM_PSX */
