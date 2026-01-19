/*
 * RetroRacer - Native Platform Implementation
 *
 * For development/testing on PC without console hardware.
 * Used when no other platform is defined.
 */

#include "platform.h"

#if defined(PLATFORM_NATIVE) || (!defined(PLATFORM_PSX) && !defined(PLATFORM_PS2) && \
    !defined(PLATFORM_PS3) && !defined(PLATFORM_XBOX) && !defined(PLATFORM_XBOX360) && \
    !defined(PLATFORM_DREAMCAST))

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

/*============================================================================
 * Native Platform State
 *============================================================================*/

static struct {
    uint64_t start_time;
    int running;
    uint32_t buttons;
    float analog_x, analog_y;
} native_state;

/* Platform capabilities */
static const platform_caps_t native_caps = {
    .name = "Native (Development)",
    .ram_size = 1024 * 1024 * 1024,     /* 1GB */
    .vram_size = 512 * 1024 * 1024,     /* 512MB */
    .max_texture_size = 8192,
    .max_vertices = 10000000,
    .screen_width = 640,
    .screen_height = 480,
    .has_analog_sticks = 1,
    .has_triggers = 1,
    .has_rumble = 0,
    .color_depth = 32
};

/*============================================================================
 * Platform Core Functions
 *============================================================================*/

const platform_caps_t *platform_get_caps(void) {
    return &native_caps;
}

static uint64_t get_time_us_internal(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000000 + tv.tv_usec;
}

int platform_init(void) {
    printf("[Platform] Native platform initialized\n");
    native_state.start_time = get_time_us_internal();
    native_state.running = 1;
    native_state.buttons = 0;
    native_state.analog_x = 0.0f;
    native_state.analog_y = 0.0f;
    return 1;
}

void platform_shutdown(void) {
    printf("[Platform] Native platform shutdown\n");
}

uint64_t platform_get_time_us(void) {
    return get_time_us_internal() - native_state.start_time;
}

void platform_sleep_ms(uint32_t ms) {
    usleep(ms * 1000);
}

void platform_exit(void) {
    native_state.running = 0;
    exit(0);
}

/*============================================================================
 * Memory Management
 *============================================================================*/

void *platform_malloc(size_t size) {
    return malloc(size);
}

void *platform_malloc_aligned(size_t size, size_t alignment) {
    void *ptr = NULL;
    if (posix_memalign(&ptr, alignment, size) != 0) {
        return NULL;
    }
    return ptr;
}

void *platform_realloc(void *ptr, size_t size) {
    return realloc(ptr, size);
}

void platform_free(void *ptr) {
    free(ptr);
}

void *platform_vram_alloc(size_t size) {
    return malloc(size);
}

void platform_vram_free(void *ptr) {
    free(ptr);
}

/*============================================================================
 * Graphics Implementation (Stub)
 *============================================================================*/

static int frame_count = 0;

void platform_gfx_init(void) {
    printf("[Platform] Graphics initialized (stub)\n");
}

void platform_gfx_shutdown(void) {
    printf("[Platform] Graphics shutdown (stub)\n");
}

void platform_gfx_begin_frame(void) {
    frame_count++;
}

void platform_gfx_end_frame(void) {
    /* Print frame counter occasionally */
    if (frame_count % 300 == 0) {
        printf("[Platform] Frame %d\n", frame_count);
    }
}

void platform_gfx_set_render_list(render_list_t list) {
    (void)list;
}

void platform_gfx_clear(uint32_t color) {
    (void)color;
}

/*============================================================================
 * Texture Support (Stub)
 *============================================================================*/

platform_texture_t *platform_texture_create(uint16_t w, uint16_t h, tex_format_t fmt) {
    platform_texture_t *tex = platform_malloc(sizeof(platform_texture_t));
    if (!tex) return NULL;

    tex->width = w;
    tex->height = h;
    tex->format = fmt;
    tex->data = platform_malloc(w * h * 4);
    tex->platform_handle = 0;

    return tex;
}

void platform_texture_upload(platform_texture_t *tex, const void *data) {
    if (tex && data && tex->data) {
        memcpy(tex->data, data, tex->width * tex->height * 4);
    }
}

void platform_texture_destroy(platform_texture_t *tex) {
    if (tex) {
        if (tex->data) platform_free(tex->data);
        platform_free(tex);
    }
}

void platform_texture_bind(platform_texture_t *tex) {
    (void)tex;
}

/*============================================================================
 * Audio Implementation (Stub)
 *============================================================================*/

void platform_audio_init(void) {
    printf("[Platform] Audio initialized (stub)\n");
}

void platform_audio_shutdown(void) {
    printf("[Platform] Audio shutdown (stub)\n");
}

void platform_audio_update(void) {
    /* Nothing */
}

platform_audio_t *platform_audio_load(const char *path) {
    platform_audio_t *audio = platform_malloc(sizeof(platform_audio_t));
    if (!audio) return NULL;

    printf("[Platform] Audio load: %s (stub)\n", path);

    audio->data = NULL;
    audio->size = 0;
    audio->sample_rate = 44100;
    audio->format = AUDIO_FMT_PCM16;
    audio->channels = 2;
    audio->platform_handle = 0;

    return audio;
}

void platform_audio_unload(platform_audio_t *audio) {
    if (audio) platform_free(audio);
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
    FILE *fp;
};

platform_file_t *platform_file_open(const char *path, const char *mode) {
    platform_file_t *file = platform_malloc(sizeof(platform_file_t));
    if (!file) return NULL;

    file->fp = fopen(path, mode);
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
    FILE *fp = fopen(path, "rb");
    if (fp) {
        fclose(fp);
        return 1;
    }
    return 0;
}

const char *platform_get_data_path(void) {
    return "./data/";
}

const char *platform_get_save_path(void) {
    return "./save/";
}

/*============================================================================
 * Native Input (Stub - simulates controller)
 *============================================================================*/

uint32_t native_get_buttons(int port) {
    (void)port;
    return native_state.buttons;
}

void native_get_analog(int port, int *lx, int *ly, int *rx, int *ry) {
    (void)port;
    *lx = 128 + (int)(native_state.analog_x * 127);
    *ly = 128 + (int)(native_state.analog_y * 127);
    *rx = 128;
    *ry = 128;
}

/* For testing - simulate button presses */
void native_set_buttons(uint32_t buttons) {
    native_state.buttons = buttons;
}

void native_set_analog(float x, float y) {
    native_state.analog_x = x;
    native_state.analog_y = y;
}

#endif /* PLATFORM_NATIVE */
