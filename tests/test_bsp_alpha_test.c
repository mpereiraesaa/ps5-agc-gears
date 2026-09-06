#include "../src/bsp_alpha_test.h"

#include <assert.h>
#include <math.h>
#include <stdint.h>

int main(void)
{
    BspBundleTexture textures[3] = {
        {.flags = 0u},
        {.flags = BSP_BUNDLE_TEXTURE_TRANSPARENT},
        {.flags = BSP_BUNDLE_TEXTURE_TRANSPARENT |
                  BSP_BUNDLE_TEXTURE_NODRAW},
    };
    BspBundleVertex vertices[4] = {
        {.position = {10.0f, 0.0f, 0.0f}},
        {.position = {10.0f, 1.0f, 0.0f}},
        {.position = {10.0f, 0.0f, 1.0f}},
        {.position = {2.0f, 0.0f, 0.0f}},
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
        .textures = textures, .texture_count = 3u,
    };
    const float camera[3] = {0.0f, 0.0f, 0.0f};
    BspAlphaTestPlan plan;
    assert(bsp_alpha_test_plan(&bundle, camera, &plan) == 0);
    assert(plan.texture_count == 1u && plan.draw_count == 2u);
    assert(plan.target_texture == 1u && plan.target_face == 20u);
    const float length = sqrtf(plan.target_forward[0] *
                               plan.target_forward[0] +
                               plan.target_forward[1] *
                               plan.target_forward[1] +
                               plan.target_forward[2] *
                               plan.target_forward[2]);
    assert(fabsf(length - 1.0f) < 0.00001f);
    assert(bsp_alpha_test_plan(0, camera, &plan) == -1);
    textures[1].flags = 0u;
    assert(bsp_alpha_test_plan(&bundle, camera, &plan) == -2);
    textures[1].flags = BSP_BUNDLE_TEXTURE_TRANSPARENT;
    draws[1].first_index = 7u;
    assert(bsp_alpha_test_plan(&bundle, camera, &plan) == -3);
    return 0;
}
