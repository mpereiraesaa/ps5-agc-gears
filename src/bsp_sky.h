#ifndef PS5_AGC_GEARS_BSP_SKY_H
#define PS5_AGC_GEARS_BSP_SKY_H

#include "bsp_bundle.h"

#include <stdint.h>

typedef struct BspSkyPlan {
    uint32_t texture_count;
    uint32_t draw_count;
    uint32_t target_texture;
    uint32_t target_face;
    float target_position[3];
    float target_forward[3];
} BspSkyPlan;

/* Count baked `sky` consumers and aim a gate camera at the nearest one. */
int bsp_sky_plan(const BspBundleView *bundle,
                 const float camera_position[3],
                 BspSkyPlan *out);

#endif
