/*
 * RetroRacer - Super Nintendo (SNES) Rendering Implementation
 * Uses Mode 7 for pseudo-3D racing (F-Zero / Super Mario Kart style)
 *
 * Mode 7 provides hardware affine transformation of a single background
 * layer, allowing rotation and scaling for a pseudo-3D floor effect.
 *
 * Vehicles and objects are rendered as sprites since SNES cannot do
 * arbitrary triangle rendering.
 */

#include "render.h"
#include "platform.h"

#ifdef PLATFORM_SNES

#include <snes.h>

/* SNES screen dimensions */
#define SCREEN_WIDTH 256
#define SCREEN_HEIGHT 224
#define HORIZON_LINE 80

/* Mode 7 registers */
#define REG_M7SEL   0x211A
#define REG_M7A     0x211B
#define REG_M7B     0x211C
#define REG_M7C     0x211D
#define REG_M7D     0x211E
#define REG_M7X     0x211F
#define REG_M7Y     0x2120
#define REG_M7HOFS  0x210D
#define REG_M7VOFS  0x210E

/* Camera state for Mode 7 calculation */
static camera_t *current_camera = NULL;

/* Mode 7 parameters - updated per frame */
static int16_t m7_pos_x = 0;      /* Camera X position (fixed point 8.8) */
static int16_t m7_pos_y = 0;      /* Camera Y position (fixed point 8.8) */
static uint8_t m7_angle = 0;      /* Camera angle (0-255 = 0-360 degrees) */
static int16_t m7_scale = 256;    /* Base scale factor */

/* Sin/Cos lookup table (256 entries, fixed point 8.8) */
static const int16_t sin_table[256] = {
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

#define SIN(a) sin_table[(a) & 0xFF]
#define COS(a) sin_table[((a) + 64) & 0xFF]

/* HDMA table for per-scanline Mode 7 scaling (perspective effect) */
static uint8_t hdma_table[SCREEN_HEIGHT * 4];

/* Sprite tracking for vehicles */
#define MAX_SPRITES 32
static int sprite_count = 0;

/* Mesh types for SNES */
#define MESH_TYPE_VEHICLE 1
#define MESH_TYPE_TRACK   2
#define MESH_TYPE_OTHER   3

/* Extended mesh for SNES with metadata */
typedef struct {
    int mesh_type;
    uint32_t color;
    float width;
    float length;
    int sprite_base;  /* Base sprite tile for vehicles */
} snes_mesh_data_t;

/* Build HDMA table for Mode 7 perspective (closer = more stretched) */
static void build_perspective_table(void) {
    int idx = 0;

    for (int y = HORIZON_LINE; y < SCREEN_HEIGHT; y++) {
        /* Distance from horizon (pseudo-Z) */
        int dist = y - HORIZON_LINE;
        if (dist == 0) dist = 1;

        /* Scale factor increases with distance from horizon */
        /* This creates the perspective floor effect */
        int scale = (256 * 32) / dist;  /* Fixed point scaling */
        if (scale > 0x7FFF) scale = 0x7FFF;

        /* HDMA entry: count, data low, data high */
        hdma_table[idx++] = 1;  /* 1 scanline */
        hdma_table[idx++] = scale & 0xFF;
        hdma_table[idx++] = (scale >> 8) & 0xFF;
    }

    /* End HDMA table */
    hdma_table[idx] = 0;
}

void render_init(void) {
    /* Initialize SNES */
    consoleInit();

    /* Set screen mode - Mode 7 for BG1 (track), Mode 1 for sprites/BG2 */
    setMode(BG_MODE7, 0);

    /* Enable Mode 7 on BG1 */
    REG(REG_M7SEL) = 0x00;  /* No screen flip, no tiling */

    /* Initialize Mode 7 parameters */
    REG(REG_M7A) = 0x01;  /* Scale X = 1.0 */
    REG(REG_M7A) = 0x00;
    REG(REG_M7B) = 0x00;  /* Shear = 0 */
    REG(REG_M7B) = 0x00;
    REG(REG_M7C) = 0x00;  /* Shear = 0 */
    REG(REG_M7C) = 0x00;
    REG(REG_M7D) = 0x01;  /* Scale Y = 1.0 */
    REG(REG_M7D) = 0x00;

    /* Set rotation center to screen center */
    REG(REG_M7X) = (SCREEN_WIDTH / 2) & 0xFF;
    REG(REG_M7X) = (SCREEN_WIDTH / 2) >> 8;
    REG(REG_M7Y) = (SCREEN_HEIGHT / 2) & 0xFF;
    REG(REG_M7Y) = (SCREEN_HEIGHT / 2) >> 8;

    /* Build HDMA table for perspective effect */
    build_perspective_table();

    /* Initialize OAM (sprite) system */
    oamInit();
    oamSetEx(0, OBJ_SMALL, OBJ_SHOW);

    /* Enable screen */
    setScreenOn();
}

void render_begin_frame(void) {
    WaitForVBlank();

    /* Reset sprite count for this frame */
    sprite_count = 0;
    oamClear(0, 0);  /* Clear all sprites */

    /* Update Mode 7 transformation matrix based on camera */
    if (current_camera) {
        /* Convert camera position to Mode 7 space */
        m7_pos_x = (int16_t)(current_camera->position.x * 256);
        m7_pos_y = (int16_t)(current_camera->position.z * 256);

        /* Convert camera rotation to 0-255 angle */
        /* Calculate angle from camera direction */
        float dx = current_camera->target.x - current_camera->position.x;
        float dz = current_camera->target.z - current_camera->position.z;
        float angle_rad = atan2f(dx, dz);
        m7_angle = (uint8_t)(angle_rad * 256.0f / (2.0f * 3.14159f));
    }

    /* Calculate Mode 7 matrix from angle */
    int16_t cos_a = COS(m7_angle);
    int16_t sin_a = SIN(m7_angle);

    /* Set Mode 7 matrix (rotation + scale) */
    int16_t m7a = (cos_a * m7_scale) >> 8;
    int16_t m7b = (sin_a * m7_scale) >> 8;
    int16_t m7c = (-sin_a * m7_scale) >> 8;
    int16_t m7d = (cos_a * m7_scale) >> 8;

    REG(REG_M7A) = m7a & 0xFF;
    REG(REG_M7A) = (m7a >> 8) & 0xFF;
    REG(REG_M7B) = m7b & 0xFF;
    REG(REG_M7B) = (m7b >> 8) & 0xFF;
    REG(REG_M7C) = m7c & 0xFF;
    REG(REG_M7C) = (m7c >> 8) & 0xFF;
    REG(REG_M7D) = m7d & 0xFF;
    REG(REG_M7D) = (m7d >> 8) & 0xFF;

    /* Set scroll position (camera position in Mode 7 space) */
    REG(REG_M7HOFS) = m7_pos_x & 0xFF;
    REG(REG_M7HOFS) = (m7_pos_x >> 8) & 0xFF;
    REG(REG_M7VOFS) = m7_pos_y & 0xFF;
    REG(REG_M7VOFS) = (m7_pos_y >> 8) & 0xFF;
}

void render_end_frame(void) {
    /* Copy OAM data to PPU */
    oamUpdate();
}

void render_begin_hud(void) {
    /* HUD uses BG2/BG3 text layer on SNES */
}

void render_end_hud(void) {
    /* Nothing needed */
}

void render_clear(uint32_t color) {
    /* Background color is set via CGRAM */
    uint8_t r = ((color >> 16) & 0xFF) >> 3;  /* 5-bit */
    uint8_t g = ((color >> 8) & 0xFF) >> 3;
    uint8_t b = (color & 0xFF) >> 3;

    uint16_t snes_color = r | (g << 5) | (b << 10);

    /* Set backdrop color (palette entry 0) */
    REG(0x2121) = 0;  /* CGADD */
    REG(0x2122) = snes_color & 0xFF;  /* CGDATA low */
    REG(0x2122) = (snes_color >> 8) & 0xFF;  /* CGDATA high */
}

void render_set_camera(camera_t *cam) {
    current_camera = cam;
}

void camera_update(camera_t *cam) {
    /* Store rotation for Mode 7 */
    cam->view_matrix = mat4_look_at(cam->position, cam->target, cam->up);
    cam->proj_matrix = mat4_identity();  /* Mode 7 handles projection */
}

/*
 * Project a 3D world position to SNES screen coordinates
 * Returns 0 if behind camera, 1 if visible
 */
static int project_to_screen(vec3_t world_pos, int *screen_x, int *screen_y, int *depth) {
    if (!current_camera) return 0;

    /* Transform to camera space */
    vec3_t rel;
    rel.x = world_pos.x - current_camera->position.x;
    rel.y = world_pos.y - current_camera->position.y;
    rel.z = world_pos.z - current_camera->position.z;

    /* Rotate by camera angle (simplified 2D rotation for Mode 7 style) */
    float angle_rad = (float)m7_angle * (2.0f * 3.14159f) / 256.0f;
    float cos_a = cosf(angle_rad);
    float sin_a = sinf(angle_rad);

    float cam_x = rel.x * cos_a - rel.z * sin_a;
    float cam_z = rel.x * sin_a + rel.z * cos_a;

    /* Check if in front of camera */
    if (cam_z < 1.0f) return 0;

    /* Simple perspective projection */
    float inv_z = 128.0f / cam_z;  /* FOV factor */

    *screen_x = (int)(SCREEN_WIDTH / 2 + cam_x * inv_z);
    *screen_y = (int)(HORIZON_LINE + (current_camera->position.y - world_pos.y + 2.0f) * inv_z);
    *depth = (int)(cam_z * 4);  /* For sprite priority */

    /* Clamp to screen bounds check */
    if (*screen_x < -32 || *screen_x > SCREEN_WIDTH + 32) return 0;
    if (*screen_y < -32 || *screen_y > SCREEN_HEIGHT + 32) return 0;

    return 1;
}

/*
 * On SNES, triangles cannot be directly rendered.
 * This function projects the triangle center and draws a sprite if appropriate.
 */
void render_draw_triangle(vertex_t *v0, vertex_t *v1, vertex_t *v2) {
    /* SNES cannot render arbitrary triangles - this is called by the grass
     * rendering in track.c. We handle the ground via Mode 7 instead.
     * Individual triangles are ignored - the game still runs, just uses
     * Mode 7 for the ground instead of triangle-based grass. */
    (void)v0; (void)v1; (void)v2;
}

/*
 * Draw a mesh as a sprite on SNES
 * Meshes are converted to sprites with distance-based scaling
 */
void render_draw_mesh(mesh_t *mesh, mat4_t transform) {
    if (!mesh || !current_camera) return;
    if (sprite_count >= MAX_SPRITES) return;

    /* Get mesh metadata */
    snes_mesh_data_t *data = (snes_mesh_data_t *)mesh->triangles;
    if (!data) return;

    /* Extract position from transform matrix */
    vec3_t world_pos;
    world_pos.x = transform.m[12];
    world_pos.y = transform.m[13];
    world_pos.z = transform.m[14];

    /* Project to screen */
    int screen_x, screen_y, depth;
    if (!project_to_screen(world_pos, &screen_x, &screen_y, &depth)) return;

    /* Select sprite size based on distance */
    int sprite_size;
    int tile_offset;

    if (depth < 16) {
        /* Close - large sprite (32x32) */
        sprite_size = OBJ_SIZE32_64;
        tile_offset = 0;
        screen_x -= 16;
        screen_y -= 16;
    } else if (depth < 48) {
        /* Medium distance - medium sprite (16x16) */
        sprite_size = OBJ_SIZE16_32;
        tile_offset = 32;  /* Different tiles for smaller version */
        screen_x -= 8;
        screen_y -= 8;
    } else {
        /* Far - small sprite (8x8) */
        sprite_size = OBJ_SIZE8_16;
        tile_offset = 64;
        screen_x -= 4;
        screen_y -= 4;
    }

    /* Calculate sprite palette from mesh color */
    int palette = 0;
    uint8_t r = (data->color >> 16) & 0xFF;
    uint8_t g = (data->color >> 8) & 0xFF;
    uint8_t b = data->color & 0xFF;

    /* Simple palette selection based on dominant color */
    if (r > g && r > b) palette = 1;       /* Red palette */
    else if (g > r && g > b) palette = 2;  /* Green palette */
    else if (b > r && b > g) palette = 3;  /* Blue palette */
    else palette = 0;                       /* Default/yellow palette */

    /* Calculate sprite priority (lower depth = higher priority) */
    int priority = 3;  /* Default high priority */
    if (depth > 32) priority = 2;
    if (depth > 64) priority = 1;

    /* Draw sprite */
    int sprite_id = sprite_count++;
    int tile = data->sprite_base + tile_offset;

    oamSet(sprite_id, screen_x, screen_y, priority, 0, 0, tile, palette);
    oamSetEx(sprite_id, sprite_size, OBJ_SHOW);
}

void render_draw_quad(vec3_t pos, float width, float height, uint32_t color) {
    /* Quads used for start/finish line - draw as sprite */
    if (!current_camera) return;
    if (sprite_count >= MAX_SPRITES) return;

    int screen_x, screen_y, depth;
    if (!project_to_screen(pos, &screen_x, &screen_y, &depth)) return;

    /* Draw a simple horizontal line sprite */
    int sprite_id = sprite_count++;
    oamSet(sprite_id, screen_x - 16, screen_y, 2, 0, 0, 96, 0);  /* White tile */
    oamSetEx(sprite_id, OBJ_SIZE32_64, OBJ_SHOW);

    (void)width; (void)height; (void)color;
}

void render_wait_vram_ready(void) {
    WaitForVBlank();
}

void render_draw_sky_background(uint32_t color) {
    /* Sky is the backdrop color above the Mode 7 horizon */
    render_clear(color);
}

void render_draw_rect_2d(int x, int y, int w, int h, uint32_t color) {
    /* 2D rectangles for HUD - use sprites or just skip on SNES */
    /* For now, minimal implementation using a sprite */
    if (sprite_count >= MAX_SPRITES) return;

    int sprite_id = sprite_count++;
    int palette = 0;

    /* Simple color to palette mapping */
    uint8_t r = (color >> 16) & 0xFF;
    if (r > 200) palette = 1;  /* Bright = white/yellow */

    /* Draw sprite at position (limited size) */
    oamSet(sprite_id, x, y, 3, 0, 0, 100, palette);
    oamSetEx(sprite_id, OBJ_SIZE8_16, OBJ_SHOW);

    (void)w; (void)h;
}

void render_draw_text(int x, int y, uint32_t color, const char *text) {
    /* Use PVSnesLib's console text function */
    int tile_x = x / 8;
    int tile_y = y / 8;

    if (tile_x >= 0 && tile_x < 32 && tile_y >= 0 && tile_y < 28) {
        consoleDrawText(tile_x, tile_y, "%s", text);
    }

    (void)color;
}

/*
 * SNES mesh creation - returns a mesh_t with embedded SNES-specific data
 * Instead of actual triangles, we store sprite metadata
 */
mesh_t *mesh_create_cube(float size, uint32_t color) {
    mesh_t *mesh = (mesh_t *)malloc(sizeof(mesh_t));
    snes_mesh_data_t *data = (snes_mesh_data_t *)malloc(sizeof(snes_mesh_data_t));

    data->mesh_type = MESH_TYPE_OTHER;
    data->color = color;
    data->width = size;
    data->length = size;
    data->sprite_base = 0;  /* Default sprite tiles */

    mesh->triangles = (triangle_t *)data;  /* Store our data in triangles pointer */
    mesh->tri_count = 0;  /* No actual triangles */
    mesh->base_color = color;

    return mesh;
}

mesh_t *mesh_create_vehicle(uint32_t color) {
    mesh_t *mesh = (mesh_t *)malloc(sizeof(mesh_t));
    snes_mesh_data_t *data = (snes_mesh_data_t *)malloc(sizeof(snes_mesh_data_t));

    data->mesh_type = MESH_TYPE_VEHICLE;
    data->color = color;
    data->width = 0.8f;
    data->length = 1.5f;
    data->sprite_base = 0;  /* Vehicle sprite tiles start at 0 */

    mesh->triangles = (triangle_t *)data;
    mesh->tri_count = 0;
    mesh->base_color = color;

    return mesh;
}

mesh_t *mesh_create_track_segment(float width, float length, uint32_t color) {
    /* Track segments are handled by Mode 7, but we still need a valid mesh
     * for the track rendering code to not crash */
    mesh_t *mesh = (mesh_t *)malloc(sizeof(mesh_t));
    snes_mesh_data_t *data = (snes_mesh_data_t *)malloc(sizeof(snes_mesh_data_t));

    data->mesh_type = MESH_TYPE_TRACK;
    data->color = color;
    data->width = width;
    data->length = length;
    data->sprite_base = 80;  /* Track marker sprites */

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

#endif /* PLATFORM_SNES */
