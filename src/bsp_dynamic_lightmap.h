#ifndef PS5_AGC_GEARS_BSP_DYNAMIC_LIGHTMAP_H
#define PS5_AGC_GEARS_BSP_DYNAMIC_LIGHTMAP_H

#include "bsp_bundle.h"
#include "ps5_transient_ring.h"

#include <stddef.h>
#include <stdint.h>

enum {
    BSP_DYNAMIC_LIGHTMAP_GUARD_BYTES = 256,
    BSP_DYNAMIC_LIGHTMAP_PATCH_EDGE = 8,
    BSP_DYNAMIC_LIGHTMAP_PATTERN_COUNT = 2,
};

typedef struct BspDynamicLightmapLayout {
    uint32_t image_width;
    uint32_t image_height;
    uint32_t row_pitch;
    uint32_t patch_x;
    uint32_t patch_y;
    uint32_t patch_width;
    uint32_t patch_height;
    uint32_t hit_face;
    size_t image_bytes;
    size_t patch_bytes;
    size_t dirty_offset;
    size_t dirty_span_bytes;
} BspDynamicLightmapLayout;

typedef struct BspDynamicLightmapSlot {
    uint8_t *allocation;
    size_t allocation_bytes;
    uint8_t *pixels;
    uint64_t surrounding_hash;
    uint64_t last_frame;
    uint32_t last_pattern;
    uint8_t first_upload_pending;
    uint8_t initialized;
} BspDynamicLightmapSlot;

typedef struct BspDynamicLightmapUpdate {
    const void *staging_address;
    size_t staging_bytes;
    const void *written_address;
    size_t written_span_bytes;
    size_t uploaded_bytes;
    uint64_t patch_hash;
    uint32_t pattern;
    uint8_t first_upload;
} BspDynamicLightmapUpdate;

int bsp_dynamic_lightmap_select(const BspBundleView *bundle,
                                BspDynamicLightmapLayout *layout);
int bsp_dynamic_lightmap_allocation_bytes(
    const BspDynamicLightmapLayout *layout, size_t *bytes);
int bsp_dynamic_lightmap_slot_init(
    BspDynamicLightmapSlot *slot, void *allocation, size_t allocation_bytes,
    const uint8_t *source_pixels, size_t source_bytes,
    const BspDynamicLightmapLayout *layout);
int bsp_dynamic_lightmap_update(
    BspDynamicLightmapSlot *slot, const BspDynamicLightmapLayout *layout,
    Ps5TransientRing *ring, uint32_t slot_index, uint64_t frame_index,
    BspDynamicLightmapUpdate *update);
int bsp_dynamic_lightmap_guards_intact(
    const BspDynamicLightmapSlot *slot,
    const BspDynamicLightmapLayout *layout);
uint64_t bsp_dynamic_lightmap_patch_hash(
    const uint8_t *pixels, const BspDynamicLightmapLayout *layout);
uint64_t bsp_dynamic_lightmap_surrounding_hash(
    const uint8_t *pixels, const BspDynamicLightmapLayout *layout);

#endif
