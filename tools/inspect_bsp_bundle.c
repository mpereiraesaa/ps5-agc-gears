#include "bsp_bundle.h"

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
    putchar('\n');
    free(data);
    return 0;
}
