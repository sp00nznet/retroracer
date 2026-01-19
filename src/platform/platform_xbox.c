/*
 * RetroRacer - Original Xbox Platform Implementation
 *
 * Hardware Specs:
 * - CPU: 733 MHz Intel Pentium III (Coppermine)
 * - RAM: 64MB DDR SDRAM (unified)
 * - GPU: NV2A (Custom NVidia GeForce 3), 233 MHz
 * - Audio: APU with 64 3D voices
 * - Media: DVD-ROM, 8GB HDD
 *
 * SDK: OpenXDK / NXDK (Open Source)
 */

#include "platform.h"

#ifdef PLATFORM_XBOX

#include <hal/debug.h>
#include <hal/video.h>
#include <hal/xbox.h>
#include <hal/fileio.h>
#include <xboxkrnl/xboxkrnl.h>
#include <pbkit/pbkit.h>
#include <xgu/xgu.h>
#include <xgu/xgux.h>
#include <SDL.h>

/*============================================================================
 * Xbox-Specific Defines
 *============================================================================*/

#define XBOX_SCREEN_WIDTH   640
#define XBOX_SCREEN_HEIGHT  480
#define XBOX_MAX_CONTROLLERS 4

/* Push buffer size */
#define XBOX_PUSHBUF_SIZE   (512 * 1024)

/*============================================================================
 * Xbox State
 *============================================================================*/

static struct {
    /* Video mode */
    int width;
    int height;
    int bpp;

    /* Frame buffers */
    uint32_t *framebuffer[2];
    int current_buffer;

    /* Push buffer for GPU commands */
    uint32_t *pushbuf;
    uint32_t *pushbuf_ptr;

    /* Controller state */
    HANDLE gamepad_handles[XBOX_MAX_CONTROLLERS];
    XINPUT_STATE gamepad_state[XBOX_MAX_CONTROLLERS];

    /* Timing */
    LARGE_INTEGER perf_freq;
    LARGE_INTEGER start_time;

    /* Running state */
    int running;
} xbox_state;

/* Platform capabilities */
static const platform_caps_t xbox_caps = {
    .name = "Xbox",
    .ram_size = 64 * 1024 * 1024,       /* 64MB */
    .vram_size = 64 * 1024 * 1024,      /* Unified */
    .max_texture_size = 4096,
    .max_vertices = 500000,
    .screen_width = XBOX_SCREEN_WIDTH,
    .screen_height = XBOX_SCREEN_HEIGHT,
    .has_analog_sticks = 1,
    .has_triggers = 1,
    .has_rumble = 1,
    .color_depth = 32
};

/*============================================================================
 * Platform Core Functions
 *============================================================================*/

const platform_caps_t *platform_get_caps(void) {
    return &xbox_caps;
}

int platform_init(void) {
    /* Initialize Xbox hardware via pbkit (push buffer kit) */
    if (pb_init() != 0) {
        debugPrint("Failed to initialize pbkit\n");
        return 0;
    }

    /* Set video mode */
    pb_show_front_screen();

    /* Get video mode info */
    xbox_state.width = pb_back_buffer_width();
    xbox_state.height = pb_back_buffer_height();
    xbox_state.bpp = 32;

    /* Allocate frame buffers */
    size_t fb_size = xbox_state.width * xbox_state.height * 4;
    xbox_state.framebuffer[0] = (uint32_t *)MmAllocateContiguousMemoryEx(fb_size, 0, 0xFFFFFFFF, 0, PAGE_READWRITE | PAGE_WRITECOMBINE);
    xbox_state.framebuffer[1] = (uint32_t *)MmAllocateContiguousMemoryEx(fb_size, 0, 0xFFFFFFFF, 0, PAGE_READWRITE | PAGE_WRITECOMBINE);

    if (!xbox_state.framebuffer[0] || !xbox_state.framebuffer[1]) {
        debugPrint("Failed to allocate frame buffers\n");
        pb_kill();
        return 0;
    }

    xbox_state.current_buffer = 0;

    /* Allocate push buffer */
    xbox_state.pushbuf = (uint32_t *)MmAllocateContiguousMemoryEx(XBOX_PUSHBUF_SIZE, 0, 0xFFFFFFFF, 0, PAGE_READWRITE | PAGE_WRITECOMBINE);
    xbox_state.pushbuf_ptr = xbox_state.pushbuf;

    /* Initialize input */
    XInitDevices(0, NULL);

    /* Open gamepad handles */
    for (int i = 0; i < XBOX_MAX_CONTROLLERS; i++) {
        xbox_state.gamepad_handles[i] = NULL;
        memset(&xbox_state.gamepad_state[i], 0, sizeof(XINPUT_STATE));
    }

    /* Query performance counter */
    KeQueryPerformanceCounter(&xbox_state.start_time);
    xbox_state.perf_freq.QuadPart = 733333333;  /* Xbox CPU freq */

    xbox_state.running = 1;

    debugPrint("Xbox platform initialized: %dx%d\n", xbox_state.width, xbox_state.height);

    return 1;
}

void platform_shutdown(void) {
    /* Close gamepad handles */
    for (int i = 0; i < XBOX_MAX_CONTROLLERS; i++) {
        if (xbox_state.gamepad_handles[i]) {
            XInputClose(xbox_state.gamepad_handles[i]);
            xbox_state.gamepad_handles[i] = NULL;
        }
    }

    /* Free frame buffers */
    if (xbox_state.framebuffer[0]) {
        MmFreeContiguousMemory(xbox_state.framebuffer[0]);
    }
    if (xbox_state.framebuffer[1]) {
        MmFreeContiguousMemory(xbox_state.framebuffer[1]);
    }
    if (xbox_state.pushbuf) {
        MmFreeContiguousMemory(xbox_state.pushbuf);
    }

    /* Shutdown pbkit */
    pb_kill();
}

uint64_t platform_get_time_us(void) {
    LARGE_INTEGER now;
    KeQueryPerformanceCounter(&now);
    uint64_t elapsed = now.QuadPart - xbox_state.start_time.QuadPart;
    return (elapsed * 1000000) / xbox_state.perf_freq.QuadPart;
}

void platform_sleep_ms(uint32_t ms) {
    LARGE_INTEGER delay;
    delay.QuadPart = -(int64_t)ms * 10000;  /* 100ns units, negative = relative */
    KeDelayExecutionThread(KernelMode, FALSE, &delay);
}

void platform_exit(void) {
    xbox_state.running = 0;
    XLaunchNewImage(NULL, NULL);  /* Reboot to dashboard */
}

/*============================================================================
 * Memory Management
 *============================================================================*/

void *platform_malloc(size_t size) {
    return ExAllocatePoolWithTag(size, 'RASR');
}

void *platform_malloc_aligned(size_t size, size_t alignment) {
    /* Xbox uses contiguous memory for GPU-accessible allocations */
    if (alignment >= 4096) {
        return MmAllocateContiguousMemoryEx(size, 0, 0xFFFFFFFF, alignment, PAGE_READWRITE);
    }
    return ExAllocatePoolWithTag(size, 'RASR');
}

void *platform_realloc(void *ptr, size_t size) {
    void *new_ptr = platform_malloc(size);
    if (new_ptr && ptr) {
        /* Can't know old size, caller must handle */
    }
    /* Note: Can't free old ptr without tracking */
    return new_ptr;
}

void platform_free(void *ptr) {
    if (ptr) {
        ExFreePool(ptr);
    }
}

void *platform_vram_alloc(size_t size) {
    /* Allocate GPU-accessible memory */
    return MmAllocateContiguousMemoryEx(size, 0, 0xFFFFFFFF, 0x1000, PAGE_READWRITE | PAGE_WRITECOMBINE);
}

void platform_vram_free(void *ptr) {
    if (ptr) {
        MmFreeContiguousMemory(ptr);
    }
}

/*============================================================================
 * Graphics Implementation (NV2A via pbkit)
 *============================================================================*/

void platform_gfx_init(void) {
    /* pbkit already initialized */
}

void platform_gfx_shutdown(void) {
    /* Nothing extra */
}

void platform_gfx_begin_frame(void) {
    /* Reset push buffer pointer */
    xbox_state.pushbuf_ptr = xbox_state.pushbuf;

    /* Wait for GPU to be ready */
    pb_wait_for_vbl();

    /* Begin scene */
    pb_reset();
    pb_target_back_buffer();
}

void platform_gfx_end_frame(void) {
    /* End scene and flip */
    while (pb_busy()) {
        /* Wait for GPU */
    }

    while (pb_finished()) {
        /* Wait for flip */
    }

    /* Swap buffers */
    xbox_state.current_buffer ^= 1;
}

void platform_gfx_set_render_list(render_list_t list) {
    uint32_t *p = pb_begin();

    switch (list) {
        case RENDER_LIST_OPAQUE:
            /* Disable alpha blending */
            p = xgu_set_blend_enable(p, false);
            break;
        case RENDER_LIST_TRANSPARENT:
            /* Enable alpha blending */
            p = xgu_set_blend_enable(p, true);
            p = xgu_set_blend_func_sfactor(p, XGU_FACTOR_SRC_ALPHA);
            p = xgu_set_blend_func_dfactor(p, XGU_FACTOR_ONE_MINUS_SRC_ALPHA);
            break;
        case RENDER_LIST_PUNCHTHROUGH:
            p = xgu_set_blend_enable(p, false);
            /* Alpha test for punch-through */
            break;
    }

    pb_end(p);
}

void platform_gfx_clear(uint32_t color) {
    /* Clear with pbkit */
    pb_erase_depth_stencil_buffer(0, 0, xbox_state.width, xbox_state.height);
    pb_fill(0, 0, xbox_state.width, xbox_state.height, color);
}

/*============================================================================
 * Xbox-Specific Rendering (NV2A)
 *============================================================================*/

/* Draw flat-shaded triangle using push buffer */
void xbox_draw_tri_flat(float x0, float y0, float z0,
                         float x1, float y1, float z1,
                         float x2, float y2, float z2,
                         uint32_t color) {
    uint32_t *p = pb_begin();

    /* Set up vertex format: position + diffuse color */
    p = xgu_begin(p, XGU_TRIANGLES);

    /* Vertex 0 */
    p = xgu_vertex4f(p, x0, y0, z0, 1.0f);
    p = xgu_color4ub(p, (color >> 16) & 0xFF, (color >> 8) & 0xFF, color & 0xFF, (color >> 24) & 0xFF);

    /* Vertex 1 */
    p = xgu_vertex4f(p, x1, y1, z1, 1.0f);
    p = xgu_color4ub(p, (color >> 16) & 0xFF, (color >> 8) & 0xFF, color & 0xFF, (color >> 24) & 0xFF);

    /* Vertex 2 */
    p = xgu_vertex4f(p, x2, y2, z2, 1.0f);
    p = xgu_color4ub(p, (color >> 16) & 0xFF, (color >> 8) & 0xFF, color & 0xFF, (color >> 24) & 0xFF);

    p = xgu_end(p);

    pb_end(p);
}

/* Draw gouraud-shaded triangle */
void xbox_draw_tri_gouraud(float x0, float y0, float z0, uint32_t c0,
                            float x1, float y1, float z1, uint32_t c1,
                            float x2, float y2, float z2, uint32_t c2) {
    uint32_t *p = pb_begin();

    p = xgu_begin(p, XGU_TRIANGLES);

    p = xgu_vertex4f(p, x0, y0, z0, 1.0f);
    p = xgu_color4ub(p, (c0 >> 16) & 0xFF, (c0 >> 8) & 0xFF, c0 & 0xFF, (c0 >> 24) & 0xFF);

    p = xgu_vertex4f(p, x1, y1, z1, 1.0f);
    p = xgu_color4ub(p, (c1 >> 16) & 0xFF, (c1 >> 8) & 0xFF, c1 & 0xFF, (c1 >> 24) & 0xFF);

    p = xgu_vertex4f(p, x2, y2, z2, 1.0f);
    p = xgu_color4ub(p, (c2 >> 16) & 0xFF, (c2 >> 8) & 0xFF, c2 & 0xFF, (c2 >> 24) & 0xFF);

    p = xgu_end(p);

    pb_end(p);
}

/* Draw 2D rectangle for HUD */
void xbox_draw_rect_2d(int x, int y, int w, int h, uint32_t color) {
    float x0 = (float)x;
    float y0 = (float)y;
    float x1 = (float)(x + w);
    float y1 = (float)(y + h);

    uint32_t *p = pb_begin();

    p = xgu_begin(p, XGU_TRIANGLE_STRIP);

    p = xgu_vertex4f(p, x0, y0, 0.0f, 1.0f);
    p = xgu_color4ub(p, (color >> 16) & 0xFF, (color >> 8) & 0xFF, color & 0xFF, (color >> 24) & 0xFF);

    p = xgu_vertex4f(p, x1, y0, 0.0f, 1.0f);
    p = xgu_color4ub(p, (color >> 16) & 0xFF, (color >> 8) & 0xFF, color & 0xFF, (color >> 24) & 0xFF);

    p = xgu_vertex4f(p, x0, y1, 0.0f, 1.0f);
    p = xgu_color4ub(p, (color >> 16) & 0xFF, (color >> 8) & 0xFF, color & 0xFF, (color >> 24) & 0xFF);

    p = xgu_vertex4f(p, x1, y1, 0.0f, 1.0f);
    p = xgu_color4ub(p, (color >> 16) & 0xFF, (color >> 8) & 0xFF, color & 0xFF, (color >> 24) & 0xFF);

    p = xgu_end(p);

    pb_end(p);
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

    /* Allocate texture in GPU-accessible memory */
    int bpp = (fmt == TEX_FMT_ARGB8888) ? 4 : 2;
    /* Xbox textures must be swizzled */
    tex->data = platform_vram_alloc(w * h * bpp);

    /* Generate texture handle */
    tex->platform_handle = (uint32_t)(uintptr_t)tex->data;

    return tex;
}

void platform_texture_upload(platform_texture_t *tex, const void *data) {
    if (!tex || !data) return;

    int bpp = (tex->format == TEX_FMT_ARGB8888) ? 4 : 2;

    /* Xbox requires swizzled textures for optimal performance */
    /* For simplicity, copy linear (would need swizzle for production) */
    memcpy(tex->data, data, tex->width * tex->height * bpp);
}

void platform_texture_destroy(platform_texture_t *tex) {
    if (tex) {
        if (tex->data) platform_vram_free(tex->data);
        platform_free(tex);
    }
}

void platform_texture_bind(platform_texture_t *tex) {
    if (!tex) return;

    uint32_t *p = pb_begin();

    /* Set texture stage 0 */
    int fmt = (tex->format == TEX_FMT_ARGB8888) ? NV097_SET_TEXTURE_FORMAT_COLOR_SZ_A8R8G8B8 :
                                                  NV097_SET_TEXTURE_FORMAT_COLOR_SZ_R5G6B5;

    p = xgu_set_texture_offset(p, 0, (uint32_t)(uintptr_t)tex->data & 0x03FFFFFF);
    p = xgu_set_texture_format(p, 0, 2, false, fmt, 2, tex->width, tex->height, 1);
    p = xgu_set_texture_control0(p, 0, true, 0, 0);
    p = xgu_set_texture_filter(p, 0, 0, XGU_TEXTURE_CONVOLUTION_GAUSSIAN, 2, 2, false, false, false, false);

    pb_end(p);
}

/*============================================================================
 * Audio Implementation (DirectSound)
 *============================================================================*/

static void *dsound_buffer = NULL;

void platform_audio_init(void) {
    /* Initialize DirectSound */
    DirectSoundCreate(NULL, NULL, NULL);
}

void platform_audio_shutdown(void) {
    /* Cleanup */
}

void platform_audio_update(void) {
    /* Audio streaming */
}

platform_audio_t *platform_audio_load(const char *path) {
    platform_audio_t *audio = platform_malloc(sizeof(platform_audio_t));
    if (!audio) return NULL;

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
    HANDLE handle;
    int is_open;
};

platform_file_t *platform_file_open(const char *path, const char *mode) {
    platform_file_t *file = platform_malloc(sizeof(platform_file_t));
    if (!file) return NULL;

    char full_path[256];
    snprintf(full_path, sizeof(full_path), "D:\\%s", path);  /* DVD drive */

    DWORD access = GENERIC_READ;
    DWORD creation = OPEN_EXISTING;

    if (mode[0] == 'w') {
        access = GENERIC_WRITE;
        creation = CREATE_ALWAYS;
        snprintf(full_path, sizeof(full_path), "E:\\%s", path);  /* HDD */
    }

    OBJECT_ATTRIBUTES obj;
    ANSI_STRING ansi_path;
    IO_STATUS_BLOCK io_status;

    RtlInitAnsiString(&ansi_path, full_path);
    InitializeObjectAttributes(&obj, &ansi_path, OBJ_CASE_INSENSITIVE, NULL, NULL);

    NTSTATUS status = NtCreateFile(&file->handle, access, &obj, &io_status,
                                    NULL, FILE_ATTRIBUTE_NORMAL, 0, creation,
                                    FILE_SYNCHRONOUS_IO_NONALERT, NULL, 0);

    if (!NT_SUCCESS(status)) {
        platform_free(file);
        return NULL;
    }

    file->is_open = 1;
    return file;
}

void platform_file_close(platform_file_t *file) {
    if (file) {
        if (file->is_open) {
            NtClose(file->handle);
        }
        platform_free(file);
    }
}

size_t platform_file_read(platform_file_t *file, void *buf, size_t size) {
    if (!file || !file->is_open) return 0;

    IO_STATUS_BLOCK io_status;
    NTSTATUS status = NtReadFile(file->handle, NULL, NULL, NULL, &io_status,
                                  buf, size, NULL, NULL);

    if (!NT_SUCCESS(status)) return 0;
    return io_status.Information;
}

size_t platform_file_write(platform_file_t *file, const void *buf, size_t size) {
    if (!file || !file->is_open) return 0;

    IO_STATUS_BLOCK io_status;
    NTSTATUS status = NtWriteFile(file->handle, NULL, NULL, NULL, &io_status,
                                   (void *)buf, size, NULL, NULL);

    if (!NT_SUCCESS(status)) return 0;
    return io_status.Information;
}

int platform_file_seek(platform_file_t *file, long offset, int whence) {
    if (!file || !file->is_open) return -1;

    FILE_POSITION_INFORMATION pos_info;
    IO_STATUS_BLOCK io_status;

    LARGE_INTEGER new_pos;
    new_pos.QuadPart = offset;

    if (whence == 1) {  /* SEEK_CUR */
        NtQueryInformationFile(file->handle, &io_status, &pos_info,
                               sizeof(pos_info), FilePositionInformation);
        new_pos.QuadPart += pos_info.CurrentByteOffset.QuadPart;
    } else if (whence == 2) {  /* SEEK_END */
        FILE_STANDARD_INFORMATION std_info;
        NtQueryInformationFile(file->handle, &io_status, &std_info,
                               sizeof(std_info), FileStandardInformation);
        new_pos.QuadPart += std_info.EndOfFile.QuadPart;
    }

    pos_info.CurrentByteOffset = new_pos;
    NTSTATUS status = NtSetInformationFile(file->handle, &io_status, &pos_info,
                                            sizeof(pos_info), FilePositionInformation);

    return NT_SUCCESS(status) ? 0 : -1;
}

long platform_file_tell(platform_file_t *file) {
    if (!file || !file->is_open) return -1;

    FILE_POSITION_INFORMATION pos_info;
    IO_STATUS_BLOCK io_status;

    NtQueryInformationFile(file->handle, &io_status, &pos_info,
                           sizeof(pos_info), FilePositionInformation);

    return (long)pos_info.CurrentByteOffset.QuadPart;
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
    return "D:\\";  /* DVD drive */
}

const char *platform_get_save_path(void) {
    return "E:\\UDATA\\RETRORACR\\";  /* HDD save location */
}

/*============================================================================
 * Xbox Controller Input
 *============================================================================*/

/* Update gamepad handles */
static void update_gamepads(void) {
    DWORD inserted, removed;
    XGetDeviceChanges(XDEVICE_TYPE_GAMEPAD, &inserted, &removed);

    for (int i = 0; i < XBOX_MAX_CONTROLLERS; i++) {
        if (removed & (1 << i)) {
            if (xbox_state.gamepad_handles[i]) {
                XInputClose(xbox_state.gamepad_handles[i]);
                xbox_state.gamepad_handles[i] = NULL;
            }
        }

        if (inserted & (1 << i)) {
            xbox_state.gamepad_handles[i] = XInputOpen(XDEVICE_TYPE_GAMEPAD, i,
                                                        XDEVICE_NO_SLOT, NULL);
        }
    }
}

uint32_t xbox_get_buttons(int port) {
    if (port >= XBOX_MAX_CONTROLLERS) return 0;

    update_gamepads();

    if (!xbox_state.gamepad_handles[port]) return 0;

    if (XInputGetState(xbox_state.gamepad_handles[port], &xbox_state.gamepad_state[port]) != ERROR_SUCCESS) {
        return 0;
    }

    uint32_t result = 0;
    XINPUT_GAMEPAD *gp = &xbox_state.gamepad_state[port].Gamepad;

    if (gp->wButtons & XINPUT_GAMEPAD_A)            result |= PLAT_BTN_A;
    if (gp->wButtons & XINPUT_GAMEPAD_B)            result |= PLAT_BTN_B;
    if (gp->wButtons & XINPUT_GAMEPAD_X)            result |= PLAT_BTN_X;
    if (gp->wButtons & XINPUT_GAMEPAD_Y)            result |= PLAT_BTN_Y;
    if (gp->wButtons & XINPUT_GAMEPAD_START)        result |= PLAT_BTN_START;
    if (gp->wButtons & XINPUT_GAMEPAD_BACK)         result |= PLAT_BTN_BACK;
    if (gp->wButtons & XINPUT_GAMEPAD_LEFT_THUMB)   result |= PLAT_BTN_LS;
    if (gp->wButtons & XINPUT_GAMEPAD_RIGHT_THUMB)  result |= PLAT_BTN_RS;
    if (gp->wButtons & XINPUT_GAMEPAD_DPAD_UP)      result |= PLAT_BTN_DPAD_UP;
    if (gp->wButtons & XINPUT_GAMEPAD_DPAD_DOWN)    result |= PLAT_BTN_DPAD_DOWN;
    if (gp->wButtons & XINPUT_GAMEPAD_DPAD_LEFT)    result |= PLAT_BTN_DPAD_LEFT;
    if (gp->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT)   result |= PLAT_BTN_DPAD_RIGHT;

    /* Black/White buttons (Xbox-specific) */
    if (gp->bAnalogButtons[XINPUT_GAMEPAD_BLACK] > 32)  result |= PLAT_BTN_LB;
    if (gp->bAnalogButtons[XINPUT_GAMEPAD_WHITE] > 32)  result |= PLAT_BTN_RB;

    /* Triggers as digital */
    if (gp->bAnalogButtons[XINPUT_GAMEPAD_LEFT_TRIGGER] > 32)  result |= PLAT_BTN_LT;
    if (gp->bAnalogButtons[XINPUT_GAMEPAD_RIGHT_TRIGGER] > 32) result |= PLAT_BTN_RT;

    return result;
}

void xbox_get_analog(int port, int *lx, int *ly, int *rx, int *ry) {
    if (port >= XBOX_MAX_CONTROLLERS || !xbox_state.gamepad_handles[port]) {
        *lx = *ly = *rx = *ry = 0;
        return;
    }

    XINPUT_GAMEPAD *gp = &xbox_state.gamepad_state[port].Gamepad;

    /* Xbox thumbsticks are signed 16-bit, convert to 0-255 range */
    *lx = (gp->sThumbLX + 32768) >> 8;
    *ly = (gp->sThumbLY + 32768) >> 8;
    *rx = (gp->sThumbRX + 32768) >> 8;
    *ry = (gp->sThumbRY + 32768) >> 8;
}

void xbox_get_triggers(int port, int *lt, int *rt) {
    if (port >= XBOX_MAX_CONTROLLERS || !xbox_state.gamepad_handles[port]) {
        *lt = *rt = 0;
        return;
    }

    XINPUT_GAMEPAD *gp = &xbox_state.gamepad_state[port].Gamepad;

    *lt = gp->bAnalogButtons[XINPUT_GAMEPAD_LEFT_TRIGGER];
    *rt = gp->bAnalogButtons[XINPUT_GAMEPAD_RIGHT_TRIGGER];
}

#endif /* PLATFORM_XBOX */
