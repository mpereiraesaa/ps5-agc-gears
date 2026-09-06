#include "ps5_gfx1013_descriptor.h"

#include <limits.h>
#include <string.h>

/* Sanitized field packing derived from Mesa's MIT-licensed
 * ac_build_buffer_descriptor/ac_build_image_descriptor and gfx10-rsrc.json. */
enum {
    GFX10_FORMAT_8_8_8_8_UNORM = 56,
    SQ_SEL_X = 4,
    SQ_SEL_Y = 5,
    SQ_SEL_Z = 6,
    SQ_SEL_W = 7,
    SQ_RSRC_IMG_2D = 9,
};

static int address_48(uint64_t address)
{
    return address != 0u && address < (UINT64_C(1) << 48);
}

int ps5_gfx1013_build_vsharp(uint32_t out[PS5_GFX1013_VSHARP_DWORDS],
                             uint64_t gpu_address, uint32_t stride,
                             uint32_t records)
{
    if (!out || !address_48(gpu_address) || (gpu_address & 3u) != 0u ||
        stride == 0u || stride > 0x3fffu || records == 0u)
        return -1;
    out[0] = (uint32_t)gpu_address;
    out[1] = (uint32_t)((gpu_address >> 32) & 0xffffu) | (stride << 16);
    out[2] = records;
    out[3] = UINT32_C(0x11014fac);
    return 0;
}

int ps5_gfx1013_build_constant_vsharp(
    uint32_t out[PS5_GFX1013_VSHARP_DWORDS], uint64_t gpu_address,
    uint32_t bytes)
{
    if (!out || !address_48(gpu_address) || (gpu_address & 3u) != 0u ||
        bytes == 0u || bytes > UINT32_MAX - 3u)
        return -1;
    out[0] = (uint32_t)gpu_address;
    out[1] = (uint32_t)((gpu_address >> 32) & 0xffffu);
    out[2] = (bytes + 3u) & ~UINT32_C(3);
    /* XYZW, R32_FLOAT, resource-level, raw OOB checking. */
    out[3] = UINT32_C(0x31016fac);
    return 0;
}

int ps5_gfx1013_build_tsharp_rgba8(
    uint32_t out[PS5_GFX1013_TSHARP_DWORDS], uint64_t gpu_address,
    uint32_t width, uint32_t height, uint32_t row_pitch)
{
    return ps5_gfx1013_build_tsharp_rgba8_mip(
        out, gpu_address, width, height, row_pitch, 1u);
}

int ps5_gfx1013_build_tsharp_rgba8_mip(
    uint32_t out[PS5_GFX1013_TSHARP_DWORDS], uint64_t gpu_address,
    uint32_t width, uint32_t height, uint32_t row_pitch,
    uint32_t mip_count)
{
    if (!out || !address_48(gpu_address) || (gpu_address & 255u) != 0u ||
        width == 0u || height == 0u || width > 16384u ||
        height > 16384u || width > UINT32_MAX / 4u ||
        row_pitch < width * 4u || (row_pitch & 255u) != 0u ||
        row_pitch / 4u > 16384u || mip_count == 0u || mip_count > 15u)
        return -1;
    uint32_t maximum_mips = 1u;
    for (uint32_t dimension = width > height ? width : height;
         dimension > 1u; dimension >>= 1)
        ++maximum_mips;
    if (mip_count > maximum_mips ||
        (mip_count > 1u && row_pitch !=
            ((width * 4u + 255u) & ~UINT32_C(255))))
        return -1;
    const uint32_t width_minus_one = width - 1u;
    const uint32_t pitch_minus_one = row_pitch / 4u - 1u;
    memset(out, 0, PS5_GFX1013_TSHARP_DWORDS * sizeof(*out));
    out[0] = (uint32_t)(gpu_address >> 8);
    out[1] = (uint32_t)(gpu_address >> 40) |
             (GFX10_FORMAT_8_8_8_8_UNORM << 20) |
             ((width_minus_one & 3u) << 30);
    out[2] = ((width_minus_one >> 2) & 0xfffu) |
             ((height - 1u) << 14) | (UINT32_C(1) << 31);
    out[3] = SQ_SEL_X | (SQ_SEL_Y << 3) | (SQ_SEL_Z << 6) |
             (SQ_SEL_W << 9) | ((mip_count - 1u) << 16) |
             ((uint32_t)SQ_RSRC_IMG_2D << 28);
    /* GFX10.3's custom PITCH encoding is valid only for one-level images.
     * Mip chains use AddrLib's implicit 256-byte pitch alignment. */
    if (mip_count == 1u && row_pitch / 4u != width)
        out[4] = (pitch_minus_one & 0x1fffu) |
                 (((pitch_minus_one >> 13) & 1u) << 13);
    out[5] = ((mip_count - 1u) << 4) | (4u << 20);
    return 0;
}

int ps5_gfx1013_build_ssharp(uint32_t out[PS5_GFX1013_SSHARP_DWORDS],
                             enum ps5_gfx1013_address_mode address_mode,
                             enum ps5_gfx1013_filter filter)
{
    return ps5_gfx1013_build_ssharp_mip(out, address_mode, filter, 1u);
}

int ps5_gfx1013_build_ssharp_mip(
    uint32_t out[PS5_GFX1013_SSHARP_DWORDS],
    enum ps5_gfx1013_address_mode address_mode,
    enum ps5_gfx1013_filter filter, uint32_t mip_count)
{
    if (!out ||
        (address_mode != PS5_GFX1013_REPEAT &&
         address_mode != PS5_GFX1013_CLAMP_LAST_TEXEL) ||
        (filter != PS5_GFX1013_FILTER_POINT &&
         filter != PS5_GFX1013_FILTER_BILINEAR &&
         filter != PS5_GFX1013_FILTER_TRILINEAR &&
         filter != PS5_GFX1013_FILTER_ANISOTROPIC_4X) ||
        mip_count == 0u || mip_count > 15u ||
        ((filter == PS5_GFX1013_FILTER_TRILINEAR ||
          filter == PS5_GFX1013_FILTER_ANISOTROPIC_4X) &&
         mip_count == 1u))
        return -1;
    memset(out, 0, PS5_GFX1013_SSHARP_DWORDS * sizeof(*out));
    out[0] = (uint32_t)address_mode |
             ((uint32_t)address_mode << 3) |
             ((uint32_t)address_mode << 6);
    if (filter == PS5_GFX1013_FILTER_BILINEAR ||
        filter == PS5_GFX1013_FILTER_TRILINEAR) {
        out[2] = (UINT32_C(1) << 20) | (UINT32_C(1) << 22);
    } else if (filter == PS5_GFX1013_FILTER_ANISOTROPIC_4X) {
        const uint32_t ratio = 2u;
        out[0] |= (ratio << 9) | ((ratio >> 1) << 16) |
                  (ratio << 21);
        out[1] |= (ratio + 6u) << 24;
        out[2] = (UINT32_C(3) << 20) | (UINT32_C(3) << 22) |
                 (UINT32_C(1) << 29);
    }
    if (filter == PS5_GFX1013_FILTER_TRILINEAR ||
        filter == PS5_GFX1013_FILTER_ANISOTROPIC_4X) {
        out[1] |= ((mip_count - 1u) << 8) << 12;
        out[2] |= UINT32_C(2) << 26;
    }
    return 0;
}
