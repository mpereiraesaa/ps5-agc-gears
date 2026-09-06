#ifndef PS5_AGC_GEARS_BSP_RESOURCE_FRAME_H
#define PS5_AGC_GEARS_BSP_RESOURCE_FRAME_H

#include "bsp_bundle.h"
#include "bsp_flat_draw.h"
#include "ps5_gfx1013_descriptor.h"
#include "ps5_transient_ring.h"

#include <stddef.h>
#include <stdint.h>

enum {
    BSP_RESOURCE_CONSTANT_DWORDS = 32,
    BSP_RESOURCE_OVERLAY_VERTICES = 4,
    BSP_RESOURCE_OVERLAY_INDICES = 6,
};

typedef struct BspResourceConstants {
    float mvp[16];
    float control[4];
    float debug_values[12];
} BspResourceConstants;

typedef struct BspOverlayConstants {
    float color[4];
    float debug_values[28];
} BspOverlayConstants;

typedef struct BspResourceFrame {
    const uint32_t *map_constant_table;
    const uint32_t *clear_constant_table;
    const uint32_t *map_vertex_table;
    const uint32_t *clear_vertex_table;
    const uint32_t *texture_tables;
    uint32_t texture_table_dwords;
    const uint32_t *overlay_constant_table;
    const uint16_t *overlay_indices;
    uint32_t overlay_index_count;
    size_t transient_bytes;
} BspResourceFrame;

int bsp_resource_frame_build(
    BspResourceFrame *out, Ps5TransientRing *ring, uint32_t slot_index,
    const void *gpu_mapping, size_t gpu_mapping_bytes,
    const BspBundleView *bundle, uint64_t lightmap_pixels_gpu_address,
    const BspBundleVertex clear_vertices[3],
    const uint16_t clear_indices[3],
    const float camera_position[3], const float camera_forward[3],
    float aspect_ratio, uint64_t frame_index,
    enum ps5_gfx1013_filter base_filter);

#endif
