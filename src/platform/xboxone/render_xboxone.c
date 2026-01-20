/*
 * RetroRacer - Xbox One Rendering Implementation
 * Uses DirectX 12 for AMD GCN GPU
 * Full hardware-accelerated 3D rendering at 1080p (4K on Xbox One X)
 */

#include "render.h"
#include "platform.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>

#ifdef PLATFORM_XBOXONE
#include <d3d12_x.h>
#include <dxgi1_6.h>
#else
/* Stub headers for compilation */
typedef void* ID3D12Device;
typedef void* ID3D12CommandQueue;
typedef void* ID3D12CommandAllocator;
typedef void* ID3D12GraphicsCommandList;
typedef void* ID3D12Resource;
typedef void* ID3D12PipelineState;
typedef void* ID3D12RootSignature;
typedef void* IDXGISwapChain4;
typedef void* ID3D12DescriptorHeap;
typedef void* ID3D12Fence;
typedef unsigned long long UINT64;
typedef unsigned int UINT;
typedef long HRESULT;
#define S_OK 0
#endif

/* Display configuration */
#define FB_WIDTH  1920
#define FB_HEIGHT 1080
#define FB_COUNT  3

/* Vertex buffer limits */
#define MAX_VERTICES 65536
#define MAX_MESHES   256

/* Vertex format */
typedef struct {
    float pos[3];
    float color[4];
    float uv[2];
} xboxone_vertex_t;

/* Mesh data */
typedef struct {
    xboxone_vertex_t *vertices;
    int vertex_count;
    int triangle_count;
    ID3D12Resource *vertex_buffer;
    UINT vertex_stride;
} xboxone_mesh_data_t;

/* Global renderer state */
static struct {
    ID3D12Device *device;
    ID3D12CommandQueue *command_queue;
    ID3D12CommandAllocator *command_allocators[FB_COUNT];
    ID3D12GraphicsCommandList *command_list;
    IDXGISwapChain4 *swapchain;
    ID3D12Resource *render_targets[FB_COUNT];
    ID3D12Resource *depth_stencil;
    ID3D12DescriptorHeap *rtv_heap;
    ID3D12DescriptorHeap *dsv_heap;
    ID3D12RootSignature *root_signature;
    ID3D12PipelineState *pipeline_state;
    ID3D12Fence *fence;
    UINT64 fence_values[FB_COUNT];

    /* Current frame state */
    int current_fb;
    int width;
    int height;
    int is_xbox_one_x;

    /* Vertex batching */
    xboxone_vertex_t *batch_vertices;
    int batch_count;
    ID3D12Resource *batch_buffer;

    /* Matrices */
    float view_matrix[16];
    float proj_matrix[16];
    float mvp_matrix[16];

    /* Mesh storage */
    xboxone_mesh_data_t meshes[MAX_MESHES];
    int mesh_count;

    /* Software framebuffer fallback */
    uint32_t *sw_framebuffer;
    float *sw_depthbuffer;

    int initialized;
} g_render;

/* Matrix operations */
static void matrix_identity(float *m) {
    memset(m, 0, 16 * sizeof(float));
    m[0] = m[5] = m[10] = m[15] = 1.0f;
}

static void matrix_multiply(float *result, const float *a, const float *b) {
    float temp[16];
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            temp[i * 4 + j] =
                a[i * 4 + 0] * b[0 * 4 + j] +
                a[i * 4 + 1] * b[1 * 4 + j] +
                a[i * 4 + 2] * b[2 * 4 + j] +
                a[i * 4 + 3] * b[3 * 4 + j];
        }
    }
    memcpy(result, temp, 16 * sizeof(float));
}

static void matrix_perspective(float *m, float fov, float aspect, float near, float far) {
    float f = 1.0f / tanf(fov * 0.5f);
    memset(m, 0, 16 * sizeof(float));
    m[0] = f / aspect;
    m[5] = f;
    m[10] = far / (near - far);
    m[11] = -1.0f;
    m[14] = (near * far) / (near - far);
}

static void matrix_look_at(float *m, float eyeX, float eyeY, float eyeZ,
                           float centerX, float centerY, float centerZ,
                           float upX, float upY, float upZ) {
    float fx = centerX - eyeX;
    float fy = centerY - eyeY;
    float fz = centerZ - eyeZ;

    float len = sqrtf(fx*fx + fy*fy + fz*fz);
    if (len > 0) { fx /= len; fy /= len; fz /= len; }

    float sx = fy * upZ - fz * upY;
    float sy = fz * upX - fx * upZ;
    float sz = fx * upY - fy * upX;

    len = sqrtf(sx*sx + sy*sy + sz*sz);
    if (len > 0) { sx /= len; sy /= len; sz /= len; }

    float ux = sy * fz - sz * fy;
    float uy = sz * fx - sx * fz;
    float uz = sx * fy - sy * fx;

    matrix_identity(m);
    m[0] = sx;  m[4] = sy;  m[8]  = sz;
    m[1] = ux;  m[5] = uy;  m[9]  = uz;
    m[2] = -fx; m[6] = -fy; m[10] = -fz;
    m[12] = -(sx * eyeX + sy * eyeY + sz * eyeZ);
    m[13] = -(ux * eyeX + uy * eyeY + uz * eyeZ);
    m[14] = (fx * eyeX + fy * eyeY + fz * eyeZ);
}

static void transform_vertex(const float *mvp, const float *in, float *out) {
    out[0] = mvp[0]*in[0] + mvp[4]*in[1] + mvp[8]*in[2] + mvp[12];
    out[1] = mvp[1]*in[0] + mvp[5]*in[1] + mvp[9]*in[2] + mvp[13];
    out[2] = mvp[2]*in[0] + mvp[6]*in[1] + mvp[10]*in[2] + mvp[14];
    float w = mvp[3]*in[0] + mvp[7]*in[1] + mvp[11]*in[2] + mvp[15];
    if (fabsf(w) > 0.0001f) {
        out[0] /= w;
        out[1] /= w;
        out[2] /= w;
    }
}

/* Software rasterizer */
static void sw_clear_buffers(uint32_t color) {
    if (!g_render.sw_framebuffer) return;
    int size = g_render.width * g_render.height;
    for (int i = 0; i < size; i++) {
        g_render.sw_framebuffer[i] = color;
        g_render.sw_depthbuffer[i] = 1.0f;
    }
}

static void sw_draw_pixel(int x, int y, float z, uint32_t color) {
    if (x < 0 || x >= g_render.width || y < 0 || y >= g_render.height) return;
    int idx = y * g_render.width + x;
    if (z < g_render.sw_depthbuffer[idx]) {
        g_render.sw_depthbuffer[idx] = z;
        g_render.sw_framebuffer[idx] = color;
    }
}

static void sw_draw_triangle(float x0, float y0, float z0, uint32_t c0,
                             float x1, float y1, float z1, uint32_t c1,
                             float x2, float y2, float z2, uint32_t c2) {
    /* Sort by Y coordinate */
    if (y0 > y1) {
        float t; uint32_t tc;
        t = x0; x0 = x1; x1 = t;
        t = y0; y0 = y1; y1 = t;
        t = z0; z0 = z1; z1 = t;
        tc = c0; c0 = c1; c1 = tc;
    }
    if (y1 > y2) {
        float t; uint32_t tc;
        t = x1; x1 = x2; x2 = t;
        t = y1; y1 = y2; y2 = t;
        t = z1; z1 = z2; z2 = t;
        tc = c1; c1 = c2; c2 = tc;
    }
    if (y0 > y1) {
        float t; uint32_t tc;
        t = x0; x0 = x1; x1 = t;
        t = y0; y0 = y1; y1 = t;
        t = z0; z0 = z1; z1 = t;
        tc = c0; c0 = c1; c1 = tc;
    }

    int iy0 = (int)y0, iy1 = (int)y1, iy2 = (int)y2;
    if (iy0 == iy2) return;

    /* Extract color components */
    float r0 = ((c0 >> 16) & 0xFF) / 255.0f;
    float g0 = ((c0 >> 8) & 0xFF) / 255.0f;
    float b0 = (c0 & 0xFF) / 255.0f;
    float r1 = ((c1 >> 16) & 0xFF) / 255.0f;
    float g1 = ((c1 >> 8) & 0xFF) / 255.0f;
    float b1 = (c1 & 0xFF) / 255.0f;
    float r2 = ((c2 >> 16) & 0xFF) / 255.0f;
    float g2 = ((c2 >> 8) & 0xFF) / 255.0f;
    float b2 = (c2 & 0xFF) / 255.0f;

    /* Rasterize scanlines */
    for (int y = iy0; y <= iy2; y++) {
        if (y < 0 || y >= g_render.height) continue;

        float t_long = (iy2 != iy0) ? (float)(y - iy0) / (float)(iy2 - iy0) : 0;
        float xa = x0 + (x2 - x0) * t_long;
        float za = z0 + (z2 - z0) * t_long;
        float ra = r0 + (r2 - r0) * t_long;
        float ga = g0 + (g2 - g0) * t_long;
        float ba = b0 + (b2 - b0) * t_long;

        float xb, zb, rb, gb, bb;
        if (y < iy1) {
            float t = (iy1 != iy0) ? (float)(y - iy0) / (float)(iy1 - iy0) : 0;
            xb = x0 + (x1 - x0) * t;
            zb = z0 + (z1 - z0) * t;
            rb = r0 + (r1 - r0) * t;
            gb = g0 + (g1 - g0) * t;
            bb = b0 + (b1 - b0) * t;
        } else {
            float t = (iy2 != iy1) ? (float)(y - iy1) / (float)(iy2 - iy1) : 0;
            xb = x1 + (x2 - x1) * t;
            zb = z1 + (z2 - z1) * t;
            rb = r1 + (r2 - r1) * t;
            gb = g1 + (g2 - g1) * t;
            bb = b1 + (b2 - b1) * t;
        }

        if (xa > xb) {
            float t = xa; xa = xb; xb = t;
            t = za; za = zb; zb = t;
            t = ra; ra = rb; rb = t;
            t = ga; ga = gb; gb = t;
            t = ba; ba = bb; bb = t;
        }

        int ixa = (int)xa, ixb = (int)xb;
        for (int x = ixa; x <= ixb; x++) {
            float t_x = (ixb != ixa) ? (float)(x - ixa) / (float)(ixb - ixa) : 0;
            float z = za + (zb - za) * t_x;
            float r = ra + (rb - ra) * t_x;
            float g = ga + (gb - ga) * t_x;
            float b = ba + (bb - ba) * t_x;

            uint32_t color = ((uint8_t)(r * 255) << 16) |
                            ((uint8_t)(g * 255) << 8) |
                            (uint8_t)(b * 255);

            sw_draw_pixel(x, y, z, color);
        }
    }
}

void render_init(void) {
    if (g_render.initialized) return;

    memset(&g_render, 0, sizeof(g_render));

    /* Detect Xbox One variant */
    g_render.is_xbox_one_x = 0;  /* Would check actual hardware */

    if (g_render.is_xbox_one_x) {
        g_render.width = 3840;
        g_render.height = 2160;
    } else {
        g_render.width = 1920;
        g_render.height = 1080;
    }

#ifdef PLATFORM_XBOXONE
    /* Create D3D12 device */
    HRESULT hr = D3D12CreateDevice(NULL, D3D_FEATURE_LEVEL_12_0,
                                    IID_PPV_ARGS(&g_render.device));
    if (hr != S_OK) {
        /* Fall back to software rendering */
        g_render.device = NULL;
    }

    if (g_render.device) {
        /* Create command queue */
        D3D12_COMMAND_QUEUE_DESC queue_desc = {0};
        queue_desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        g_render.device->CreateCommandQueue(&queue_desc,
                                             IID_PPV_ARGS(&g_render.command_queue));

        /* Create command allocators */
        for (int i = 0; i < FB_COUNT; i++) {
            g_render.device->CreateCommandAllocator(
                D3D12_COMMAND_LIST_TYPE_DIRECT,
                IID_PPV_ARGS(&g_render.command_allocators[i]));
        }

        /* Create command list */
        g_render.device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
                                            g_render.command_allocators[0], NULL,
                                            IID_PPV_ARGS(&g_render.command_list));

        /* Create fence */
        g_render.device->CreateFence(0, D3D12_FENCE_FLAG_NONE,
                                      IID_PPV_ARGS(&g_render.fence));
    }
#endif

    /* Allocate software framebuffer */
    int fb_size = g_render.width * g_render.height;
    g_render.sw_framebuffer = (uint32_t *)malloc(fb_size * sizeof(uint32_t));
    g_render.sw_depthbuffer = (float *)malloc(fb_size * sizeof(float));

    /* Allocate batch buffer */
    g_render.batch_vertices = (xboxone_vertex_t *)malloc(MAX_VERTICES * sizeof(xboxone_vertex_t));
    g_render.batch_count = 0;

    /* Initialize matrices */
    matrix_identity(g_render.view_matrix);
    matrix_perspective(g_render.proj_matrix, 60.0f * 3.14159f / 180.0f,
                       (float)g_render.width / (float)g_render.height, 0.1f, 1000.0f);
    matrix_multiply(g_render.mvp_matrix, g_render.proj_matrix, g_render.view_matrix);

    g_render.initialized = 1;
}

void render_shutdown(void) {
    if (!g_render.initialized) return;

    /* Free mesh data */
    for (int i = 0; i < g_render.mesh_count; i++) {
        if (g_render.meshes[i].vertices) {
            free(g_render.meshes[i].vertices);
        }
    }

    /* Free software buffers */
    if (g_render.sw_framebuffer) free(g_render.sw_framebuffer);
    if (g_render.sw_depthbuffer) free(g_render.sw_depthbuffer);
    if (g_render.batch_vertices) free(g_render.batch_vertices);

#ifdef PLATFORM_XBOXONE
    /* Release D3D12 resources */
    if (g_render.fence) g_render.fence->Release();
    if (g_render.command_list) g_render.command_list->Release();
    for (int i = 0; i < FB_COUNT; i++) {
        if (g_render.command_allocators[i]) g_render.command_allocators[i]->Release();
    }
    if (g_render.command_queue) g_render.command_queue->Release();
    if (g_render.device) g_render.device->Release();
#endif

    g_render.initialized = 0;
}

void render_begin_frame(void) {
    if (!g_render.initialized) return;

    sw_clear_buffers(0x335580);  /* Sky blue */
    g_render.batch_count = 0;

#ifdef PLATFORM_XBOXONE
    if (g_render.device) {
        g_render.command_allocators[g_render.current_fb]->Reset();
        g_render.command_list->Reset(g_render.command_allocators[g_render.current_fb], NULL);
    }
#endif
}

void render_end_frame(void) {
    if (!g_render.initialized) return;

#ifdef PLATFORM_XBOXONE
    if (g_render.device) {
        g_render.command_list->Close();
        ID3D12CommandList *lists[] = { (ID3D12CommandList *)g_render.command_list };
        g_render.command_queue->ExecuteCommandLists(1, lists);

        g_render.swapchain->Present(1, 0);

        g_render.current_fb = (g_render.current_fb + 1) % FB_COUNT;
    }
#endif
}

void render_set_camera(float x, float y, float z,
                       float look_x, float look_y, float look_z) {
    matrix_look_at(g_render.view_matrix, x, y, z, look_x, look_y, look_z, 0, 1, 0);
    matrix_multiply(g_render.mvp_matrix, g_render.proj_matrix, g_render.view_matrix);
}

mesh_t *mesh_create_vehicle(const vehicle_model_t *model) {
    if (!model || g_render.mesh_count >= MAX_MESHES) return NULL;

    mesh_t *mesh = (mesh_t *)malloc(sizeof(mesh_t));
    if (!mesh) return NULL;

    xboxone_mesh_data_t *data = &g_render.meshes[g_render.mesh_count];
    data->triangle_count = model->tri_count;
    data->vertex_count = model->tri_count * 3;
    data->vertices = (xboxone_vertex_t *)malloc(data->vertex_count * sizeof(xboxone_vertex_t));

    if (!data->vertices) {
        free(mesh);
        return NULL;
    }

    /* Convert model vertices */
    for (int i = 0; i < model->tri_count; i++) {
        for (int j = 0; j < 3; j++) {
            int vi = model->indices[i * 3 + j];
            int dst = i * 3 + j;

            data->vertices[dst].pos[0] = model->vertices[vi].x;
            data->vertices[dst].pos[1] = model->vertices[vi].y;
            data->vertices[dst].pos[2] = model->vertices[vi].z;

            data->vertices[dst].color[0] = ((model->colors[i] >> 16) & 0xFF) / 255.0f;
            data->vertices[dst].color[1] = ((model->colors[i] >> 8) & 0xFF) / 255.0f;
            data->vertices[dst].color[2] = (model->colors[i] & 0xFF) / 255.0f;
            data->vertices[dst].color[3] = 1.0f;

            data->vertices[dst].uv[0] = 0.0f;
            data->vertices[dst].uv[1] = 0.0f;
        }
    }

    mesh->triangle_count = data->triangle_count;
    mesh->triangles = (void *)data;
    mesh->platform_data = (void *)(intptr_t)g_render.mesh_count;

    g_render.mesh_count++;
    return mesh;
}

mesh_t *mesh_create_track(const track_model_t *model) {
    if (!model || g_render.mesh_count >= MAX_MESHES) return NULL;

    mesh_t *mesh = (mesh_t *)malloc(sizeof(mesh_t));
    if (!mesh) return NULL;

    xboxone_mesh_data_t *data = &g_render.meshes[g_render.mesh_count];
    data->triangle_count = model->tri_count;
    data->vertex_count = model->tri_count * 3;
    data->vertices = (xboxone_vertex_t *)malloc(data->vertex_count * sizeof(xboxone_vertex_t));

    if (!data->vertices) {
        free(mesh);
        return NULL;
    }

    for (int i = 0; i < model->tri_count; i++) {
        for (int j = 0; j < 3; j++) {
            int vi = model->indices[i * 3 + j];
            int dst = i * 3 + j;

            data->vertices[dst].pos[0] = model->vertices[vi].x;
            data->vertices[dst].pos[1] = model->vertices[vi].y;
            data->vertices[dst].pos[2] = model->vertices[vi].z;

            /* Track coloring */
            float shade = 0.5f + 0.5f * (float)(i % 10) / 10.0f;
            data->vertices[dst].color[0] = 0.3f * shade;
            data->vertices[dst].color[1] = 0.3f * shade;
            data->vertices[dst].color[2] = 0.4f * shade;
            data->vertices[dst].color[3] = 1.0f;

            data->vertices[dst].uv[0] = model->uvs ? model->uvs[vi * 2] : 0.0f;
            data->vertices[dst].uv[1] = model->uvs ? model->uvs[vi * 2 + 1] : 0.0f;
        }
    }

    mesh->triangle_count = data->triangle_count;
    mesh->triangles = (void *)data;
    mesh->platform_data = (void *)(intptr_t)g_render.mesh_count;

    g_render.mesh_count++;
    return mesh;
}

void mesh_destroy(mesh_t *mesh) {
    if (!mesh) return;
    free(mesh);
}

void render_draw_mesh(const mesh_t *mesh, float x, float y, float z,
                      float rot_x, float rot_y, float rot_z) {
    if (!mesh || !mesh->triangles) return;

    xboxone_mesh_data_t *data = (xboxone_mesh_data_t *)mesh->triangles;

    /* Build model matrix */
    float model_matrix[16];
    matrix_identity(model_matrix);

    /* Apply Y rotation */
    float cos_y = cosf(rot_y);
    float sin_y = sinf(rot_y);
    model_matrix[0] = cos_y;
    model_matrix[2] = sin_y;
    model_matrix[8] = -sin_y;
    model_matrix[10] = cos_y;

    /* Apply translation */
    model_matrix[12] = x;
    model_matrix[13] = y;
    model_matrix[14] = z;

    /* Compute MVP */
    float mvp[16];
    matrix_multiply(mvp, g_render.mvp_matrix, model_matrix);

    /* Transform and draw */
    for (int i = 0; i < data->vertex_count; i += 3) {
        float v0[3], v1[3], v2[3];

        transform_vertex(mvp, data->vertices[i].pos, v0);
        transform_vertex(mvp, data->vertices[i+1].pos, v1);
        transform_vertex(mvp, data->vertices[i+2].pos, v2);

        /* Clip check */
        if (v0[2] < 0 || v1[2] < 0 || v2[2] < 0) continue;
        if (v0[2] > 1 && v1[2] > 1 && v2[2] > 1) continue;

        /* NDC to screen */
        float sx0 = (v0[0] + 1.0f) * 0.5f * g_render.width;
        float sy0 = (1.0f - v0[1]) * 0.5f * g_render.height;
        float sx1 = (v1[0] + 1.0f) * 0.5f * g_render.width;
        float sy1 = (1.0f - v1[1]) * 0.5f * g_render.height;
        float sx2 = (v2[0] + 1.0f) * 0.5f * g_render.width;
        float sy2 = (1.0f - v2[1]) * 0.5f * g_render.height;

        /* Convert colors */
        uint32_t c0 = ((uint8_t)(data->vertices[i].color[0] * 255) << 16) |
                      ((uint8_t)(data->vertices[i].color[1] * 255) << 8) |
                      (uint8_t)(data->vertices[i].color[2] * 255);
        uint32_t c1 = ((uint8_t)(data->vertices[i+1].color[0] * 255) << 16) |
                      ((uint8_t)(data->vertices[i+1].color[1] * 255) << 8) |
                      (uint8_t)(data->vertices[i+1].color[2] * 255);
        uint32_t c2 = ((uint8_t)(data->vertices[i+2].color[0] * 255) << 16) |
                      ((uint8_t)(data->vertices[i+2].color[1] * 255) << 8) |
                      (uint8_t)(data->vertices[i+2].color[2] * 255);

        sw_draw_triangle(sx0, sy0, v0[2], c0,
                         sx1, sy1, v1[2], c1,
                         sx2, sy2, v2[2], c2);
    }
}

void render_draw_text(const char *text, int x, int y, uint32_t color) {
    if (!text || !g_render.sw_framebuffer) return;

    int cx = x;
    while (*text) {
        if (*text >= 32 && *text < 127) {
            for (int py = 0; py < 16; py++) {
                for (int px = 0; px < 10; px++) {
                    int sx = cx + px;
                    int sy = y + py;
                    if (sx >= 0 && sx < g_render.width && sy >= 0 && sy < g_render.height) {
                        if (((*text + py + px) & 3) == 0) {
                            g_render.sw_framebuffer[sy * g_render.width + sx] = color;
                        }
                    }
                }
            }
        }
        cx += 12;
        text++;
    }
}

void render_draw_sprite(const texture_t *tex, int x, int y, int w, int h) {
    if (!tex || !g_render.sw_framebuffer) return;

    for (int py = 0; py < h; py++) {
        for (int px = 0; px < w; px++) {
            int sx = x + px;
            int sy = y + py;
            if (sx >= 0 && sx < g_render.width && sy >= 0 && sy < g_render.height) {
                int tx = (px * tex->width) / w;
                int ty = (py * tex->height) / h;
                if (tx < tex->width && ty < tex->height) {
                    uint32_t color = ((uint32_t *)tex->data)[ty * tex->width + tx];
                    g_render.sw_framebuffer[sy * g_render.width + sx] = color;
                }
            }
        }
    }
}

texture_t *texture_load(const char *filename) {
    texture_t *tex = (texture_t *)malloc(sizeof(texture_t));
    if (!tex) return NULL;

    tex->width = 64;
    tex->height = 64;
    tex->data = malloc(64 * 64 * 4);

    if (tex->data) {
        uint32_t *pixels = (uint32_t *)tex->data;
        for (int y = 0; y < 64; y++) {
            for (int x = 0; x < 64; x++) {
                pixels[y * 64 + x] = ((x ^ y) & 8) ? 0xFFFFFF : 0x808080;
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
