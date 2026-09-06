#ifndef PS5_AGC_GEARS_BSP_FLAT_SCENE_H
#define PS5_AGC_GEARS_BSP_FLAT_SCENE_H

#include "bsp_bundle.h"
#include "bsp_flat_draw.h"

/* Build the fixed spawn camera and one immutable draw block per BSP face. */
int bsp_flat_build_scene(BspFlatDraw *out, uint32_t capacity,
                         const BspBundleView *bundle,
                         uint32_t vertex_srd_table_address,
                         float aspect_ratio);
int bsp_flat_build_clear(BspFlatDraw *out, BspBundleVertex vertices[3],
                         uint16_t indices[3],
                         uint32_t vertex_srd_table_address);
int bsp_flat_update_camera(BspFlatDraw *draws, uint32_t draw_count,
                           const float position[3], const float forward[3],
                           float aspect_ratio);
int bsp_flat_camera_matrix(float out[16], const float position[3],
                           const float forward[3], float aspect_ratio);

#endif
