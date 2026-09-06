#include "../src/bsp_texture_descriptor.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

int main(void)
{
    uint32_t descriptor[BSP_GFX1013_COMBINED_DWORDS];
    memset(descriptor, 0xa5, sizeof(descriptor));
    assert(bsp_gfx1013_combined_descriptor(
               descriptor, UINT64_C(0x0000123456789a00), 64u, 32u, 256u,
               BSP_TEXTURE_REPEAT, BSP_TEXTURE_FILTER_BILINEAR) == 0);
    const uint32_t expected[] = {
        UINT32_C(0x3456789a), UINT32_C(0xc3800012),
        UINT32_C(0x8007c00f), UINT32_C(0x90000fac),
        0u, UINT32_C(0x00400000), 0u, 0u,
        0u, 0u, UINT32_C(0x00500000), 0u,
    };
    assert(memcmp(descriptor, expected, sizeof(expected)) == 0);

    assert(bsp_gfx1013_combined_descriptor(
               descriptor, UINT64_C(0x0000123456789a00), 17u, 8u, 256u,
               BSP_TEXTURE_CLAMP_LAST_TEXEL,
               BSP_TEXTURE_FILTER_POINT) == 0);
    assert(descriptor[4] == 63u);
    assert(descriptor[8] == UINT32_C(0x92));
    assert(descriptor[10] == 0u);
    assert(bsp_gfx1013_combined_descriptor(
               descriptor, UINT64_C(0x0000123456789a01), 17u, 8u, 256u,
               BSP_TEXTURE_REPEAT, BSP_TEXTURE_FILTER_POINT) == -1);
    assert(bsp_gfx1013_combined_descriptor(
               descriptor, UINT64_C(0x0000123456789a00), 65u, 8u, 256u,
               BSP_TEXTURE_REPEAT, BSP_TEXTURE_FILTER_POINT) == -1);

    const BspBundleTexture textures[2] = {
        {.offset = 0u, .bytes = 2048u, .width = 64u, .height = 8u,
         .row_pitch = 256u, .format = BSP_BUNDLE_IMAGE_RGBA8_UNORM,
         .name_hash = 1u, .flags = 0u},
        {.offset = 2048u, .bytes = 2048u, .width = 17u, .height = 8u,
         .row_pitch = 256u, .format = BSP_BUNDLE_IMAGE_RGBA8_UNORM,
         .name_hash = 2u, .flags = 0u},
    };
    const BspBundleImage lightmap = {
        .width = 128u, .height = 32u, .row_pitch = 512u,
        .format = BSP_BUNDLE_IMAGE_RGBA8_UNORM,
    };
    uint8_t texture_pixels[1] = {0};
    uint8_t lightmap_pixels[1] = {0};
    BspBundleView bundle = {
        .lightmap_image = &lightmap,
        .lightmap_pixels = lightmap_pixels,
        .textures = textures,
        .texture_count = 2u,
        .texture_pixels = texture_pixels,
        .texture_pixel_bytes = 4096u,
        .lightmap_pixel_count = 4096u,
    };
    uint32_t required = 0u;
    assert(bsp_texture_table_required_dwords(&bundle, &required) == 0);
    assert(required == 2u * BSP_TEXTURE_TABLE_DWORDS);
    uint32_t tables[2u * BSP_TEXTURE_TABLE_DWORDS];
    uint32_t written = 99u;
    assert(bsp_texture_build_tables(
               tables, required - 1u, &bundle,
               UINT64_C(0x0000123400000000),
               UINT64_C(0x0000123500000000), &written) == -2);
    assert(written == 0u);
    assert(bsp_texture_build_tables(
               tables, required, &bundle,
               UINT64_C(0x0000123400000000),
               UINT64_C(0x0000123500000000), &written) == 0);
    assert(written == required);
    assert(tables[0] == UINT32_C(0x34000000));
    assert(tables[BSP_GFX1013_COMBINED_DWORDS + 8u] == UINT32_C(0x92));
    assert(tables[BSP_TEXTURE_TABLE_DWORDS] == UINT32_C(0x34000008));
    assert(memcmp(tables + BSP_GFX1013_COMBINED_DWORDS,
                  tables + BSP_TEXTURE_TABLE_DWORDS +
                      BSP_GFX1013_COMBINED_DWORDS,
                  BSP_GFX1013_COMBINED_DWORDS * sizeof(uint32_t)) == 0);
    BspBundleTexture corrupt[2];
    memcpy(corrupt, textures, sizeof(corrupt));
    corrupt[1].offset = 1u;
    bundle.textures = corrupt;
    written = 99u;
    assert(bsp_texture_build_tables(
               tables, required, &bundle,
               UINT64_C(0x0000123400000000),
               UINT64_C(0x0000123500000000), &written) == -4);
    assert(written == 0u);
    return 0;
}
