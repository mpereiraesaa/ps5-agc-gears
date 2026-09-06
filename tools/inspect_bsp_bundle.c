#include "bsp_bundle.h"
#include "bsp_dynamic_lightmap.h"
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
        if (bsp_texture_table_required_dwords(&view,
                                                &descriptor_dwords) != 0) {
            fprintf(stderr, "descriptor sizing failed\n");
            free(data);
            return 1;
        }
        printf(" textures=%u texture_bytes=%u descriptor_dwords=%u",
               view.texture_count, view.texture_pixel_bytes,
               descriptor_dwords);
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
    }
    putchar('\n');
    free(data);
    return 0;
}
