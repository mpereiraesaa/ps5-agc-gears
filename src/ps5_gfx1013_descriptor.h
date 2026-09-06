#ifndef PS5_AGC_GEARS_GFX1013_DESCRIPTOR_H
#define PS5_AGC_GEARS_GFX1013_DESCRIPTOR_H

#include <stdint.h>

enum {
    PS5_GFX1013_VSHARP_DWORDS = 4,
    PS5_GFX1013_TSHARP_DWORDS = 8,
    PS5_GFX1013_SSHARP_DWORDS = 4,
};

enum ps5_gfx1013_address_mode {
    PS5_GFX1013_REPEAT = 0,
    PS5_GFX1013_CLAMP_LAST_TEXEL = 2,
};

enum ps5_gfx1013_filter {
    PS5_GFX1013_FILTER_POINT = 0,
    PS5_GFX1013_FILTER_BILINEAR = 1,
};

/* Public PAL GFX10.3 buffer SRD (V#), suitable for vertex and structured
 * constant data. The table containing it must itself be 16-byte aligned. */
int ps5_gfx1013_build_vsharp(uint32_t out[PS5_GFX1013_VSHARP_DWORDS],
                             uint64_t gpu_address, uint32_t stride,
                             uint32_t records);

/* Raw, read-only V# used by DescriptorConstBuffer. Size is a byte range. */
int ps5_gfx1013_build_constant_vsharp(
    uint32_t out[PS5_GFX1013_VSHARP_DWORDS], uint64_t gpu_address,
    uint32_t bytes);

/* Public Mesa/PAL GFX10.3 image resource (T#), linear RGBA8_UNORM. */
int ps5_gfx1013_build_tsharp_rgba8(
    uint32_t out[PS5_GFX1013_TSHARP_DWORDS], uint64_t gpu_address,
    uint32_t width, uint32_t height, uint32_t row_pitch);

/* Public Mesa/PAL GFX10.3 sampler state (S#). */
int ps5_gfx1013_build_ssharp(uint32_t out[PS5_GFX1013_SSHARP_DWORDS],
                             enum ps5_gfx1013_address_mode address_mode,
                             enum ps5_gfx1013_filter filter);

#endif
