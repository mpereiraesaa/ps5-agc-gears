#include "bsp_texture_descriptor.h"

#include <limits.h>
#include <string.h>

/*
 * Field positions follow Mesa's MIT-licensed GFX10 descriptor builders in
 * ac_descriptors.c and the register descriptions in gfx10-rsrc.json.  Keeping
 * the small checked builder local avoids publishing generated AMD headers.
 */
enum {
    GFX10_FORMAT_8_8_8_8_UNORM = 56,
    SQ_SEL_X = 4,
    SQ_SEL_Y = 5,
    SQ_SEL_Z = 6,
    SQ_SEL_W = 7,
    SQ_RSRC_IMG_2D = 9,
};

static int valid_address(uint64_t address)
{
    return address != 0u && (address & UINT64_C(255)) == 0u &&
           address < (UINT64_C(1) << 48);
}

int bsp_gfx1013_combined_descriptor(
    uint32_t out[BSP_GFX1013_COMBINED_DWORDS], uint64_t gpu_address,
    uint32_t width, uint32_t height, uint32_t row_pitch,
    enum bsp_texture_address_mode address_mode,
    enum bsp_texture_filter filter)
{
    if (!out || !valid_address(gpu_address) || width == 0u ||
        height == 0u || width > 16384u || height > 16384u ||
        width > UINT32_MAX / 4u || row_pitch < width * 4u ||
        (row_pitch & 255u) != 0u || row_pitch / 4u > 16384u ||
        (address_mode != BSP_TEXTURE_REPEAT &&
         address_mode != BSP_TEXTURE_CLAMP_LAST_TEXEL) ||
        (filter != BSP_TEXTURE_FILTER_POINT &&
         filter != BSP_TEXTURE_FILTER_BILINEAR))
        return -1;

    const uint32_t width_minus_one = width - 1u;
    const uint32_t pitch_minus_one = row_pitch / 4u - 1u;
    memset(out, 0, BSP_GFX1013_COMBINED_DWORDS * sizeof(*out));
    out[0] = (uint32_t)(gpu_address >> 8);
    out[1] = (uint32_t)(gpu_address >> 40) |
             (GFX10_FORMAT_8_8_8_8_UNORM << 20) |
             ((width_minus_one & 3u) << 30);
    out[2] = ((width_minus_one >> 2) & 0xfffu) |
             ((height - 1u) << 14) | (UINT32_C(1) << 31);
    out[3] = SQ_SEL_X | (SQ_SEL_Y << 3) | (SQ_SEL_Z << 6) |
             (SQ_SEL_W << 9) | ((uint32_t)SQ_RSRC_IMG_2D << 28);
    if (row_pitch / 4u != width)
        out[4] = (pitch_minus_one & 0x1fffu) |
                 (((pitch_minus_one >> 13) & 1u) << 13);
    out[5] = 4u << 20;

    out[8] = (uint32_t)address_mode |
             ((uint32_t)address_mode << 3) |
             ((uint32_t)address_mode << 6);
    out[10] = filter == BSP_TEXTURE_FILTER_BILINEAR
                  ? (UINT32_C(1) << 20) | (UINT32_C(1) << 22)
                  : 0u;
    return 0;
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
                             uint32_t *written_dwords)
{
    uint32_t required = 0u;
    if (!out || !bundle || !written_dwords || !bundle->texture_pixels ||
        bundle->texture_pixel_bytes == 0u ||
        !bundle->lightmap_image || !bundle->lightmap_pixels ||
        bsp_texture_table_required_dwords(bundle, &required) != 0)
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
        if (texture->format != BSP_BUNDLE_IMAGE_RGBA8_UNORM ||
            texture->width == 0u || texture->height == 0u ||
            texture->width > 16384u || texture->height > 16384u ||
            texture->row_pitch < texture->width * 4u ||
            (texture->row_pitch & 255u) != 0u ||
            texture->height > UINT32_MAX / texture->row_pitch ||
            texture->bytes != texture->height * texture->row_pitch ||
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
                BSP_TEXTURE_REPEAT, BSP_TEXTURE_FILTER_BILINEAR) != 0 ||
            bsp_gfx1013_combined_descriptor(
                table + BSP_GFX1013_COMBINED_DWORDS,
                lightmap_pixels_gpu_address, bundle->lightmap_image->width,
                bundle->lightmap_image->height,
                bundle->lightmap_image->row_pitch,
                BSP_TEXTURE_CLAMP_LAST_TEXEL,
                BSP_TEXTURE_FILTER_BILINEAR) != 0)
            return -5;
    }
    *written_dwords = required;
    return 0;
}
