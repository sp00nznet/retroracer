/*
 * RetroRacer - Sega Genesis/Mega Drive Rendering Implementation
 * Uses SGDK with VDP (Video Display Processor)
 *
 * Genesis cannot do true 3D - uses pseudo-3D similar to Road Rash/OutRun
 * with scaled sprites and line-by-line horizontal scrolling for ground
 */

#include "render.h"
#include "platform.h"

#ifdef PLATFORM_GENESIS

#include <genesis.h>

/* Screen dimensions */
#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 224
#define HORIZON_LINE 80

/* Camera state */
static camera_t *current_camera = NULL;

/* Pseudo-3D ground rendering state */
static s16 ground_scroll_x = 0;
static s16 ground_scroll_y = 0;
static u8 ground_angle = 0;

/* Sprite tracking */
#define MAX_SPRITES 80  /* Genesis has 80 sprite limit */
static int sprite_count = 0;

/* Mesh types */
#define MESH_TYPE_VEHICLE 1
#define MESH_TYPE_TRACK   2
#define MESH_TYPE_OTHER   3

/* Genesis mesh metadata */
typedef struct {
    int mesh_type;
    u32 color;
    u16 sprite_def;    /* Sprite definition index */
    s16 width;
    s16 height;
} genesis_mesh_data_t;

/* Horizontal scroll table for pseudo-3D ground effect */
static s16 hscroll_table[SCREEN_HEIGHT];

/* Pre-calculated perspective table for ground rendering */
static void build_perspective_table(void) {
    for (int y = HORIZON_LINE; y < SCREEN_HEIGHT; y++) {
        int dist = y - HORIZON_LINE;
        if (dist == 0) dist = 1;
        /* Scale factor for this scanline */
        hscroll_table[y] = 0;  /* Will be updated per-frame */
    }
    for (int y = 0; y < HORIZON_LINE; y++) {
        hscroll_table[y] = 0;  /* Sky doesn't scroll */
    }
}

void render_init(void) {
    /* Initialize VDP */
    VDP_init();

    /* Set display mode - 320x224, 64 colors */
    VDP_setScreenWidth320();
    VDP_setScreenHeight224();

    /* Set background color (sky blue) */
    PAL_setColor(0, RGB24_TO_VDPCOLOR(0x87CEEB));

    /* Setup plane A for ground (scrolling) */
    VDP_setPlaneSize(64, 32, TRUE);

    /* Setup plane B for sky/background */
    VDP_setPlaneSize(64, 32, FALSE);

    /* Enable horizontal scroll per-line for ground perspective */
    VDP_setScrollingMode(HSCROLL_LINE, VSCROLL_PLANE);

    /* Initialize sprite engine */
    SPR_init();

    /* Build perspective lookup table */
    build_perspective_table();
}

void render_begin_frame(void) {
    /* Wait for VBlank */
    SYS_doVBlankProcess();

    /* Reset sprite count */
    sprite_count = 0;

    /* Update ground scroll based on camera */
    if (current_camera) {
        ground_scroll_x = (s16)(current_camera->position.x * 8);
        ground_scroll_y = (s16)(current_camera->position.z * 8);

        /* Calculate angle from camera direction */
        float dx = current_camera->target.x - current_camera->position.x;
        float dz = current_camera->target.z - current_camera->position.z;
        float angle = atan2f(dx, dz);
        ground_angle = (u8)(angle * 256.0f / (2.0f * 3.14159f));
    }

    /* Update per-line horizontal scroll for road perspective */
    s16 sin_a = sinFix16(ground_angle * 4);  /* SGDK fixed point sin */
    s16 cos_a = cosFix16(ground_angle * 4);

    for (int y = HORIZON_LINE; y < SCREEN_HEIGHT; y++) {
        int dist = y - HORIZON_LINE;
        if (dist == 0) dist = 1;

        /* Road curves based on angle and distance */
        s32 curve = (sin_a * (SCREEN_HEIGHT - y)) >> 8;
        s32 offset = ground_scroll_x + curve;

        hscroll_table[y] = (s16)(-offset);
    }

    /* Upload scroll table */
    VDP_setHorizontalScrollLine(BG_A, 0, hscroll_table, SCREEN_HEIGHT, DMA);
    VDP_setVerticalScroll(BG_A, ground_scroll_y);
}

void render_end_frame(void) {
    /* Update sprites */
    SPR_update();
}

void render_begin_hud(void) {
    /* HUD uses window plane or high-priority sprites */
}

void render_end_hud(void) {
    /* Nothing special needed */
}

void render_clear(u32 color) {
    /* Set background/border color */
    u8 r = (color >> 16) & 0xFF;
    u8 g = (color >> 8) & 0xFF;
    u8 b = color & 0xFF;

    /* Convert to Genesis 9-bit color (3 bits per channel) */
    PAL_setColor(0, RGB24_TO_VDPCOLOR(RGB3_3_3_TO_RGB8_8_8(r >> 5, g >> 5, b >> 5)));
}

void render_set_camera(camera_t *cam) {
    current_camera = cam;
}

void camera_update(camera_t *cam) {
    /* Store matrices for projection calculation */
    cam->view_matrix = mat4_look_at(cam->position, cam->target, cam->up);
    cam->proj_matrix = mat4_identity();  /* Pseudo-3D handles projection */
}

/*
 * Project 3D position to screen coordinates for sprite placement
 */
static int project_to_screen(vec3_t world_pos, s16 *screen_x, s16 *screen_y, s16 *scale) {
    if (!current_camera) return 0;

    /* Transform to camera-relative position */
    float rel_x = world_pos.x - current_camera->position.x;
    float rel_y = world_pos.y - current_camera->position.y;
    float rel_z = world_pos.z - current_camera->position.z;

    /* Rotate by camera angle */
    float angle = (float)ground_angle * (2.0f * 3.14159f) / 256.0f;
    float cos_a = cosf(angle);
    float sin_a = sinf(angle);

    float cam_x = rel_x * cos_a - rel_z * sin_a;
    float cam_z = rel_x * sin_a + rel_z * cos_a;

    /* Behind camera check */
    if (cam_z < 1.0f) return 0;

    /* Perspective projection */
    float inv_z = 128.0f / cam_z;

    *screen_x = (s16)(SCREEN_WIDTH / 2 + cam_x * inv_z);
    *screen_y = (s16)(HORIZON_LINE + (2.0f - rel_y) * inv_z);
    *scale = (s16)(inv_z * 2);  /* For sprite scaling */

    /* Bounds check */
    if (*screen_x < -64 || *screen_x > SCREEN_WIDTH + 64) return 0;
    if (*screen_y < -64 || *screen_y > SCREEN_HEIGHT + 32) return 0;

    return 1;
}

void render_draw_triangle(vertex_t *v0, vertex_t *v1, vertex_t *v2) {
    /* Genesis cannot draw arbitrary triangles
     * Ground rendering is handled via horizontal scrolling
     * Individual triangles are ignored */
    (void)v0; (void)v1; (void)v2;
}

void render_draw_mesh(mesh_t *mesh, mat4_t transform) {
    if (!mesh || !current_camera) return;
    if (sprite_count >= MAX_SPRITES) return;

    genesis_mesh_data_t *data = (genesis_mesh_data_t *)mesh->triangles;
    if (!data) return;

    /* Extract world position from transform */
    vec3_t world_pos;
    world_pos.x = transform.m[12];
    world_pos.y = transform.m[13];
    world_pos.z = transform.m[14];

    /* Project to screen */
    s16 screen_x, screen_y, scale;
    if (!project_to_screen(world_pos, &screen_x, &screen_y, &scale)) return;

    /* Determine sprite size based on distance */
    u16 sprite_size;
    s16 offset_x, offset_y;
    u8 priority;

    if (scale > 32) {
        /* Close - use 32x32 sprite */
        sprite_size = SPRITE_SIZE(4, 4);
        offset_x = 16;
        offset_y = 16;
        priority = 1;
    } else if (scale > 16) {
        /* Medium - use 16x16 sprite */
        sprite_size = SPRITE_SIZE(2, 2);
        offset_x = 8;
        offset_y = 8;
        priority = 1;
    } else {
        /* Far - use 8x8 sprite */
        sprite_size = SPRITE_SIZE(1, 1);
        offset_x = 4;
        offset_y = 4;
        priority = 0;
    }

    /* Determine palette based on color */
    u16 palette = PAL0;
    u8 r = (data->color >> 16) & 0xFF;
    u8 g = (data->color >> 8) & 0xFF;
    u8 b = data->color & 0xFF;

    if (r > g && r > b) palette = PAL1;       /* Red */
    else if (g > r && g > b) palette = PAL2;  /* Green */
    else if (b > r && b > g) palette = PAL3;  /* Blue */

    /* Add sprite using SGDK */
    /* Note: In real implementation, you'd use SPR_addSprite with pre-loaded sprite definitions */
    VDP_setSpriteFull(sprite_count++,
                      screen_x - offset_x,
                      screen_y - offset_y,
                      sprite_size,
                      TILE_ATTR_FULL(palette, priority, FALSE, FALSE, data->sprite_def),
                      0);  /* No link for now */
}

void render_draw_quad(vec3_t pos, float width, float height, u32 color) {
    /* Draw as sprite */
    if (sprite_count >= MAX_SPRITES) return;

    s16 screen_x, screen_y, scale;
    if (!project_to_screen(pos, &screen_x, &screen_y, &scale)) return;

    /* Draw a simple marker sprite */
    VDP_setSpriteFull(sprite_count++,
                      screen_x - 8,
                      screen_y - 4,
                      SPRITE_SIZE(2, 1),
                      TILE_ATTR_FULL(PAL0, 1, FALSE, FALSE, 1),
                      0);

    (void)width; (void)height; (void)color;
}

void render_wait_vram_ready(void) {
    VDP_waitDMACompletion();
}

void render_draw_sky_background(u32 color) {
    /* Sky is just the background color on Genesis */
    render_clear(color);
}

void render_draw_rect_2d(int x, int y, int w, int h, u32 color) {
    /* Use window plane or sprites for HUD rectangles */
    if (sprite_count >= MAX_SPRITES) return;

    /* Simple implementation - draw a sprite */
    u16 palette = PAL0;
    u8 r = (color >> 16) & 0xFF;
    if (r > 200) palette = PAL1;

    VDP_setSpriteFull(sprite_count++, x, y,
                      SPRITE_SIZE(w/8 > 0 ? w/8 : 1, h/8 > 0 ? h/8 : 1),
                      TILE_ATTR_FULL(palette, 1, FALSE, FALSE, 2),
                      0);

    (void)w; (void)h;
}

void render_draw_text(int x, int y, u32 color, const char *text) {
    /* Use SGDK's built-in text rendering */
    VDP_drawText(text, x / 8, y / 8);
    (void)color;
}

mesh_t *mesh_create_cube(float size, u32 color) {
    mesh_t *mesh = (mesh_t *)MEM_alloc(sizeof(mesh_t));
    genesis_mesh_data_t *data = (genesis_mesh_data_t *)MEM_alloc(sizeof(genesis_mesh_data_t));

    data->mesh_type = MESH_TYPE_OTHER;
    data->color = color;
    data->sprite_def = 0;
    data->width = (s16)(size * 16);
    data->height = (s16)(size * 16);

    mesh->triangles = (triangle_t *)data;
    mesh->tri_count = 0;
    mesh->base_color = color;

    return mesh;
}

mesh_t *mesh_create_vehicle(u32 color) {
    mesh_t *mesh = (mesh_t *)MEM_alloc(sizeof(mesh_t));
    genesis_mesh_data_t *data = (genesis_mesh_data_t *)MEM_alloc(sizeof(genesis_mesh_data_t));

    data->mesh_type = MESH_TYPE_VEHICLE;
    data->color = color;
    data->sprite_def = 16;  /* Vehicle sprites start at tile 16 */
    data->width = 32;
    data->height = 24;

    mesh->triangles = (triangle_t *)data;
    mesh->tri_count = 0;
    mesh->base_color = color;

    return mesh;
}

mesh_t *mesh_create_track_segment(float width, float length, u32 color) {
    mesh_t *mesh = (mesh_t *)MEM_alloc(sizeof(mesh_t));
    genesis_mesh_data_t *data = (genesis_mesh_data_t *)MEM_alloc(sizeof(genesis_mesh_data_t));

    data->mesh_type = MESH_TYPE_TRACK;
    data->color = color;
    data->sprite_def = 64;  /* Track marker sprites */
    data->width = (s16)(width * 8);
    data->height = (s16)(length * 8);

    mesh->triangles = (triangle_t *)data;
    mesh->tri_count = 0;
    mesh->base_color = color;

    return mesh;
}

void mesh_destroy(mesh_t *mesh) {
    if (mesh) {
        if (mesh->triangles) MEM_free(mesh->triangles);
        MEM_free(mesh);
    }
}

#endif /* PLATFORM_GENESIS */
