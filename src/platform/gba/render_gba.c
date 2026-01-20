/*
 * RetroRacer - Game Boy Advance Rendering Implementation
 * Uses libgba/devkitARM with Mode 7-style affine background
 *
 * GBA has affine backgrounds (similar to SNES Mode 7) for pseudo-3D
 * Resolution: 240x160, 15-bit color
 */

#include "render.h"
#include "platform.h"

#ifdef PLATFORM_GBA

#include <gba_console.h>
#include <gba_video.h>
#include <gba_interrupt.h>
#include <gba_systemcalls.h>
#include <gba_sprites.h>
#include <gba_dma.h>
#include <stdlib.h>
#include <string.h>

/* Screen dimensions */
#define SCREEN_WIDTH 240
#define SCREEN_HEIGHT 160
#define HORIZON_LINE 50

/* GBA display registers */
#define REG_DISPCNT     (*(volatile u16*)0x04000000)
#define REG_BG2CNT      (*(volatile u16*)0x0400000C)
#define REG_BG2PA       (*(volatile s16*)0x04000020)
#define REG_BG2PB       (*(volatile s16*)0x04000022)
#define REG_BG2PC       (*(volatile s16*)0x04000024)
#define REG_BG2PD       (*(volatile s16*)0x04000026)
#define REG_BG2X        (*(volatile s32*)0x04000028)
#define REG_BG2Y        (*(volatile s32*)0x0400002C)

#define MODE_1          0x0001  /* Mode 1 - tiled + affine BG2 */
#define BG2_ENABLE      0x0400
#define OBJ_ENABLE      0x1000
#define OBJ_1D_MAP      0x0040

/* OAM (sprite) memory */
#define OAM             ((volatile OBJ_ATTR*)0x07000000)
#define OAM_COUNT       128

/* Palette memory */
#define BG_PALETTE      ((volatile u16*)0x05000000)
#define OBJ_PALETTE     ((volatile u16*)0x05000200)

/* VRAM */
#define VRAM            ((volatile u16*)0x06000000)

typedef struct {
    u16 attr0;
    u16 attr1;
    u16 attr2;
    s16 fill;
} OBJ_ATTR;

static camera_t *current_camera = NULL;

/* Mode 7 parameters */
static s16 m7_angle = 0;
static s32 m7_pos_x = 0;
static s32 m7_pos_y = 0;

/* Sin/cos table (8.8 fixed point, 256 entries) */
static const s16 sin_lut[256] = {
    0, 6, 12, 18, 25, 31, 37, 43, 49, 56, 62, 68, 74, 80, 86, 92,
    97, 103, 109, 115, 120, 126, 131, 136, 142, 147, 152, 157, 162, 167, 171, 176,
    181, 185, 189, 193, 197, 201, 205, 209, 212, 216, 219, 222, 225, 228, 231, 234,
    236, 238, 241, 243, 244, 246, 248, 249, 251, 252, 253, 254, 254, 255, 255, 255,
    256, 255, 255, 255, 254, 254, 253, 252, 251, 249, 248, 246, 244, 243, 241, 238,
    236, 234, 231, 228, 225, 222, 219, 216, 212, 209, 205, 201, 197, 193, 189, 185,
    181, 176, 171, 167, 162, 157, 152, 147, 142, 136, 131, 126, 120, 115, 109, 103,
    97, 92, 86, 80, 74, 68, 62, 56, 49, 43, 37, 31, 25, 18, 12, 6,
    0, -6, -12, -18, -25, -31, -37, -43, -49, -56, -62, -68, -74, -80, -86, -92,
    -97, -103, -109, -115, -120, -126, -131, -136, -142, -147, -152, -157, -162, -167, -171, -176,
    -181, -185, -189, -193, -197, -201, -205, -209, -212, -216, -219, -222, -225, -228, -231, -234,
    -236, -238, -241, -243, -244, -246, -248, -249, -251, -252, -253, -254, -254, -255, -255, -255,
    -256, -255, -255, -255, -254, -254, -253, -252, -251, -249, -248, -246, -244, -243, -241, -238,
    -236, -234, -231, -228, -225, -222, -219, -216, -212, -209, -205, -201, -197, -193, -189, -185,
    -181, -176, -171, -167, -162, -157, -152, -147, -142, -136, -131, -126, -120, -115, -109, -103,
    -97, -92, -86, -80, -74, -68, -62, -56, -49, -43, -37, -31, -25, -18, -12, -6
};

#define SIN(a) sin_lut[(a) & 0xFF]
#define COS(a) sin_lut[((a) + 64) & 0xFF]

/* Sprite tracking */
static int sprite_count = 0;

/* Mesh type identifiers */
#define MESH_TYPE_VEHICLE 1
#define MESH_TYPE_TRACK   2
#define MESH_TYPE_OTHER   3

typedef struct {
    int mesh_type;
    u32 color;
    u16 tile_id;
    u8 palette;
} gba_mesh_data_t;

/* HBlank interrupt handler for per-scanline Mode 7 scaling */
static s16 hscroll_params[SCREEN_HEIGHT][4];  /* pa, pb, pc, pd per line */

static void hblank_handler(void) {
    int line = REG_VCOUNT;

    if (line >= HORIZON_LINE && line < SCREEN_HEIGHT) {
        REG_BG2PA = hscroll_params[line][0];
        REG_BG2PB = hscroll_params[line][1];
        REG_BG2PC = hscroll_params[line][2];
        REG_BG2PD = hscroll_params[line][3];
    }
}

void render_init(void) {
    /* Set video mode: Mode 1 with BG2 (affine) and sprites */
    REG_DISPCNT = MODE_1 | BG2_ENABLE | OBJ_ENABLE | OBJ_1D_MAP;

    /* Setup BG2 as 256x256 affine background */
    REG_BG2CNT = 0x0080;  /* 256x256, 8bpp */

    /* Initialize affine matrix to identity */
    REG_BG2PA = 256;  /* 1.0 in 8.8 fixed point */
    REG_BG2PB = 0;
    REG_BG2PC = 0;
    REG_BG2PD = 256;
    REG_BG2X = 0;
    REG_BG2Y = 0;

    /* Set background color (sky blue) */
    BG_PALETTE[0] = RGB5(17, 25, 29);  /* Sky blue */

    /* Setup sprite palettes */
    OBJ_PALETTE[1] = RGB5(31, 0, 0);   /* Red */
    OBJ_PALETTE[17] = RGB5(0, 31, 0);  /* Green */
    OBJ_PALETTE[33] = RGB5(0, 0, 31);  /* Blue */
    OBJ_PALETTE[49] = RGB5(31, 31, 0); /* Yellow */

    /* Enable HBlank interrupt for Mode 7 effect */
    irqInit();
    irqSet(IRQ_HBLANK, hblank_handler);
    irqEnable(IRQ_HBLANK);

    /* Hide all sprites initially */
    for (int i = 0; i < OAM_COUNT; i++) {
        OAM[i].attr0 = 0x0200;  /* Hide sprite */
    }

    /* Build perspective table */
    for (int y = 0; y < SCREEN_HEIGHT; y++) {
        hscroll_params[y][0] = 256;
        hscroll_params[y][1] = 0;
        hscroll_params[y][2] = 0;
        hscroll_params[y][3] = 256;
    }
}

void render_begin_frame(void) {
    VBlankIntrWait();

    /* Reset sprite count */
    sprite_count = 0;

    /* Update Mode 7 parameters from camera */
    if (current_camera) {
        m7_pos_x = (s32)(current_camera->position.x * 256);
        m7_pos_y = (s32)(current_camera->position.z * 256);

        /* Calculate angle from camera direction */
        float dx = current_camera->target.x - current_camera->position.x;
        float dz = current_camera->target.z - current_camera->position.z;
        float angle = atan2f(dx, dz);
        m7_angle = (s16)(angle * 256.0f / (2.0f * 3.14159f));
    }

    /* Build per-scanline perspective table */
    s16 cos_a = COS(m7_angle);
    s16 sin_a = SIN(m7_angle);

    for (int y = HORIZON_LINE; y < SCREEN_HEIGHT; y++) {
        int dist = y - HORIZON_LINE;
        if (dist == 0) dist = 1;

        /* Scale factor increases with distance from horizon */
        s32 scale = (256 * 32) / dist;
        if (scale > 0x7FFF) scale = 0x7FFF;

        /* Calculate affine matrix for this scanline */
        hscroll_params[y][0] = (cos_a * scale) >> 8;   /* PA */
        hscroll_params[y][1] = (sin_a * scale) >> 8;   /* PB */
        hscroll_params[y][2] = (-sin_a * scale) >> 8;  /* PC */
        hscroll_params[y][3] = (cos_a * scale) >> 8;   /* PD */
    }

    /* Update BG2 scroll position */
    REG_BG2X = m7_pos_x;
    REG_BG2Y = m7_pos_y;
}

void render_end_frame(void) {
    /* Hide unused sprites */
    for (int i = sprite_count; i < OAM_COUNT; i++) {
        OAM[i].attr0 = 0x0200;
    }
}

void render_begin_hud(void) {
    /* HUD uses high-priority sprites */
}

void render_end_hud(void) {
    /* Nothing special */
}

void render_clear(u32 color) {
    u8 r = ((color >> 16) & 0xFF) >> 3;
    u8 g = ((color >> 8) & 0xFF) >> 3;
    u8 b = (color & 0xFF) >> 3;

    BG_PALETTE[0] = RGB5(r, g, b);
}

void render_set_camera(camera_t *cam) {
    current_camera = cam;
}

void camera_update(camera_t *cam) {
    cam->view_matrix = mat4_look_at(cam->position, cam->target, cam->up);
    cam->proj_matrix = mat4_identity();
}

/* Project 3D position to screen */
static int project_to_screen(vec3_t pos, int *sx, int *sy, int *scale) {
    if (!current_camera) return 0;

    float rel_x = pos.x - current_camera->position.x;
    float rel_y = pos.y - current_camera->position.y;
    float rel_z = pos.z - current_camera->position.z;

    float angle = (float)m7_angle * (2.0f * 3.14159f) / 256.0f;
    float cos_a = cosf(angle);
    float sin_a = sinf(angle);

    float cam_x = rel_x * cos_a - rel_z * sin_a;
    float cam_z = rel_x * sin_a + rel_z * cos_a;

    if (cam_z < 1.0f) return 0;

    float inv_z = 80.0f / cam_z;

    *sx = (int)(SCREEN_WIDTH / 2 + cam_x * inv_z);
    *sy = (int)(HORIZON_LINE + (1.5f - rel_y) * inv_z);
    *scale = (int)(inv_z * 2);

    if (*sx < -32 || *sx > SCREEN_WIDTH + 32) return 0;
    if (*sy < -32 || *sy > SCREEN_HEIGHT + 16) return 0;

    return 1;
}

void render_draw_triangle(vertex_t *v0, vertex_t *v1, vertex_t *v2) {
    /* GBA cannot render arbitrary triangles - ground uses Mode 7 */
    (void)v0; (void)v1; (void)v2;
}

void render_draw_mesh(mesh_t *mesh, mat4_t transform) {
    if (!mesh || !current_camera) return;
    if (sprite_count >= OAM_COUNT) return;

    gba_mesh_data_t *data = (gba_mesh_data_t *)mesh->triangles;
    if (!data) return;

    /* Get world position */
    vec3_t world_pos;
    world_pos.x = transform.m[12];
    world_pos.y = transform.m[13];
    world_pos.z = transform.m[14];

    int sx, sy, scale;
    if (!project_to_screen(world_pos, &sx, &sy, &scale)) return;

    /* Determine sprite size based on distance */
    u16 size_bits;
    int offset;

    if (scale > 24) {
        /* Large: 32x32 */
        size_bits = 0x8000;  /* Size 2 */
        offset = 16;
    } else if (scale > 12) {
        /* Medium: 16x16 */
        size_bits = 0x4000;  /* Size 1 */
        offset = 8;
    } else {
        /* Small: 8x8 */
        size_bits = 0x0000;  /* Size 0 */
        offset = 4;
    }

    /* Determine palette */
    int palette = 0;
    u8 r = (data->color >> 16) & 0xFF;
    u8 g = (data->color >> 8) & 0xFF;
    u8 b = data->color & 0xFF;

    if (r > g && r > b) palette = 1;
    else if (g > r && g > b) palette = 2;
    else if (b > r && b > g) palette = 3;

    /* Set sprite attributes */
    int id = sprite_count++;

    OAM[id].attr0 = ((sy - offset) & 0xFF) | size_bits;
    OAM[id].attr1 = ((sx - offset) & 0x1FF);
    OAM[id].attr2 = data->tile_id | (palette << 12);
}

void render_draw_quad(vec3_t pos, float width, float height, u32 color) {
    if (sprite_count >= OAM_COUNT) return;

    int sx, sy, scale;
    if (!project_to_screen(pos, &sx, &sy, &scale)) return;

    int id = sprite_count++;
    OAM[id].attr0 = ((sy - 4) & 0xFF) | 0x4000;
    OAM[id].attr1 = ((sx - 8) & 0x1FF);
    OAM[id].attr2 = 32;  /* Some tile for markers */

    (void)width; (void)height; (void)color;
}

void render_wait_vram_ready(void) {
    VBlankIntrWait();
}

void render_draw_sky_background(u32 color) {
    render_clear(color);
}

void render_draw_rect_2d(int x, int y, int w, int h, u32 color) {
    if (sprite_count >= OAM_COUNT) return;

    int id = sprite_count++;
    OAM[id].attr0 = (y & 0xFF) | 0x0000;
    OAM[id].attr1 = (x & 0x1FF);
    OAM[id].attr2 = 48;  /* Solid tile */

    (void)w; (void)h; (void)color;
}

void render_draw_text(int x, int y, u32 color, const char *text) {
    /* Use simple sprite-based text */
    int cx = x;

    while (*text && sprite_count < OAM_COUNT) {
        char c = *text++;

        if (c == ' ') {
            cx += 8;
            continue;
        }

        int id = sprite_count++;
        OAM[id].attr0 = (y & 0xFF);
        OAM[id].attr1 = (cx & 0x1FF);
        OAM[id].attr2 = 64 + (c - 'A');  /* Assumes font tiles at 64+ */

        cx += 8;
    }

    (void)color;
}

mesh_t *mesh_create_cube(float size, u32 color) {
    mesh_t *mesh = (mesh_t *)malloc(sizeof(mesh_t));
    gba_mesh_data_t *data = (gba_mesh_data_t *)malloc(sizeof(gba_mesh_data_t));

    data->mesh_type = MESH_TYPE_OTHER;
    data->color = color;
    data->tile_id = 0;
    data->palette = 0;

    mesh->triangles = (triangle_t *)data;
    mesh->tri_count = 0;
    mesh->base_color = color;

    return mesh;
}

mesh_t *mesh_create_vehicle(u32 color) {
    mesh_t *mesh = (mesh_t *)malloc(sizeof(mesh_t));
    gba_mesh_data_t *data = (gba_mesh_data_t *)malloc(sizeof(gba_mesh_data_t));

    data->mesh_type = MESH_TYPE_VEHICLE;
    data->color = color;
    data->tile_id = 0;  /* Vehicle sprite tiles */
    data->palette = 0;

    mesh->triangles = (triangle_t *)data;
    mesh->tri_count = 0;
    mesh->base_color = color;

    return mesh;
}

mesh_t *mesh_create_track_segment(float width, float length, u32 color) {
    mesh_t *mesh = (mesh_t *)malloc(sizeof(mesh_t));
    gba_mesh_data_t *data = (gba_mesh_data_t *)malloc(sizeof(gba_mesh_data_t));

    data->mesh_type = MESH_TYPE_TRACK;
    data->color = color;
    data->tile_id = 32;  /* Track marker tiles */
    data->palette = 0;

    mesh->triangles = (triangle_t *)data;
    mesh->tri_count = 0;
    mesh->base_color = color;

    return mesh;
}

void mesh_destroy(mesh_t *mesh) {
    if (mesh) {
        if (mesh->triangles) free(mesh->triangles);
        free(mesh);
    }
}

#endif /* PLATFORM_GBA */
