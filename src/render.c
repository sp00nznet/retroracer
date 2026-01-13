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
#include <dc/biosfont.h>
#endif

#define SCREEN_WIDTH 640
#define SCREEN_HEIGHT 480

static camera_t *current_camera = NULL;
static mat4_t view_proj_matrix;

#ifdef DREAMCAST
static pvr_poly_hdr_t poly_hdr;
static int poly_hdr_initialized = 0;

static void init_poly_header(void) {
    if (poly_hdr_initialized) return;

    pvr_poly_cxt_t cxt;
    pvr_poly_cxt_col(&cxt, PVR_LIST_OP_POLY);
    cxt.gen.culling = PVR_CULLING_NONE;  /* Disable culling to see all faces */
    pvr_poly_compile(&poly_hdr, &cxt);
    poly_hdr_initialized = 1;
}
#endif

void render_init(void) {
#ifdef DREAMCAST
    /* Initialize PVR with proper settings */
    pvr_init_params_t params = {
        { PVR_BINSIZE_16, PVR_BINSIZE_0, PVR_BINSIZE_16, PVR_BINSIZE_0, PVR_BINSIZE_0 },
        512 * 1024
    };
    pvr_init(&params);

    /* Set video mode */
    vid_set_mode(DM_640x480, PM_RGB565);

    /* Initialize polygon header */
    init_poly_header();
#endif
}

void render_begin_frame(void) {
#ifdef DREAMCAST
    pvr_wait_ready();
    pvr_scene_begin();
    pvr_list_begin(PVR_LIST_OP_POLY);

    /* Submit the polygon header once at the start */
    pvr_prim(&poly_hdr, sizeof(poly_hdr));
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
    if (cam) {
        view_proj_matrix = mat4_multiply(cam->proj_matrix, cam->view_matrix);
    }
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

/* Transform world position to screen space */
static int transform_to_screen(vec3_t world_pos, float *sx, float *sy, float *sz) {
    if (!current_camera) return 0;

    /* Transform by view-projection matrix */
    vec3_t clip = mat4_transform_vec3(view_proj_matrix, world_pos);

    /* Check if behind camera */
    if (clip.z < 0.1f) {
        return 0;
    }

    /* Perspective divide */
    float inv_z = 1.0f / clip.z;

    /* Convert to screen coordinates */
    *sx = (clip.x * inv_z * 0.5f + 0.5f) * SCREEN_WIDTH;
    *sy = (0.5f - clip.y * inv_z * 0.5f) * SCREEN_HEIGHT;
    *sz = inv_z;  /* PVR uses 1/z for depth */

    return 1;
}

void render_draw_triangle(vertex_t *v0, vertex_t *v1, vertex_t *v2) {
    if (!current_camera) return;

    float sx0, sy0, sz0;
    float sx1, sy1, sz1;
    float sx2, sy2, sz2;

    /* Transform all three vertices */
    if (!transform_to_screen(v0->pos, &sx0, &sy0, &sz0)) return;
    if (!transform_to_screen(v1->pos, &sx1, &sy1, &sz1)) return;
    if (!transform_to_screen(v2->pos, &sx2, &sy2, &sz2)) return;

    /* Basic screen bounds check */
    if ((sx0 < 0 && sx1 < 0 && sx2 < 0) ||
        (sx0 > SCREEN_WIDTH && sx1 > SCREEN_WIDTH && sx2 > SCREEN_WIDTH) ||
        (sy0 < 0 && sy1 < 0 && sy2 < 0) ||
        (sy0 > SCREEN_HEIGHT && sy1 > SCREEN_HEIGHT && sy2 > SCREEN_HEIGHT)) {
        return;
    }

#ifdef DREAMCAST
    pvr_vertex_t vert;

    /* Vertex 0 */
    vert.flags = PVR_CMD_VERTEX;
    vert.x = sx0;
    vert.y = sy0;
    vert.z = sz0;
    vert.u = 0.0f;
    vert.v = 0.0f;
    vert.argb = v0->color;
    vert.oargb = 0;
    pvr_prim(&vert, sizeof(vert));

    /* Vertex 1 */
    vert.flags = PVR_CMD_VERTEX;
    vert.x = sx1;
    vert.y = sy1;
    vert.z = sz1;
    vert.argb = v1->color;
    pvr_prim(&vert, sizeof(vert));

    /* Vertex 2 - end of strip */
    vert.flags = PVR_CMD_VERTEX_EOL;
    vert.x = sx2;
    vert.y = sy2;
    vert.z = sz2;
    vert.argb = v2->color;
    pvr_prim(&vert, sizeof(vert));
#endif
}

void render_draw_mesh(mesh_t *mesh, mat4_t transform) {
    if (!mesh || !current_camera) return;

    for (int i = 0; i < mesh->tri_count; i++) {
        triangle_t *tri = &mesh->triangles[i];

        /* Transform vertices by model matrix */
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

    /* Create quad vertices on XZ plane */
    v0.pos = vec3_create(pos.x - hw, pos.y, pos.z - hh);
    v1.pos = vec3_create(pos.x + hw, pos.y, pos.z - hh);
    v2.pos = vec3_create(pos.x + hw, pos.y, pos.z + hh);
    v3.pos = vec3_create(pos.x - hw, pos.y, pos.z + hh);

    v0.color = v1.color = v2.color = v3.color = color;

    /* Draw as two triangles */
    render_draw_triangle(&v0, &v1, &v2);
    render_draw_triangle(&v0, &v2, &v3);
}

void render_draw_text(int x, int y, uint32_t color, const char *text) {
#ifdef DREAMCAST
    /* End current polygon list to draw text */
    /* Text is drawn directly to framebuffer */
    bfont_set_foreground_color(color);
    bfont_draw_str(vram_s + y * 640 + x, 640, 0, text);
#else
    (void)x; (void)y; (void)color;
    printf("%s\n", text);
#endif
}

/* Create a simple cube mesh */
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

/* Create N64-style low-poly vehicle mesh */
mesh_t *mesh_create_vehicle(uint32_t color) {
    mesh_t *mesh = (mesh_t *)malloc(sizeof(mesh_t));
    mesh->tri_count = 12;  /* Simplified car */
    mesh->triangles = (triangle_t *)malloc(sizeof(triangle_t) * mesh->tri_count);
    mesh->base_color = color;

    /* Simple boxy car */
    float bw = 0.8f;   /* body width */
    float bh = 0.4f;   /* body height */
    float bl = 1.5f;   /* body length */

    vec3_t body[8] = {
        {-bw/2, 0, -bl/2}, {bw/2, 0, -bl/2},
        {bw/2, bh, -bl/2}, {-bw/2, bh, -bl/2},
        {-bw/2, 0, bl/2}, {bw/2, 0, bl/2},
        {bw/2, bh, bl/2}, {-bw/2, bh, bl/2}
    };

    int faces[6][4] = {
        {0, 1, 2, 3}, /* front */
        {5, 4, 7, 6}, /* back */
        {4, 0, 3, 7}, /* left */
        {1, 5, 6, 2}, /* right */
        {3, 2, 6, 7}, /* top */
        {4, 5, 1, 0}  /* bottom */
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

/* Create track segment mesh */
mesh_t *mesh_create_track_segment(float width, float length, uint32_t color) {
    mesh_t *mesh = (mesh_t *)malloc(sizeof(mesh_t));
    mesh->tri_count = 2;
    mesh->triangles = (triangle_t *)malloc(sizeof(triangle_t) * mesh->tri_count);
    mesh->base_color = color;

    float hw = width * 0.5f;

    /* Two triangles forming a quad on the XZ plane */
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
