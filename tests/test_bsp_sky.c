#include "../src/bsp_sky.h"

#include <assert.h>
#include <math.h>
#include <stdint.h>

int main(void)
{
    BspBundleTexture textures[2] = {
        {.flags = 0u},
        {.flags = BSP_BUNDLE_TEXTURE_SKY},
    };
    BspBundleVertex vertices[4] = {
        {.position = {120.0f, 0.0f, 0.0f}},
        {.position = {120.0f, 1.0f, 0.0f}},
        {.position = {120.0f, 0.0f, 1.0f}},
        {.position = {80.0f, 0.0f, 0.0f}},
    };
    const uint16_t indices[6] = {0u, 1u, 2u, 3u, 1u, 2u};
    BspBundleDraw draws[2] = {
        {.first_index = 0u, .index_count = 3u, .base_texture = 1u,
         .face_id = 10u},
        {.first_index = 3u, .index_count = 3u, .base_texture = 1u,
         .face_id = 20u},
    };
    BspBundleView bundle = {
        .vertices = vertices, .vertex_count = 4u,
        .indices = indices, .index_count = 6u,
        .draws = draws, .draw_count = 2u,
        .textures = textures, .texture_count = 2u,
    };
    const float camera[3] = {0.0f, 0.0f, 0.0f};
    BspSkyPlan plan;
    assert(bsp_sky_plan(&bundle, camera, &plan) == 0);
    assert(plan.texture_count == 1u && plan.draw_count == 2u);
    assert(plan.target_texture == 1u && plan.target_face == 20u);
    const float length = sqrtf(plan.target_forward[0] * plan.target_forward[0] +
                               plan.target_forward[1] * plan.target_forward[1] +
                               plan.target_forward[2] * plan.target_forward[2]);
    assert(fabsf(length - 1.0f) < 0.00001f);
    const float center[3] = {
        (80.0f + 120.0f + 120.0f) / 3.0f, 1.0f / 3.0f, 1.0f / 3.0f,
    };
    const float standoff = sqrtf(
        (center[0] - plan.target_position[0]) *
            (center[0] - plan.target_position[0]) +
        (center[1] - plan.target_position[1]) *
            (center[1] - plan.target_position[1]) +
        (center[2] - plan.target_position[2]) *
            (center[2] - plan.target_position[2]));
    assert(fabsf(standoff - 64.0f) < 0.0001f);
    assert(bsp_sky_plan(0, camera, &plan) == -1);
    textures[1].flags = 0u;
    assert(bsp_sky_plan(&bundle, camera, &plan) == -2);
    textures[1].flags = BSP_BUNDLE_TEXTURE_SKY;
    draws[1].first_index = 7u;
    assert(bsp_sky_plan(&bundle, camera, &plan) == -3);
    return 0;
}
