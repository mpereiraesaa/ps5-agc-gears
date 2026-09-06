#include "../src/bsp_dynamic_lightmap.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

int main(void)
{
    BspBundleVertex vertices[3] = {
        {{-2.0f, -2.0f, 0.0f}, {0, 0}, {0.25f, 0.25f}, 17u},
        {{ 2.0f, -2.0f, 0.0f}, {0, 0}, {0.75f, 0.25f}, 17u},
        {{ 0.0f,  2.0f, 0.0f}, {0, 0}, {0.50f, 0.75f}, 17u},
    };
    const uint16_t indices[3] = {0u, 1u, 2u};
    const BspBundleDraw draw = {
        .first_index = 0u, .index_count = 3u, .base_texture = 0u,
        .lightmap = 0u, .face_id = 17u,
    };
    const BspBundleTexture texture = {
        .width = 1u, .height = 1u, .row_pitch = 256u,
        .format = BSP_BUNDLE_IMAGE_RGBA8_UNORM,
    };
    const BspBundleImage image = {
        .width = 16u, .height = 16u, .row_pitch = 256u,
        .format = BSP_BUNDLE_IMAGE_RGBA8_UNORM,
    };
    uint8_t source[4096];
    memset(source, 0x6cu, sizeof(source));
    BspBundleView bundle = {
        .vertices = vertices, .vertex_count = 3u,
        .indices = indices, .index_count = 3u,
        .draws = &draw, .draw_count = 1u,
        .lightmap_image = &image, .lightmap_pixels = source,
        .lightmap_pixel_count = 256u,
        .textures = &texture, .texture_count = 1u,
        .camera_position = {0.0f, 0.0f, 2.0f},
        .camera_forward = {0.0f, 0.0f, -1.0f},
    };
    BspDynamicLightmapLayout layout;
    assert(bsp_dynamic_lightmap_select(&bundle, &layout) == 0);
    assert(layout.hit_face == 17u);
    assert(layout.patch_width == 8u && layout.patch_height == 8u);
    assert(layout.patch_x == 4u && layout.patch_y == 4u);
    assert(layout.image_bytes == sizeof(source));
    assert(layout.patch_bytes == 256u);
    assert(layout.dirty_span_bytes == 7u * 256u + 32u);

    size_t allocation_bytes = 0u;
    assert(bsp_dynamic_lightmap_allocation_bytes(&layout,
                                                 &allocation_bytes) == 0);
    assert(allocation_bytes == sizeof(source) + 512u);
    _Alignas(256) uint8_t allocation[sizeof(source) + 512u];
    BspDynamicLightmapSlot slot;
    assert(bsp_dynamic_lightmap_slot_init(
               &slot, allocation, sizeof(allocation), source,
               sizeof(source), &layout) == 0);
    const uint64_t surrounding = slot.surrounding_hash;

    _Alignas(256) uint8_t transient[4096] = {0};
    Ps5TransientRing ring;
    assert(ps5_transient_ring_init(&ring, transient, sizeof(transient),
                                   2u, 256u) == 0);
    BspDynamicLightmapUpdate update;
    assert(bsp_dynamic_lightmap_update(
               &slot, &layout, &ring, 0u, 0u, &update) == -1);
    assert(ps5_transient_ring_begin(&ring, 0u, 0u, 0) == 0);
    assert(bsp_dynamic_lightmap_update(
               &slot, &layout, &ring, 0u, 0u, &update) == 0);
    assert(update.first_upload == 1u && update.pattern == 0u);
    assert(update.written_address == slot.pixels);
    assert(update.written_span_bytes == layout.image_bytes);
    assert(update.uploaded_bytes == layout.image_bytes);
    const uint64_t bright_hash = update.patch_hash;
    assert(slot.pixels[layout.dirty_offset] == 255u);
    assert(bsp_dynamic_lightmap_surrounding_hash(slot.pixels, &layout) ==
           surrounding);
    assert(ps5_transient_ring_seal(&ring, 0u, 101u) == 0);
    assert(bsp_dynamic_lightmap_update(
               &slot, &layout, &ring, 0u, 1u, &update) == -1);
    assert(ps5_transient_ring_begin(&ring, 0u, 100u, 1) ==
           PS5_TRANSIENT_TOKEN_MISMATCH);
    assert(ps5_transient_ring_begin(&ring, 0u, 101u, 1) == 0);
    assert(bsp_dynamic_lightmap_update(
               &slot, &layout, &ring, 0u, 1u, &update) == 0);
    assert(update.first_upload == 0u && update.pattern == 1u);
    assert(update.written_address == slot.pixels + layout.dirty_offset);
    assert(update.written_span_bytes == layout.dirty_span_bytes);
    assert(update.uploaded_bytes == layout.patch_bytes);
    assert(update.patch_hash != bright_hash);
    assert(slot.pixels[layout.dirty_offset] == 24u);
    assert(bsp_dynamic_lightmap_surrounding_hash(slot.pixels, &layout) ==
           surrounding);
    assert(bsp_dynamic_lightmap_guards_intact(&slot, &layout));
    allocation[0] ^= 1u;
    assert(!bsp_dynamic_lightmap_guards_intact(&slot, &layout));

    bundle.camera_forward[2] = 1.0f;
    assert(bsp_dynamic_lightmap_select(&bundle, &layout) == -2);
    return 0;
}
