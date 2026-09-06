#ifndef PS5_AGC_GEARS_BSP_BUNDLE_H
#define PS5_AGC_GEARS_BSP_BUNDLE_H

#include <stddef.h>
#include <stdint.h>

enum bsp_bundle_result {
    BSP_BUNDLE_OK = 0,
    BSP_BUNDLE_PRECONDITION = -1,
    BSP_BUNDLE_HEADER_INVALID = -2,
    BSP_BUNDLE_DIRECTORY_INVALID = -3,
    BSP_BUNDLE_CHECKSUM_MISMATCH = -4,
    BSP_BUNDLE_GEOMETRY_INVALID = -5,
};

typedef struct BspBundleVertex {
    float position[3];
    float base_uv[2];
    float light_uv[2];
    uint32_t face_id;
} BspBundleVertex;

typedef struct BspBundleDraw {
    uint32_t first_index;
    uint32_t index_count;
    uint32_t base_texture;
    uint32_t lightmap;
    uint32_t face_id;
    uint32_t flags;
    uint32_t reserved[2];
} BspBundleDraw;

enum { BSP_BUNDLE_IMAGE_RGBA8_UNORM = 1 };

typedef struct BspBundleImage {
    uint32_t width;
    uint32_t height;
    uint32_t row_pitch;
    uint32_t format;
} BspBundleImage;

enum {
    BSP_BUNDLE_TEXTURE_TRANSPARENT = 1u,
    BSP_BUNDLE_TEXTURE_FALLBACK = 2u,
    BSP_BUNDLE_TEXTURE_NODRAW = 4u,
    BSP_BUNDLE_TEXTURE_SKY = 8u,
};

typedef struct BspBundleTexture {
    uint32_t offset;
    uint32_t bytes;
    uint32_t width;
    uint32_t height;
    uint32_t row_pitch;
    uint32_t format;
    uint32_t name_hash;
    uint32_t flags;
    uint32_t mip_count;
    uint32_t reserved[3];
} BspBundleTexture;

typedef struct BspBundleMipLevel {
    uint32_t offset;
    uint32_t bytes;
    uint32_t width;
    uint32_t height;
    uint32_t row_pitch;
} BspBundleMipLevel;

typedef struct BspBundleView {
    const void *data;
    size_t bytes;
    const BspBundleVertex *vertices;
    uint32_t vertex_count;
    const uint16_t *indices;
    uint32_t index_count;
    const BspBundleDraw *draws;
    uint32_t draw_count;
    const BspBundleImage *lightmap_image;
    const uint8_t *lightmap_pixels;
    uint32_t lightmap_pixel_count;
    const BspBundleTexture *textures;
    uint32_t texture_count;
    const uint8_t *texture_pixels;
    uint32_t texture_pixel_bytes;
    float camera_position[3];
    float camera_forward[3];
} BspBundleView;

/* Validate the complete file before exposing any GPU-upload span. */
int bsp_bundle_open(const void *data, size_t bytes, BspBundleView *view);

/* Derive one level of the public AddrLib GFX10 ADDR_SW_LINEAR layout. */
int bsp_bundle_texture_mip_level(const BspBundleTexture *texture,
                                 uint32_t level,
                                 BspBundleMipLevel *out);

#endif
