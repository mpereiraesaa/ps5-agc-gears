#include "gears_scene.h"

#include <math.h>
#include <string.h>

typedef struct Matrix4 { float v[16]; } Matrix4;

static uint32_t bits(float value)
{
    uint32_t result;
    memcpy(&result, &value, sizeof(result));
    return result;
}

static Matrix4 multiply(Matrix4 a, Matrix4 b)
{
    Matrix4 result = {{0}};
    for (unsigned column = 0; column < 4; ++column)
        for (unsigned row = 0; row < 4; ++row)
            for (unsigned k = 0; k < 4; ++k)
                result.v[column * 4 + row] +=
                    a.v[k * 4 + row] * b.v[column * 4 + k];
    return result;
}

static Matrix4 translation(float x, float y, float z)
{
    Matrix4 result = {{1, 0, 0, 0, 0, 1, 0, 0,
                       0, 0, 1, 0, x, y, z, 1}};
    return result;
}

static Matrix4 rotation_x(float angle)
{
    const float c = cosf(angle), s = sinf(angle);
    Matrix4 result = {{1, 0, 0, 0, 0, c, s, 0,
                       0, -s, c, 0, 0, 0, 0, 1}};
    return result;
}

static Matrix4 rotation_z(float angle)
{
    const float c = cosf(angle), s = sinf(angle);
    Matrix4 result = {{c, s, 0, 0, -s, c, 0, 0,
                       0, 0, 1, 0, 0, 0, 0, 1}};
    return result;
}

static Matrix4 perspective(float vertical_fov, float aspect,
                           float near_z, float far_z)
{
    const float f = 1.0f / tanf(vertical_fov * 0.5f);
    Matrix4 result = {{f / aspect, 0, 0, 0, 0, f, 0, 0,
                       0, 0, far_z / (near_z - far_z), -1,
                       0, 0, (near_z * far_z) / (near_z - far_z), 0}};
    return result;
}

static void quaternion_xz(float x_angle, float z_angle, float out[4])
{
    const float hx = x_angle * 0.5f, hz = z_angle * 0.5f;
    const float sx = sinf(hx), cx = cosf(hx);
    const float sz = sinf(hz), cz = cosf(hz);
    /* qx * qz, xyzw. */
    out[0] = sx * cz;
    out[1] = -sx * sz;
    out[2] = cx * sz;
    out[3] = cx * cz;
}

void gears_scene_pose(float time, float positions[GEARS_SCENE_DRAW_COUNT][3],
                      float angles[GEARS_SCENE_DRAW_COUNT])
{
    /* Original es2gears layout scaled uniformly by 0.25. */
    static const float layout[3][3] = {
        {-0.75f, -0.50f, 0.0f}, {0.775f, -0.50f, 0.0f},
        {-0.775f, 1.05f, 0.0f},
    };
    memcpy(positions, layout, sizeof(layout));
    angles[0] = time;
    angles[1] = -2.0f * time - 0.1570796327f;
    angles[2] = -2.0f * time - 0.4363323130f;
}

int gears_build_scene(GearsSceneDraw out[GEARS_SCENE_DRAW_COUNT], float time,
                      const uint32_t tables[GEARS_SCENE_DRAW_COUNT],
                      const uint32_t counts[GEARS_SCENE_DRAW_COUNT])
{
    if (!out || !tables || !counts || !__builtin_isfinite(time)) return -1;
    for (unsigned i = 0; i < GEARS_SCENE_DRAW_COUNT; ++i)
        if (!tables[i] || !counts[i]) return -1;

    float positions[GEARS_SCENE_DRAW_COUNT][3];
    float angles[GEARS_SCENE_DRAW_COUNT];
    gears_scene_pose(time, positions, angles);
    static const float materials[3][4] = {
        {0.80f, 0.10f, 0.00f, 1.0f}, {0.00f, 0.80f, 0.20f, 1.0f},
        {0.20f, 0.20f, 1.00f, 1.0f},
    };
    const float tilt = -0.48f;
    const Matrix4 projection = perspective(0.78f, 16.0f / 9.0f, 0.1f, 40.0f);
    const Matrix4 view = multiply(translation(0, 0, -6.2f), rotation_x(tilt));

    for (unsigned draw = 0; draw < GEARS_SCENE_DRAW_COUNT; ++draw) {
        const Matrix4 model = multiply(
            translation(positions[draw][0], positions[draw][1], positions[draw][2]),
            rotation_z(angles[draw]));
        const Matrix4 mvp = multiply(projection, multiply(view, model));
        float quaternion[4];
        quaternion_xz(tilt, angles[draw], quaternion);
        for (unsigned i = 0; i < 16; ++i) out[draw].sh_words[i] = bits(mvp.v[i]);
        for (unsigned i = 0; i < 4; ++i)
            out[draw].sh_words[16 + i] = bits(quaternion[i]);
        for (unsigned i = 0; i < 4; ++i)
            out[draw].sh_words[20 + i] = bits(materials[draw][i]);
        out[draw].sh_words[24] = tables[draw];
        out[draw].vertex_count = counts[draw];
    }
    return 0;
}
