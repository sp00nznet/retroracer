/*
 * RetroRacer - PlayStation 2 Rendering Implementation
 * Uses PS2SDK with GS (Graphics Synthesizer) and VU1
 */

#include "render.h"
#include "platform.h"

#ifdef PLATFORM_PS2

#include <tamtypes.h>
#include <kernel.h>
#include <graph.h>
#include <dma.h>
#include <gs_psm.h>
#include <draw.h>
#include <draw3d.h>
#include <packet.h>

/* Double buffering */
static framebuffer_t frame[2];
static zbuffer_t zb;
static int current_buffer = 0;

/* GS packet for DMA */
static packet_t *packet;
static packet_t *flip_packet;
static qword_t *dma_buffer;

static camera_t *current_camera = NULL;

/* Vertex transformation matrices */
static MATRIX view_matrix;
static MATRIX proj_matrix;
static MATRIX vp_matrix;

void render_init(void) {
    /* Initialize GS */
    graph_set_mode(GRAPH_MODE_INTERLACED, GRAPH_MODE_NTSC, GRAPH_MODE_FIELD, GRAPH_ENABLE);
    graph_set_screen(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);

    /* Setup framebuffers */
    frame[0].width = SCREEN_WIDTH;
    frame[0].height = SCREEN_HEIGHT;
    frame[0].psm = GS_PSM_32;
    frame[0].address = graph_vram_allocate(SCREEN_WIDTH, SCREEN_HEIGHT, GS_PSM_32, GRAPH_ALIGN_PAGE);

    frame[1].width = SCREEN_WIDTH;
    frame[1].height = SCREEN_HEIGHT;
    frame[1].psm = GS_PSM_32;
    frame[1].address = graph_vram_allocate(SCREEN_WIDTH, SCREEN_HEIGHT, GS_PSM_32, GRAPH_ALIGN_PAGE);

    /* Setup Z-buffer */
    zb.enable = DRAW_ENABLE;
    zb.method = ZTEST_METHOD_GREATER_EQUAL;
    zb.zsm = GS_ZBUF_24;
    zb.address = graph_vram_allocate(SCREEN_WIDTH, SCREEN_HEIGHT, GS_ZBUF_24, GRAPH_ALIGN_PAGE);

    /* Initialize DMA controller */
    dma_channel_initialize(DMA_CHANNEL_GIF, NULL, 0);
    dma_channel_fast_waits(DMA_CHANNEL_GIF);

    /* Allocate DMA packets */
    packet = packet_init(50000, PACKET_NORMAL);
    flip_packet = packet_init(8, PACKET_NORMAL);

    graph_set_framebuffer_filtered(frame[0].address, SCREEN_WIDTH, GS_PSM_32, 0, 0);

    current_buffer = 0;
}

void render_begin_frame(void) {
    /* Start new packet */
    qword_t *q = packet->data;

    /* Clear screen */
    q = draw_setup_environment(q, 0, &frame[current_buffer], &zb);
    q = draw_clear(q, 0, 0.0f, 0.0f, frame[current_buffer].width, frame[current_buffer].height,
                   0x87, 0xCE, 0xEB);  /* Sky blue */
    q = draw_primitive_xyoffset(q, 0, 2048.0f - (SCREEN_WIDTH / 2), 2048.0f - (SCREEN_HEIGHT / 2));

    dma_buffer = q;
}

void render_end_frame(void) {
    qword_t *q = dma_buffer;

    q = draw_finish(q);

    /* Send to GIF */
    dma_channel_send_normal(DMA_CHANNEL_GIF, packet->data, q - packet->data, 0, 0);
    dma_wait_fast();
}

void render_begin_hud(void) {
    /* HUD uses same drawing context on PS2 */
}

void render_end_hud(void) {
    /* Flip buffers */
    graph_wait_vsync();

    qword_t *q = flip_packet->data;
    q = draw_framebuffer(q, 0, &frame[current_buffer ^ 1]);
    q = draw_finish(q);

    dma_channel_send_normal(DMA_CHANNEL_GIF, flip_packet->data, q - flip_packet->data, 0, 0);
    dma_wait_fast();

    current_buffer ^= 1;
}

void render_clear(uint32_t color) {
    /* Clear is done in render_begin_frame */
    (void)color;
}

void render_set_camera(camera_t *cam) {
    current_camera = cam;

    if (!cam) return;

    /* Build view matrix */
    VECTOR position, target, up;

    position[0] = cam->position.x;
    position[1] = cam->position.y;
    position[2] = cam->position.z;
    position[3] = 1.0f;

    target[0] = cam->target.x;
    target[1] = cam->target.y;
    target[2] = cam->target.z;
    target[3] = 1.0f;

    up[0] = cam->up.x;
    up[1] = cam->up.y;
    up[2] = cam->up.z;
    up[3] = 1.0f;

    create_view_screen(view_matrix, 16.0f/9.0f, -0.5f, 0.5f, -0.5f, 0.5f, 1.0f, 2000.0f);
    create_local_world(proj_matrix, position, (VECTOR){0, 0, 0, 0});

    /* Combine matrices */
    matrix_multiply(vp_matrix, proj_matrix, view_matrix);
}

void camera_update(camera_t *cam) {
    cam->view_matrix = mat4_look_at(cam->position, cam->target, cam->up);
    cam->proj_matrix = mat4_perspective(
        deg_to_rad(cam->fov),
        cam->aspect,
        cam->near_plane,
        cam->far_plane
    );
}

void render_draw_triangle(vertex_t *v0, vertex_t *v1, vertex_t *v2) {
    if (!current_camera) return;

    qword_t *q = dma_buffer;

    /* Transform vertices */
    VECTOR vert[3];
    vert[0][0] = v0->pos.x; vert[0][1] = v0->pos.y; vert[0][2] = v0->pos.z; vert[0][3] = 1.0f;
    vert[1][0] = v1->pos.x; vert[1][1] = v1->pos.y; vert[1][2] = v1->pos.z; vert[1][3] = 1.0f;
    vert[2][0] = v2->pos.x; vert[2][1] = v2->pos.y; vert[2][2] = v2->pos.z; vert[2][3] = 1.0f;

    VECTOR transformed[3];
    VECTOR screen[3];

    for (int i = 0; i < 3; i++) {
        vector_apply(transformed[i], vert[i], vp_matrix);

        /* Perspective divide and screen transform */
        if (transformed[i][3] != 0.0f) {
            float inv_w = 1.0f / transformed[i][3];
            screen[i][0] = (transformed[i][0] * inv_w * SCREEN_WIDTH/2) + SCREEN_WIDTH/2 + 2048.0f;
            screen[i][1] = (-transformed[i][1] * inv_w * SCREEN_HEIGHT/2) + SCREEN_HEIGHT/2 + 2048.0f;
            screen[i][2] = (transformed[i][2] * inv_w + 1.0f) * 0.5f * ((float)0xFFFFFF);
        }
    }

    /* Check if triangle is visible */
    if (screen[0][2] < 0 || screen[1][2] < 0 || screen[2][2] < 0) return;

    /* Draw gouraud shaded triangle */
    color_t colors[3];
    colors[0].r = (v0->color >> 16) & 0xFF;
    colors[0].g = (v0->color >> 8) & 0xFF;
    colors[0].b = v0->color & 0xFF;
    colors[0].a = 0x80;

    colors[1].r = (v1->color >> 16) & 0xFF;
    colors[1].g = (v1->color >> 8) & 0xFF;
    colors[1].b = v1->color & 0xFF;
    colors[1].a = 0x80;

    colors[2].r = (v2->color >> 16) & 0xFF;
    colors[2].g = (v2->color >> 8) & 0xFF;
    colors[2].b = v2->color & 0xFF;
    colors[2].a = 0x80;

    xyz_t xyz[3];
    xyz[0].x = (u16)screen[0][0];
    xyz[0].y = (u16)screen[0][1];
    xyz[0].z = (u32)screen[0][2];

    xyz[1].x = (u16)screen[1][0];
    xyz[1].y = (u16)screen[1][1];
    xyz[1].z = (u32)screen[1][2];

    xyz[2].x = (u16)screen[2][0];
    xyz[2].y = (u16)screen[2][1];
    xyz[2].z = (u32)screen[2][2];

    q = draw_triangle_filled(q, &xyz[0], &xyz[1], &xyz[2], colors[0], colors[1], colors[2]);

    dma_buffer = q;
}

void render_draw_mesh(mesh_t *mesh, mat4_t transform) {
    if (!mesh || !current_camera) return;

    for (int i = 0; i < mesh->tri_count; i++) {
        triangle_t *tri = &mesh->triangles[i];

        vertex_t v0 = tri->v[0];
        vertex_t v1 = tri->v[1];
        vertex_t v2 = tri->v[2];

        v0.pos = mat4_transform_vec3(transform, v0.pos);
        v1.pos = mat4_transform_vec3(transform, v1.pos);
        v2.pos = mat4_transform_vec3(transform, v2.pos);

        render_draw_triangle(&v0, &v1, &v2);
    }
}

void render_draw_quad(vec3_t pos, float width, float height, uint32_t color) {
    vertex_t v0, v1, v2, v3;
    float hw = width * 0.5f;
    float hh = height * 0.5f;

    v0.pos = vec3_create(pos.x - hw, pos.y, pos.z - hh);
    v1.pos = vec3_create(pos.x + hw, pos.y, pos.z - hh);
    v2.pos = vec3_create(pos.x + hw, pos.y, pos.z + hh);
    v3.pos = vec3_create(pos.x - hw, pos.y, pos.z + hh);

    v0.color = v1.color = v2.color = v3.color = color;

    render_draw_triangle(&v0, &v1, &v2);
    render_draw_triangle(&v0, &v2, &v3);
}

void render_wait_vram_ready(void) {
    dma_wait_fast();
}

void render_draw_sky_background(uint32_t color) {
    /* Sky is handled by clear color on PS2 */
    (void)color;
}

void render_draw_rect_2d(int x, int y, int w, int h, uint32_t color) {
    qword_t *q = dma_buffer;

    color_t c;
    c.r = (color >> 16) & 0xFF;
    c.g = (color >> 8) & 0xFF;
    c.b = color & 0xFF;
    c.a = (color >> 24) & 0xFF;
    if (c.a == 0) c.a = 0x80;

    rect_t rect;
    rect.v0.x = x + 2048.0f;
    rect.v0.y = y + 2048.0f;
    rect.v1.x = x + w + 2048.0f;
    rect.v1.y = y + h + 2048.0f;

    q = draw_rect_filled(q, 0, &rect, c);

    dma_buffer = q;
}

void render_draw_text(int x, int y, uint32_t color, const char *text) {
    int cx = x;
    int char_width = 12;
    int char_height = 20;

    while (*text) {
        char c = *text++;

        if (c == ' ') {
            cx += char_width;
            continue;
        }

        render_draw_rect_2d(cx, y, char_width - 2, char_height, color);
        cx += char_width;
    }
}

mesh_t *mesh_create_cube(float size, uint32_t color) {
    mesh_t *mesh = (mesh_t *)malloc(sizeof(mesh_t));
    mesh->tri_count = 12;
    mesh->triangles = (triangle_t *)malloc(sizeof(triangle_t) * mesh->tri_count);
    mesh->base_color = color;

    float hs = size * 0.5f;

    vec3_t corners[8] = {
        {-hs, -hs, -hs}, {hs, -hs, -hs}, {hs, hs, -hs}, {-hs, hs, -hs},
        {-hs, -hs, hs}, {hs, -hs, hs}, {hs, hs, hs}, {-hs, hs, hs}
    };

    int faces[6][4] = {
        {0, 1, 2, 3}, {5, 4, 7, 6}, {4, 0, 3, 7},
        {1, 5, 6, 2}, {3, 2, 6, 7}, {4, 5, 1, 0}
    };

    int ti = 0;
    for (int f = 0; f < 6; f++) {
        mesh->triangles[ti].v[0].pos = corners[faces[f][0]];
        mesh->triangles[ti].v[1].pos = corners[faces[f][1]];
        mesh->triangles[ti].v[2].pos = corners[faces[f][2]];
        mesh->triangles[ti].v[0].color = color;
        mesh->triangles[ti].v[1].color = color;
        mesh->triangles[ti].v[2].color = color;
        ti++;

        mesh->triangles[ti].v[0].pos = corners[faces[f][0]];
        mesh->triangles[ti].v[1].pos = corners[faces[f][2]];
        mesh->triangles[ti].v[2].pos = corners[faces[f][3]];
        mesh->triangles[ti].v[0].color = color;
        mesh->triangles[ti].v[1].color = color;
        mesh->triangles[ti].v[2].color = color;
        ti++;
    }

    return mesh;
}

mesh_t *mesh_create_vehicle(uint32_t color) {
    mesh_t *mesh = (mesh_t *)malloc(sizeof(mesh_t));
    mesh->tri_count = 12;
    mesh->triangles = (triangle_t *)malloc(sizeof(triangle_t) * mesh->tri_count);
    mesh->base_color = color;

    float bw = 0.8f, bh = 0.4f, bl = 1.5f;

    vec3_t body[8] = {
        {-bw/2, 0, -bl/2}, {bw/2, 0, -bl/2},
        {bw/2, bh, -bl/2}, {-bw/2, bh, -bl/2},
        {-bw/2, 0, bl/2}, {bw/2, 0, bl/2},
        {bw/2, bh, bl/2}, {-bw/2, bh, bl/2}
    };

    int faces[6][4] = {
        {0, 1, 2, 3}, {5, 4, 7, 6}, {4, 0, 3, 7},
        {1, 5, 6, 2}, {3, 2, 6, 7}, {4, 5, 1, 0}
    };

    int ti = 0;
    for (int f = 0; f < 6 && ti < 12; f++) {
        mesh->triangles[ti].v[0].pos = body[faces[f][0]];
        mesh->triangles[ti].v[1].pos = body[faces[f][1]];
        mesh->triangles[ti].v[2].pos = body[faces[f][2]];
        mesh->triangles[ti].v[0].color = color;
        mesh->triangles[ti].v[1].color = color;
        mesh->triangles[ti].v[2].color = color;
        ti++;

        mesh->triangles[ti].v[0].pos = body[faces[f][0]];
        mesh->triangles[ti].v[1].pos = body[faces[f][2]];
        mesh->triangles[ti].v[2].pos = body[faces[f][3]];
        mesh->triangles[ti].v[0].color = color;
        mesh->triangles[ti].v[1].color = color;
        mesh->triangles[ti].v[2].color = color;
        ti++;
    }

    mesh->tri_count = ti;
    return mesh;
}

mesh_t *mesh_create_track_segment(float width, float length, uint32_t color) {
    mesh_t *mesh = (mesh_t *)malloc(sizeof(mesh_t));
    mesh->tri_count = 2;
    mesh->triangles = (triangle_t *)malloc(sizeof(triangle_t) * mesh->tri_count);
    mesh->base_color = color;

    float hw = width * 0.5f;

    mesh->triangles[0].v[0].pos = vec3_create(-hw, 0, 0);
    mesh->triangles[0].v[1].pos = vec3_create(hw, 0, 0);
    mesh->triangles[0].v[2].pos = vec3_create(hw, 0, length);
    mesh->triangles[0].v[0].color = color;
    mesh->triangles[0].v[1].color = color;
    mesh->triangles[0].v[2].color = color;

    mesh->triangles[1].v[0].pos = vec3_create(-hw, 0, 0);
    mesh->triangles[1].v[1].pos = vec3_create(hw, 0, length);
    mesh->triangles[1].v[2].pos = vec3_create(-hw, 0, length);
    mesh->triangles[1].v[0].color = color;
    mesh->triangles[1].v[1].color = color;
    mesh->triangles[1].v[2].color = color;

    return mesh;
}

void mesh_destroy(mesh_t *mesh) {
    if (mesh) {
        if (mesh->triangles) free(mesh->triangles);
        free(mesh);
    }
}

#endif /* PLATFORM_PS2 */
