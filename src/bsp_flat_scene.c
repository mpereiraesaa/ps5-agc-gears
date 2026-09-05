#include "bsp_flat_scene.h"

#include <math.h>
#include <string.h>

typedef struct Matrix4 { float v[16]; } Matrix4;
typedef struct Vec3 { float x, y, z; } Vec3;

static uint32_t bits(float value)
{
    uint32_t result;
    memcpy(&result, &value, sizeof(result));
    return result;
}

static float dot(Vec3 a, Vec3 b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

static Vec3 cross(Vec3 a, Vec3 b)
{
    const Vec3 result = {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x,
    };
    return result;
}

static int normalize(Vec3 *value)
{
    const float length = sqrtf(dot(*value, *value));
    if (!(length > 0.000001f) || !__builtin_isfinite(length))
        return -1;
    value->x /= length; value->y /= length; value->z /= length;
    return 0;
}

static Matrix4 multiply(Matrix4 a, Matrix4 b)
{
    Matrix4 result = {{0}};
    for (unsigned column = 0; column < 4u; ++column)
        for (unsigned row = 0; row < 4u; ++row)
            for (unsigned k = 0; k < 4u; ++k)
                result.v[column * 4u + row] +=
                    a.v[k * 4u + row] * b.v[column * 4u + k];
    return result;
}

static Matrix4 perspective(float vertical_fov, float aspect,
                           float near_z, float far_z)
{
    const float scale = 1.0f / tanf(vertical_fov * 0.5f);
    const Matrix4 result = {{
        scale / aspect, 0, 0, 0, 0, scale, 0, 0,
        0, 0, far_z / (near_z - far_z), -1,
        0, 0, (near_z * far_z) / (near_z - far_z), 0,
    }};
    return result;
}

static int view_matrix(const float position[3], const float forward_in[3],
                       Matrix4 *result)
{
    Vec3 forward = {forward_in[0], forward_in[1], forward_in[2]};
    if (normalize(&forward) != 0)
        return -1;
    Vec3 up = {0, 1, 0};
    if (fabsf(dot(forward, up)) > 0.999f)
        up = (Vec3){0, 0, 1};
    Vec3 side = cross(forward, up);
    if (normalize(&side) != 0)
        return -1;
    const Vec3 camera_up = cross(side, forward);
    const Vec3 eye = {position[0], position[1], position[2]};
    *result = (Matrix4){{
        side.x, camera_up.x, -forward.x, 0,
        side.y, camera_up.y, -forward.y, 0,
        side.z, camera_up.z, -forward.z, 0,
        -dot(side, eye), -dot(camera_up, eye), dot(forward, eye), 1,
    }};
    return 0;
}

static void face_color(uint32_t face, float color[4])
{
    uint32_t hash = (face + 1u) * UINT32_C(0x9e3779b9);
    hash ^= hash >> 16;
    hash *= UINT32_C(0x7feb352d);
    hash ^= hash >> 15;
    color[0] = 0.25f + (float)(hash & 255u) * (0.70f / 255.0f);
    color[1] = 0.25f + (float)((hash >> 8) & 255u) * (0.70f / 255.0f);
    color[2] = 0.25f + (float)((hash >> 16) & 255u) * (0.70f / 255.0f);
    color[3] = 1.0f;
}

int bsp_flat_build_clear(BspFlatDraw *out, BspBundleVertex vertices[3],
                         uint32_t indices[3],
                         uint32_t vertex_srd_table_address)
{
    if (!out || !vertices || !indices || vertex_srd_table_address == 0u)
        return -1;
    memset(out, 0, sizeof(*out));
    memset(vertices, 0, 3u * sizeof(*vertices));
    vertices[0].position[0] = -1.0f;
    vertices[0].position[1] = -1.0f;
    vertices[1].position[0] = 3.0f;
    vertices[1].position[1] = -1.0f;
    vertices[2].position[0] = -1.0f;
    vertices[2].position[1] = 3.0f;
    for (unsigned vertex = 0; vertex < 3u; ++vertex)
        vertices[vertex].position[2] = 0.999f;
    indices[0] = 0u; indices[1] = 1u; indices[2] = 2u;
    out->sh_words[0] = bits(1.0f);
    out->sh_words[5] = bits(1.0f);
    out->sh_words[10] = bits(1.0f);
    out->sh_words[15] = bits(1.0f);
    out->sh_words[16] = bits(0.02f);
    out->sh_words[17] = bits(0.02f);
    out->sh_words[18] = bits(0.025f);
    out->sh_words[19] = bits(1.0f);
    out->sh_words[20] = vertex_srd_table_address;
    out->index_count = 3u;
    out->indices = indices;
    return 0;
}

int bsp_flat_build_scene(BspFlatDraw *out, uint32_t capacity,
                         const BspBundleView *bundle,
                         uint32_t vertex_srd_table_address,
                         float aspect_ratio)
{
    if (!out || !bundle || !bundle->draws || !bundle->indices ||
        bundle->draw_count == 0u || capacity < bundle->draw_count ||
        vertex_srd_table_address == 0u || !(aspect_ratio > 0.0f) ||
        !__builtin_isfinite(aspect_ratio))
        return -1;
    for (uint32_t index = 0; index < bundle->draw_count; ++index) {
        const BspBundleDraw *const source = &bundle->draws[index];
        if (source->first_index > bundle->index_count ||
            source->index_count > bundle->index_count - source->first_index)
            return -3;
        float color[4];
        face_color(source->face_id, color);
        for (unsigned word = 0; word < 4u; ++word)
            out[index].sh_words[16u + word] = bits(color[word]);
        out[index].sh_words[20] = vertex_srd_table_address;
        out[index].index_count = source->index_count;
        out[index].indices = bundle->indices + source->first_index;
    }
    return bsp_flat_update_camera(out, bundle->draw_count,
                                  bundle->camera_position,
                                  bundle->camera_forward, aspect_ratio);
}

int bsp_flat_update_camera(BspFlatDraw *draws, uint32_t draw_count,
                           const float position[3], const float forward[3],
                           float aspect_ratio)
{
    if (!draws || draw_count == 0u || !position || !forward ||
        !(aspect_ratio > 0.0f) || !__builtin_isfinite(aspect_ratio))
        return -1;
    Matrix4 view;
    if (view_matrix(position, forward, &view) != 0)
        return -2;
    const Matrix4 projection = perspective(1.309f, aspect_ratio, 1.0f,
                                           8192.0f);
    const Matrix4 mvp = multiply(projection, view);
    for (uint32_t draw = 0; draw < draw_count; ++draw)
        for (unsigned word = 0; word < 16u; ++word)
            draws[draw].sh_words[word] = bits(mvp.v[word]);
    return 0;
}
