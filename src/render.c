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
#include <dc/matrix.h>
#endif

#define SCREEN_WIDTH 640.0f
#define SCREEN_HEIGHT 480.0f
#define NEAR_CLIP 1.0f

static camera_t *current_camera = NULL;

#ifdef DREAMCAST
static pvr_poly_hdr_t poly_hdr;
static pvr_poly_hdr_t poly_hdr_tr;  /* Transparent list header for HUD */
static int poly_hdr_initialized = 0;
static int in_hud_mode = 0;  /* Track if we're rendering HUD */

static void init_poly_header(void) {
    if (poly_hdr_initialized) return;

    pvr_poly_cxt_t cxt;

    /* Opaque polygon header for 3D geometry */
    pvr_poly_cxt_col(&cxt, PVR_LIST_OP_POLY);
    cxt.gen.culling = PVR_CULLING_NONE;
    pvr_poly_compile(&poly_hdr, &cxt);

    /* Transparent polygon header for HUD elements */
    pvr_poly_cxt_col(&cxt, PVR_LIST_TR_POLY);
    cxt.gen.culling = PVR_CULLING_NONE;
    cxt.blend.src = PVR_BLEND_SRCALPHA;
    cxt.blend.dst = PVR_BLEND_INVSRCALPHA;
    pvr_poly_compile(&poly_hdr_tr, &cxt);

    poly_hdr_initialized = 1;
}
#endif

void render_init(void) {
#ifdef DREAMCAST
    pvr_init_params_t params = {
        { PVR_BINSIZE_16, PVR_BINSIZE_0, PVR_BINSIZE_16, PVR_BINSIZE_0, PVR_BINSIZE_0 },
        512 * 1024
    };
    pvr_init(&params);
    vid_set_mode(DM_640x480, PM_RGB565);
    init_poly_header();
#endif
}

void render_begin_frame(void) {
#ifdef DREAMCAST
    pvr_wait_ready();
    pvr_scene_begin();
    pvr_list_begin(PVR_LIST_OP_POLY);
    pvr_prim(&poly_hdr, sizeof(poly_hdr));
#endif
}

void render_end_frame(void) {
#ifdef DREAMCAST
    pvr_list_finish();
    /* Don't finish scene yet - HUD rendering may follow */
#endif
}

/* Begin HUD rendering mode - switches to transparent polygon list */
void render_begin_hud(void) {
#ifdef DREAMCAST
    pvr_list_begin(PVR_LIST_TR_POLY);
    pvr_prim(&poly_hdr_tr, sizeof(poly_hdr_tr));
    in_hud_mode = 1;
#endif
}

/* End HUD rendering and finish the scene */
void render_end_hud(void) {
#ifdef DREAMCAST
    pvr_list_finish();
    pvr_scene_finish();
    in_hud_mode = 0;
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

/* Transform point to view space (camera space) */
static vec3_t transform_to_view(vec3_t world_pos) {
    if (!current_camera) return world_pos;

    mat4_t mv = current_camera->view_matrix;

    /* Transform by view matrix only (no perspective yet) */
    vec3_t view;
    view.x = mv.m[0] * world_pos.x + mv.m[4] * world_pos.y + mv.m[8] * world_pos.z + mv.m[12];
    view.y = mv.m[1] * world_pos.x + mv.m[5] * world_pos.y + mv.m[9] * world_pos.z + mv.m[13];
    view.z = mv.m[2] * world_pos.x + mv.m[6] * world_pos.y + mv.m[10] * world_pos.z + mv.m[14];

    return view;
}

/* Project from view space to screen space */
static int project_to_screen(vec3_t view_pos, float *sx, float *sy, float *sz) {
    /* Check near plane - reject if behind camera */
    if (view_pos.z > -NEAR_CLIP) {
        return 0;
    }

    /* Get projection parameters */
    float fov_rad = deg_to_rad(current_camera->fov);
    float tan_half_fov = tanf(fov_rad / 2.0f);
    float aspect = current_camera->aspect;

    /* Perspective projection */
    float inv_z = -1.0f / view_pos.z;
    float proj_x = view_pos.x * inv_z / (aspect * tan_half_fov);
    float proj_y = view_pos.y * inv_z / tan_half_fov;

    /* Convert from NDC [-1,1] to screen coordinates */
    *sx = (proj_x + 1.0f) * 0.5f * SCREEN_WIDTH;
    *sy = (1.0f - proj_y) * 0.5f * SCREEN_HEIGHT;
    *sz = inv_z;  /* PVR uses 1/z for depth */

    /* Clamp depth to valid PVR range */
    /* Lower minimum allows better depth separation for far geometry */
    if (*sz < 0.0001f) *sz = 0.0001f;
    if (*sz > 1.0f) *sz = 1.0f;

    return 1;
}

/* Clip and interpolate edge against near plane */
static vec3_t clip_edge(vec3_t v_in, vec3_t v_out, uint32_t c_in, uint32_t c_out, uint32_t *c_new) {
    float d_in = -v_in.z - NEAR_CLIP;
    float d_out = -v_out.z - NEAR_CLIP;
    float t = d_in / (d_in - d_out);

    vec3_t result;
    result.x = v_in.x + t * (v_out.x - v_in.x);
    result.y = v_in.y + t * (v_out.y - v_in.y);
    result.z = v_in.z + t * (v_out.z - v_in.z);

    /* Interpolate color (simple approach) */
    *c_new = c_in;  /* Just use the inside color for simplicity */
    (void)c_out;

    return result;
}

/* Submit a triangle to PVR */
static void submit_triangle(float x0, float y0, float z0, uint32_t c0,
                            float x1, float y1, float z1, uint32_t c1,
                            float x2, float y2, float z2, uint32_t c2) {
#ifdef DREAMCAST
    pvr_vertex_t vert;

    vert.flags = PVR_CMD_VERTEX;
    vert.x = x0; vert.y = y0; vert.z = z0;
    vert.u = 0; vert.v = 0;
    vert.argb = c0;
    vert.oargb = 0;
    pvr_prim(&vert, sizeof(vert));

    vert.x = x1; vert.y = y1; vert.z = z1;
    vert.argb = c1;
    pvr_prim(&vert, sizeof(vert));

    vert.flags = PVR_CMD_VERTEX_EOL;
    vert.x = x2; vert.y = y2; vert.z = z2;
    vert.argb = c2;
    pvr_prim(&vert, sizeof(vert));
#else
    (void)x0; (void)y0; (void)z0; (void)c0;
    (void)x1; (void)y1; (void)z1; (void)c1;
    (void)x2; (void)y2; (void)z2; (void)c2;
#endif
}

void render_draw_triangle(vertex_t *v0, vertex_t *v1, vertex_t *v2) {
    if (!current_camera) return;

    /* Transform to view space */
    vec3_t vp0 = transform_to_view(v0->pos);
    vec3_t vp1 = transform_to_view(v1->pos);
    vec3_t vp2 = transform_to_view(v2->pos);

    /* Check which vertices are in front of near plane */
    /* In view space, camera looks down -Z, so z < -NEAR_CLIP is visible */
    int in0 = (vp0.z < -NEAR_CLIP) ? 1 : 0;
    int in1 = (vp1.z < -NEAR_CLIP) ? 1 : 0;
    int in2 = (vp2.z < -NEAR_CLIP) ? 1 : 0;
    int num_in = in0 + in1 + in2;

    /* All vertices behind camera - skip entirely */
    if (num_in == 0) return;

    float sx0, sy0, sz0;
    float sx1, sy1, sz1;
    float sx2, sy2, sz2;

    if (num_in == 3) {
        /* All vertices visible - project and draw */
        if (!project_to_screen(vp0, &sx0, &sy0, &sz0)) return;
        if (!project_to_screen(vp1, &sx1, &sy1, &sz1)) return;
        if (!project_to_screen(vp2, &sx2, &sy2, &sz2)) return;

        submit_triangle(sx0, sy0, sz0, v0->color,
                        sx1, sy1, sz1, v1->color,
                        sx2, sy2, sz2, v2->color);
    }
    else if (num_in == 1) {
        /* One vertex visible - clip to form one triangle */
        vec3_t vp_in, vp_out1, vp_out2;
        uint32_t c_in, c_out1, c_out2;

        if (in0) {
            vp_in = vp0; c_in = v0->color;
            vp_out1 = vp1; c_out1 = v1->color;
            vp_out2 = vp2; c_out2 = v2->color;
        } else if (in1) {
            vp_in = vp1; c_in = v1->color;
            vp_out1 = vp0; c_out1 = v0->color;
            vp_out2 = vp2; c_out2 = v2->color;
        } else {
            vp_in = vp2; c_in = v2->color;
            vp_out1 = vp0; c_out1 = v0->color;
            vp_out2 = vp1; c_out2 = v1->color;
        }

        uint32_t c_clip1, c_clip2;
        vec3_t vp_clip1 = clip_edge(vp_in, vp_out1, c_in, c_out1, &c_clip1);
        vec3_t vp_clip2 = clip_edge(vp_in, vp_out2, c_in, c_out2, &c_clip2);

        float sx_in, sy_in, sz_in;
        float sx_c1, sy_c1, sz_c1;
        float sx_c2, sy_c2, sz_c2;

        if (!project_to_screen(vp_in, &sx_in, &sy_in, &sz_in)) return;
        if (!project_to_screen(vp_clip1, &sx_c1, &sy_c1, &sz_c1)) return;
        if (!project_to_screen(vp_clip2, &sx_c2, &sy_c2, &sz_c2)) return;

        submit_triangle(sx_in, sy_in, sz_in, c_in,
                        sx_c1, sy_c1, sz_c1, c_clip1,
                        sx_c2, sy_c2, sz_c2, c_clip2);
    }
    else if (num_in == 2) {
        /* Two vertices visible - clip to form a quad (two triangles) */
        vec3_t vp_in1, vp_in2, vp_out;
        uint32_t c_in1, c_in2, c_out;

        if (!in0) {
            vp_out = vp0; c_out = v0->color;
            vp_in1 = vp1; c_in1 = v1->color;
            vp_in2 = vp2; c_in2 = v2->color;
        } else if (!in1) {
            vp_out = vp1; c_out = v1->color;
            vp_in1 = vp0; c_in1 = v0->color;
            vp_in2 = vp2; c_in2 = v2->color;
        } else {
            vp_out = vp2; c_out = v2->color;
            vp_in1 = vp0; c_in1 = v0->color;
            vp_in2 = vp1; c_in2 = v1->color;
        }

        uint32_t c_clip1, c_clip2;
        vec3_t vp_clip1 = clip_edge(vp_in1, vp_out, c_in1, c_out, &c_clip1);
        vec3_t vp_clip2 = clip_edge(vp_in2, vp_out, c_in2, c_out, &c_clip2);

        float sx_i1, sy_i1, sz_i1;
        float sx_i2, sy_i2, sz_i2;
        float sx_c1, sy_c1, sz_c1;
        float sx_c2, sy_c2, sz_c2;

        if (!project_to_screen(vp_in1, &sx_i1, &sy_i1, &sz_i1)) return;
        if (!project_to_screen(vp_in2, &sx_i2, &sy_i2, &sz_i2)) return;
        if (!project_to_screen(vp_clip1, &sx_c1, &sy_c1, &sz_c1)) return;
        if (!project_to_screen(vp_clip2, &sx_c2, &sy_c2, &sz_c2)) return;

        /* First triangle */
        submit_triangle(sx_i1, sy_i1, sz_i1, c_in1,
                        sx_c1, sy_c1, sz_c1, c_clip1,
                        sx_i2, sy_i2, sz_i2, c_in2);

        /* Second triangle */
        submit_triangle(sx_i2, sy_i2, sz_i2, c_in2,
                        sx_c1, sy_c1, sz_c1, c_clip1,
                        sx_c2, sy_c2, sz_c2, c_clip2);
    }
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

/* Call this before drawing HUD text - kept for compatibility */
void render_wait_vram_ready(void) {
    /* No-op - HUD now uses PVR transparent list */
}

/* Draw a full-screen sky background at minimum depth (behind all 3D geometry) */
void render_draw_sky_background(uint32_t color) {
#ifdef DREAMCAST
    pvr_vertex_t vert;

    /* Use a small depth value to ensure sky is always behind 3D geometry */
    /* PVR uses 1/z where higher values are closer, so small values are far away */
    /* Must be less than track's minimum (0.001) but large enough for PVR precision */
    float z = 0.0005f;

    /* Full screen quad covering the entire viewport */
    vert.flags = PVR_CMD_VERTEX;
    vert.x = 0.0f;
    vert.y = 0.0f;
    vert.z = z;
    vert.u = 0;
    vert.v = 0;
    vert.argb = color;
    vert.oargb = 0;
    pvr_prim(&vert, sizeof(vert));

    vert.x = 640.0f;
    vert.y = 0.0f;
    pvr_prim(&vert, sizeof(vert));

    vert.x = 0.0f;
    vert.y = 480.0f;
    pvr_prim(&vert, sizeof(vert));

    vert.flags = PVR_CMD_VERTEX_EOL;
    vert.x = 640.0f;
    vert.y = 480.0f;
    pvr_prim(&vert, sizeof(vert));
#else
    (void)color;
#endif
}

/* Draw a 2D rectangle on screen (in HUD mode) */
void render_draw_rect_2d(int x, int y, int w, int h, uint32_t color) {
#ifdef DREAMCAST
    pvr_vertex_t vert;

    float fx = (float)x;
    float fy = (float)y;
    float fw = (float)w;
    float fh = (float)h;
    float z = 1.0f;  /* Front of screen */

    vert.flags = PVR_CMD_VERTEX;
    vert.x = fx;
    vert.y = fy;
    vert.z = z;
    vert.u = 0;
    vert.v = 0;
    vert.argb = color;
    vert.oargb = 0;
    pvr_prim(&vert, sizeof(vert));

    vert.x = fx + fw;
    vert.y = fy;
    pvr_prim(&vert, sizeof(vert));

    vert.x = fx;
    vert.y = fy + fh;
    pvr_prim(&vert, sizeof(vert));

    vert.flags = PVR_CMD_VERTEX_EOL;
    vert.x = fx + fw;
    vert.y = fy + fh;
    pvr_prim(&vert, sizeof(vert));
#else
    (void)x; (void)y; (void)w; (void)h; (void)color;
#endif
}

/* Draw text using simple rectangle-based characters (PVR compatible) */
/* This draws blocky text that works with PVR rendering */
void render_draw_text(int x, int y, uint32_t color, const char *text) {
#ifdef DREAMCAST
    int char_width = 12;
    int char_height = 24;
    int cx = x;

    while (*text) {
        char c = *text++;

        if (c == ' ') {
            cx += char_width;
            continue;
        }

        /* Draw a simple filled rectangle for each character */
        /* This gives us visible text until we implement proper font textures */

        /* Draw character background block */
        render_draw_rect_2d(cx, y, char_width - 2, char_height, color);

        /* For numbers and some letters, draw a pattern to make them recognizable */
        if (c >= '0' && c <= '9') {
            /* Draw number pattern - dark inset */
            uint32_t dark = 0xFF000000 | ((color & 0x00FEFEFE) >> 1);
            int n = c - '0';

            /* Simple patterns for digits */
            if (n == 0) {
                render_draw_rect_2d(cx + 3, y + 3, char_width - 8, char_height - 6, dark);
            } else if (n == 1) {
                render_draw_rect_2d(cx + 1, y + 2, 4, char_height - 4, dark);
            } else {
                /* Other numbers - just show solid */
            }
        }

        cx += char_width;
    }
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

/* Create track segment mesh */
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
