#include "bsp_bundle.h"
#include "bsp_dynamic_lightmap.h"
#include "bsp_alpha_test.h"
#include "bsp_sky.h"
#include "bsp_texture_descriptor.h"

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "usage: inspect_bsp_bundle BUNDLE\n");
        return 2;
    }
    FILE *file = fopen(argv[1], "rb");
    if (!file || fseek(file, 0, SEEK_END) != 0) {
        fprintf(stderr, "bundle open failed\n");
        if (file) fclose(file);
        return 1;
    }
    const long length = ftell(file);
    if (length <= 0 || fseek(file, 0, SEEK_SET) != 0) {
        fprintf(stderr, "bundle size failed\n");
        fclose(file);
        return 1;
    }
    void *data = malloc((size_t)length);
    if (!data || fread(data, 1u, (size_t)length, file) != (size_t)length ||
        fclose(file) != 0) {
        fprintf(stderr, "bundle read failed\n");
        free(data);
        return 1;
    }
    BspBundleView view;
    const int result = bsp_bundle_open(data, (size_t)length, &view);
    if (result != BSP_BUNDLE_OK) {
        fprintf(stderr, "bundle validation failed: %d\n", result);
        free(data);
        return 1;
    }
    printf("bundle valid: bytes=%zu vertices=%u indices=%u draws=%u",
           view.bytes, view.vertex_count, view.index_count, view.draw_count);
    if (view.lightmap_image)
        printf(" lightmap=%ux%u lightmap_pixels=%u",
               view.lightmap_image->width, view.lightmap_image->height,
               view.lightmap_pixel_count);
    if (view.textures) {
        uint32_t descriptor_dwords = 0u;
        uint32_t minimum_mips = 15u;
        uint32_t maximum_mips = 0u;
        uint64_t chain_bytes = 0u;
        if (bsp_texture_table_required_dwords(&view,
                                                &descriptor_dwords) != 0) {
            fprintf(stderr, "descriptor sizing failed\n");
            free(data);
            return 1;
        }
        for (uint32_t index = 0u; index < view.texture_count; ++index) {
            const BspBundleTexture *const texture = &view.textures[index];
            BspBundleMipLevel base;
            if (bsp_bundle_texture_mip_level(texture, 0u, &base) != 0 ||
                base.offset > texture->bytes ||
                base.bytes != texture->bytes - base.offset) {
                fprintf(stderr, "mip layout derivation failed\n");
                free(data);
                return 1;
            }
            if (texture->mip_count < minimum_mips)
                minimum_mips = texture->mip_count;
            if (texture->mip_count > maximum_mips)
                maximum_mips = texture->mip_count;
            chain_bytes += texture->bytes;
        }
        printf(" textures=%u texture_bytes=%u descriptor_dwords=%u "
               "mip_layout=addr-sw-linear mip_order=smallest-to-base "
               "mip_levels=%u..%u mip_chain_bytes=%llu",
               view.texture_count, view.texture_pixel_bytes,
               descriptor_dwords, minimum_mips, maximum_mips,
               (unsigned long long)chain_bytes);
        BspDynamicLightmapLayout dynamic;
        if (bsp_dynamic_lightmap_select(&view, &dynamic) != 0) {
            fprintf(stderr, "dynamic lightmap selection failed\n");
            free(data);
            return 1;
        }
        printf(" dynamic_patch=%u,%u+%ux%u hit_face=%u patch_bytes=%zu "
               "dirty_span_bytes=%zu",
               dynamic.patch_x, dynamic.patch_y, dynamic.patch_width,
               dynamic.patch_height, dynamic.hit_face, dynamic.patch_bytes,
               dynamic.dirty_span_bytes);
        BspAlphaTestPlan alpha;
        const int alpha_result =
            bsp_alpha_test_plan(&view, view.camera_position, &alpha);
        if (alpha_result == 0)
            printf(" alpha_textures=%u alpha_draws=%u alpha_target=%u:%u",
                   alpha.texture_count, alpha.draw_count,
                   alpha.target_texture, alpha.target_face);
        else if (alpha_result == -2)
            printf(" alpha_textures=0 alpha_draws=0");
        else {
            fprintf(stderr, "alpha-test planning failed: %d\n", alpha_result);
            free(data);
            return 1;
        }
        BspSkyPlan sky;
        const int sky_result =
            bsp_sky_plan(&view, view.camera_position, &sky);
        if (sky_result == 0)
            printf(" sky_textures=%u sky_draws=%u sky_target=%u:%u",
                   sky.texture_count, sky.draw_count,
                   sky.target_texture, sky.target_face);
        else if (sky_result == -2)
            printf(" sky_textures=0 sky_draws=0");
        else {
            fprintf(stderr, "sky planning failed: %d\n", sky_result);
            free(data);
            return 1;
        }
    }
    putchar('\n');
    free(data);
    return 0;
}
