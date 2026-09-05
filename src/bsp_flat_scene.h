#ifndef PS5_AGC_GEARS_BSP_FLAT_SCENE_H
#define PS5_AGC_GEARS_BSP_FLAT_SCENE_H

#include "bsp_bundle.h"
#include "bsp_flat_draw.h"

/* Build the fixed spawn camera and one immutable draw block per BSP face. */
int bsp_flat_build_scene(BspFlatDraw *out, uint32_t capacity,
                         const BspBundleView *bundle,
                         uint32_t vertex_srd_table_address,
                         float aspect_ratio);

#endif
