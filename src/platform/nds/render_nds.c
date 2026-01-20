/*
 * RetroRacer - Nintendo DS Rendering Implementation
 * Uses libnds with DS 3D hardware
 *
 * The Nintendo DS has actual 3D hardware! It can render ~6144 vertices per frame
 * with texture mapping, lighting, and fog.
 *
 * Main screen: 256x192 with 3D
 * Sub screen: 256x192 (used for HUD/map)
 */

#include "render.h"
#include "platform.h"

#ifdef PLATFORM_NDS

#include <nds.h>
#include <nds/arm9/video.h>
#include <nds/arm9/videoGL.h>
#include <stdlib.h>
#include <string.h>

/* Screen dimensions */
#define SCREEN_WIDTH 256
#define SCREEN_HEIGHT 192

static camera_t *current_camera = NULL;

/* Vertex counter for polygon limits */
static int vertex_count = 0;
#define MAX_VERTICES 6144

void render_init(void) {
    /* Power on 3D hardware */
    powerOn(POWER_ALL_2D | POWER_3D_CORE | POWER_MATRIX);

    /* Setup main screen for 3D */
    videoSetMode(MODE_0_3D);

    /* Setup sub screen for 2D (HUD/map) */
    videoSetModeSub(MODE_0_2D);
    vramSetBankC(VRAM_C_SUB_BG);

    /* Initialize OpenGL-like 3D engine */
    glInit();

    /* Setup viewport */
    glViewport(0, 0, SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1);

    /* Setup projection matrix */
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(70, (float)SCREEN_WIDTH / (float)SCREEN_HEIGHT, 0.1, 100.0);

    /* Switch to modelview */
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    /* Enable backface culling */
    glPolyFmt(POLY_ALPHA(31) | POLY_CULL_BACK);

    /* Clear color: sky blue */
    glClearColor(17, 25, 29, 31);
    glClearPolyID(63);
    glClearDepth(0x7FFF);

    /* Enable textures and antialiasing */
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_ANTIALIAS);
    glEnable(GL_BLEND);

    /* Setup sub screen console for debug text */
    consoleDemoInit();
}

void render_begin_frame(void) {
    /* Reset vertex count */
    vertex_count = 0;

    /* Begin new frame */
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    /* Apply camera view matrix */
    if (current_camera) {
        /* gluLookAt equivalent */
        gluLookAt(
            current_camera->position.x, current_camera->position.y, current_camera->position.z,
            current_camera->target.x, current_camera->target.y, current_camera->target.z,
            current_camera->up.x, current_camera->up.y, current_camera->up.z
        );
    }
}

void render_end_frame(void) {
    /* Flush 3D commands */
    glFlush(0);
}

void render_begin_hud(void) {
    /* Switch to 2D mode for HUD on main screen */
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrthof32(0, SCREEN_WIDTH, SCREEN_HEIGHT, 0, -1, 1);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    /* Disable depth test for HUD */
    glPolyFmt(POLY_ALPHA(31) | POLY_CULL_NONE);
}

void render_end_hud(void) {
    /* Restore 3D matrices */
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix(1);

    glMatrixMode(GL_PROJECTION);
    glPopMatrix(1);

    /* Re-enable culling */
    glPolyFmt(POLY_ALPHA(31) | POLY_CULL_BACK);

    /* Wait for VBlank and swap */
    swiWaitForVBlank();
}

void render_clear(u32 color) {
    u8 r = ((color >> 16) & 0xFF) >> 3;
    u8 g = ((color >> 8) & 0xFF) >> 3;
    u8 b = (color & 0xFF) >> 3;

    glClearColor(r, g, b, 31);
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

/* Convert float to DS fixed point (4.12) */
static inline v16 floatToV16(float f) {
    return (v16)(f * (1 << 12));
}

/* Convert float to DS fixed point (20.12) for positions */
static inline int32 floatToFixed(float f) {
    return (int32)(f * (1 << 12));
}

void render_draw_triangle(vertex_t *v0, vertex_t *v1, vertex_t *v2) {
    if (vertex_count + 3 > MAX_VERTICES) return;
    vertex_count += 3;

    /* Begin triangle */
    glBegin(GL_TRIANGLES);

    /* Vertex 0 */
    u8 r0 = (v0->color >> 16) & 0xFF;
    u8 g0 = (v0->color >> 8) & 0xFF;
    u8 b0 = v0->color & 0xFF;
    glColor3b(r0, g0, b0);
    glVertex3v16(floatToV16(v0->pos.x), floatToV16(v0->pos.y), floatToV16(v0->pos.z));

    /* Vertex 1 */
    u8 r1 = (v1->color >> 16) & 0xFF;
    u8 g1 = (v1->color >> 8) & 0xFF;
    u8 b1 = v1->color & 0xFF;
    glColor3b(r1, g1, b1);
    glVertex3v16(floatToV16(v1->pos.x), floatToV16(v1->pos.y), floatToV16(v1->pos.z));

    /* Vertex 2 */
    u8 r2 = (v2->color >> 16) & 0xFF;
    u8 g2 = (v2->color >> 8) & 0xFF;
    u8 b2 = v2->color & 0xFF;
    glColor3b(r2, g2, b2);
    glVertex3v16(floatToV16(v2->pos.x), floatToV16(v2->pos.y), floatToV16(v2->pos.z));

    glEnd();
}

void render_draw_mesh(mesh_t *mesh, mat4_t transform) {
    if (!mesh || !current_camera) return;

    /* Push matrix and apply transform */
    glPushMatrix();

    /* Apply model transform */
    /* DS uses column-major 4x3 matrices */
    m4x4 ds_matrix;
    ds_matrix.m[0] = floatToFixed(transform.m[0]);
    ds_matrix.m[1] = floatToFixed(transform.m[1]);
    ds_matrix.m[2] = floatToFixed(transform.m[2]);
    ds_matrix.m[3] = floatToFixed(transform.m[3]);
    ds_matrix.m[4] = floatToFixed(transform.m[4]);
    ds_matrix.m[5] = floatToFixed(transform.m[5]);
    ds_matrix.m[6] = floatToFixed(transform.m[6]);
    ds_matrix.m[7] = floatToFixed(transform.m[7]);
    ds_matrix.m[8] = floatToFixed(transform.m[8]);
    ds_matrix.m[9] = floatToFixed(transform.m[9]);
    ds_matrix.m[10] = floatToFixed(transform.m[10]);
    ds_matrix.m[11] = floatToFixed(transform.m[11]);
    ds_matrix.m[12] = floatToFixed(transform.m[12]);
    ds_matrix.m[13] = floatToFixed(transform.m[13]);
    ds_matrix.m[14] = floatToFixed(transform.m[14]);
    ds_matrix.m[15] = floatToFixed(transform.m[15]);

    glMultMatrix4x4(&ds_matrix);

    /* Draw triangles */
    for (int i = 0; i < mesh->tri_count; i++) {
        if (vertex_count + 3 > MAX_VERTICES) break;

        triangle_t *tri = &mesh->triangles[i];

        glBegin(GL_TRIANGLES);

        for (int v = 0; v < 3; v++) {
            u8 r = (tri->v[v].color >> 16) & 0xFF;
            u8 g = (tri->v[v].color >> 8) & 0xFF;
            u8 b = tri->v[v].color & 0xFF;
            glColor3b(r, g, b);
            glVertex3v16(
                floatToV16(tri->v[v].pos.x),
                floatToV16(tri->v[v].pos.y),
                floatToV16(tri->v[v].pos.z)
            );
            vertex_count++;
        }

        glEnd();
    }

    glPopMatrix(1);
}

void render_draw_quad(vec3_t pos, float width, float height, u32 color) {
    if (vertex_count + 6 > MAX_VERTICES) return;

    float hw = width * 0.5f;
    float hh = height * 0.5f;

    u8 r = (color >> 16) & 0xFF;
    u8 g = (color >> 8) & 0xFF;
    u8 b = color & 0xFF;

    glBegin(GL_QUADS);
    glColor3b(r, g, b);
    glVertex3v16(floatToV16(pos.x - hw), floatToV16(pos.y), floatToV16(pos.z - hh));
    glVertex3v16(floatToV16(pos.x + hw), floatToV16(pos.y), floatToV16(pos.z - hh));
    glVertex3v16(floatToV16(pos.x + hw), floatToV16(pos.y), floatToV16(pos.z + hh));
    glVertex3v16(floatToV16(pos.x - hw), floatToV16(pos.y), floatToV16(pos.z + hh));
    glEnd();

    vertex_count += 4;
}

void render_wait_vram_ready(void) {
    swiWaitForVBlank();
}

void render_draw_sky_background(u32 color) {
    render_clear(color);
}

void render_draw_rect_2d(int x, int y, int w, int h, u32 color) {
    if (vertex_count + 4 > MAX_VERTICES) return;

    u8 r = (color >> 16) & 0xFF;
    u8 g = (color >> 8) & 0xFF;
    u8 b = color & 0xFF;

    glBegin(GL_QUADS);
    glColor3b(r, g, b);
    glVertex3v16(floatToV16(x), floatToV16(y), 0);
    glVertex3v16(floatToV16(x + w), floatToV16(y), 0);
    glVertex3v16(floatToV16(x + w), floatToV16(y + h), 0);
    glVertex3v16(floatToV16(x), floatToV16(y + h), 0);
    glEnd();

    vertex_count += 4;
}

void render_draw_text(int x, int y, u32 color, const char *text) {
    /* Use sub screen console for text */
    iprintf("\x1b[%d;%dH%s", y / 8, x / 8, text);
    (void)color;
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

#endif /* PLATFORM_NDS */
