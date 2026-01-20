/*
 * RetroRacer - Sega Game Gear Rendering Implementation
 * Uses SDCC with devkitSMS for VDP hardware
 * 160x144 visible screen, 32 colors from 4096 color palette
 * Sprite-based pseudo-3D racing with Mode 4 graphics
 */

#include "render.h"
#include "platform.h"
#include <string.h>
#include <stdlib.h>

#ifdef PLATFORM_GAMEGEAR
#include <sms.h>
#include <gg.h>
#else
/* Stub definitions for compilation */
typedef unsigned char UINT8;
typedef unsigned short UINT16;
#endif

/* Game Gear display constants */
#define GG_SCREEN_WIDTH   160
#define GG_SCREEN_HEIGHT  144
#define GG_TILE_WIDTH     20   /* 160 / 8 */
#define GG_TILE_HEIGHT    18   /* 144 / 8 */
#define GG_MAX_SPRITES    64
#define GG_SPRITE_SIZE    8
#define GG_VRAM_TILES     0x0000
#define GG_VRAM_TILEMAP   0x3800
#define GG_VRAM_SAT       0x3F00

/* Sprite limits */
#define SPRITE_VEHICLE_COUNT  8
#define SPRITE_ROAD_COUNT    16
#define SPRITE_HUD_COUNT      8
#define SPRITES_PER_LINE      8

/* Road rendering */
#define HORIZON_Y        40
#define ROAD_SEGMENTS    12
#define ROAD_WIDTH_NEAR  70
#define ROAD_WIDTH_FAR   15

/* Game Gear 12-bit RGB colors (4096 palette) */
#define GG_COLOR(r, g, b) (((b) << 8) | ((g) << 4) | (r))

/* Predefined colors */
#define COLOR_SKY        GG_COLOR(4, 8, 15)
#define COLOR_GRASS_1    GG_COLOR(2, 12, 2)
#define COLOR_GRASS_2    GG_COLOR(1, 10, 1)
#define COLOR_ROAD_1     GG_COLOR(6, 6, 6)
#define COLOR_ROAD_2     GG_COLOR(8, 8, 8)
#define COLOR_STRIPE     GG_COLOR(15, 15, 15)
#define COLOR_CAR_BODY   GG_COLOR(15, 0, 0)
#define COLOR_CAR_WINDOW GG_COLOR(4, 8, 12)
#define COLOR_YELLOW     GG_COLOR(15, 14, 0)

/* Road line data for perspective */
typedef struct {
    UINT8 y;
    UINT8 width;
    UINT8 left_x;
    UINT8 right_x;
    UINT8 stripe;
} gg_road_line_t;

/* Mesh representation for Game Gear */
typedef struct {
    int sprite_id;
    int width;
    int height;
    UINT8 *tile_data;
    int tile_count;
    UINT16 colors[4];
} gg_mesh_data_t;

/* Sprite attribute entry */
typedef struct {
    UINT8 y;
    UINT8 x;
    UINT8 tile;
} gg_sprite_t;

/* Global renderer state */
static struct {
    /* Road perspective lookup */
    gg_road_line_t road_lines[ROAD_SEGMENTS];

    /* Sprite management */
    gg_sprite_t sprites[GG_MAX_SPRITES];
    int sprite_count;

    /* Background tile buffer */
    UINT8 tilemap[GG_TILE_WIDTH * GG_TILE_HEIGHT * 2];

    /* Camera state */
    int camera_x;
    int camera_z;
    int road_curve;
    int road_scroll;

    /* Mesh storage */
    gg_mesh_data_t meshes[16];
    int mesh_count;

    /* Software framebuffer for complex rendering */
    UINT8 framebuffer[GG_SCREEN_WIDTH * GG_SCREEN_HEIGHT];

    /* CRAM palette cache */
    UINT16 palette[32];

    int initialized;
} g_render;

/* Fixed-point math */
#define FP_SHIFT 8
#define FP_ONE (1 << FP_SHIFT)
#define INT_TO_FP(x) ((x) << FP_SHIFT)
#define FP_TO_INT(x) ((x) >> FP_SHIFT)
#define FP_MUL(a, b) (((long)(a) * (b)) >> FP_SHIFT)
#define FP_DIV(a, b) (((long)(a) << FP_SHIFT) / (b))

/* VDP register access */
#ifdef PLATFORM_GAMEGEAR
static void vdp_write_reg(UINT8 reg, UINT8 value) {
    gg_vdp_out(reg, value);
}

static void vdp_write_vram(UINT16 addr, const UINT8 *data, UINT16 len) {
    SMS_mapROMBank(addr);
    for (UINT16 i = 0; i < len; i++) {
        SMS_VRAMmemcpy(addr + i, &data[i], 1);
    }
}

static void vdp_write_cram(UINT8 index, UINT16 color) {
    GG_setBGPaletteColor(index, color);
}
#else
static void vdp_write_reg(UINT8 reg, UINT8 value) { (void)reg; (void)value; }
static void vdp_write_vram(UINT16 addr, const UINT8 *data, UINT16 len) { (void)addr; (void)data; (void)len; }
static void vdp_write_cram(UINT8 index, UINT16 color) { (void)index; (void)color; }
#endif

/* Initialize road perspective table */
static void init_road_perspective(void) {
    for (int i = 0; i < ROAD_SEGMENTS; i++) {
        int y = HORIZON_Y + i * ((GG_SCREEN_HEIGHT - HORIZON_Y) / ROAD_SEGMENTS);
        int width = ROAD_WIDTH_NEAR - (ROAD_WIDTH_NEAR - ROAD_WIDTH_FAR) * i / ROAD_SEGMENTS;

        g_render.road_lines[i].y = y;
        g_render.road_lines[i].width = width;
        g_render.road_lines[i].left_x = (GG_SCREEN_WIDTH - width) / 2;
        g_render.road_lines[i].right_x = (GG_SCREEN_WIDTH + width) / 2;
        g_render.road_lines[i].stripe = (i / 2) & 1;
    }
}

/* Software framebuffer operations */
static void fb_clear(UINT8 color) {
    memset(g_render.framebuffer, color, sizeof(g_render.framebuffer));
}

static void fb_pixel(int x, int y, UINT8 color) {
    if (x < 0 || x >= GG_SCREEN_WIDTH || y < 0 || y >= GG_SCREEN_HEIGHT) return;
    g_render.framebuffer[y * GG_SCREEN_WIDTH + x] = color;
}

static void fb_hline(int x1, int x2, int y, UINT8 color) {
    if (y < 0 || y >= GG_SCREEN_HEIGHT) return;
    if (x1 > x2) { int t = x1; x1 = x2; x2 = t; }
    if (x1 < 0) x1 = 0;
    if (x2 >= GG_SCREEN_WIDTH) x2 = GG_SCREEN_WIDTH - 1;

    UINT8 *row = &g_render.framebuffer[y * GG_SCREEN_WIDTH];
    for (int x = x1; x <= x2; x++) {
        row[x] = color;
    }
}

static void fb_rect(int x, int y, int w, int h, UINT8 color) {
    for (int py = y; py < y + h; py++) {
        fb_hline(x, x + w - 1, py, color);
    }
}

/* Draw road segment */
static void draw_road_segment(int seg, int curve) {
    gg_road_line_t *line = &g_render.road_lines[seg];

    /* Apply curve offset */
    int curve_offset = FP_TO_INT(FP_MUL(INT_TO_FP(curve), INT_TO_FP(seg))) / ROAD_SEGMENTS;
    int left = line->left_x + curve_offset;
    int right = line->right_x + curve_offset;

    /* Clamp */
    if (left < 0) left = 0;
    if (right >= GG_SCREEN_WIDTH) right = GG_SCREEN_WIDTH - 1;

    int y = line->y;
    int seg_height = (seg < ROAD_SEGMENTS - 1) ?
                     g_render.road_lines[seg + 1].y - y : GG_SCREEN_HEIGHT - y;

    /* Draw grass on both sides */
    UINT8 grass_color = ((seg + g_render.road_scroll) & 1) ? 2 : 3;
    for (int py = y; py < y + seg_height && py < GG_SCREEN_HEIGHT; py++) {
        fb_hline(0, left - 1, py, grass_color);
        fb_hline(right + 1, GG_SCREEN_WIDTH - 1, py, grass_color);
    }

    /* Draw road surface */
    UINT8 road_color = line->stripe ? 4 : 5;
    for (int py = y; py < y + seg_height && py < GG_SCREEN_HEIGHT; py++) {
        fb_hline(left, right, py, road_color);
    }

    /* Draw center stripe */
    if (line->stripe) {
        int center = (left + right) / 2;
        for (int py = y; py < y + seg_height && py < GG_SCREEN_HEIGHT; py++) {
            fb_hline(center - 2, center + 2, py, 6);
        }
    }

    /* Draw road edges */
    for (int py = y; py < y + seg_height && py < GG_SCREEN_HEIGHT; py++) {
        fb_pixel(left, py, 6);
        fb_pixel(left + 1, py, 6);
        fb_pixel(right - 1, py, 6);
        fb_pixel(right, py, 6);
    }
}

/* Draw vehicle sprite */
static void draw_vehicle(int screen_x, int screen_y, int scale, UINT8 body_color) {
    int w = 8 + scale * 2;
    int h = 6 + scale;

    int x = screen_x - w / 2;
    int y = screen_y - h;

    /* Body */
    fb_rect(x, y, w, h, body_color);

    /* Roof */
    fb_rect(x + w / 4, y - scale - 2, w / 2, scale + 2, body_color);

    /* Windows */
    fb_rect(x + w / 4 + 1, y - scale - 1, w / 2 - 2, scale, 8);

    /* Wheels */
    if (scale >= 1) {
        fb_rect(x, y + h - 2, 3, 2, 1);
        fb_rect(x + w - 3, y + h - 2, 3, 2, 1);
    }
}

void render_init(void) {
    if (g_render.initialized) return;

    memset(&g_render, 0, sizeof(g_render));

#ifdef PLATFORM_GAMEGEAR
    /* Initialize VDP for Mode 4 */
    SMS_init();
    SMS_VDPturnOnFeature(VDPFEATURE_HIDEFIRSTCOL);

    /* Set up display */
    SMS_displayOn();
#endif

    /* Initialize palette */
    g_render.palette[0] = COLOR_SKY;        /* Background / sky */
    g_render.palette[1] = GG_COLOR(0, 0, 0); /* Black */
    g_render.palette[2] = COLOR_GRASS_1;    /* Grass light */
    g_render.palette[3] = COLOR_GRASS_2;    /* Grass dark */
    g_render.palette[4] = COLOR_ROAD_1;     /* Road light */
    g_render.palette[5] = COLOR_ROAD_2;     /* Road dark */
    g_render.palette[6] = COLOR_STRIPE;     /* Stripe white */
    g_render.palette[7] = COLOR_CAR_BODY;   /* Player car */
    g_render.palette[8] = COLOR_CAR_WINDOW; /* Windows */
    g_render.palette[9] = COLOR_YELLOW;     /* HUD */

    /* Upload palette to CRAM */
    for (int i = 0; i < 16; i++) {
        vdp_write_cram(i, g_render.palette[i]);
    }

    /* Initialize road perspective */
    init_road_perspective();

    g_render.initialized = 1;
}

void render_shutdown(void) {
    if (!g_render.initialized) return;

    for (int i = 0; i < g_render.mesh_count; i++) {
        if (g_render.meshes[i].tile_data) {
            free(g_render.meshes[i].tile_data);
        }
    }

#ifdef PLATFORM_GAMEGEAR
    SMS_displayOff();
#endif

    g_render.initialized = 0;
}

void render_begin_frame(void) {
    if (!g_render.initialized) return;

    /* Clear to sky color */
    fb_clear(0);

    /* Draw sky gradient (upper portion) */
    for (int y = 0; y < HORIZON_Y; y++) {
        /* Slight gradient effect */
        UINT8 sky_shade = (y < HORIZON_Y / 2) ? 0 : 0;
        fb_hline(0, GG_SCREEN_WIDTH - 1, y, sky_shade);
    }

    /* Reset sprite count */
    g_render.sprite_count = 0;

    /* Increment scroll */
    g_render.road_scroll++;
}

void render_end_frame(void) {
    if (!g_render.initialized) return;

#ifdef PLATFORM_GAMEGEAR
    /* Wait for VBlank */
    SMS_waitForVBlank();

    /* Convert framebuffer to tiles and upload to VRAM */
    /* In a real implementation, this would be optimized */
    /* For now, we use sprite overlay for dynamic elements */

    /* Update sprite attribute table */
    for (int i = 0; i < g_render.sprite_count && i < GG_MAX_SPRITES; i++) {
        SMS_addSprite(g_render.sprites[i].x, g_render.sprites[i].y,
                      g_render.sprites[i].tile);
    }
    SMS_copySpritestoSAT();
#endif
}

void render_set_camera(float x, float y, float z,
                       float look_x, float look_y, float look_z) {
    g_render.camera_x = (int)(x * FP_ONE);
    g_render.camera_z = (int)(z * FP_ONE);

    /* Calculate curve from look direction */
    float dx = look_x - x;
    g_render.road_curve = (int)(dx * 24);
}

mesh_t *mesh_create_vehicle(const vehicle_model_t *model) {
    if (!model || g_render.mesh_count >= 16) return NULL;

    mesh_t *mesh = (mesh_t *)malloc(sizeof(mesh_t));
    if (!mesh) return NULL;

    gg_mesh_data_t *data = &g_render.meshes[g_render.mesh_count];

    data->width = 2;
    data->height = 2;
    data->tile_count = 4;
    data->sprite_id = g_render.mesh_count;

    /* Allocate tile data */
    data->tile_data = (UINT8 *)malloc(data->tile_count * 32);
    if (!data->tile_data) {
        free(mesh);
        return NULL;
    }

    /* Generate simple car sprite in Mode 4 format */
    /* Each tile is 8x8 pixels, 4bpp = 32 bytes per tile */
    memset(data->tile_data, 0, data->tile_count * 32);

    /* Set vehicle colors from model */
    data->colors[0] = GG_COLOR(0, 0, 0);  /* Transparent */
    data->colors[1] = COLOR_CAR_BODY;
    data->colors[2] = COLOR_CAR_WINDOW;
    data->colors[3] = GG_COLOR(4, 4, 4);  /* Wheels */

    /* Simple car pattern for tile 0 */
    UINT8 *tile = data->tile_data;
    for (int row = 0; row < 8; row++) {
        UINT8 pattern = 0;
        if (row >= 2 && row <= 6) pattern = 0x7E;
        if (row == 3 || row == 4) pattern = 0x7E;

        /* Mode 4: 4 bitplanes */
        tile[row * 4 + 0] = pattern;
        tile[row * 4 + 1] = (row >= 3 && row <= 4) ? 0x42 : 0;
        tile[row * 4 + 2] = 0;
        tile[row * 4 + 3] = 0;
    }

    mesh->triangle_count = 0;
    mesh->triangles = (void *)data;
    mesh->platform_data = (void *)(intptr_t)g_render.mesh_count;

    g_render.mesh_count++;
    return mesh;
}

mesh_t *mesh_create_track(const track_model_t *model) {
    if (!model) return NULL;

    mesh_t *mesh = (mesh_t *)malloc(sizeof(mesh_t));
    if (!mesh) return NULL;

    mesh->triangle_count = 0;
    mesh->triangles = NULL;
    mesh->platform_data = NULL;

    return mesh;
}

void mesh_destroy(mesh_t *mesh) {
    if (!mesh) return;
    free(mesh);
}

void render_draw_mesh(const mesh_t *mesh, float x, float y, float z,
                      float rot_x, float rot_y, float rot_z) {
    if (!mesh) return;

    gg_mesh_data_t *data = (gg_mesh_data_t *)mesh->triangles;

    if (!data) {
        /* Track mesh - render road */
        for (int i = 0; i < ROAD_SEGMENTS; i++) {
            draw_road_segment(i, g_render.road_curve);
        }
        return;
    }

    /* Vehicle - project to screen */
    int rel_z = (int)(z * FP_ONE) - g_render.camera_z;
    if (rel_z <= 0) return;

    int rel_x = (int)(x * FP_ONE) - g_render.camera_x;

    /* Perspective projection */
    int screen_x = GG_SCREEN_WIDTH / 2 + FP_TO_INT(FP_DIV(rel_x * 48, rel_z));
    int screen_y = HORIZON_Y + FP_TO_INT(FP_DIV(INT_TO_FP(GG_SCREEN_HEIGHT - HORIZON_Y) * 24, rel_z));

    /* Scale based on distance */
    int scale = FP_TO_INT(FP_DIV(INT_TO_FP(192), rel_z + FP_ONE));
    if (scale < 0) scale = 0;
    if (scale > 3) scale = 3;

    /* Draw vehicle */
    draw_vehicle(screen_x, screen_y, scale, 7);

#ifdef PLATFORM_GAMEGEAR
    /* Add hardware sprite for flicker-free top layer */
    if (g_render.sprite_count < GG_MAX_SPRITES - 4) {
        g_render.sprites[g_render.sprite_count].x = screen_x - 8;
        g_render.sprites[g_render.sprite_count].y = screen_y - 8;
        g_render.sprites[g_render.sprite_count].tile = data->sprite_id * 4;
        g_render.sprite_count++;
    }
#endif
}

void render_draw_text(const char *text, int x, int y, uint32_t color) {
    if (!text) return;

    UINT8 gg_color = 9;  /* Yellow */

    int cx = x;
    while (*text) {
        char c = *text;

        if (c >= '0' && c <= '9') {
            /* Draw digit */
            fb_rect(cx, y, 4, 7, gg_color);
            fb_pixel(cx + 1, y + 3, 0);
        } else if (c >= 'A' && c <= 'Z') {
            fb_rect(cx, y, 4, 7, gg_color);
            fb_pixel(cx + 1, y + 1, 0);
            fb_pixel(cx + 2, y + 5, 0);
        } else if (c == ':') {
            fb_pixel(cx + 1, y + 2, gg_color);
            fb_pixel(cx + 1, y + 5, gg_color);
        }

        cx += 5;
        text++;
    }
}

void render_draw_sprite(const texture_t *tex, int x, int y, int w, int h) {
    if (!tex || !tex->data) return;

    /* Convert 32-bit texture to 4-bit palette indices */
    for (int py = 0; py < h && py < tex->height; py++) {
        for (int px = 0; px < w && px < tex->width; px++) {
            uint32_t color = ((uint32_t *)tex->data)[py * tex->width + px];

            /* Extract RGB and find closest palette entry */
            int r = ((color >> 16) & 0xFF) >> 4;
            int g = ((color >> 8) & 0xFF) >> 4;
            int b = (color & 0xFF) >> 4;

            /* Simple palette matching */
            UINT8 pal_idx = 1;  /* Default to black */
            if (r > 8 && g < 4 && b < 4) pal_idx = 7;  /* Red */
            else if (r > 8 && g > 8) pal_idx = 9;      /* Yellow */
            else if (g > 8) pal_idx = 2;               /* Green */
            else if (b > 8) pal_idx = 8;               /* Blue */
            else if (r + g + b > 36) pal_idx = 6;      /* White */
            else if (r + g + b > 18) pal_idx = 5;      /* Gray */

            fb_pixel(x + px, y + py, pal_idx);
        }
    }
}

texture_t *texture_load(const char *filename) {
    texture_t *tex = (texture_t *)malloc(sizeof(texture_t));
    if (!tex) return NULL;

    tex->width = 16;
    tex->height = 16;
    tex->data = malloc(16 * 16 * 4);

    if (tex->data) {
        uint32_t *pixels = (uint32_t *)tex->data;
        for (int y = 0; y < 16; y++) {
            for (int x = 0; x < 16; x++) {
                int shade = ((x ^ y) & 2) ? 0xA0 : 0x60;
                pixels[y * 16 + x] = (shade << 16) | (shade << 8) | shade;
            }
        }
    }

    return tex;
}

void texture_destroy(texture_t *tex) {
    if (!tex) return;
    if (tex->data) free(tex->data);
    free(tex);
}
