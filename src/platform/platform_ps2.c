/*
 * RetroRacer - PlayStation 2 Platform Implementation
 *
 * Hardware Specs:
 * - CPU: 294.912 MHz MIPS R5900 (Emotion Engine)
 * - RAM: 32MB Main + 4MB Video
 * - GPU: Graphics Synthesizer (GS), ~75M polygons/sec
 * - Audio: SPU2 with 48 voices, 2MB sound RAM
 * - Media: DVD-ROM 4x speed
 *
 * SDK: PS2SDK (Open Source)
 */

#include "platform.h"

#ifdef PLATFORM_PS2

#include <kernel.h>
#include <sifrpc.h>
#include <loadfile.h>
#include <iopheap.h>
#include <sbv_patches.h>
#include <libpad.h>
#include <libmc.h>
#include <audsrv.h>
#include <dma.h>
#include <graph.h>
#include <draw.h>
#include <gs_psm.h>
#include <gs_gp.h>
#include <packet.h>
#include <dma_tags.h>
#include <gif_tags.h>

/*============================================================================
 * PS2-Specific Defines
 *============================================================================*/

#define PS2_SCREEN_WIDTH    640
#define PS2_SCREEN_HEIGHT   448
#define PS2_FRAME_SIZE      (PS2_SCREEN_WIDTH * PS2_SCREEN_HEIGHT * 4)

/* Frame buffer addresses in GS VRAM (4MB total) */
#define GS_FRAME_0          0
#define GS_FRAME_1          (PS2_FRAME_SIZE / 64)  /* In 64-byte pages */
#define GS_ZBUFFER          (GS_FRAME_1 + PS2_FRAME_SIZE / 64)
#define GS_TEXBUFFER        (GS_ZBUFFER + PS2_FRAME_SIZE / 64)

/*============================================================================
 * PS2 State
 *============================================================================*/

static struct {
    /* GS state */
    framebuffer_t fb[2];
    zbuffer_t zb;
    int current_fb;

    /* DMA packets */
    packet_t *packet;
    qword_t *dma_buf;

    /* Controller */
    char padbuf[256] __attribute__((aligned(64)));
    struct padButtonStatus pad_status;
    int pad_connected;

    /* Timing */
    uint64_t frame_counter;
    int vsync_callback_id;
} ps2_state;

/* Platform capabilities */
static const platform_caps_t ps2_caps = {
    .name = "PlayStation 2",
    .ram_size = 32 * 1024 * 1024,       /* 32MB */
    .vram_size = 4 * 1024 * 1024,       /* 4MB */
    .max_texture_size = 1024,
    .max_vertices = 100000,             /* GS is very fast */
    .screen_width = PS2_SCREEN_WIDTH,
    .screen_height = PS2_SCREEN_HEIGHT,
    .has_analog_sticks = 1,
    .has_triggers = 1,                  /* Pressure sensitive */
    .has_rumble = 1,
    .color_depth = 32
};

/*============================================================================
 * Platform Core Functions
 *============================================================================*/

const platform_caps_t *platform_get_caps(void) {
    return &ps2_caps;
}

/* VSync callback */
static int vsync_handler(void) {
    ps2_state.frame_counter++;
    ExitHandler();
    return 0;
}

int platform_init(void) {
    /* Initialize SIF RPC (EE <-> IOP communication) */
    SifInitRpc(0);

    /* Apply SBV patches for loading IRX modules */
    sbv_patch_enable_lmb();
    sbv_patch_disable_prefix_check();

    /* Load required IOP modules */
    SifLoadModule("rom0:SIO2MAN", 0, NULL);
    SifLoadModule("rom0:MCMAN", 0, NULL);
    SifLoadModule("rom0:MCSERV", 0, NULL);
    SifLoadModule("rom0:PADMAN", 0, NULL);

    /* Initialize DMA controller */
    dma_channel_initialize(DMA_CHANNEL_GIF, NULL, 0);
    dma_channel_fast_waits(DMA_CHANNEL_GIF);

    /* Setup GS video mode */
    graph_set_mode(GRAPH_MODE_INTERLACED, GRAPH_MODE_NTSC, GRAPH_MODE_FIELD, GRAPH_ENABLE);
    graph_set_screen(0, 0, PS2_SCREEN_WIDTH, PS2_SCREEN_HEIGHT);
    graph_set_bgcolor(0, 0, 0);

    /* Setup frame buffers */
    ps2_state.fb[0].width = PS2_SCREEN_WIDTH;
    ps2_state.fb[0].height = PS2_SCREEN_HEIGHT;
    ps2_state.fb[0].psm = GS_PSM_32;
    ps2_state.fb[0].address = graph_vram_allocate(PS2_SCREEN_WIDTH, PS2_SCREEN_HEIGHT, GS_PSM_32, GRAPH_ALIGN_PAGE);

    ps2_state.fb[1].width = PS2_SCREEN_WIDTH;
    ps2_state.fb[1].height = PS2_SCREEN_HEIGHT;
    ps2_state.fb[1].psm = GS_PSM_32;
    ps2_state.fb[1].address = graph_vram_allocate(PS2_SCREEN_WIDTH, PS2_SCREEN_HEIGHT, GS_PSM_32, GRAPH_ALIGN_PAGE);

    /* Setup Z buffer */
    ps2_state.zb.enable = DRAW_ENABLE;
    ps2_state.zb.method = ZTEST_METHOD_GREATER_EQUAL;
    ps2_state.zb.zsm = GS_ZBUF_24;
    ps2_state.zb.address = graph_vram_allocate(PS2_SCREEN_WIDTH, PS2_SCREEN_HEIGHT, GS_ZBUF_24, GRAPH_ALIGN_PAGE);
    ps2_state.zb.mask = 0;

    ps2_state.current_fb = 0;

    /* Create DMA packet */
    ps2_state.packet = packet_init(100, PACKET_NORMAL);

    /* Initialize controller */
    padInit(0);
    padPortOpen(0, 0, ps2_state.padbuf);
    ps2_state.pad_connected = 0;

    /* Setup vsync callback */
    ps2_state.frame_counter = 0;
    ps2_state.vsync_callback_id = graph_add_vsync_handler(vsync_handler);

    /* Enable display */
    graph_enable_output();

    return 1;
}

void platform_shutdown(void) {
    graph_remove_vsync_handler(ps2_state.vsync_callback_id);
    packet_free(ps2_state.packet);
    padPortClose(0, 0);
    padEnd();
    dma_channel_shutdown(DMA_CHANNEL_GIF, 0);
}

uint64_t platform_get_time_us(void) {
    /* EE timer or approximate from vsync */
    return ps2_state.frame_counter * 16667;  /* ~60fps NTSC */
}

void platform_sleep_ms(uint32_t ms) {
    /* Convert to vsyncs */
    int frames = (ms * 60) / 1000;
    if (frames < 1) frames = 1;
    while (frames-- > 0) {
        graph_wait_vsync();
    }
}

void platform_exit(void) {
    /* Return to browser/OSD */
    SifExitRpc();
    Exit(0);
}

/*============================================================================
 * Memory Management
 *============================================================================*/

void *platform_malloc(size_t size) {
    return memalign(64, size);  /* 64-byte aligned for DMA */
}

void *platform_malloc_aligned(size_t size, size_t alignment) {
    return memalign(alignment, size);
}

void *platform_realloc(void *ptr, size_t size) {
    /* PS2SDK may not have realloc, implement manually */
    void *new_ptr = platform_malloc(size);
    if (new_ptr && ptr) {
        /* Copy old data - unsafe without knowing old size */
        /* In practice, track sizes or avoid realloc */
    }
    if (ptr) platform_free(ptr);
    return new_ptr;
}

void platform_free(void *ptr) {
    free(ptr);
}

void *platform_vram_alloc(size_t size) {
    /* GS VRAM is managed by graph_vram_allocate */
    (void)size;
    return NULL;
}

void platform_vram_free(void *ptr) {
    (void)ptr;
}

/*============================================================================
 * Graphics Implementation (GS)
 *============================================================================*/

void platform_gfx_init(void) {
    /* GS already initialized in platform_init */
}

void platform_gfx_shutdown(void) {
    /* Nothing extra needed */
}

void platform_gfx_begin_frame(void) {
    /* Reset packet */
    packet_reset(ps2_state.packet);
    ps2_state.dma_buf = ps2_state.packet->data;

    /* Setup drawing environment */
    qword_t *q = ps2_state.dma_buf;

    PACK_GIFTAG(q, GIF_SET_TAG(1, 1, GIF_PRE_DISABLE, 0, GIF_FLG_PACKED, 1),
                GIF_REG_AD);
    q++;

    PACK_GIFTAG(q, GS_SET_FRAME(ps2_state.fb[ps2_state.current_fb].address >> 11,
                                 ps2_state.fb[ps2_state.current_fb].width >> 6,
                                 ps2_state.fb[ps2_state.current_fb].psm, 0),
                GS_REG_FRAME);
    q++;

    ps2_state.dma_buf = q;
}

void platform_gfx_end_frame(void) {
    /* Flush DMA packet */
    dma_channel_send_normal(DMA_CHANNEL_GIF, ps2_state.packet->data,
                            ps2_state.dma_buf - ps2_state.packet->data, 0, 0);
    dma_channel_wait(DMA_CHANNEL_GIF, 0);

    /* Wait for vsync and flip */
    graph_wait_vsync();
    graph_set_framebuffer_filtered(ps2_state.fb[ps2_state.current_fb].address,
                                    ps2_state.fb[ps2_state.current_fb].width,
                                    ps2_state.fb[ps2_state.current_fb].psm, 0, 0);

    ps2_state.current_fb ^= 1;
}

void platform_gfx_set_render_list(render_list_t list) {
    /* PS2 GS doesn't have hardware render lists like PVR */
    /* Handle transparency via alpha blending setup */
    (void)list;
}

void platform_gfx_clear(uint32_t color) {
    qword_t *q = ps2_state.dma_buf;

    u8 r = (color >> 16) & 0xFF;
    u8 g = (color >> 8) & 0xFF;
    u8 b = color & 0xFF;
    u8 a = (color >> 24) & 0xFF;

    /* Clear screen using a full-screen sprite */
    PACK_GIFTAG(q, GIF_SET_TAG(4, 1, GIF_PRE_ENABLE, GS_SET_PRIM(GS_PRIM_SPRITE, 0, 0, 0, 0, 0, 0, 0, 0),
                               GIF_FLG_PACKED, 4),
                GIF_REG_RGBAQ | (GIF_REG_XYZ2 << 4) | (GIF_REG_RGBAQ << 8) | (GIF_REG_XYZ2 << 12));
    q++;

    PACK_GIFTAG(q, GS_SET_RGBAQ(r, g, b, a, 0x3F800000), 0);
    q++;

    PACK_GIFTAG(q, GS_SET_XYZ(0 << 4, 0 << 4, 0), 0);
    q++;

    PACK_GIFTAG(q, GS_SET_RGBAQ(r, g, b, a, 0x3F800000), 0);
    q++;

    PACK_GIFTAG(q, GS_SET_XYZ(PS2_SCREEN_WIDTH << 4, PS2_SCREEN_HEIGHT << 4, 0), 0);
    q++;

    /* Clear Z buffer */
    PACK_GIFTAG(q, GIF_SET_TAG(1, 1, GIF_PRE_DISABLE, 0, GIF_FLG_PACKED, 1),
                GIF_REG_AD);
    q++;

    PACK_GIFTAG(q, GS_SET_ZBUF(ps2_state.zb.address >> 11, ps2_state.zb.zsm, 0),
                GS_REG_ZBUF);
    q++;

    ps2_state.dma_buf = q;
}

/*============================================================================
 * PS2-Specific Rendering
 *============================================================================*/

/* Draw flat-shaded triangle */
void ps2_draw_tri_flat(float x0, float y0, float z0,
                        float x1, float y1, float z1,
                        float x2, float y2, float z2,
                        u8 r, u8 g, u8 b, u8 a) {
    qword_t *q = ps2_state.dma_buf;

    /* Convert to GS fixed point (4 bits sub-pixel) */
    int ix0 = (int)(x0 * 16.0f) + (2048 << 4);
    int iy0 = (int)(y0 * 16.0f) + (2048 << 4);
    int ix1 = (int)(x1 * 16.0f) + (2048 << 4);
    int iy1 = (int)(y1 * 16.0f) + (2048 << 4);
    int ix2 = (int)(x2 * 16.0f) + (2048 << 4);
    int iy2 = (int)(y2 * 16.0f) + (2048 << 4);
    int iz0 = (int)(z0 * 65535.0f);
    int iz1 = (int)(z1 * 65535.0f);
    int iz2 = (int)(z2 * 65535.0f);

    PACK_GIFTAG(q, GIF_SET_TAG(4, 1, GIF_PRE_ENABLE,
                               GS_SET_PRIM(GS_PRIM_TRIANGLE, 0, 0, 0, 0, 0, 0, 0, 0),
                               GIF_FLG_PACKED, 4),
                GIF_REG_RGBAQ | (GIF_REG_XYZ2 << 4) | (GIF_REG_XYZ2 << 8) | (GIF_REG_XYZ2 << 12));
    q++;

    PACK_GIFTAG(q, GS_SET_RGBAQ(r, g, b, a, 0x3F800000), 0);
    q++;

    PACK_GIFTAG(q, GS_SET_XYZ(ix0, iy0, iz0), 0);
    q++;

    PACK_GIFTAG(q, GS_SET_XYZ(ix1, iy1, iz1), 0);
    q++;

    PACK_GIFTAG(q, GS_SET_XYZ(ix2, iy2, iz2), 0);
    q++;

    ps2_state.dma_buf = q;
}

/* Draw gouraud-shaded triangle */
void ps2_draw_tri_gouraud(float x0, float y0, float z0, u8 r0, u8 g0, u8 b0,
                           float x1, float y1, float z1, u8 r1, u8 g1, u8 b1,
                           float x2, float y2, float z2, u8 r2, u8 g2, u8 b2,
                           u8 a) {
    qword_t *q = ps2_state.dma_buf;

    int ix0 = (int)(x0 * 16.0f) + (2048 << 4);
    int iy0 = (int)(y0 * 16.0f) + (2048 << 4);
    int ix1 = (int)(x1 * 16.0f) + (2048 << 4);
    int iy1 = (int)(y1 * 16.0f) + (2048 << 4);
    int ix2 = (int)(x2 * 16.0f) + (2048 << 4);
    int iy2 = (int)(y2 * 16.0f) + (2048 << 4);
    int iz0 = (int)(z0 * 65535.0f);
    int iz1 = (int)(z1 * 65535.0f);
    int iz2 = (int)(z2 * 65535.0f);

    PACK_GIFTAG(q, GIF_SET_TAG(6, 1, GIF_PRE_ENABLE,
                               GS_SET_PRIM(GS_PRIM_TRIANGLE, 1, 0, 0, 0, 0, 0, 0, 0),
                               GIF_FLG_PACKED, 2),
                GIF_REG_RGBAQ | (GIF_REG_XYZ2 << 4));
    q++;

    PACK_GIFTAG(q, GS_SET_RGBAQ(r0, g0, b0, a, 0x3F800000), 0);
    q++;
    PACK_GIFTAG(q, GS_SET_XYZ(ix0, iy0, iz0), 0);
    q++;

    PACK_GIFTAG(q, GS_SET_RGBAQ(r1, g1, b1, a, 0x3F800000), 0);
    q++;
    PACK_GIFTAG(q, GS_SET_XYZ(ix1, iy1, iz1), 0);
    q++;

    PACK_GIFTAG(q, GS_SET_RGBAQ(r2, g2, b2, a, 0x3F800000), 0);
    q++;
    PACK_GIFTAG(q, GS_SET_XYZ(ix2, iy2, iz2), 0);
    q++;

    ps2_state.dma_buf = q;
}

/*============================================================================
 * Texture Support
 *============================================================================*/

static u32 tex_vram_ptr = 0;

platform_texture_t *platform_texture_create(uint16_t w, uint16_t h, tex_format_t fmt) {
    platform_texture_t *tex = platform_malloc(sizeof(platform_texture_t));
    if (!tex) return NULL;

    tex->width = w;
    tex->height = h;
    tex->format = fmt;
    tex->data = platform_malloc(w * h * 4);

    /* Allocate VRAM */
    int psm = (fmt == TEX_FMT_ARGB8888) ? GS_PSM_32 : GS_PSM_16;
    tex->platform_handle = graph_vram_allocate(w, h, psm, GRAPH_ALIGN_BLOCK);

    return tex;
}

void platform_texture_upload(platform_texture_t *tex, const void *data) {
    if (!tex || !data) return;

    /* Copy to texture data buffer */
    int size = tex->width * tex->height * ((tex->format == TEX_FMT_ARGB8888) ? 4 : 2);
    memcpy(tex->data, data, size);

    /* Upload to GS VRAM via DMA */
    /* This would use GS packet to transfer texture data */
    /* Simplified for this implementation */
}

void platform_texture_destroy(platform_texture_t *tex) {
    if (tex) {
        if (tex->data) platform_free(tex->data);
        platform_free(tex);
    }
}

void platform_texture_bind(platform_texture_t *tex) {
    if (!tex) return;

    qword_t *q = ps2_state.dma_buf;

    int psm = (tex->format == TEX_FMT_ARGB8888) ? GS_PSM_32 : GS_PSM_16;
    int tw = 0, th = 0;
    for (int i = 0; i < 10; i++) {
        if ((1 << i) >= tex->width) { tw = i; break; }
    }
    for (int i = 0; i < 10; i++) {
        if ((1 << i) >= tex->height) { th = i; break; }
    }

    PACK_GIFTAG(q, GIF_SET_TAG(1, 1, GIF_PRE_DISABLE, 0, GIF_FLG_PACKED, 1),
                GIF_REG_AD);
    q++;

    PACK_GIFTAG(q, GS_SET_TEX0(tex->platform_handle >> 6, tex->width >> 6, psm,
                               tw, th, 1, 0, 0, 0, 0, 0, 0),
                GS_REG_TEX0);
    q++;

    ps2_state.dma_buf = q;
}

/*============================================================================
 * Audio Implementation (AUDSRV)
 *============================================================================*/

void platform_audio_init(void) {
    /* Load AUDSRV module */
    SifLoadModule("rom0:LIBSD", 0, NULL);
    audsrv_init();
}

void platform_audio_shutdown(void) {
    audsrv_quit();
}

void platform_audio_update(void) {
    /* AUDSRV handles streaming automatically */
}

platform_audio_t *platform_audio_load(const char *path) {
    platform_audio_t *audio = platform_malloc(sizeof(platform_audio_t));
    if (!audio) return NULL;

    /* TODO: Load ADPCM file */
    audio->data = NULL;
    audio->size = 0;
    audio->sample_rate = 48000;
    audio->format = AUDIO_FMT_ADPCM;
    audio->channels = 2;
    audio->platform_handle = 0;

    (void)path;
    return audio;
}

void platform_audio_unload(platform_audio_t *audio) {
    if (audio) {
        if (audio->data) platform_free(audio->data);
        platform_free(audio);
    }
}

void platform_audio_play(platform_audio_t *audio, int loop) {
    if (!audio || !audio->data) return;

    struct audsrv_fmt_t format;
    format.bits = 16;
    format.freq = audio->sample_rate;
    format.channels = audio->channels;

    audsrv_set_format(&format);
    audsrv_play_audio((char *)audio->data, audio->size);

    (void)loop;
}

void platform_audio_stop(platform_audio_t *audio) {
    (void)audio;
    audsrv_stop_audio();
}

void platform_audio_set_volume(int volume) {
    audsrv_set_volume(volume);
}

/*============================================================================
 * File I/O
 *============================================================================*/

struct platform_file {
    int fd;
    int is_open;
};

platform_file_t *platform_file_open(const char *path, const char *mode) {
    platform_file_t *file = platform_malloc(sizeof(platform_file_t));
    if (!file) return NULL;

    int flags = O_RDONLY;
    if (mode[0] == 'w') flags = O_WRONLY | O_CREAT | O_TRUNC;
    else if (mode[0] == 'a') flags = O_WRONLY | O_CREAT | O_APPEND;

    /* Prepend device path */
    char full_path[256];
    if (path[0] != '/') {
        snprintf(full_path, sizeof(full_path), "cdfs:/%s", path);
    } else {
        snprintf(full_path, sizeof(full_path), "cdfs:%s", path);
    }

    file->fd = open(full_path, flags);
    if (file->fd < 0) {
        platform_free(file);
        return NULL;
    }

    file->is_open = 1;
    return file;
}

void platform_file_close(platform_file_t *file) {
    if (file) {
        if (file->is_open) close(file->fd);
        platform_free(file);
    }
}

size_t platform_file_read(platform_file_t *file, void *buf, size_t size) {
    if (!file || !file->is_open) return 0;
    return read(file->fd, buf, size);
}

size_t platform_file_write(platform_file_t *file, const void *buf, size_t size) {
    if (!file || !file->is_open) return 0;
    return write(file->fd, buf, size);
}

int platform_file_seek(platform_file_t *file, long offset, int whence) {
    if (!file || !file->is_open) return -1;
    return lseek(file->fd, offset, whence) >= 0 ? 0 : -1;
}

long platform_file_tell(platform_file_t *file) {
    if (!file || !file->is_open) return -1;
    return lseek(file->fd, 0, SEEK_CUR);
}

int platform_file_exists(const char *path) {
    char full_path[256];
    snprintf(full_path, sizeof(full_path), "cdfs:/%s", path);
    int fd = open(full_path, O_RDONLY);
    if (fd >= 0) {
        close(fd);
        return 1;
    }
    return 0;
}

const char *platform_get_data_path(void) {
    return "cdfs:/";
}

const char *platform_get_save_path(void) {
    return "mc0:/RETRORACR/";
}

/*============================================================================
 * PS2 Controller Input
 *============================================================================*/

uint32_t ps2_get_buttons(int port) {
    struct padButtonStatus buttons;
    int state = padGetState(port, 0);

    if (state != PAD_STATE_STABLE && state != PAD_STATE_FINDCTP1) {
        return 0;
    }

    if (padRead(port, 0, &buttons) == 0) {
        return 0;
    }

    uint32_t result = 0;
    uint16_t raw = ~buttons.btns;

    if (raw & PAD_CROSS)    result |= PLAT_BTN_CROSS;
    if (raw & PAD_CIRCLE)   result |= PLAT_BTN_CIRCLE;
    if (raw & PAD_SQUARE)   result |= PLAT_BTN_SQUARE;
    if (raw & PAD_TRIANGLE) result |= PLAT_BTN_TRIANGLE;
    if (raw & PAD_START)    result |= PLAT_BTN_START;
    if (raw & PAD_SELECT)   result |= PLAT_BTN_SELECT;
    if (raw & PAD_L1)       result |= PLAT_BTN_L1;
    if (raw & PAD_R1)       result |= PLAT_BTN_R1;
    if (raw & PAD_L2)       result |= PLAT_BTN_L2;
    if (raw & PAD_R2)       result |= PLAT_BTN_R2;
    if (raw & PAD_L3)       result |= PLAT_BTN_L3;
    if (raw & PAD_R3)       result |= PLAT_BTN_R3;
    if (raw & PAD_UP)       result |= PLAT_BTN_DPAD_UP;
    if (raw & PAD_DOWN)     result |= PLAT_BTN_DPAD_DOWN;
    if (raw & PAD_LEFT)     result |= PLAT_BTN_DPAD_LEFT;
    if (raw & PAD_RIGHT)    result |= PLAT_BTN_DPAD_RIGHT;

    return result;
}

void ps2_get_analog(int port, int *lx, int *ly, int *rx, int *ry) {
    struct padButtonStatus buttons;

    if (padRead(port, 0, &buttons) == 0) {
        *lx = *ly = *rx = *ry = 128;
        return;
    }

    *lx = buttons.ljoy_h;
    *ly = buttons.ljoy_v;
    *rx = buttons.rjoy_h;
    *ry = buttons.rjoy_v;
}

/* Get pressure-sensitive trigger values (0-255) */
void ps2_get_pressure(int port, int *l2, int *r2) {
    struct padButtonStatus buttons;

    if (padRead(port, 0, &buttons) == 0) {
        *l2 = *r2 = 0;
        return;
    }

    *l2 = buttons.l2_p;
    *r2 = buttons.r2_p;
}

#endif /* PLATFORM_PS2 */
