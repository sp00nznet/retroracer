/*
 * RetroRacer - Rendering System Implementation
 * PowerVR Graphics for Dreamcast
 */

#include "render.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef DREAMCAST
#include <kos.h>
#include <dc/pvr.h>
#include <dc/video.h>
#else
/* Stubs for non-Dreamcast builds */
#endif

#define SCREEN_WIDTH 640
#define SCREEN_HEIGHT 480

static camera_t *current_camera = NULL;

void render_init(void) {
#ifdef DREAMCAST
    /* Initialize PVR */
    pvr_init_params_t params = {
        { PVR_BINSIZE_16, PVR_BINSIZE_0, PVR_BINSIZE_16, PVR_BINSIZE_0, PVR_BINSIZE_0 },
        512 * 1024
    };
    pvr_init(&params);

    /* Set video mode */
    vid_set_mode(DM_640x480, PM_RGB565);
#endif
}

void render_begin_frame(void) {
#ifdef DREAMCAST
    pvr_wait_ready();
    pvr_scene_begin();
    pvr_list_begin(PVR_LIST_OP_POLY);
#endif
}

void render_end_frame(void) {
#ifdef DREAMCAST
    pvr_list_finish();
    pvr_scene_finish();
#endif
}

void render_clear(uint32_t color) {
#ifdef DREAMCAST
    pvr_set_bg_color(
        ((color >> 16) & 0xFF) / 255.0f,
        ((color >> 8) & 0xFF) / 255.0f,
        (color & 0xFF) / 255.0f
    );
#endif
}

void render_set_camera(camera_t *cam) {
    current_camera = cam;
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

/* Transform vertex to screen space */
static int transform_vertex(vertex_t *v, vec3_t *screen_pos, mat4_t mvp) {
    vec3_t transformed = mat4_transform_vec3(mvp, v->pos);

    /* Clip against near plane */
    if (transformed.z < 0.1f) {
        return 0;
    }

    /* Perspective divide and screen transform */
    float inv_z = 1.0f / transformed.z;
    screen_pos->x = (transformed.x * inv_z + 1.0f) * 0.5f * SCREEN_WIDTH;
    screen_pos->y = (1.0f - transformed.y * inv_z) * 0.5f * SCREEN_HEIGHT;
    screen_pos->z = inv_z;

    return 1;
}

void render_draw_triangle(vertex_t *v0, vertex_t *v1, vertex_t *v2) {
    if (!current_camera) return;

    mat4_t mvp = mat4_multiply(current_camera->proj_matrix, current_camera->view_matrix);

    vec3_t s0, s1, s2;
    if (!transform_vertex(v0, &s0, mvp)) return;
    if (!transform_vertex(v1, &s1, mvp)) return;
    if (!transform_vertex(v2, &s2, mvp)) return;

#ifdef DREAMCAST
    pvr_vertex_t vert;
    pvr_poly_cxt_t cxt;
    pvr_poly_hdr_t hdr;

    pvr_poly_cxt_col(&cxt, PVR_LIST_OP_POLY);
    pvr_poly_compile(&hdr, &cxt);
    pvr_prim(&hdr, sizeof(hdr));

    vert.flags = PVR_CMD_VERTEX;
    vert.x = s0.x;
    vert.y = s0.y;
    vert.z = s0.z;
    vert.u = v0->u;
    vert.v = v0->v;
    vert.argb = v0->color;
    vert.oargb = 0;
    pvr_prim(&vert, sizeof(vert));

    vert.x = s1.x;
    vert.y = s1.y;
    vert.z = s1.z;
    vert.u = v1->u;
    vert.v = v1->v;
    vert.argb = v1->color;
    pvr_prim(&vert, sizeof(vert));

    vert.flags = PVR_CMD_VERTEX_EOL;
    vert.x = s2.x;
    vert.y = s2.y;
    vert.z = s2.z;
    vert.u = v2->u;
    vert.v = v2->v;
    vert.argb = v2->color;
    pvr_prim(&vert, sizeof(vert));
#endif
}

void render_draw_mesh(mesh_t *mesh, mat4_t transform) {
    if (!mesh || !current_camera) return;

    mat4_t model_view = mat4_multiply(current_camera->view_matrix, transform);
    mat4_t mvp = mat4_multiply(current_camera->proj_matrix, model_view);

    for (int i = 0; i < mesh->tri_count; i++) {
        triangle_t *tri = &mesh->triangles[i];

        /* Transform vertices */
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
    v0.u = 0; v0.v = 0;
    v1.u = 1; v1.v = 0;
    v2.u = 1; v2.v = 1;
    v3.u = 0; v3.v = 1;

    render_draw_triangle(&v0, &v1, &v2);
    render_draw_triangle(&v0, &v2, &v3);
}

void render_draw_text(int x, int y, uint32_t color, const char *text) {
#ifdef DREAMCAST
    /* Use BIOS font for simple text rendering */
    bfont_set_foreground_color(color);
    bfont_draw_str(vram_s + y * 640 + x, 640, 0, text);
#else
    /* Printf for non-DC builds */
    (void)x; (void)y; (void)color;
    printf("%s\n", text);
#endif
}

/* Create a simple cube mesh */
mesh_t *mesh_create_cube(float size, uint32_t color) {
    mesh_t *mesh = (mesh_t *)malloc(sizeof(mesh_t));
    mesh->tri_count = 12;  /* 6 faces * 2 triangles */
    mesh->triangles = (triangle_t *)malloc(sizeof(triangle_t) * mesh->tri_count);
    mesh->base_color = color;

    float hs = size * 0.5f;

    /* Cube vertices */
    vec3_t corners[8] = {
        {-hs, -hs, -hs}, {hs, -hs, -hs}, {hs, hs, -hs}, {-hs, hs, -hs},
        {-hs, -hs, hs}, {hs, -hs, hs}, {hs, hs, hs}, {-hs, hs, hs}
    };

    /* Face indices */
    int faces[6][4] = {
        {0, 1, 2, 3}, /* Front */
        {5, 4, 7, 6}, /* Back */
        {4, 0, 3, 7}, /* Left */
        {1, 5, 6, 2}, /* Right */
        {3, 2, 6, 7}, /* Top */
        {4, 5, 1, 0}  /* Bottom */
    };

    int ti = 0;
    for (int f = 0; f < 6; f++) {
        /* First triangle */
        mesh->triangles[ti].v[0].pos = corners[faces[f][0]];
        mesh->triangles[ti].v[1].pos = corners[faces[f][1]];
        mesh->triangles[ti].v[2].pos = corners[faces[f][2]];
        mesh->triangles[ti].v[0].color = color;
        mesh->triangles[ti].v[1].color = color;
        mesh->triangles[ti].v[2].color = color;
        ti++;

        /* Second triangle */
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

/* Create N64-style low-poly vehicle mesh */
mesh_t *mesh_create_vehicle(uint32_t color) {
    mesh_t *mesh = (mesh_t *)malloc(sizeof(mesh_t));
    mesh->tri_count = 20;  /* Simple car shape */
    mesh->triangles = (triangle_t *)malloc(sizeof(triangle_t) * mesh->tri_count);
    mesh->base_color = color;

    /* Car body - boxy N64 style */
    float body_w = 1.0f;
    float body_h = 0.5f;
    float body_l = 2.0f;

    /* Cabin */
    float cab_w = 0.8f;
    float cab_h = 0.4f;
    float cab_l = 1.0f;

    /* Main body vertices */
    vec3_t body[8] = {
        {-body_w/2, 0, -body_l/2}, {body_w/2, 0, -body_l/2},
        {body_w/2, body_h, -body_l/2}, {-body_w/2, body_h, -body_l/2},
        {-body_w/2, 0, body_l/2}, {body_w/2, 0, body_l/2},
        {body_w/2, body_h, body_l/2}, {-body_w/2, body_h, body_l/2}
    };

    /* Cabin vertices */
    vec3_t cab[8] = {
        {-cab_w/2, body_h, -cab_l/2}, {cab_w/2, body_h, -cab_l/2},
        {cab_w/2, body_h + cab_h, -cab_l/2 + 0.2f}, {-cab_w/2, body_h + cab_h, -cab_l/2 + 0.2f},
        {-cab_w/2, body_h, cab_l/2 - 0.3f}, {cab_w/2, body_h, cab_l/2 - 0.3f},
        {cab_w/2, body_h + cab_h, cab_l/2 - 0.5f}, {-cab_w/2, body_h + cab_h, cab_l/2 - 0.5f}
    };

    uint32_t body_color = color;
    uint32_t cab_color = PACK_COLOR(255, 100, 100, 100);

    int ti = 0;

    /* Body faces */
    int body_faces[6][4] = {
        {0, 1, 2, 3}, {5, 4, 7, 6}, {4, 0, 3, 7},
        {1, 5, 6, 2}, {3, 2, 6, 7}, {4, 5, 1, 0}
    };

    for (int f = 0; f < 6; f++) {
        mesh->triangles[ti].v[0].pos = body[body_faces[f][0]];
        mesh->triangles[ti].v[1].pos = body[body_faces[f][1]];
        mesh->triangles[ti].v[2].pos = body[body_faces[f][2]];
        mesh->triangles[ti].v[0].color = body_color;
        mesh->triangles[ti].v[1].color = body_color;
        mesh->triangles[ti].v[2].color = body_color;
        ti++;

        mesh->triangles[ti].v[0].pos = body[body_faces[f][0]];
        mesh->triangles[ti].v[1].pos = body[body_faces[f][2]];
        mesh->triangles[ti].v[2].pos = body[body_faces[f][3]];
        mesh->triangles[ti].v[0].color = body_color;
        mesh->triangles[ti].v[1].color = body_color;
        mesh->triangles[ti].v[2].color = body_color;
        ti++;
    }

    /* Cabin top and front */
    mesh->triangles[ti].v[0].pos = cab[3];
    mesh->triangles[ti].v[1].pos = cab[2];
    mesh->triangles[ti].v[2].pos = cab[6];
    mesh->triangles[ti].v[0].color = cab_color;
    mesh->triangles[ti].v[1].color = cab_color;
    mesh->triangles[ti].v[2].color = cab_color;
    ti++;

    mesh->triangles[ti].v[0].pos = cab[3];
    mesh->triangles[ti].v[1].pos = cab[6];
    mesh->triangles[ti].v[2].pos = cab[7];
    mesh->triangles[ti].v[0].color = cab_color;
    mesh->triangles[ti].v[1].color = cab_color;
    mesh->triangles[ti].v[2].color = cab_color;
    ti++;

    /* Front windshield */
    mesh->triangles[ti].v[0].pos = cab[0];
    mesh->triangles[ti].v[1].pos = cab[1];
    mesh->triangles[ti].v[2].pos = cab[2];
    mesh->triangles[ti].v[0].color = PACK_COLOR(200, 100, 150, 200);
    mesh->triangles[ti].v[1].color = PACK_COLOR(200, 100, 150, 200);
    mesh->triangles[ti].v[2].color = PACK_COLOR(200, 100, 150, 200);
    ti++;

    mesh->triangles[ti].v[0].pos = cab[0];
    mesh->triangles[ti].v[1].pos = cab[2];
    mesh->triangles[ti].v[2].pos = cab[3];
    mesh->triangles[ti].v[0].color = PACK_COLOR(200, 100, 150, 200);
    mesh->triangles[ti].v[1].color = PACK_COLOR(200, 100, 150, 200);
    mesh->triangles[ti].v[2].color = PACK_COLOR(200, 100, 150, 200);
    ti++;

    mesh->tri_count = ti;
    return mesh;
}

/* Create track segment mesh */
mesh_t *mesh_create_track_segment(float width, float length, uint32_t color) {
    mesh_t *mesh = (mesh_t *)malloc(sizeof(mesh_t));
    mesh->tri_count = 2;
    mesh->triangles = (triangle_t *)malloc(sizeof(triangle_t) * mesh->tri_count);
    mesh->base_color = color;

    float hw = width * 0.5f;

    /* Simple quad for track surface */
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
        if (mesh->triangles) {
            free(mesh->triangles);
        }
        free(mesh);
    }
}
