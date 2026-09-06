#include "../src/bsp_resource_frame.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

int main(void)
{
    _Alignas(256) uint8_t mapping[8192] = {0};
    BspBundleVertex *vertices = (BspBundleVertex *)(mapping + 0u);
    uint8_t *texture_pixels = mapping + 256u;
    uint8_t *lightmap_pixels = mapping + 512u;
    BspBundleVertex *clear_vertices = (BspBundleVertex *)(mapping + 1024u);
    uint16_t *clear_indices = (uint16_t *)(mapping + 1152u);
    const BspBundleTexture texture = {
        .offset = 0u, .bytes = 256u, .width = 1u, .height = 1u,
        .row_pitch = 256u, .format = BSP_BUNDLE_IMAGE_RGBA8_UNORM,
    };
    const BspBundleImage lightmap = {
        .width = 1u, .height = 1u, .row_pitch = 256u,
        .format = BSP_BUNDLE_IMAGE_RGBA8_UNORM,
    };
    const BspBundleView bundle = {
        .vertices = vertices,
        .vertex_count = 3u,
        .textures = &texture,
        .texture_count = 1u,
        .texture_pixels = texture_pixels,
        .texture_pixel_bytes = 256u,
        .lightmap_image = &lightmap,
        .lightmap_pixels = lightmap_pixels,
        .lightmap_pixel_count = 1u,
    };
    clear_indices[0] = 0u;
    clear_indices[1] = 1u;
    clear_indices[2] = 2u;
    Ps5TransientRing ring;
    assert(ps5_transient_ring_init(&ring, mapping + 4096u, 4096u,
                                   2u, 256u) == 0);
    assert(ps5_transient_ring_begin(&ring, 0u, 0u, 0) == 0);
    const float position[3] = {0, 0, 4};
    const float forward[3] = {0, 0, -1};
    BspResourceFrame frame;
    assert(bsp_resource_frame_build(
               &frame, &ring, 0u, mapping, sizeof(mapping), &bundle,
               (uintptr_t)lightmap_pixels, clear_vertices, clear_indices,
               position, forward,
               16.0f / 9.0f, 17u) == 0);
    assert(frame.map_constant_table && frame.clear_constant_table);
    assert(frame.map_vertex_table && frame.clear_vertex_table);
    assert(frame.texture_tables && frame.texture_table_dwords == 24u);
    assert(frame.overlay_constant_table && frame.overlay_indices);
    assert(frame.overlay_index_count == 6u && frame.transient_bytes > 0u);
    assert(frame.map_constant_table[3] == UINT32_C(0x31016fac));
    assert(frame.map_vertex_table[3] == UINT32_C(0x11014fac));
    assert(frame.texture_tables[10] == UINT32_C(0x00500000));
    assert(frame.overlay_constant_table[2] == BSP_RESOURCE_CONSTANT_DWORDS *
                                                  sizeof(uint32_t));
    assert(frame.overlay_constant_table[3] == UINT32_C(0x31016fac));
    const uintptr_t overlay_address =
        (uintptr_t)frame.overlay_constant_table[0] |
        ((uintptr_t)(frame.overlay_constant_table[1] & 0xffffu) << 32);
    const BspOverlayConstants *overlay =
        (const BspOverlayConstants *)overlay_address;
    assert(overlay->color[0] == 0.04f);
    assert(overlay->color[1] == 0.35f + 17.0f / 128.0f);
    assert(overlay->color[2] == 0.10f && overlay->color[3] == 1.0f);
    const uint16_t expected_overlay_indices[6] = {0, 1, 2, 0, 2, 3};
    assert(memcmp(frame.overlay_indices, expected_overlay_indices,
                  sizeof(expected_overlay_indices)) == 0);
    assert(ps5_transient_ring_seal(&ring, 0u, 77u) == 0);
    assert(ps5_transient_ring_begin(&ring, 1u, 0u, 0) == 0);
    assert(bsp_resource_frame_build(
               &frame, &ring, 1u, mapping, sizeof(mapping), &bundle,
               UINT64_C(0x0000123500000000), clear_vertices,
               clear_indices, position, forward, 16.0f / 9.0f, 18u) == -5);
    return 0;
}
