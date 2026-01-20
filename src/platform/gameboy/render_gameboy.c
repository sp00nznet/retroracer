/*
 * RetroRacer - Game Boy Rendering Implementation
 * Uses GBDK for DMG/CGB hardware
 * 160x144 screen, 4 shades (DMG) or 32768 colors (CGB)
 * Sprite-based pseudo-3D racing (no true 3D hardware)
 */

#include "render.h"
#include "platform.h"
#include <string.h>
#include <stdlib.h>

#ifdef PLATFORM_GAMEBOY
#include <gb/gb.h>
#include <gb/cgb.h>
#include <gb/drawing.h>
#else
/* Stub definitions for compilation */
typedef unsigned char UINT8;
typedef unsigned short UINT16;
#define SPRITES_8x8 0
#define SPRITES_8x16 1
#endif

/* Game Boy display constants */
#define GB_SCREEN_WIDTH  160
#define GB_SCREEN_HEIGHT 144
#define GB_TILE_WIDTH    20
#define GB_TILE_HEIGHT   18
#define GB_MAX_SPRITES   40
#define GB_SPRITE_SIZE   8

/* Sprite slot allocation */
#define SPRITE_VEHICLE_START   0
#define SPRITE_VEHICLE_COUNT   8
#define SPRITE_ROAD_START      8
#define SPRITE_ROAD_COUNT      24
#define SPRITE_HUD_START       32
#define SPRITE_HUD_COUNT       8

/* Pseudo-3D road rendering */
#define HORIZON_Y        48
#define ROAD_SEGMENTS    16
#define ROAD_WIDTH_NEAR  80
#define ROAD_WIDTH_FAR   20

/* Color palettes (CGB) */
static const UINT16 cgb_palette_bg[] = {
    0x7FFF, 0x5294, 0x294A, 0x0000,  /* Grayscale */
    0x7C00, 0x5000, 0x2800, 0x0000,  /* Red road */
    0x03E0, 0x02A0, 0x0140, 0x0000,  /* Green grass */
    0x001F, 0x0014, 0x000A, 0x0000,  /* Blue sky */
};

static const UINT16 cgb_palette_sprite[] = {
    0x7FFF, 0x001F, 0x7C00, 0x0000,  /* Vehicle colors */
    0x7FFF, 0x7FE0, 0x03FF, 0x0000,  /* Opponent colors */
};

/* Pre-rendered sprite data for road perspective */
typedef struct {
    UINT8 y;           /* Screen Y position */
    UINT8 width;       /* Road width at this Y */
    UINT8 left_edge;   /* Left edge X */
    UINT8 right_edge;  /* Right edge X */
    UINT8 stripe;      /* Stripe pattern (0/1) */
} road_line_t;

/* Mesh representation for Game Boy (sprite-based) */
typedef struct {
    int sprite_id;      /* Base sprite ID (0-3 for different angles) */
    int width;          /* Sprite width in tiles */
    int height;         /* Sprite height in tiles */
    UINT8 *tile_data;   /* Tile graphics data */
    int tile_count;     /* Number of tiles */
} gb_mesh_data_t;

/* Global renderer state */
static struct {
    /* Display state */
    int is_cgb;

    /* Camera/view state */
    int camera_x;
    int camera_z;
    int camera_angle;

    /* Road rendering */
    road_line_t road_lines[ROAD_SEGMENTS];
    int road_scroll;
    int road_curve;

    /* Sprite management */
    UINT8 sprite_used[GB_MAX_SPRITES];
    int next_sprite;

    /* Background tiles */
    UINT8 bg_tiles[GB_TILE_WIDTH * GB_TILE_HEIGHT];

    /* Mesh storage */
    gb_mesh_data_t meshes[16];
    int mesh_count;

    /* Frame buffer for software pre-rendering */
    UINT8 framebuffer[GB_SCREEN_WIDTH * GB_SCREEN_HEIGHT / 4];

    int initialized;
} g_render;

/* Fixed-point math helpers */
#define FP_SHIFT 8
#define FP_ONE (1 << FP_SHIFT)
#define INT_TO_FP(x) ((x) << FP_SHIFT)
#define FP_TO_INT(x) ((x) >> FP_SHIFT)
#define FP_MUL(a, b) (((a) * (b)) >> FP_SHIFT)
#define FP_DIV(a, b) (((a) << FP_SHIFT) / (b))

/* Sin/cos lookup table (256 entries, scaled to FP_ONE) */
static const int sin_table[256] = {
    0, 6, 13, 19, 25, 31, 37, 44, 50, 56, 62, 68, 74, 80, 86, 92,
    97, 103, 109, 114, 120, 125, 131, 136, 141, 146, 151, 156, 161, 166, 171, 175,
    180, 184, 189, 193, 197, 201, 205, 209, 213, 216, 220, 223, 226, 230, 233, 236,
    238, 241, 243, 246, 248, 250, 252, 253, 255, 256, 257, 258, 259, 260, 260, 261,
    261, 261, 261, 261, 260, 260, 259, 258, 257, 256, 255, 253, 252, 250, 248, 246,
    243, 241, 238, 236, 233, 230, 226, 223, 220, 216, 213, 209, 205, 201, 197, 193,
    189, 184, 180, 175, 171, 166, 161, 156, 151, 146, 141, 136, 131, 125, 120, 114,
    109, 103, 97, 92, 86, 80, 74, 68, 62, 56, 50, 44, 37, 31, 25, 19,
    13, 6, 0, -6, -13, -19, -25, -31, -37, -44, -50, -56, -62, -68, -74, -80,
    -86, -92, -97, -103, -109, -114, -120, -125, -131, -136, -141, -146, -151, -156, -161, -166,
    -171, -175, -180, -184, -189, -193, -197, -201, -205, -209, -213, -216, -220, -223, -226, -230,
    -233, -236, -238, -241, -243, -246, -248, -250, -252, -253, -255, -256, -257, -258, -259, -260,
    -260, -261, -261, -261, -261, -261, -260, -260, -259, -258, -257, -256, -255, -253, -252, -250,
    -248, -246, -243, -241, -238, -236, -233, -230, -226, -223, -220, -216, -213, -209, -205, -201,
    -197, -193, -189, -184, -180, -175, -171, -166, -161, -156, -151, -146, -141, -136, -131, -125,
    -120, -114, -109, -103, -97, -92, -86, -80, -74, -68, -62, -56, -50, -44, -37, -31
};

static int fp_sin(int angle) {
    return sin_table[angle & 255];
}

static int fp_cos(int angle) {
    return sin_table[(angle + 64) & 255];
}

/* Initialize road perspective lookup table */
static void init_road_perspective(void) {
    for (int i = 0; i < ROAD_SEGMENTS; i++) {
        int y = HORIZON_Y + i * ((GB_SCREEN_HEIGHT - HORIZON_Y) / ROAD_SEGMENTS);

        /* Calculate perspective width */
        int depth = i + 1;
        int width = ROAD_WIDTH_NEAR - (ROAD_WIDTH_NEAR - ROAD_WIDTH_FAR) * i / ROAD_SEGMENTS;

        g_render.road_lines[i].y = y;
        g_render.road_lines[i].width = width;
        g_render.road_lines[i].left_edge = (GB_SCREEN_WIDTH - width) / 2;
        g_render.road_lines[i].right_edge = (GB_SCREEN_WIDTH + width) / 2;
        g_render.road_lines[i].stripe = (i / 2) & 1;
    }
}

/* Draw a single road segment using background tiles */
static void draw_road_segment(int segment, int curve_offset) {
    road_line_t *line = &g_render.road_lines[segment];
    int y_tile = line->y / 8;

    if (y_tile >= GB_TILE_HEIGHT) return;

    /* Calculate curved road edges */
    int curve = FP_MUL(curve_offset, INT_TO_FP(segment)) / ROAD_SEGMENTS;
    int left = line->left_edge + FP_TO_INT(curve);
    int right = line->right_edge + FP_TO_INT(curve);

    /* Clamp to screen */
    if (left < 0) left = 0;
    if (right > GB_SCREEN_WIDTH) right = GB_SCREEN_WIDTH;

    /* Set background tiles for this row */
    int left_tile = left / 8;
    int right_tile = right / 8;

    for (int x = 0; x < GB_TILE_WIDTH; x++) {
        int tile_idx = y_tile * GB_TILE_WIDTH + x;

        if (x < left_tile) {
            /* Grass */
            g_render.bg_tiles[tile_idx] = 4 + ((segment + g_render.road_scroll) & 1);
        } else if (x > right_tile) {
            /* Grass */
            g_render.bg_tiles[tile_idx] = 4 + ((segment + g_render.road_scroll) & 1);
        } else if (x == left_tile || x == right_tile) {
            /* Road edge */
            g_render.bg_tiles[tile_idx] = 2 + line->stripe;
        } else {
            /* Road surface */
            g_render.bg_tiles[tile_idx] = line->stripe;
        }
    }
}

/* Software pixel drawing for framebuffer */
static void draw_pixel(int x, int y, UINT8 color) {
    if (x < 0 || x >= GB_SCREEN_WIDTH || y < 0 || y >= GB_SCREEN_HEIGHT) return;

    int byte_idx = (y * GB_SCREEN_WIDTH + x) / 4;
    int bit_shift = (3 - (x & 3)) * 2;

    g_render.framebuffer[byte_idx] &= ~(3 << bit_shift);
    g_render.framebuffer[byte_idx] |= (color & 3) << bit_shift;
}

static void draw_line(int x0, int y0, int x1, int y1, UINT8 color) {
    int dx = x1 - x0;
    int dy = y1 - y0;
    int sx = (dx > 0) ? 1 : -1;
    int sy = (dy > 0) ? 1 : -1;

    if (dx < 0) dx = -dx;
    if (dy < 0) dy = -dy;

    int err = dx - dy;

    while (1) {
        draw_pixel(x0, y0, color);

        if (x0 == x1 && y0 == y1) break;

        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x0 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y0 += sy;
        }
    }
}

static void fill_rect(int x, int y, int w, int h, UINT8 color) {
    for (int py = y; py < y + h; py++) {
        for (int px = x; px < x + w; px++) {
            draw_pixel(px, py, color);
        }
    }
}

void render_init(void) {
    if (g_render.initialized) return;

    memset(&g_render, 0, sizeof(g_render));

#ifdef PLATFORM_GAMEBOY
    /* Detect Game Boy Color */
    g_render.is_cgb = (_cpu == CGB_TYPE);

    /* Initialize display */
    DISPLAY_ON;

    /* Set sprite mode to 8x8 */
    SPRITES_8x8;

    /* Initialize palettes */
    if (g_render.is_cgb) {
        /* Set CGB palettes */
        set_bkg_palette(0, 4, cgb_palette_bg);
        set_sprite_palette(0, 2, cgb_palette_sprite);
    } else {
        /* DMG palette */
        BGP_REG = 0xE4;  /* 11 10 01 00 */
        OBP0_REG = 0xD0; /* 11 01 00 00 */
        OBP1_REG = 0xE0; /* 11 10 00 00 */
    }

    /* Hide all sprites initially */
    for (int i = 0; i < GB_MAX_SPRITES; i++) {
        move_sprite(i, 0, 0);
    }
#endif

    /* Initialize road perspective */
    init_road_perspective();

    /* Clear framebuffer */
    memset(g_render.framebuffer, 0, sizeof(g_render.framebuffer));

    g_render.initialized = 1;
}

void render_shutdown(void) {
    if (!g_render.initialized) return;

    /* Free mesh data */
    for (int i = 0; i < g_render.mesh_count; i++) {
        if (g_render.meshes[i].tile_data) {
            free(g_render.meshes[i].tile_data);
        }
    }

#ifdef PLATFORM_GAMEBOY
    DISPLAY_OFF;
#endif

    g_render.initialized = 0;
}

void render_begin_frame(void) {
    if (!g_render.initialized) return;

    /* Clear framebuffer - sky color (light) */
    memset(g_render.framebuffer, 0x00, sizeof(g_render.framebuffer));

    /* Reset sprite allocation */
    memset(g_render.sprite_used, 0, sizeof(g_render.sprite_used));
    g_render.next_sprite = 0;

    /* Increment road scroll for animation */
    g_render.road_scroll++;
}

void render_end_frame(void) {
    if (!g_render.initialized) return;

#ifdef PLATFORM_GAMEBOY
    /* Wait for VBlank */
    wait_vbl_done();

    /* Update background tiles */
    set_bkg_tiles(0, 0, GB_TILE_WIDTH, GB_TILE_HEIGHT, g_render.bg_tiles);

    /* Hide unused sprites */
    for (int i = g_render.next_sprite; i < GB_MAX_SPRITES; i++) {
        move_sprite(i, 0, 0);
    }
#endif
}

void render_set_camera(float x, float y, float z,
                       float look_x, float look_y, float look_z) {
    /* Convert to fixed-point for Game Boy */
    g_render.camera_x = (int)(x * FP_ONE);
    g_render.camera_z = (int)(z * FP_ONE);

    /* Calculate curve based on look direction */
    float dx = look_x - x;
    g_render.road_curve = (int)(dx * 32);
}

/* Create vehicle mesh as sprite data */
mesh_t *mesh_create_vehicle(const vehicle_model_t *model) {
    if (!model || g_render.mesh_count >= 16) return NULL;

    mesh_t *mesh = (mesh_t *)malloc(sizeof(mesh_t));
    if (!mesh) return NULL;

    gb_mesh_data_t *data = &g_render.meshes[g_render.mesh_count];

    /* Create simple vehicle sprite (2x2 tiles = 16x16 pixels) */
    data->width = 2;
    data->height = 2;
    data->tile_count = 4;
    data->sprite_id = g_render.mesh_count;

    /* Allocate tile data (16 bytes per tile for 8x8 2bpp) */
    data->tile_data = (UINT8 *)malloc(data->tile_count * 16);
    if (!data->tile_data) {
        free(mesh);
        return NULL;
    }

    /* Generate simple car sprite pattern */
    /* Tile 0: Top-left of car */
    UINT8 *tile = data->tile_data;
    tile[0] = 0x00; tile[1] = 0x00;  /* Row 0 */
    tile[2] = 0x18; tile[3] = 0x18;  /* Row 1 - roof start */
    tile[4] = 0x3C; tile[5] = 0x3C;  /* Row 2 */
    tile[6] = 0x7E; tile[7] = 0x42;  /* Row 3 - windows */
    tile[8] = 0x7E; tile[9] = 0x42;  /* Row 4 */
    tile[10] = 0x7E; tile[11] = 0x7E; /* Row 5 - body */
    tile[12] = 0x7E; tile[13] = 0x7E; /* Row 6 */
    tile[14] = 0x66; tile[15] = 0x66; /* Row 7 - wheels */

    /* Tile 1: Top-right of car (mirror) */
    tile = data->tile_data + 16;
    tile[0] = 0x00; tile[1] = 0x00;
    tile[2] = 0x18; tile[3] = 0x18;
    tile[4] = 0x3C; tile[5] = 0x3C;
    tile[6] = 0x7E; tile[7] = 0x42;
    tile[8] = 0x7E; tile[9] = 0x42;
    tile[10] = 0x7E; tile[11] = 0x7E;
    tile[12] = 0x7E; tile[13] = 0x7E;
    tile[14] = 0x66; tile[15] = 0x66;

    /* Tiles 2-3: Bottom half */
    tile = data->tile_data + 32;
    tile[0] = 0x7E; tile[1] = 0x7E;
    tile[2] = 0x7E; tile[3] = 0x7E;
    tile[4] = 0x66; tile[5] = 0x66;
    tile[6] = 0x66; tile[7] = 0x00;
    tile[8] = 0x00; tile[9] = 0x00;
    tile[10] = 0x00; tile[11] = 0x00;
    tile[12] = 0x00; tile[13] = 0x00;
    tile[14] = 0x00; tile[15] = 0x00;

    tile = data->tile_data + 48;
    memcpy(tile, data->tile_data + 32, 16);

    mesh->triangle_count = 0;  /* Not applicable for sprite-based */
    mesh->triangles = (void *)data;
    mesh->platform_data = (void *)(intptr_t)g_render.mesh_count;

    g_render.mesh_count++;
    return mesh;
}

/* Create track - for Game Boy this sets up the road rendering params */
mesh_t *mesh_create_track(const track_model_t *model) {
    if (!model) return NULL;

    mesh_t *mesh = (mesh_t *)malloc(sizeof(mesh_t));
    if (!mesh) return NULL;

    /* Track mesh doesn't store geometry on Game Boy */
    /* Road is rendered procedurally using perspective lookup */
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

    gb_mesh_data_t *data = (gb_mesh_data_t *)mesh->triangles;

    if (!data) {
        /* This is a track mesh - render road */
        for (int i = 0; i < ROAD_SEGMENTS; i++) {
            draw_road_segment(i, g_render.road_curve);
        }
        return;
    }

    /* Vehicle rendering as sprite */
    /* Project 3D position to screen */
    int rel_z = (int)(z * FP_ONE) - g_render.camera_z;
    if (rel_z <= 0) return;  /* Behind camera */

    int rel_x = (int)(x * FP_ONE) - g_render.camera_x;

    /* Perspective projection */
    int screen_x = GB_SCREEN_WIDTH / 2 + FP_TO_INT(FP_DIV(rel_x * 64, rel_z));
    int screen_y = HORIZON_Y + FP_TO_INT(FP_DIV(INT_TO_FP(GB_SCREEN_HEIGHT - HORIZON_Y) * 32, rel_z));

    /* Scale based on distance */
    int scale = FP_TO_INT(FP_DIV(INT_TO_FP(256), rel_z + FP_ONE));
    if (scale < 1) scale = 1;
    if (scale > 4) scale = 4;

    /* Draw vehicle as filled rectangle (simplified) */
    int w = 8 * scale;
    int h = 6 * scale;
    int draw_x = screen_x - w / 2;
    int draw_y = screen_y - h;

    /* Draw vehicle body */
    fill_rect(draw_x, draw_y, w, h, 3);  /* Dark color */
    fill_rect(draw_x + 1, draw_y + 1, w - 2, h / 2, 1);  /* Window */

#ifdef PLATFORM_GAMEBOY
    /* Also set hardware sprites for flicker-free rendering */
    if (g_render.next_sprite < GB_MAX_SPRITES - 4) {
        int sprite_idx = g_render.next_sprite;

        /* Position sprites */
        move_sprite(sprite_idx, draw_x + 8, draw_y + 16);
        move_sprite(sprite_idx + 1, draw_x + 16, draw_y + 16);

        g_render.next_sprite += 2;
    }
#endif
}

void render_draw_text(const char *text, int x, int y, uint32_t color) {
    if (!text) return;

    /* Simple 4x6 font rendering */
    UINT8 gb_color = (color > 0) ? 3 : 1;

    int cx = x;
    while (*text) {
        char c = *text;

        /* Draw simple character pattern */
        if (c >= '0' && c <= '9') {
            /* Number - draw as small rectangle with identifier */
            fill_rect(cx, y, 4, 6, gb_color);
            draw_pixel(cx + 1, y + 2, 0);
            draw_pixel(cx + 2, y + 3, 0);
        } else if (c >= 'A' && c <= 'Z') {
            /* Letter */
            fill_rect(cx, y, 4, 6, gb_color);
            draw_pixel(cx + 1, y + 1, 0);
            draw_pixel(cx + 2, y + 4, 0);
        } else if (c == ' ') {
            /* Space - skip */
        } else {
            /* Other characters */
            fill_rect(cx, y, 3, 5, gb_color);
        }

        cx += 5;
        text++;
    }
}

void render_draw_sprite(const texture_t *tex, int x, int y, int w, int h) {
    if (!tex) return;

    /* Convert texture to 2-bit and draw */
    for (int py = 0; py < h && py < tex->height; py++) {
        for (int px = 0; px < w && px < tex->width; px++) {
            int sx = x + px;
            int sy = y + py;

            /* Sample texture and convert to 2-bit */
            uint32_t color = ((uint32_t *)tex->data)[py * tex->width + px];
            int r = (color >> 16) & 0xFF;
            int g = (color >> 8) & 0xFF;
            int b = color & 0xFF;
            int gray = (r + g + b) / 3;

            UINT8 gb_color;
            if (gray > 192) gb_color = 0;
            else if (gray > 128) gb_color = 1;
            else if (gray > 64) gb_color = 2;
            else gb_color = 3;

            draw_pixel(sx, sy, gb_color);
        }
    }
}

texture_t *texture_load(const char *filename) {
    texture_t *tex = (texture_t *)malloc(sizeof(texture_t));
    if (!tex) return NULL;

    /* Create small default texture */
    tex->width = 16;
    tex->height = 16;
    tex->data = malloc(16 * 16 * 4);

    if (tex->data) {
        uint32_t *pixels = (uint32_t *)tex->data;
        for (int y = 0; y < 16; y++) {
            for (int x = 0; x < 16; x++) {
                /* Checkerboard */
                int shade = ((x ^ y) & 2) ? 0xC0 : 0x40;
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
