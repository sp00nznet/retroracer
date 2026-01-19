/*
 * RetroRacer - Xbox 360 Platform Implementation
 *
 * Hardware Specs:
 * - CPU: 3.2 GHz PowerPC Tri-Core Xenon
 * - RAM: 512MB GDDR3 (unified)
 * - GPU: ATI Xenos (R500 based), 500 MHz
 * - Audio: Multi-channel XMA decoder
 * - Media: DVD-DL, HDD
 *
 * SDK: XDK (Xbox Development Kit) / LibXenon (Open Source)
 */

#include "platform.h"

#ifdef PLATFORM_XBOX360

#include <stdint.h>
#include <string.h>
#include <stdio.h>

/* LibXenon headers */
#include <xenos/xe.h>
#include <xenos/xenos.h>
#include <xenos/edram.h>
#include <console/console.h>
#include <input/input.h>
#include <ppc/timebase.h>
#include <diskio/ata.h>
#include <usb/usbmain.h>
#include <xenon_smc/xenon_smc.h>
#include <xenon_sound/sound.h>

/*============================================================================
 * Xbox 360-Specific Defines
 *============================================================================*/

#define X360_SCREEN_WIDTH   1280
#define X360_SCREEN_HEIGHT  720
#define X360_MAX_CONTROLLERS 4

/* Vertex buffer sizes */
#define X360_VB_SIZE        (4 * 1024 * 1024)   /* 4MB vertex buffer */
#define X360_IB_SIZE        (1 * 1024 * 1024)   /* 1MB index buffer */

/*============================================================================
 * Xbox 360 State
 *============================================================================*/

static struct {
    /* Xenos GPU context */
    struct XenosDevice *xe;
    struct XenosShader *sh_vertex;
    struct XenosShader *sh_pixel;

    /* Vertex/Index buffers */
    struct XenosVertexBuffer *vb;
    struct XenosIndexBuffer *ib;
    void *vb_data;
    int vb_offset;

    /* Frame buffer */
    struct XenosSurface *fb;

    /* Controller state */
    struct controller_data_s pad_data[X360_MAX_CONTROLLERS];

    /* Timing */
    uint64_t start_time;

    /* Running state */
    int running;
} x360_state;

/* Platform capabilities */
static const platform_caps_t x360_caps = {
    .name = "Xbox 360",
    .ram_size = 512 * 1024 * 1024,      /* 512MB unified */
    .vram_size = 512 * 1024 * 1024,     /* Unified */
    .max_texture_size = 8192,
    .max_vertices = 2000000,
    .screen_width = X360_SCREEN_WIDTH,
    .screen_height = X360_SCREEN_HEIGHT,
    .has_analog_sticks = 1,
    .has_triggers = 1,
    .has_rumble = 1,
    .color_depth = 32
};

/*============================================================================
 * Vertex Shader (Xbox 360 uses shaders for everything)
 *============================================================================*/

/* Simple vertex shader for transformed & lit vertices */
static const char *vertex_shader_src =
    "xvs.1.1\n"
    "dp4 oPos.x, v0, c0\n"
    "dp4 oPos.y, v0, c1\n"
    "dp4 oPos.z, v0, c2\n"
    "dp4 oPos.w, v0, c3\n"
    "mov oD0, v1\n"
    "mov oT0, v2\n";

/* Simple pixel shader */
static const char *pixel_shader_src =
    "xps.1.1\n"
    "tex t0\n"
    "mul r0, t0, v0\n";

/*============================================================================
 * Platform Core Functions
 *============================================================================*/

const platform_caps_t *platform_get_caps(void) {
    return &x360_caps;
}

int platform_init(void) {
    /* Initialize Xenos GPU */
    xenos_init(VIDEO_MODE_AUTO);

    x360_state.xe = &xe;  /* Global Xenos device */

    /* Wait for GPU to be ready */
    Xe_SetRenderTarget(x360_state.xe, Xe_GetFramebufferSurface(x360_state.xe));

    /* Create vertex buffer */
    x360_state.vb = Xe_CreateVertexBuffer(x360_state.xe, X360_VB_SIZE);
    x360_state.vb_data = Xe_VB_Lock(x360_state.xe, x360_state.vb, 0, X360_VB_SIZE, XE_LOCK_WRITE);
    x360_state.vb_offset = 0;

    /* Create index buffer */
    x360_state.ib = Xe_CreateIndexBuffer(x360_state.xe, X360_IB_SIZE, XE_FMT_INDEX16);

    /* Load shaders */
    x360_state.sh_vertex = Xe_LoadShaderFromMemory(x360_state.xe, (void *)vertex_shader_src);
    x360_state.sh_pixel = Xe_LoadShaderFromMemory(x360_state.xe, (void *)pixel_shader_src);

    Xe_InstantiateShader(x360_state.xe, x360_state.sh_vertex, 0);
    Xe_InstantiateShader(x360_state.xe, x360_state.sh_pixel, 0);

    /* Initialize USB (for controllers) */
    usb_init();
    usb_do_poll();

    /* Initialize timing */
    x360_state.start_time = mftb();

    x360_state.running = 1;

    console_init();
    printf("Xbox 360 platform initialized: %dx%d\n", X360_SCREEN_WIDTH, X360_SCREEN_HEIGHT);

    return 1;
}

void platform_shutdown(void) {
    /* Free resources */
    if (x360_state.vb) {
        Xe_VB_Unlock(x360_state.xe, x360_state.vb);
        Xe_DestroyVertexBuffer(x360_state.xe, x360_state.vb);
    }
    if (x360_state.ib) {
        Xe_DestroyIndexBuffer(x360_state.xe, x360_state.ib);
    }
}

uint64_t platform_get_time_us(void) {
    uint64_t now = mftb();
    uint64_t elapsed = now - x360_state.start_time;
    /* Xbox 360 timebase is ~50MHz */
    return (elapsed * 1000000) / 50000000;
}

void platform_sleep_ms(uint32_t ms) {
    uint64_t target = mftb() + (ms * 50000);  /* 50MHz timebase */
    while (mftb() < target) {
        /* Spin wait */
    }
}

void platform_exit(void) {
    x360_state.running = 0;
    /* Return to dashboard */
    xenon_smc_power_reboot();
}

/*============================================================================
 * Memory Management
 *============================================================================*/

void *platform_malloc(size_t size) {
    return memalign(128, size);  /* 128-byte alignment for GPU */
}

void *platform_malloc_aligned(size_t size, size_t alignment) {
    return memalign(alignment, size);
}

void *platform_realloc(void *ptr, size_t size) {
    return realloc(ptr, size);
}

void platform_free(void *ptr) {
    free(ptr);
}

void *platform_vram_alloc(size_t size) {
    /* Xbox 360 has unified memory */
    return Xe_CreateTexture(x360_state.xe, size, 1, 1, XE_FMT_8888, 0);
}

void platform_vram_free(void *ptr) {
    if (ptr) {
        Xe_DestroyTexture(x360_state.xe, ptr);
    }
}

/*============================================================================
 * Graphics Implementation (Xenos)
 *============================================================================*/

void platform_gfx_init(void) {
    /* Xenos already initialized */
}

void platform_gfx_shutdown(void) {
    /* Nothing extra */
}

void platform_gfx_begin_frame(void) {
    /* Reset vertex buffer offset */
    x360_state.vb_offset = 0;

    /* Set render target */
    x360_state.fb = Xe_GetFramebufferSurface(x360_state.xe);
    Xe_SetRenderTarget(x360_state.xe, x360_state.fb);

    /* Set shaders */
    Xe_SetShader(x360_state.xe, SHADER_TYPE_VERTEX, x360_state.sh_vertex, 0);
    Xe_SetShader(x360_state.xe, SHADER_TYPE_PIXEL, x360_state.sh_pixel, 0);

    /* Set default state */
    Xe_SetCullMode(x360_state.xe, XE_CULL_CCW);
    Xe_SetZEnable(x360_state.xe, 1);
    Xe_SetZWrite(x360_state.xe, 1);
    Xe_SetZFunc(x360_state.xe, XE_CMP_LESSEQUAL);
}

void platform_gfx_end_frame(void) {
    /* Resolve and present */
    Xe_Resolve(x360_state.xe);
    Xe_Sync(x360_state.xe);

    /* Poll USB for controller updates */
    usb_do_poll();
}

void platform_gfx_set_render_list(render_list_t list) {
    switch (list) {
        case RENDER_LIST_OPAQUE:
            Xe_SetBlendControl(x360_state.xe, XE_BLEND_ONE, XE_BLENDOP_ADD, XE_BLEND_ZERO,
                              XE_BLEND_ONE, XE_BLENDOP_ADD, XE_BLEND_ZERO);
            Xe_SetAlphaTestEnable(x360_state.xe, 0);
            break;
        case RENDER_LIST_TRANSPARENT:
            Xe_SetBlendControl(x360_state.xe, XE_BLEND_SRCALPHA, XE_BLENDOP_ADD, XE_BLEND_INVSRCALPHA,
                              XE_BLEND_SRCALPHA, XE_BLENDOP_ADD, XE_BLEND_INVSRCALPHA);
            break;
        case RENDER_LIST_PUNCHTHROUGH:
            Xe_SetAlphaTestEnable(x360_state.xe, 1);
            Xe_SetAlphaFunc(x360_state.xe, XE_CMP_GREATEREQUAL);
            Xe_SetAlphaRef(x360_state.xe, 0.5f);
            break;
    }
}

void platform_gfx_clear(uint32_t color) {
    float r = ((color >> 16) & 0xFF) / 255.0f;
    float g = ((color >> 8) & 0xFF) / 255.0f;
    float b = (color & 0xFF) / 255.0f;
    float a = ((color >> 24) & 0xFF) / 255.0f;

    Xe_SetClearColor(x360_state.xe, (uint32_t)(a * 255) << 24 |
                                     (uint32_t)(r * 255) << 16 |
                                     (uint32_t)(g * 255) << 8 |
                                     (uint32_t)(b * 255));
    Xe_Clear(x360_state.xe, XE_CLEAR_COLOR | XE_CLEAR_DS);
}

/*============================================================================
 * Xbox 360-Specific Rendering
 *============================================================================*/

/* Vertex format for 360 */
typedef struct {
    float x, y, z, w;       /* Position */
    uint32_t color;         /* Diffuse color */
    float u, v;             /* Texture coords */
} x360_vertex_t;

/* Add vertices to buffer */
static x360_vertex_t *alloc_vertices(int count) {
    int size = count * sizeof(x360_vertex_t);
    if (x360_state.vb_offset + size > X360_VB_SIZE) {
        return NULL;  /* Buffer full */
    }

    x360_vertex_t *v = (x360_vertex_t *)((uint8_t *)x360_state.vb_data + x360_state.vb_offset);
    x360_state.vb_offset += size;
    return v;
}

/* Draw flat-shaded triangle */
void x360_draw_tri_flat(float x0, float y0, float z0,
                         float x1, float y1, float z1,
                         float x2, float y2, float z2,
                         uint32_t color) {
    x360_vertex_t *v = alloc_vertices(3);
    if (!v) return;

    v[0].x = x0; v[0].y = y0; v[0].z = z0; v[0].w = 1.0f;
    v[0].color = color; v[0].u = 0; v[0].v = 0;

    v[1].x = x1; v[1].y = y1; v[1].z = z1; v[1].w = 1.0f;
    v[1].color = color; v[1].u = 0; v[1].v = 0;

    v[2].x = x2; v[2].y = y2; v[2].z = z2; v[2].w = 1.0f;
    v[2].color = color; v[2].u = 0; v[2].v = 0;

    /* Set vertex stream */
    Xe_SetStreamSource(x360_state.xe, 0, x360_state.vb,
                       x360_state.vb_offset - 3 * sizeof(x360_vertex_t),
                       sizeof(x360_vertex_t));

    /* Draw */
    Xe_DrawPrimitive(x360_state.xe, XE_PRIMTYPE_TRIANGLELIST, 0, 1);
}

/* Draw gouraud-shaded triangle */
void x360_draw_tri_gouraud(float x0, float y0, float z0, uint32_t c0,
                            float x1, float y1, float z1, uint32_t c1,
                            float x2, float y2, float z2, uint32_t c2) {
    x360_vertex_t *v = alloc_vertices(3);
    if (!v) return;

    v[0].x = x0; v[0].y = y0; v[0].z = z0; v[0].w = 1.0f;
    v[0].color = c0; v[0].u = 0; v[0].v = 0;

    v[1].x = x1; v[1].y = y1; v[1].z = z1; v[1].w = 1.0f;
    v[1].color = c1; v[1].u = 0; v[1].v = 0;

    v[2].x = x2; v[2].y = y2; v[2].z = z2; v[2].w = 1.0f;
    v[2].color = c2; v[2].u = 0; v[2].v = 0;

    Xe_SetStreamSource(x360_state.xe, 0, x360_state.vb,
                       x360_state.vb_offset - 3 * sizeof(x360_vertex_t),
                       sizeof(x360_vertex_t));

    Xe_DrawPrimitive(x360_state.xe, XE_PRIMTYPE_TRIANGLELIST, 0, 1);
}

/* Draw 2D rectangle for HUD */
void x360_draw_rect_2d(int x, int y, int w, int h, uint32_t color) {
    x360_vertex_t *v = alloc_vertices(4);
    if (!v) return;

    float x0 = (float)x / X360_SCREEN_WIDTH * 2.0f - 1.0f;
    float y0 = 1.0f - (float)y / X360_SCREEN_HEIGHT * 2.0f;
    float x1 = (float)(x + w) / X360_SCREEN_WIDTH * 2.0f - 1.0f;
    float y1 = 1.0f - (float)(y + h) / X360_SCREEN_HEIGHT * 2.0f;

    v[0].x = x0; v[0].y = y0; v[0].z = 0; v[0].w = 1.0f;
    v[0].color = color; v[0].u = 0; v[0].v = 0;

    v[1].x = x1; v[1].y = y0; v[1].z = 0; v[1].w = 1.0f;
    v[1].color = color; v[1].u = 1; v[1].v = 0;

    v[2].x = x0; v[2].y = y1; v[2].z = 0; v[2].w = 1.0f;
    v[2].color = color; v[2].u = 0; v[2].v = 1;

    v[3].x = x1; v[3].y = y1; v[3].z = 0; v[3].w = 1.0f;
    v[3].color = color; v[3].u = 1; v[3].v = 1;

    Xe_SetStreamSource(x360_state.xe, 0, x360_state.vb,
                       x360_state.vb_offset - 4 * sizeof(x360_vertex_t),
                       sizeof(x360_vertex_t));

    Xe_DrawPrimitive(x360_state.xe, XE_PRIMTYPE_TRIANGLESTRIP, 0, 2);
}

/* Set transformation matrices */
void x360_set_transform(float *world_view_proj) {
    /* Set vertex shader constants (c0-c3 = WVP matrix rows) */
    Xe_SetVertexShaderConstantF(x360_state.xe, 0, world_view_proj, 4);
}

/*============================================================================
 * Texture Support
 *============================================================================*/

platform_texture_t *platform_texture_create(uint16_t w, uint16_t h, tex_format_t fmt) {
    platform_texture_t *tex = platform_malloc(sizeof(platform_texture_t));
    if (!tex) return NULL;

    tex->width = w;
    tex->height = h;
    tex->format = fmt;

    /* Create Xenos texture */
    int xe_fmt = (fmt == TEX_FMT_ARGB8888) ? XE_FMT_8888 : XE_FMT_565;
    struct XenosSurface *surf = Xe_CreateTexture(x360_state.xe, w, h, 1, xe_fmt, 0);

    tex->data = surf;
    tex->platform_handle = (uint32_t)(uintptr_t)surf;

    return tex;
}

void platform_texture_upload(platform_texture_t *tex, const void *data) {
    if (!tex || !data || !tex->data) return;

    struct XenosSurface *surf = (struct XenosSurface *)tex->data;

    /* Lock and copy texture data */
    void *tex_data = Xe_Surface_LockRect(x360_state.xe, surf, 0, 0, 0, 0, XE_LOCK_WRITE);
    if (tex_data) {
        int bpp = (tex->format == TEX_FMT_ARGB8888) ? 4 : 2;
        memcpy(tex_data, data, tex->width * tex->height * bpp);
        Xe_Surface_Unlock(x360_state.xe, surf);
    }
}

void platform_texture_destroy(platform_texture_t *tex) {
    if (tex) {
        if (tex->data) {
            Xe_DestroyTexture(x360_state.xe, (struct XenosSurface *)tex->data);
        }
        platform_free(tex);
    }
}

void platform_texture_bind(platform_texture_t *tex) {
    if (!tex || !tex->data) return;

    Xe_SetTexture(x360_state.xe, 0, (struct XenosSurface *)tex->data);
}

/*============================================================================
 * Audio Implementation (Xenon Sound)
 *============================================================================*/

void platform_audio_init(void) {
    xenon_sound_init();
}

void platform_audio_shutdown(void) {
    /* Cleanup */
}

void platform_audio_update(void) {
    /* Audio runs on separate thread */
}

platform_audio_t *platform_audio_load(const char *path) {
    platform_audio_t *audio = platform_malloc(sizeof(platform_audio_t));
    if (!audio) return NULL;

    /* TODO: Load XMA file */
    audio->data = NULL;
    audio->size = 0;
    audio->sample_rate = 48000;
    audio->format = AUDIO_FMT_XMA;
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

    /* Would use xenon_sound_submit */
    (void)loop;
}

void platform_audio_stop(platform_audio_t *audio) {
    (void)audio;
}

void platform_audio_set_volume(int volume) {
    (void)volume;
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

    char full_path[256];

    /* Try DVD first, then USB, then HDD */
    const char *prefixes[] = { "dvd:/", "usb:/", "hdd1:/" };

    for (int i = 0; i < 3; i++) {
        snprintf(full_path, sizeof(full_path), "%s%s", prefixes[i], path);

        int flags = 0;  /* O_RDONLY */
        if (mode[0] == 'w') flags = 1;  /* O_WRONLY */
        else if (mode[0] == 'a') flags = 2;  /* O_APPEND */

        file->fd = open(full_path, flags);
        if (file->fd >= 0) {
            file->is_open = 1;
            return file;
        }
    }

    platform_free(file);
    return NULL;
}

void platform_file_close(platform_file_t *file) {
    if (file) {
        if (file->is_open && file->fd >= 0) {
            close(file->fd);
        }
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
    return lseek(file->fd, 0, 1);  /* SEEK_CUR */
}

int platform_file_exists(const char *path) {
    platform_file_t *file = platform_file_open(path, "rb");
    if (file) {
        platform_file_close(file);
        return 1;
    }
    return 0;
}

const char *platform_get_data_path(void) {
    return "dvd:/";
}

const char *platform_get_save_path(void) {
    return "hdd1:/retroracer/";
}

/*============================================================================
 * Xbox 360 Controller Input
 *============================================================================*/

uint32_t x360_get_buttons(int port) {
    if (port >= X360_MAX_CONTROLLERS) return 0;

    struct controller_data_s *pad = &x360_state.pad_data[port];

    /* Get controller state */
    if (get_controller_data(pad, port) < 0) {
        return 0;
    }

    uint32_t result = 0;

    if (pad->a)             result |= PLAT_BTN_A;
    if (pad->b)             result |= PLAT_BTN_B;
    if (pad->x)             result |= PLAT_BTN_X;
    if (pad->y)             result |= PLAT_BTN_Y;
    if (pad->start)         result |= PLAT_BTN_START;
    if (pad->back)          result |= PLAT_BTN_BACK;
    if (pad->lb)            result |= PLAT_BTN_LB;
    if (pad->rb)            result |= PLAT_BTN_RB;
    if (pad->lt > 32)       result |= PLAT_BTN_LT;
    if (pad->rt > 32)       result |= PLAT_BTN_RT;
    if (pad->s1_z)          result |= PLAT_BTN_LS;  /* Left stick press */
    if (pad->s2_z)          result |= PLAT_BTN_RS;  /* Right stick press */
    if (pad->up)            result |= PLAT_BTN_DPAD_UP;
    if (pad->down)          result |= PLAT_BTN_DPAD_DOWN;
    if (pad->left)          result |= PLAT_BTN_DPAD_LEFT;
    if (pad->right)         result |= PLAT_BTN_DPAD_RIGHT;

    return result;
}

void x360_get_analog(int port, int *lx, int *ly, int *rx, int *ry) {
    if (port >= X360_MAX_CONTROLLERS) {
        *lx = *ly = *rx = *ry = 128;
        return;
    }

    struct controller_data_s *pad = &x360_state.pad_data[port];

    /* LibXenon sticks are signed 16-bit, convert to 0-255 */
    *lx = (pad->s1_x + 32768) >> 8;
    *ly = (pad->s1_y + 32768) >> 8;
    *rx = (pad->s2_x + 32768) >> 8;
    *ry = (pad->s2_y + 32768) >> 8;
}

void x360_get_triggers(int port, int *lt, int *rt) {
    if (port >= X360_MAX_CONTROLLERS) {
        *lt = *rt = 0;
        return;
    }

    struct controller_data_s *pad = &x360_state.pad_data[port];

    *lt = pad->lt;
    *rt = pad->rt;
}

/* Set controller rumble */
void x360_set_rumble(int port, int left_motor, int right_motor) {
    if (port >= X360_MAX_CONTROLLERS) return;

    set_controller_rumble(port, left_motor, right_motor);
}

#endif /* PLATFORM_XBOX360 */
