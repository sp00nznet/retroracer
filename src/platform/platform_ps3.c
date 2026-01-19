/*
 * RetroRacer - PlayStation 3 Platform Implementation
 *
 * Hardware Specs:
 * - CPU: 3.2 GHz Cell Broadband Engine (1 PPE + 6 SPEs available)
 * - RAM: 256MB XDR Main + 256MB GDDR3 Video
 * - GPU: RSX Reality Synthesizer (NVidia G70 derived)
 * - Audio: Multi-channel audio processor
 * - Media: Blu-ray Disc, HDD
 *
 * SDK: PSL1GHT (Open Source) / Official Sony SDK
 */

#include "platform.h"

#ifdef PLATFORM_PS3

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ppu-lv2.h>
#include <rsx/rsx.h>
#include <rsx/gcm_sys.h>
#include <sysutil/sysutil.h>
#include <sysutil/video.h>
#include <io/pad.h>
#include <audio/audio.h>
#include <sys/thread.h>
#include <sys/time.h>
#include <lv2/process.h>

/*============================================================================
 * PS3-Specific Defines
 *============================================================================*/

#define PS3_SCREEN_WIDTH    1280
#define PS3_SCREEN_HEIGHT   720
#define PS3_COLOR_BUFFER_SIZE (PS3_SCREEN_WIDTH * PS3_SCREEN_HEIGHT * 4)
#define PS3_DEPTH_BUFFER_SIZE (PS3_SCREEN_WIDTH * PS3_SCREEN_HEIGHT * 4)

#define PS3_HOST_BUFFER_SIZE (128 * 1024 * 1024)  /* 128MB for RSX */
#define PS3_CB_SIZE          (1024 * 1024)        /* Command buffer size */

#define MAX_BUFFERS          2

/*============================================================================
 * PS3 State
 *============================================================================*/

static struct {
    /* RSX context */
    gcmContextData *context;
    void *host_addr;

    /* Frame buffers */
    u32 color_offset[MAX_BUFFERS];
    u32 depth_offset;
    u32 *color_buffer[MAX_BUFFERS];
    u32 *depth_buffer;
    int current_buffer;

    /* Video configuration */
    videoState video_state;
    videoResolution video_res;

    /* Controller state */
    padInfo pad_info;
    padData pad_data[MAX_PADS];

    /* Timing */
    u64 start_time;

    /* Exit flag */
    int running;
} ps3_state;

/* Platform capabilities */
static const platform_caps_t ps3_caps = {
    .name = "PlayStation 3",
    .ram_size = 256 * 1024 * 1024,      /* 256MB main */
    .vram_size = 256 * 1024 * 1024,     /* 256MB video */
    .max_texture_size = 4096,
    .max_vertices = 1000000,
    .screen_width = PS3_SCREEN_WIDTH,
    .screen_height = PS3_SCREEN_HEIGHT,
    .has_analog_sticks = 1,
    .has_triggers = 1,
    .has_rumble = 1,
    .color_depth = 32
};

/*============================================================================
 * Platform Core Functions
 *============================================================================*/

const platform_caps_t *platform_get_caps(void) {
    return &ps3_caps;
}

/* System utility callback */
static void sysutil_callback(u64 status, u64 param, void *userdata) {
    (void)param;
    (void)userdata;

    switch (status) {
        case SYSUTIL_EXIT_GAME:
            ps3_state.running = 0;
            break;
        case SYSUTIL_DRAW_BEGIN:
        case SYSUTIL_DRAW_END:
            break;
    }
}

/* Wait for RSX flip */
static void wait_flip(void) {
    while (gcmGetFlipStatus() != 0) {
        sysUtilCheckCallback();
    }
    gcmResetFlipStatus();
}

int platform_init(void) {
    /* Initialize video */
    videoGetState(0, 0, &ps3_state.video_state);
    videoGetResolution(ps3_state.video_state.displayMode.resolution, &ps3_state.video_res);

    /* Allocate RSX memory */
    ps3_state.host_addr = memalign(1024 * 1024, PS3_HOST_BUFFER_SIZE);
    if (!ps3_state.host_addr) {
        return 0;
    }

    /* Initialize RSX */
    ps3_state.context = rsxInit(PS3_CB_SIZE, PS3_HOST_BUFFER_SIZE, ps3_state.host_addr);
    if (!ps3_state.context) {
        free(ps3_state.host_addr);
        return 0;
    }

    /* Configure video output */
    videoConfiguration config;
    memset(&config, 0, sizeof(config));
    config.resolution = ps3_state.video_state.displayMode.resolution;
    config.format = VIDEO_BUFFER_FORMAT_XRGB;
    config.pitch = ps3_state.video_res.width * 4;
    config.aspect = VIDEO_ASPECT_AUTO;

    videoConfigure(0, &config, NULL, 0);

    /* Allocate frame buffers */
    for (int i = 0; i < MAX_BUFFERS; i++) {
        ps3_state.color_buffer[i] = rsxMemalign(64, PS3_COLOR_BUFFER_SIZE);
        if (!ps3_state.color_buffer[i]) {
            return 0;
        }
        rsxAddressToOffset(ps3_state.color_buffer[i], &ps3_state.color_offset[i]);
        gcmSetDisplayBuffer(i, ps3_state.color_offset[i],
                           ps3_state.video_res.width * 4,
                           ps3_state.video_res.width, ps3_state.video_res.height);
    }

    /* Allocate depth buffer */
    ps3_state.depth_buffer = rsxMemalign(64, PS3_DEPTH_BUFFER_SIZE);
    if (!ps3_state.depth_buffer) {
        return 0;
    }
    rsxAddressToOffset(ps3_state.depth_buffer, &ps3_state.depth_offset);

    ps3_state.current_buffer = 0;

    /* Initialize controllers */
    ioPadInit(7);

    /* Register system callback */
    sysUtilRegisterCallback(SYSUTIL_EVENT_SLOT0, sysutil_callback, NULL);

    /* Get start time */
    ps3_state.start_time = sys_time_get_system_time();
    ps3_state.running = 1;

    return 1;
}

void platform_shutdown(void) {
    /* Wait for RSX to finish */
    rsxFinish(ps3_state.context, 1);

    /* Free buffers */
    for (int i = 0; i < MAX_BUFFERS; i++) {
        if (ps3_state.color_buffer[i]) {
            rsxFree(ps3_state.color_buffer[i]);
        }
    }
    if (ps3_state.depth_buffer) {
        rsxFree(ps3_state.depth_buffer);
    }

    /* Shutdown RSX */
    rsxFinish(ps3_state.context, 0);
    free(ps3_state.host_addr);

    /* Shutdown pad */
    ioPadEnd();
}

uint64_t platform_get_time_us(void) {
    return sys_time_get_system_time() - ps3_state.start_time;
}

void platform_sleep_ms(uint32_t ms) {
    sysThreadSleep(ms);
}

void platform_exit(void) {
    ps3_state.running = 0;
    sysProcessExit(0);
}

/*============================================================================
 * Memory Management
 *============================================================================*/

void *platform_malloc(size_t size) {
    return memalign(16, size);
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
    return rsxMemalign(64, size);
}

void platform_vram_free(void *ptr) {
    rsxFree(ptr);
}

/*============================================================================
 * Graphics Implementation (RSX)
 *============================================================================*/

void platform_gfx_init(void) {
    /* RSX already initialized */
}

void platform_gfx_shutdown(void) {
    /* Nothing extra */
}

static void setup_render_target(void) {
    gcmSurface surface;
    memset(&surface, 0, sizeof(surface));

    surface.colorFormat = GCM_SURFACE_X8R8G8B8;
    surface.colorTarget = GCM_SURFACE_TARGET_0;
    surface.colorLocation[0] = GCM_LOCATION_RSX;
    surface.colorOffset[0] = ps3_state.color_offset[ps3_state.current_buffer];
    surface.colorPitch[0] = ps3_state.video_res.width * 4;

    surface.depthFormat = GCM_SURFACE_ZETA_Z24S8;
    surface.depthLocation = GCM_LOCATION_RSX;
    surface.depthOffset = ps3_state.depth_offset;
    surface.depthPitch = ps3_state.video_res.width * 4;

    surface.type = GCM_SURFACE_TYPE_LINEAR;
    surface.antiAlias = GCM_SURFACE_CENTER_1;

    surface.width = ps3_state.video_res.width;
    surface.height = ps3_state.video_res.height;
    surface.x = 0;
    surface.y = 0;

    rsxSetSurface(ps3_state.context, &surface);
}

void platform_gfx_begin_frame(void) {
    /* Wait for flip to complete */
    wait_flip();

    /* Check system events */
    sysUtilCheckCallback();

    /* Setup render target */
    setup_render_target();

    /* Set viewport */
    rsxSetViewport(ps3_state.context, 0, 0,
                   ps3_state.video_res.width, ps3_state.video_res.height,
                   0.0f, 1.0f, 0.5f, 0.5f,
                   ps3_state.video_res.width / 2.0f,
                   ps3_state.video_res.height / 2.0f);

    /* Enable depth test */
    rsxSetDepthTestEnable(ps3_state.context, GCM_TRUE);
    rsxSetDepthFunc(ps3_state.context, GCM_LEQUAL);
    rsxSetDepthWriteEnable(ps3_state.context, GCM_TRUE);

    /* Set cull mode */
    rsxSetCullFaceEnable(ps3_state.context, GCM_TRUE);
    rsxSetCullFace(ps3_state.context, GCM_CULL_BACK);
}

void platform_gfx_end_frame(void) {
    /* Flip buffer */
    gcmSetFlip(ps3_state.context, ps3_state.current_buffer);
    rsxFlushBuffer(ps3_state.context);
    gcmSetWaitFlip(ps3_state.context);

    /* Swap buffers */
    ps3_state.current_buffer ^= 1;
}

void platform_gfx_set_render_list(render_list_t list) {
    switch (list) {
        case RENDER_LIST_OPAQUE:
            rsxSetBlendEnable(ps3_state.context, GCM_FALSE);
            break;
        case RENDER_LIST_TRANSPARENT:
            rsxSetBlendEnable(ps3_state.context, GCM_TRUE);
            rsxSetBlendFunc(ps3_state.context, GCM_SRC_ALPHA, GCM_ONE_MINUS_SRC_ALPHA,
                           GCM_SRC_ALPHA, GCM_ONE_MINUS_SRC_ALPHA);
            break;
        case RENDER_LIST_PUNCHTHROUGH:
            rsxSetBlendEnable(ps3_state.context, GCM_FALSE);
            /* Enable alpha test for punch-through */
            break;
    }
}

void platform_gfx_clear(uint32_t color) {
    u8 r = (color >> 16) & 0xFF;
    u8 g = (color >> 8) & 0xFF;
    u8 b = color & 0xFF;
    u8 a = (color >> 24) & 0xFF;

    rsxSetClearColor(ps3_state.context, (a << 24) | (r << 16) | (g << 8) | b);
    rsxSetClearDepthValue(ps3_state.context, 0xFFFFFF);
    rsxClearSurface(ps3_state.context,
                    GCM_CLEAR_R | GCM_CLEAR_G | GCM_CLEAR_B | GCM_CLEAR_A |
                    GCM_CLEAR_S | GCM_CLEAR_Z);
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

    /* Allocate in RSX memory */
    int bpp = (fmt == TEX_FMT_ARGB8888) ? 4 : 2;
    tex->data = rsxMemalign(128, w * h * bpp);

    rsxAddressToOffset(tex->data, &tex->platform_handle);

    return tex;
}

void platform_texture_upload(platform_texture_t *tex, const void *data) {
    if (!tex || !data) return;

    int bpp = (tex->format == TEX_FMT_ARGB8888) ? 4 : 2;
    memcpy(tex->data, data, tex->width * tex->height * bpp);
}

void platform_texture_destroy(platform_texture_t *tex) {
    if (tex) {
        if (tex->data) rsxFree(tex->data);
        platform_free(tex);
    }
}

void platform_texture_bind(platform_texture_t *tex) {
    if (!tex) return;

    gcmTexture gcm_tex;
    memset(&gcm_tex, 0, sizeof(gcm_tex));

    gcm_tex.format = (tex->format == TEX_FMT_ARGB8888) ?
                     GCM_TEXTURE_FORMAT_A8R8G8B8 | GCM_TEXTURE_FORMAT_LIN :
                     GCM_TEXTURE_FORMAT_R5G6B5 | GCM_TEXTURE_FORMAT_LIN;
    gcm_tex.mipmap = 1;
    gcm_tex.dimension = GCM_TEXTURE_DIMS_2D;
    gcm_tex.cubemap = GCM_FALSE;
    gcm_tex.remap = GCM_TEXTURE_REMAP_REMAP << 14 |
                    GCM_TEXTURE_REMAP_REMAP << 12 |
                    GCM_TEXTURE_REMAP_REMAP << 10 |
                    GCM_TEXTURE_REMAP_REMAP << 8 |
                    GCM_TEXTURE_REMAP_FROM_B << 6 |
                    GCM_TEXTURE_REMAP_FROM_G << 4 |
                    GCM_TEXTURE_REMAP_FROM_R << 2 |
                    GCM_TEXTURE_REMAP_FROM_A;
    gcm_tex.width = tex->width;
    gcm_tex.height = tex->height;
    gcm_tex.depth = 1;
    gcm_tex.location = GCM_LOCATION_RSX;
    gcm_tex.pitch = tex->width * ((tex->format == TEX_FMT_ARGB8888) ? 4 : 2);
    gcm_tex.offset = tex->platform_handle;

    rsxLoadTexture(ps3_state.context, 0, &gcm_tex);
    rsxTextureControl(ps3_state.context, 0, GCM_TRUE, 0 << 8, 12 << 8, GCM_TEXTURE_MAX_ANISO_1);
    rsxTextureFilter(ps3_state.context, 0, 0, GCM_TEXTURE_LINEAR, GCM_TEXTURE_LINEAR,
                     GCM_TEXTURE_CONVOLUTION_QUINCUNX);
    rsxTextureWrapMode(ps3_state.context, 0, GCM_TEXTURE_CLAMP_TO_EDGE,
                       GCM_TEXTURE_CLAMP_TO_EDGE, GCM_TEXTURE_CLAMP_TO_EDGE,
                       0, GCM_TEXTURE_ZFUNC_LESS, 0);
}

/*============================================================================
 * Audio Implementation
 *============================================================================*/

static audioPortConfig audio_config;
static u32 audio_port = 0;

void platform_audio_init(void) {
    audioPortParam params;
    memset(&params, 0, sizeof(params));
    params.numChannels = AUDIO_PORT_2CH;
    params.numBlocks = 8;
    params.attr = 0;
    params.level = 1.0f;

    audioInit();
    audioPortOpen(&params, &audio_port);
    audioGetPortConfig(audio_port, &audio_config);
    audioPortStart(audio_port);
}

void platform_audio_shutdown(void) {
    audioPortStop(audio_port);
    audioPortClose(audio_port);
    audioQuit();
}

void platform_audio_update(void) {
    /* Audio runs on separate thread */
}

platform_audio_t *platform_audio_load(const char *path) {
    platform_audio_t *audio = platform_malloc(sizeof(platform_audio_t));
    if (!audio) return NULL;

    /* TODO: Load audio file */
    audio->data = NULL;
    audio->size = 0;
    audio->sample_rate = 48000;
    audio->format = AUDIO_FMT_PCM16;
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
    (void)audio;
    (void)loop;
    /* Would use audioPortStart and buffer audio data */
}

void platform_audio_stop(platform_audio_t *audio) {
    (void)audio;
}

void platform_audio_set_volume(int volume) {
    audioSetNotifyEventQueue(0);  /* Placeholder */
    (void)volume;
}

/*============================================================================
 * File I/O
 *============================================================================*/

struct platform_file {
    FILE *fp;
};

platform_file_t *platform_file_open(const char *path, const char *mode) {
    platform_file_t *file = platform_malloc(sizeof(platform_file_t));
    if (!file) return NULL;

    char full_path[256];
    snprintf(full_path, sizeof(full_path), "/dev_bdvd/%s", path);

    file->fp = fopen(full_path, mode);
    if (!file->fp) {
        /* Try HDD */
        snprintf(full_path, sizeof(full_path), "/dev_hdd0/game/RETRORACR/USRDIR/%s", path);
        file->fp = fopen(full_path, mode);
    }

    if (!file->fp) {
        platform_free(file);
        return NULL;
    }

    return file;
}

void platform_file_close(platform_file_t *file) {
    if (file) {
        if (file->fp) fclose(file->fp);
        platform_free(file);
    }
}

size_t platform_file_read(platform_file_t *file, void *buf, size_t size) {
    if (!file || !file->fp) return 0;
    return fread(buf, 1, size, file->fp);
}

size_t platform_file_write(platform_file_t *file, const void *buf, size_t size) {
    if (!file || !file->fp) return 0;
    return fwrite(buf, 1, size, file->fp);
}

int platform_file_seek(platform_file_t *file, long offset, int whence) {
    if (!file || !file->fp) return -1;
    return fseek(file->fp, offset, whence);
}

long platform_file_tell(platform_file_t *file) {
    if (!file || !file->fp) return -1;
    return ftell(file->fp);
}

int platform_file_exists(const char *path) {
    char full_path[256];
    snprintf(full_path, sizeof(full_path), "/dev_bdvd/%s", path);
    FILE *fp = fopen(full_path, "rb");
    if (fp) {
        fclose(fp);
        return 1;
    }
    return 0;
}

const char *platform_get_data_path(void) {
    return "/dev_bdvd/";
}

const char *platform_get_save_path(void) {
    return "/dev_hdd0/game/RETRORACR/USRDIR/";
}

/*============================================================================
 * PS3 Controller Input
 *============================================================================*/

uint32_t ps3_get_buttons(int port) {
    if (port >= MAX_PADS) return 0;

    ioPadGetInfo(&ps3_state.pad_info);
    if (!ps3_state.pad_info.status[port]) return 0;

    ioPadGetData(port, &ps3_state.pad_data[port]);
    u32 btns = ps3_state.pad_data[port].BTN_CROSS;

    uint32_t result = 0;

    if (ps3_state.pad_data[port].BTN_CROSS)    result |= PLAT_BTN_CROSS;
    if (ps3_state.pad_data[port].BTN_CIRCLE)   result |= PLAT_BTN_CIRCLE;
    if (ps3_state.pad_data[port].BTN_SQUARE)   result |= PLAT_BTN_SQUARE;
    if (ps3_state.pad_data[port].BTN_TRIANGLE) result |= PLAT_BTN_TRIANGLE;
    if (ps3_state.pad_data[port].BTN_START)    result |= PLAT_BTN_START;
    if (ps3_state.pad_data[port].BTN_SELECT)   result |= PLAT_BTN_SELECT;
    if (ps3_state.pad_data[port].BTN_L1)       result |= PLAT_BTN_L1;
    if (ps3_state.pad_data[port].BTN_R1)       result |= PLAT_BTN_R1;
    if (ps3_state.pad_data[port].BTN_L2)       result |= PLAT_BTN_L2;
    if (ps3_state.pad_data[port].BTN_R2)       result |= PLAT_BTN_R2;
    if (ps3_state.pad_data[port].BTN_L3)       result |= PLAT_BTN_L3;
    if (ps3_state.pad_data[port].BTN_R3)       result |= PLAT_BTN_R3;
    if (ps3_state.pad_data[port].BTN_UP)       result |= PLAT_BTN_DPAD_UP;
    if (ps3_state.pad_data[port].BTN_DOWN)     result |= PLAT_BTN_DPAD_DOWN;
    if (ps3_state.pad_data[port].BTN_LEFT)     result |= PLAT_BTN_DPAD_LEFT;
    if (ps3_state.pad_data[port].BTN_RIGHT)    result |= PLAT_BTN_DPAD_RIGHT;

    (void)btns;
    return result;
}

void ps3_get_analog(int port, int *lx, int *ly, int *rx, int *ry) {
    if (port >= MAX_PADS) {
        *lx = *ly = *rx = *ry = 128;
        return;
    }

    *lx = ps3_state.pad_data[port].ANA_L_H;
    *ly = ps3_state.pad_data[port].ANA_L_V;
    *rx = ps3_state.pad_data[port].ANA_R_H;
    *ry = ps3_state.pad_data[port].ANA_R_V;
}

void ps3_get_triggers(int port, int *l2, int *r2) {
    if (port >= MAX_PADS) {
        *l2 = *r2 = 0;
        return;
    }

    *l2 = ps3_state.pad_data[port].PRE_L2;
    *r2 = ps3_state.pad_data[port].PRE_R2;
}

#endif /* PLATFORM_PS3 */
