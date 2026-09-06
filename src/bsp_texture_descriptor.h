#ifndef PS5_AGC_GEARS_BSP_TEXTURE_DESCRIPTOR_H
#define PS5_AGC_GEARS_BSP_TEXTURE_DESCRIPTOR_H

#include "bsp_bundle.h"
#include "ps5_gfx1013_descriptor.h"

#include <stdint.h>

enum {
    BSP_GFX1013_IMAGE_DWORDS = PS5_GFX1013_TSHARP_DWORDS,
    BSP_GFX1013_SAMPLER_DWORDS = PS5_GFX1013_SSHARP_DWORDS,
    BSP_GFX1013_COMBINED_DWORDS = 12,
    BSP_TEXTURE_TABLE_DWORDS = 24,
};

enum bsp_texture_address_mode {
    BSP_TEXTURE_REPEAT = PS5_GFX1013_REPEAT,
    BSP_TEXTURE_CLAMP_LAST_TEXEL = PS5_GFX1013_CLAMP_LAST_TEXEL,
};

enum bsp_texture_filter {
    BSP_TEXTURE_FILTER_POINT = PS5_GFX1013_FILTER_POINT,
    BSP_TEXTURE_FILTER_BILINEAR = PS5_GFX1013_FILTER_BILINEAR,
};

/* Build one GFX10.3 image+sampler descriptor for linear RGBA8_UNORM. */
int bsp_gfx1013_combined_descriptor(
    uint32_t out[BSP_GFX1013_COMBINED_DWORDS], uint64_t gpu_address,
    uint32_t width, uint32_t height, uint32_t row_pitch,
    enum bsp_texture_address_mode address_mode,
    enum bsp_texture_filter filter);

/* One 24-DWORD table per base texture: binding 0 base, binding 1 lightmap. */
int bsp_texture_table_required_dwords(const BspBundleView *bundle,
                                      uint32_t *required);
int bsp_texture_build_tables(uint32_t *out, uint32_t capacity_dwords,
                             const BspBundleView *bundle,
                             uint64_t texture_pixels_gpu_address,
                             uint64_t lightmap_pixels_gpu_address,
                             uint32_t *written_dwords);

#endif
