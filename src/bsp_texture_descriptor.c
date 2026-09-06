#include "bsp_texture_descriptor.h"

#include <limits.h>
#include <string.h>

static int valid_address(uint64_t address)
{
    return address != 0u && (address & UINT64_C(255)) == 0u &&
           address < (UINT64_C(1) << 48);
}

int bsp_gfx1013_combined_descriptor(
    uint32_t out[BSP_GFX1013_COMBINED_DWORDS], uint64_t gpu_address,
    uint32_t width, uint32_t height, uint32_t row_pitch,
    uint32_t mip_count,
    enum bsp_texture_address_mode address_mode,
    enum bsp_texture_filter filter)
{
    if (!out)
        return -1;
    memset(out, 0, BSP_GFX1013_COMBINED_DWORDS * sizeof(*out));
    return ps5_gfx1013_build_tsharp_rgba8_mip(
                   out, gpu_address, width, height, row_pitch,
                   mip_count) == 0 &&
                   ps5_gfx1013_build_ssharp_mip(
                       out + BSP_GFX1013_IMAGE_DWORDS,
                       (enum ps5_gfx1013_address_mode)address_mode,
                       (enum ps5_gfx1013_filter)filter, mip_count) == 0
               ? 0
               : -1;
}

int bsp_texture_table_required_dwords(const BspBundleView *bundle,
                                      uint32_t *required)
{
    if (!bundle || !required || !bundle->textures ||
        bundle->texture_count == 0u ||
        bundle->texture_count > UINT32_MAX / BSP_TEXTURE_TABLE_DWORDS)
        return -1;
    *required = bundle->texture_count * BSP_TEXTURE_TABLE_DWORDS;
    return 0;
}

int bsp_texture_build_tables(uint32_t *out, uint32_t capacity_dwords,
                             const BspBundleView *bundle,
                             uint64_t texture_pixels_gpu_address,
                             uint64_t lightmap_pixels_gpu_address,
                             enum bsp_texture_filter base_filter,
                             uint32_t *written_dwords)
{
    uint32_t required = 0u;
    if (!out || !bundle || !written_dwords || !bundle->texture_pixels ||
        bundle->texture_pixel_bytes == 0u ||
        !bundle->lightmap_image || !bundle->lightmap_pixels ||
        bsp_texture_table_required_dwords(bundle, &required) != 0)
        return -1;
    if (base_filter != BSP_TEXTURE_FILTER_TRILINEAR &&
        base_filter != BSP_TEXTURE_FILTER_ANISOTROPIC_4X)
        return -1;
    *written_dwords = 0u;
    if (capacity_dwords < required)
        return -2;
    if (!valid_address(texture_pixels_gpu_address) ||
        !valid_address(lightmap_pixels_gpu_address))
        return -3;

    const BspBundleImage *const lightmap = bundle->lightmap_image;
    if (lightmap->format != BSP_BUNDLE_IMAGE_RGBA8_UNORM ||
        lightmap->width == 0u || lightmap->height == 0u ||
        lightmap->width > 2048u || lightmap->height > 2048u ||
        lightmap->width > UINT32_MAX / 4u ||
        lightmap->row_pitch < lightmap->width * 4u ||
        (lightmap->row_pitch & 255u) != 0u ||
        lightmap->row_pitch / 4u > 16384u ||
        lightmap->width > UINT32_MAX / lightmap->height ||
        lightmap->width * lightmap->height != bundle->lightmap_pixel_count)
        return -4;
    for (uint32_t index = 0; index < bundle->texture_count; ++index) {
        const BspBundleTexture *const texture = &bundle->textures[index];
        BspBundleMipLevel base_level;
        if (texture->format != BSP_BUNDLE_IMAGE_RGBA8_UNORM ||
            texture->width == 0u || texture->height == 0u ||
            texture->width > 16384u || texture->height > 16384u ||
            texture->row_pitch !=
                ((texture->width * 4u + 255u) & ~UINT32_C(255)) ||
            texture->mip_count < 2u || texture->mip_count > 15u ||
            bsp_bundle_texture_mip_level(texture, 0u, &base_level) != 0 ||
            base_level.offset > UINT32_MAX - base_level.bytes ||
            texture->bytes != base_level.offset + base_level.bytes ||
            (texture->offset & 255u) != 0u ||
            texture->offset > bundle->texture_pixel_bytes ||
            texture->bytes > bundle->texture_pixel_bytes - texture->offset ||
            (uint64_t)texture->offset >
                UINT64_MAX - texture_pixels_gpu_address ||
            !valid_address(texture_pixels_gpu_address + texture->offset))
            return -4;
    }

    for (uint32_t index = 0; index < bundle->texture_count; ++index) {
        const BspBundleTexture *const texture = &bundle->textures[index];
        uint32_t *const table = out + index * BSP_TEXTURE_TABLE_DWORDS;
        if (bsp_gfx1013_combined_descriptor(
                table, texture_pixels_gpu_address + texture->offset,
                texture->width, texture->height, texture->row_pitch,
                texture->mip_count, BSP_TEXTURE_REPEAT, base_filter) != 0 ||
            bsp_gfx1013_combined_descriptor(
                table + BSP_GFX1013_COMBINED_DWORDS,
                lightmap_pixels_gpu_address, bundle->lightmap_image->width,
                bundle->lightmap_image->height,
                bundle->lightmap_image->row_pitch, 1u,
                BSP_TEXTURE_CLAMP_LAST_TEXEL,
                BSP_TEXTURE_FILTER_BILINEAR) != 0)
            return -5;
    }
    *written_dwords = required;
    return 0;
}
