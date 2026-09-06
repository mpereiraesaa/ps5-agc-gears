#ifndef PS5_AGC_GEARS_BSP_ALPHA_TEST_H
#define PS5_AGC_GEARS_BSP_ALPHA_TEST_H

#include "bsp_bundle.h"

#include <stdint.h>

typedef struct BspAlphaTestPlan {
    uint32_t texture_count;
    uint32_t draw_count;
    uint32_t target_texture;
    uint32_t target_face;
    float target_forward[3];
} BspAlphaTestPlan;

/* Count `{` texture consumers and aim a gate camera at the nearest one. */
int bsp_alpha_test_plan(const BspBundleView *bundle,
                        const float camera_position[3],
                        BspAlphaTestPlan *out);

#endif
