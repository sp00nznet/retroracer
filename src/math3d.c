/*
 * RetroRacer - 3D Math Library Implementation
 */

#include "math3d.h"
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

/* Vector operations */

vec3_t vec3_create(float x, float y, float z) {
    vec3_t v = {x, y, z};
    return v;
}

vec3_t vec3_add(vec3_t a, vec3_t b) {
    return vec3_create(a.x + b.x, a.y + b.y, a.z + b.z);
}

vec3_t vec3_sub(vec3_t a, vec3_t b) {
    return vec3_create(a.x - b.x, a.y - b.y, a.z - b.z);
}

vec3_t vec3_scale(vec3_t v, float s) {
    return vec3_create(v.x * s, v.y * s, v.z * s);
}

float vec3_length(vec3_t v) {
    return sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
}

vec3_t vec3_normalize(vec3_t v) {
    float len = vec3_length(v);
    if (len > 0.0001f) {
        return vec3_scale(v, 1.0f / len);
    }
    return vec3_create(0, 0, 0);
}

vec3_t vec3_cross(vec3_t a, vec3_t b) {
    return vec3_create(
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    );
}

float vec3_dot(vec3_t a, vec3_t b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

float vec3_distance(vec3_t a, vec3_t b) {
    return vec3_length(vec3_sub(a, b));
}

vec3_t vec3_lerp(vec3_t a, vec3_t b, float t) {
    return vec3_create(
        a.x + (b.x - a.x) * t,
        a.y + (b.y - a.y) * t,
        a.z + (b.z - a.z) * t
    );
}

/* Matrix operations */

mat4_t mat4_identity(void) {
    mat4_t m;
    memset(m.m, 0, sizeof(m.m));
    m.m[0] = m.m[5] = m.m[10] = m.m[15] = 1.0f;
    return m;
}

mat4_t mat4_multiply(mat4_t a, mat4_t b) {
    mat4_t result;
    for (int col = 0; col < 4; col++) {
        for (int row = 0; row < 4; row++) {
            result.m[col * 4 + row] =
                a.m[0 * 4 + row] * b.m[col * 4 + 0] +
                a.m[1 * 4 + row] * b.m[col * 4 + 1] +
                a.m[2 * 4 + row] * b.m[col * 4 + 2] +
                a.m[3 * 4 + row] * b.m[col * 4 + 3];
        }
    }
    return result;
}

mat4_t mat4_translate(float x, float y, float z) {
    mat4_t m = mat4_identity();
    m.m[12] = x;
    m.m[13] = y;
    m.m[14] = z;
    return m;
}

mat4_t mat4_rotate_x(float angle) {
    mat4_t m = mat4_identity();
    float c = cosf(angle);
    float s = sinf(angle);
    m.m[5] = c;
    m.m[6] = s;
    m.m[9] = -s;
    m.m[10] = c;
    return m;
}

mat4_t mat4_rotate_y(float angle) {
    mat4_t m = mat4_identity();
    float c = cosf(angle);
    float s = sinf(angle);
    m.m[0] = c;
    m.m[2] = -s;
    m.m[8] = s;
    m.m[10] = c;
    return m;
}

mat4_t mat4_rotate_z(float angle) {
    mat4_t m = mat4_identity();
    float c = cosf(angle);
    float s = sinf(angle);
    m.m[0] = c;
    m.m[1] = s;
    m.m[4] = -s;
    m.m[5] = c;
    return m;
}

mat4_t mat4_scale(float x, float y, float z) {
    mat4_t m = mat4_identity();
    m.m[0] = x;
    m.m[5] = y;
    m.m[10] = z;
    return m;
}

mat4_t mat4_perspective(float fov, float aspect, float near, float far) {
    mat4_t m;
    memset(m.m, 0, sizeof(m.m));

    float tan_half_fov = tanf(fov / 2.0f);

    m.m[0] = 1.0f / (aspect * tan_half_fov);
    m.m[5] = 1.0f / tan_half_fov;
    m.m[10] = -(far + near) / (far - near);
    m.m[11] = -1.0f;
    m.m[14] = -(2.0f * far * near) / (far - near);

    return m;
}

mat4_t mat4_look_at(vec3_t eye, vec3_t target, vec3_t up) {
    vec3_t f = vec3_normalize(vec3_sub(target, eye));
    vec3_t r = vec3_normalize(vec3_cross(f, up));
    vec3_t u = vec3_cross(r, f);

    mat4_t m = mat4_identity();

    m.m[0] = r.x;
    m.m[4] = r.y;
    m.m[8] = r.z;

    m.m[1] = u.x;
    m.m[5] = u.y;
    m.m[9] = u.z;

    m.m[2] = -f.x;
    m.m[6] = -f.y;
    m.m[10] = -f.z;

    m.m[12] = -vec3_dot(r, eye);
    m.m[13] = -vec3_dot(u, eye);
    m.m[14] = vec3_dot(f, eye);

    return m;
}

vec3_t mat4_transform_vec3(mat4_t m, vec3_t v) {
    float w = m.m[3] * v.x + m.m[7] * v.y + m.m[11] * v.z + m.m[15];
    if (w == 0) w = 1.0f;

    return vec3_create(
        (m.m[0] * v.x + m.m[4] * v.y + m.m[8] * v.z + m.m[12]) / w,
        (m.m[1] * v.x + m.m[5] * v.y + m.m[9] * v.z + m.m[13]) / w,
        (m.m[2] * v.x + m.m[6] * v.y + m.m[10] * v.z + m.m[14]) / w
    );
}

/* Utility functions */

float deg_to_rad(float degrees) {
    return degrees * M_PI / 180.0f;
}

float rad_to_deg(float radians) {
    return radians * 180.0f / M_PI;
}

float clamp(float value, float min, float max) {
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

float lerp(float a, float b, float t) {
    return a + (b - a) * t;
}
