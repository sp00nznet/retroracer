/*
 * RetroRacer - PlayStation 1 (PSX) Platform Implementation
 *
 * Hardware Specs:
 * - CPU: 33.8688 MHz MIPS R3000A
 * - RAM: 2MB Main + 1MB VRAM
 * - GPU: Custom 2D/3D, ~180K flat-shaded polys/sec
 * - Audio: SPU with 24 voices, 512KB sound RAM
 * - Media: CD-ROM 2x speed
 *
 * SDK: PsyQ / PSn00bSDK
 */

#include "platform.h"

#ifdef PLATFORM_PSX

#include <sys/types.h>
#include <libetc.h>
#include <libgte.h>
#include <libgpu.h>
#include <libgs.h>
#include <libspu.h>
#include <libcd.h>
#include <libpad.h>
#include <libapi.h>

/*============================================================================
 * PSX-Specific Defines
 *============================================================================*/

#define PSX_SCREEN_WIDTH    320
#define PSX_SCREEN_HEIGHT   240
#define PSX_VRAM_WIDTH      1024
#define PSX_VRAM_HEIGHT     512

/* Double buffer frame buffers */
#define FRAME_BUFFER_0_X    0
#define FRAME_BUFFER_0_Y    0
#define FRAME_BUFFER_1_X    0
#define FRAME_BUFFER_1_Y    240

/* Ordering table depth */
#define OT_LENGTH           4096

/* Packet buffer size */
#define PACKET_BUFFER_SIZE  32768

/*============================================================================
 * PSX State
 *============================================================================*/

static struct {
    /* Display environment */
    DISPENV disp[2];
    DRAWENV draw[2];
    int db;  /* Current buffer index */

    /* Ordering tables */
    u_long ot[2][OT_LENGTH];

    /* Primitive buffers */
    char primbuff[2][PACKET_BUFFER_SIZE];
    char *nextpri;

    /* Controller state */
    u_char padbuf[2][34];

    /* Timing */
    int vsync_count;
    u_long frame_counter;
} psx_state;

/* Platform capabilities */
static const platform_caps_t psx_caps = {
    .name = "PlayStation",
    .ram_size = 2 * 1024 * 1024,        /* 2MB */
    .vram_size = 1024 * 1024,           /* 1MB */
    .max_texture_size = 256,
    .max_vertices = 2000,               /* Conservative per-frame */
    .screen_width = PSX_SCREEN_WIDTH,
    .screen_height = PSX_SCREEN_HEIGHT,
    .has_analog_sticks = 1,             /* DualShock */
    .has_triggers = 0,                  /* Digital shoulder buttons */
    .has_rumble = 1,                    /* DualShock */
    .color_depth = 16
};

/*============================================================================
 * Platform Core Functions
 *============================================================================*/

const platform_caps_t *platform_get_caps(void) {
    return &psx_caps;
}

int platform_init(void) {
    /* Reset GPU and enable interrupts */
    ResetGraph(0);

    /* Initialize geometry transformation engine */
    InitGeom();
    SetGeomOffset(PSX_SCREEN_WIDTH / 2, PSX_SCREEN_HEIGHT / 2);
    SetGeomScreen(PSX_SCREEN_WIDTH);

    /* Setup double buffer */
    SetDefDispEnv(&psx_state.disp[0], FRAME_BUFFER_0_X, FRAME_BUFFER_0_Y,
                  PSX_SCREEN_WIDTH, PSX_SCREEN_HEIGHT);
    SetDefDispEnv(&psx_state.disp[1], FRAME_BUFFER_1_X, FRAME_BUFFER_1_Y,
                  PSX_SCREEN_WIDTH, PSX_SCREEN_HEIGHT);

    SetDefDrawEnv(&psx_state.draw[0], FRAME_BUFFER_1_X, FRAME_BUFFER_1_Y,
                  PSX_SCREEN_WIDTH, PSX_SCREEN_HEIGHT);
    SetDefDrawEnv(&psx_state.draw[1], FRAME_BUFFER_0_X, FRAME_BUFFER_0_Y,
                  PSX_SCREEN_WIDTH, PSX_SCREEN_HEIGHT);

    /* Enable background clear */
    psx_state.draw[0].isbg = 1;
    psx_state.draw[1].isbg = 1;
    setRGB0(&psx_state.draw[0], 0, 0, 0);
    setRGB0(&psx_state.draw[1], 0, 0, 0);

    psx_state.db = 0;

    /* Initialize CD-ROM */
    CdInit();

    /* Initialize controller */
    InitPAD(psx_state.padbuf[0], 34, psx_state.padbuf[1], 34);
    StartPAD();
    ChangeClearPAD(1);

    /* Enable display */
    SetDispMask(1);

    psx_state.frame_counter = 0;

    return 1;
}

void platform_shutdown(void) {
    StopPAD();
    StopCallback();
}

uint64_t platform_get_time_us(void) {
    /* PSX runs at ~33.8688 MHz, VSync at ~60Hz (NTSC) or ~50Hz (PAL) */
    /* Root counter 2 can be used for timing */
    /* Approximate using vsync count */
    return (uint64_t)psx_state.frame_counter * 16667; /* ~60fps = 16.667ms/frame */
}

void platform_sleep_ms(uint32_t ms) {
    int frames = (ms * 60) / 1000;
    if (frames < 1) frames = 1;
    while (frames-- > 0) {
        VSync(0);
    }
}

void platform_exit(void) {
    /* Return to shell/BIOS */
    StopPAD();
    StopCallback();
    ResetGraph(3);
    /* No standard way to exit on PSX, typically requires reset */
}

/*============================================================================
 * Memory Management
 *============================================================================*/

/* PSX has very limited RAM, use simple allocation */
static char heap_buffer[512 * 1024];  /* 512KB heap */
static size_t heap_used = 0;

void *platform_malloc(size_t size) {
    /* Align to 4 bytes */
    size = (size + 3) & ~3;

    if (heap_used + size > sizeof(heap_buffer)) {
        return NULL;
    }

    void *ptr = &heap_buffer[heap_used];
    heap_used += size;
    return ptr;
}

void *platform_malloc_aligned(size_t size, size_t alignment) {
    /* Simple aligned allocation */
    size_t align_mask = alignment - 1;
    heap_used = (heap_used + align_mask) & ~align_mask;
    return platform_malloc(size);
}

void *platform_realloc(void *ptr, size_t size) {
    /* Simple allocator doesn't support realloc */
    (void)ptr;
    return platform_malloc(size);
}

void platform_free(void *ptr) {
    /* Simple bump allocator - no individual free */
    (void)ptr;
}

void *platform_vram_alloc(size_t size) {
    /* VRAM is managed by GPU primitives, not direct allocation */
    (void)size;
    return NULL;
}

void platform_vram_free(void *ptr) {
    (void)ptr;
}

/*============================================================================
 * Graphics Implementation
 *============================================================================*/

void platform_gfx_init(void) {
    /* Graphics already initialized in platform_init */
    psx_state.nextpri = psx_state.primbuff[0];
}

void platform_gfx_shutdown(void) {
    /* Nothing to do */
}

void platform_gfx_begin_frame(void) {
    /* Clear ordering table */
    ClearOTagR(psx_state.ot[psx_state.db], OT_LENGTH);

    /* Reset primitive pointer */
    psx_state.nextpri = psx_state.primbuff[psx_state.db];
}

void platform_gfx_end_frame(void) {
    /* Wait for GPU to finish */
    DrawSync(0);

    /* Wait for VSync */
    VSync(0);
    psx_state.frame_counter++;

    /* Swap buffers */
    PutDispEnv(&psx_state.disp[psx_state.db]);
    PutDrawEnv(&psx_state.draw[psx_state.db]);

    /* Draw ordering table */
    DrawOTag(&psx_state.ot[psx_state.db][OT_LENGTH - 1]);

    /* Swap double buffer index */
    psx_state.db ^= 1;
}

void platform_gfx_set_render_list(render_list_t list) {
    /* PSX uses ordering table for depth sorting, no separate lists */
    (void)list;
}

void platform_gfx_clear(uint32_t color) {
    u_char r = (color >> 16) & 0xFF;
    u_char g = (color >> 8) & 0xFF;
    u_char b = color & 0xFF;

    setRGB0(&psx_state.draw[0], r, g, b);
    setRGB0(&psx_state.draw[1], r, g, b);
}

/*============================================================================
 * PSX-Specific Rendering Primitives
 *============================================================================*/

/* Add flat-shaded triangle to ordering table */
void psx_draw_tri_flat(int x0, int y0, int x1, int y1, int x2, int y2,
                        u_char r, u_char g, u_char b, int z) {
    POLY_F3 *poly = (POLY_F3 *)psx_state.nextpri;

    setPolyF3(poly);
    setRGB0(poly, r, g, b);
    setXY3(poly, x0, y0, x1, y1, x2, y2);

    addPrim(&psx_state.ot[psx_state.db][z], poly);
    psx_state.nextpri += sizeof(POLY_F3);
}

/* Add gouraud-shaded triangle */
void psx_draw_tri_gouraud(int x0, int y0, int x1, int y1, int x2, int y2,
                           u_char r0, u_char g0, u_char b0,
                           u_char r1, u_char g1, u_char b1,
                           u_char r2, u_char g2, u_char b2, int z) {
    POLY_G3 *poly = (POLY_G3 *)psx_state.nextpri;

    setPolyG3(poly);
    setRGB0(poly, r0, g0, b0);
    setRGB1(poly, r1, g1, b1);
    setRGB2(poly, r2, g2, b2);
    setXY3(poly, x0, y0, x1, y1, x2, y2);

    addPrim(&psx_state.ot[psx_state.db][z], poly);
    psx_state.nextpri += sizeof(POLY_G3);
}

/* Add flat-shaded quad */
void psx_draw_quad_flat(int x0, int y0, int x1, int y1, int x2, int y2, int x3, int y3,
                         u_char r, u_char g, u_char b, int z) {
    POLY_F4 *poly = (POLY_F4 *)psx_state.nextpri;

    setPolyF4(poly);
    setRGB0(poly, r, g, b);
    setXY4(poly, x0, y0, x1, y1, x2, y2, x3, y3);

    addPrim(&psx_state.ot[psx_state.db][z], poly);
    psx_state.nextpri += sizeof(POLY_F4);
}

/* Add 2D sprite/rectangle */
void psx_draw_rect(int x, int y, int w, int h, u_char r, u_char g, u_char b) {
    TILE *tile = (TILE *)psx_state.nextpri;

    setTile(tile);
    setRGB0(tile, r, g, b);
    setXY0(tile, x, y);
    setWH(tile, w, h);

    addPrim(&psx_state.ot[psx_state.db][0], tile);  /* Front of OT for HUD */
    psx_state.nextpri += sizeof(TILE);
}

/*============================================================================
 * Texture Support
 *============================================================================*/

/* Texture page locations in VRAM */
static int tex_page_x = 640;
static int tex_page_y = 0;

platform_texture_t *platform_texture_create(uint16_t w, uint16_t h, tex_format_t fmt) {
    platform_texture_t *tex = platform_malloc(sizeof(platform_texture_t));
    if (!tex) return NULL;

    tex->width = w;
    tex->height = h;
    tex->format = fmt;
    tex->data = NULL;

    /* Allocate texture page in VRAM */
    /* PSX textures must be in specific VRAM locations */
    tex->platform_handle = GetTPage(
        (fmt == TEX_FMT_ARGB4444) ? 2 : 1,  /* 0=4bit, 1=8bit, 2=16bit */
        0,  /* Semi-transparency mode */
        tex_page_x, tex_page_y
    );

    /* Move to next texture page position */
    tex_page_x += w;
    if (tex_page_x >= PSX_VRAM_WIDTH) {
        tex_page_x = 640;
        tex_page_y += h;
    }

    return tex;
}

void platform_texture_upload(platform_texture_t *tex, const void *data) {
    if (!tex || !data) return;

    RECT rect;
    rect.x = tex_page_x;
    rect.y = tex_page_y;
    rect.w = tex->width;
    rect.h = tex->height;

    LoadImage(&rect, (u_long *)data);
    DrawSync(0);
}

void platform_texture_destroy(platform_texture_t *tex) {
    if (tex) {
        platform_free(tex);
    }
}

void platform_texture_bind(platform_texture_t *tex) {
    /* Textures are bound per-primitive on PSX */
    (void)tex;
}

/*============================================================================
 * Audio Implementation
 *============================================================================*/

static int spu_initialized = 0;

void platform_audio_init(void) {
    SpuInit();
    SpuSetCommonMasterVolume(0x3FFF, 0x3FFF);
    spu_initialized = 1;
}

void platform_audio_shutdown(void) {
    if (spu_initialized) {
        SpuSetKey(0, 0xFFFFFF);  /* Stop all voices */
        spu_initialized = 0;
    }
}

void platform_audio_update(void) {
    /* SPU handles audio playback automatically */
}

platform_audio_t *platform_audio_load(const char *path) {
    /* Load VAG file from CD */
    platform_audio_t *audio = platform_malloc(sizeof(platform_audio_t));
    if (!audio) return NULL;

    /* TODO: Implement CD file loading */
    audio->data = NULL;
    audio->size = 0;
    audio->sample_rate = 44100;
    audio->format = AUDIO_FMT_VAG;
    audio->channels = 1;
    audio->platform_handle = 0;

    (void)path;
    return audio;
}

void platform_audio_unload(platform_audio_t *audio) {
    if (audio) {
        platform_free(audio);
    }
}

void platform_audio_play(platform_audio_t *audio, int loop) {
    if (!audio || !audio->data) return;

    /* Configure SPU voice */
    SpuVoiceAttr attr;
    attr.voice = SPU_0CH;
    attr.mask = SPU_VOICE_VOLL | SPU_VOICE_VOLR | SPU_VOICE_PITCH |
                SPU_VOICE_WDSA | SPU_VOICE_ADSR_AMODE | SPU_VOICE_ADSR_SMODE |
                SPU_VOICE_ADSR_RMODE | SPU_VOICE_ADSR_AR | SPU_VOICE_ADSR_DR |
                SPU_VOICE_ADSR_SR | SPU_VOICE_ADSR_RR | SPU_VOICE_ADSR_SL;

    attr.volume.left = 0x3FFF;
    attr.volume.right = 0x3FFF;
    attr.pitch = 0x1000;  /* 44100 Hz */
    attr.addr = (u_long)audio->data;
    attr.a_mode = SPU_VOICE_LINEARIncN;
    attr.s_mode = SPU_VOICE_LINEARIncN;
    attr.r_mode = SPU_VOICE_LINEARDecN;
    attr.ar = 0;
    attr.dr = 0;
    attr.sr = 0;
    attr.rr = 0;
    attr.sl = 0xF;

    SpuSetVoiceAttr(&attr);
    SpuSetKey(SPU_ON, SPU_0CH);

    (void)loop;
}

void platform_audio_stop(platform_audio_t *audio) {
    if (!audio) return;
    SpuSetKey(SPU_OFF, SPU_0CH);
}

void platform_audio_set_volume(int volume) {
    int vol = (volume * 0x3FFF) / 100;
    SpuSetCommonMasterVolume(vol, vol);
}

/*============================================================================
 * File I/O (CD-ROM)
 *============================================================================*/

struct platform_file {
    CdlFILE cd_file;
    u_char *buffer;
    int size;
    int pos;
    int is_open;
};

platform_file_t *platform_file_open(const char *path, const char *mode) {
    platform_file_t *file = platform_malloc(sizeof(platform_file_t));
    if (!file) return NULL;

    /* Convert path to PSX format (;1 suffix) */
    char cd_path[64];
    snprintf(cd_path, sizeof(cd_path), "\\%s;1", path);

    /* Convert to uppercase */
    for (char *p = cd_path; *p; p++) {
        if (*p >= 'a' && *p <= 'z') *p -= 32;
        if (*p == '/') *p = '\\';
    }

    if (!CdSearchFile(&file->cd_file, cd_path)) {
        platform_free(file);
        return NULL;
    }

    file->size = file->cd_file.size;
    file->pos = 0;
    file->buffer = NULL;
    file->is_open = 1;

    (void)mode;
    return file;
}

void platform_file_close(platform_file_t *file) {
    if (file) {
        if (file->buffer) {
            platform_free(file->buffer);
        }
        platform_free(file);
    }
}

size_t platform_file_read(platform_file_t *file, void *buf, size_t size) {
    if (!file || !file->is_open) return 0;

    /* Read sectors from CD */
    int sectors = (size + 2047) / 2048;
    int start_sector = file->cd_file.pos + (file->pos / 2048);

    CdReadFile((char *)buf, sectors);
    CdReadSync(0, NULL);

    size_t bytes_read = size;
    if (file->pos + size > (size_t)file->size) {
        bytes_read = file->size - file->pos;
    }

    file->pos += bytes_read;
    return bytes_read;
}

size_t platform_file_write(platform_file_t *file, const void *buf, size_t size) {
    /* CD-ROM is read-only */
    (void)file; (void)buf; (void)size;
    return 0;
}

int platform_file_seek(platform_file_t *file, long offset, int whence) {
    if (!file) return -1;

    switch (whence) {
        case 0: /* SEEK_SET */
            file->pos = offset;
            break;
        case 1: /* SEEK_CUR */
            file->pos += offset;
            break;
        case 2: /* SEEK_END */
            file->pos = file->size + offset;
            break;
    }

    if (file->pos < 0) file->pos = 0;
    if (file->pos > file->size) file->pos = file->size;

    return 0;
}

long platform_file_tell(platform_file_t *file) {
    return file ? file->pos : -1;
}

int platform_file_exists(const char *path) {
    CdlFILE file;
    char cd_path[64];
    snprintf(cd_path, sizeof(cd_path), "\\%s;1", path);
    return CdSearchFile(&file, cd_path) != 0;
}

const char *platform_get_data_path(void) {
    return "\\";
}

const char *platform_get_save_path(void) {
    return "bu00:";  /* Memory card slot 1 */
}

/*============================================================================
 * PSX Controller Input
 *============================================================================*/

uint32_t psx_get_buttons(int port) {
    PADTYPE *pad = (PADTYPE *)psx_state.padbuf[port];
    uint32_t buttons = 0;

    if (pad->stat != 0) return 0;  /* Not connected */

    /* Digital buttons (active low, invert) */
    uint16_t raw = ~pad->btn;

    if (raw & PAD_CROSS)    buttons |= PLAT_BTN_CROSS;
    if (raw & PAD_CIRCLE)   buttons |= PLAT_BTN_CIRCLE;
    if (raw & PAD_SQUARE)   buttons |= PLAT_BTN_SQUARE;
    if (raw & PAD_TRIANGLE) buttons |= PLAT_BTN_TRIANGLE;
    if (raw & PAD_START)    buttons |= PLAT_BTN_START;
    if (raw & PAD_SELECT)   buttons |= PLAT_BTN_SELECT;
    if (raw & PAD_L1)       buttons |= PLAT_BTN_L1;
    if (raw & PAD_R1)       buttons |= PLAT_BTN_R1;
    if (raw & PAD_L2)       buttons |= PLAT_BTN_L2;
    if (raw & PAD_R2)       buttons |= PLAT_BTN_R2;
    if (raw & PAD_UP)       buttons |= PLAT_BTN_DPAD_UP;
    if (raw & PAD_DOWN)     buttons |= PLAT_BTN_DPAD_DOWN;
    if (raw & PAD_LEFT)     buttons |= PLAT_BTN_DPAD_LEFT;
    if (raw & PAD_RIGHT)    buttons |= PLAT_BTN_DPAD_RIGHT;

    /* Analog stick buttons (DualShock) */
    if (pad->type == PAD_ID_ANALOG || pad->type == PAD_ID_ANALOG_STICK) {
        if (raw & PAD_L3) buttons |= PLAT_BTN_L3;
        if (raw & PAD_R3) buttons |= PLAT_BTN_R3;
    }

    return buttons;
}

void psx_get_analog(int port, int *lx, int *ly, int *rx, int *ry) {
    PADTYPE *pad = (PADTYPE *)psx_state.padbuf[port];

    if (pad->stat != 0 ||
        (pad->type != PAD_ID_ANALOG && pad->type != PAD_ID_ANALOG_STICK)) {
        *lx = *ly = *rx = *ry = 128;
        return;
    }

    *lx = pad->ls_x;
    *ly = pad->ls_y;
    *rx = pad->rs_x;
    *ry = pad->rs_y;
}

#endif /* PLATFORM_PSX */
