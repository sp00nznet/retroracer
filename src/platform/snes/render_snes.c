/*
 * RetroRacer - Super Nintendo (SNES) Rendering Implementation
 * Uses Mode 7 for pseudo-3D racing (F-Zero / Super Mario Kart style)
 *
 * Mode 7 provides hardware affine transformation of a single background
 * layer, allowing rotation and scaling for a pseudo-3D floor effect.
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

/* Sin/Cos lookup table (256 entries, fixed point) */
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

void render_init(void) {
    /* Initialize SNES */
    consoleInit();

    /* Set screen mode - Mode 7 */
    setMode(BG_MODE7, 0);

    /* Enable Mode 7 on BG1 */
    REG(REG_M7SEL) = 0x00;  /* No screen flip, no tiling */

    /* Set up HDMA for per-scanline perspective scaling */
    /* Channel 0: Update M7A and M7D (scaling) per scanline */

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

    /* Enable screen */
    setScreenOn();
}

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

void render_begin_frame(void) {
    WaitForVBlank();

    /* Update Mode 7 transformation matrix based on camera */
    if (current_camera) {
        /* Convert camera position to Mode 7 space */
        m7_pos_x = (int16_t)(current_camera->position.x * 256);
        m7_pos_y = (int16_t)(current_camera->position.z * 256);

        /* Convert camera rotation to 0-255 angle */
        m7_angle = (uint8_t)(current_camera->rotation_y * 256 / (2 * 3.14159f));
    }

    /* Calculate Mode 7 matrix from angle */
    int16_t cos_a = COS(m7_angle);
    int16_t sin_a = SIN(m7_angle);

    /* Set Mode 7 matrix (rotation + scale) */
    /* A = cos(angle) * scale */
    /* B = sin(angle) * scale */
    /* C = -sin(angle) * scale */
    /* D = cos(angle) * scale */

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
    /* Frame complete - wait for VBlank handled in begin_frame */
}

void render_begin_hud(void) {
    /* HUD uses sprites on SNES */
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
    /* Note: Mode 7 is 2D transformation, so we only use Y rotation */
    cam->view_matrix = mat4_look_at(cam->position, cam->target, cam->up);
    cam->proj_matrix = mat4_identity();  /* Mode 7 handles projection */
}

/*
 * On SNES, 3D geometry is not directly supported.
 * Vehicles and objects are rendered as sprites.
 * The track is a Mode 7 transformed tilemap.
 */

void render_draw_triangle(vertex_t *v0, vertex_t *v1, vertex_t *v2) {
    /* SNES cannot render arbitrary triangles */
    /* This would need to be pre-rendered sprites */
    (void)v0; (void)v1; (void)v2;
}

void render_draw_mesh(mesh_t *mesh, mat4_t transform) {
    /* Meshes not supported - use sprites instead */
    (void)mesh; (void)transform;
}

void render_draw_quad(vec3_t pos, float width, float height, uint32_t color) {
    /* Could be implemented as a sprite */
    (void)pos; (void)width; (void)height; (void)color;
}

void render_wait_vram_ready(void) {
    WaitForVBlank();
}

void render_draw_sky_background(uint32_t color) {
    /* Sky is drawn above the horizon line using BG2/3 */
    /* Or simply the backdrop color */
    render_clear(color);
}

void render_draw_rect_2d(int x, int y, int w, int h, uint32_t color) {
    /* 2D rectangles done via sprites or tile manipulation */
    (void)x; (void)y; (void)w; (void)h; (void)color;
}

void render_draw_text(int x, int y, uint32_t color, const char *text) {
    /* Use a tilemap layer for text, or sprites */
    /* PVSnesLib has consoleDrawText() */
    consoleDrawText(x / 8, y / 8, text);
    (void)color;
}

/*
 * SNES-specific sprite rendering for vehicles
 */
void render_draw_sprite(int x, int y, int sprite_id, int palette, int flip_h, int flip_v) {
    /* OAM (Object Attribute Memory) sprite entry */
    /* X position, Y position, tile number, attributes */

    uint8_t attr = palette << 1;
    if (flip_h) attr |= 0x40;
    if (flip_v) attr |= 0x80;

    oamSet(sprite_id, x, y, 0, flip_h, flip_v, sprite_id, palette);
}

/*
 * Draw vehicle as scaled sprite (SNES limitation - no true 3D)
 * Uses multiple sprite sizes for distance-based scaling
 */
void render_draw_vehicle_sprite(int screen_x, int screen_y, int distance, int vehicle_type, int angle) {
    /* Select sprite size based on distance */
    int sprite_size;
    int sprite_base;

    if (distance < 32) {
        sprite_size = OBJ_SIZE32_64;
        sprite_base = vehicle_type * 16;  /* Large sprites */
    } else if (distance < 64) {
        sprite_size = OBJ_SIZE16_32;
        sprite_base = vehicle_type * 16 + 256;  /* Medium sprites */
    } else {
        sprite_size = OBJ_SIZE8_16;
        sprite_base = vehicle_type * 16 + 512;  /* Small sprites */
    }

    /* Select frame based on view angle (8 or 16 angles) */
    int frame = (angle >> 5) & 0x07;

    oamSet(0, screen_x - 16, screen_y - 16, 3, 0, 0, sprite_base + frame, 0);
}

/*
 * Mesh creation stubs - SNES uses pre-made sprite graphics
 */
mesh_t *mesh_create_cube(float size, uint32_t color) {
    (void)size; (void)color;
    return NULL;  /* Not applicable for SNES */
}

mesh_t *mesh_create_vehicle(uint32_t color) {
    (void)color;
    return NULL;  /* Vehicles are sprites on SNES */
}

mesh_t *mesh_create_track_segment(float width, float length, uint32_t color) {
    (void)width; (void)length; (void)color;
    return NULL;  /* Track is Mode 7 tilemap */
}

void mesh_destroy(mesh_t *mesh) {
    (void)mesh;
}

#endif /* PLATFORM_SNES */
