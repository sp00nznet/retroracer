/*
 * RetroRacer - 3D Math Library
 * Vectors, Matrices, and Transformations for Dreamcast
 */

#ifndef MATH3D_H
#define MATH3D_H

#include <math.h>

/* 3D Vector */
typedef struct {
    float x, y, z;
} vec3_t;

/* 4D Vector (for homogeneous coordinates) */
typedef struct {
    float x, y, z, w;
} vec4_t;

/* 4x4 Matrix (column-major for OpenGL compatibility) */
typedef struct {
    float m[16];
} mat4_t;

/* Vector operations */
vec3_t vec3_create(float x, float y, float z);
vec3_t vec3_add(vec3_t a, vec3_t b);
vec3_t vec3_sub(vec3_t a, vec3_t b);
vec3_t vec3_scale(vec3_t v, float s);
vec3_t vec3_normalize(vec3_t v);
vec3_t vec3_cross(vec3_t a, vec3_t b);
float vec3_dot(vec3_t a, vec3_t b);
float vec3_length(vec3_t v);
float vec3_distance(vec3_t a, vec3_t b);
vec3_t vec3_lerp(vec3_t a, vec3_t b, float t);

/* Matrix operations */
mat4_t mat4_identity(void);
mat4_t mat4_multiply(mat4_t a, mat4_t b);
mat4_t mat4_translate(float x, float y, float z);
mat4_t mat4_rotate_x(float angle);
mat4_t mat4_rotate_y(float angle);
mat4_t mat4_rotate_z(float angle);
mat4_t mat4_scale(float x, float y, float z);
mat4_t mat4_perspective(float fov, float aspect, float near, float far);
mat4_t mat4_look_at(vec3_t eye, vec3_t target, vec3_t up);
vec3_t mat4_transform_vec3(mat4_t m, vec3_t v);

/* Utility */
float deg_to_rad(float degrees);
float rad_to_deg(float radians);
float clamp(float value, float min, float max);
float lerp(float a, float b, float t);

#endif /* MATH3D_H */
