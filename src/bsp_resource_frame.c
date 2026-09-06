#include "bsp_resource_frame.h"

#include "bsp_flat_scene.h"
#include "bsp_texture_descriptor.h"
#include "ps5_gfx1013_descriptor.h"
#include "ps5_gpu_span.h"
#include "ps5_transient_table.h"

#include <string.h>

_Static_assert(sizeof(BspResourceConstants) ==
                   BSP_RESOURCE_CONSTANT_DWORDS * sizeof(uint32_t),
               "resource constant layout");
_Static_assert(sizeof(BspOverlayVertex) == 24u, "overlay vertex layout");

static int slice(Ps5TransientRing *ring, uint32_t slot, size_t bytes,
                 size_t alignment, const void *gpu_mapping,
                 size_t gpu_mapping_bytes, Ps5TransientSlice *out)
{
    return ps5_transient_ring_allocate(ring, slot, bytes, alignment, out) ==
                       PS5_TRANSIENT_OK &&
                   ps5_gpu_span_visible(gpu_mapping, gpu_mapping_bytes,
                                        out->cpu, out->bytes)
               ? 0
               : -1;
}

static int constants(Ps5TransientRing *ring, uint32_t slot,
                     const void *gpu_mapping, size_t gpu_mapping_bytes,
                     const float mvp[16], const float control[4],
                     uint64_t frame_index, const uint32_t **table_out)
{
    Ps5TransientSlice data_slice;
    Ps5TransientTable table;
    if (slice(ring, slot, sizeof(BspResourceConstants), 256u,
              gpu_mapping, gpu_mapping_bytes, &data_slice) != 0 ||
        ps5_transient_table_allocate(ring, slot,
                                     PS5_GFX1013_VSHARP_DWORDS,
                                     gpu_mapping, gpu_mapping_bytes,
                                     &table) != 0)
        return -1;
    BspResourceConstants *data = data_slice.cpu;
    memset(data, 0, sizeof(*data));
    memcpy(data->mvp, mvp, sizeof(data->mvp));
    memcpy(data->control, control, sizeof(data->control));
    data->debug_values[0] = (float)(frame_index & UINT64_C(0xffff));
    data->debug_values[1] = (float)slot;
    if (ps5_gfx1013_build_constant_vsharp(
            table.words, (uintptr_t)data, sizeof(*data)) != 0)
        return -1;
    *table_out = table.words;
    return 0;
}

static int vertex_table(Ps5TransientRing *ring, uint32_t slot,
                        const void *gpu_mapping, size_t gpu_mapping_bytes,
                        const void *vertices, uint32_t stride,
                        uint32_t count, const uint32_t **table_out)
{
    Ps5TransientTable table;
    if (!ps5_gpu_span_visible(gpu_mapping, gpu_mapping_bytes, vertices,
                              (size_t)stride * count) ||
        ps5_transient_table_allocate(ring, slot,
                                     PS5_GFX1013_VSHARP_DWORDS,
                                     gpu_mapping, gpu_mapping_bytes,
                                     &table) != 0 ||
        ps5_gfx1013_build_vsharp(table.words, (uintptr_t)vertices,
                                 stride, count) != 0)
        return -1;
    *table_out = table.words;
    return 0;
}

int bsp_resource_frame_build(
    BspResourceFrame *out, Ps5TransientRing *ring, uint32_t slot_index,
    const void *gpu_mapping, size_t gpu_mapping_bytes,
    const BspBundleView *bundle, const BspBundleVertex clear_vertices[3],
    const uint16_t clear_indices[3],
    const float camera_position[3], const float camera_forward[3],
    float aspect_ratio, uint64_t frame_index)
{
    if (!out || !ring || slot_index >= ring->slot_count || !bundle ||
        !bundle->vertices || !bundle->textures || !clear_vertices ||
        !clear_indices || !camera_position || !camera_forward)
        return -1;
    memset(out, 0, sizeof(*out));
    float map_mvp[16];
    if (bsp_flat_camera_matrix(map_mvp, camera_position, camera_forward,
                               aspect_ratio) != 0)
        return -2;
    const float identity[16] = {
        1, 0, 0, 0, 0, 1, 0, 0,
        0, 0, 1, 0, 0, 0, 0, 1,
    };
    const float map_control[4] = {0, 0, 0, 1};
    const float clear_control[4] = {0.02f, 0.02f, 0.025f, 0};
    if (constants(ring, slot_index, gpu_mapping, gpu_mapping_bytes,
                  map_mvp, map_control, frame_index,
                  &out->map_constant_table) != 0 ||
        constants(ring, slot_index, gpu_mapping, gpu_mapping_bytes,
                  identity, clear_control, frame_index,
                  &out->clear_constant_table) != 0 ||
        vertex_table(ring, slot_index, gpu_mapping, gpu_mapping_bytes,
                     bundle->vertices, sizeof(BspBundleVertex),
                     bundle->vertex_count, &out->map_vertex_table) != 0)
        return -3;

    if (!ps5_gpu_span_visible(gpu_mapping, gpu_mapping_bytes,
                              clear_vertices, 3u * sizeof(*clear_vertices)) ||
        !ps5_gpu_span_visible(gpu_mapping, gpu_mapping_bytes,
                              clear_indices, 3u * sizeof(*clear_indices)) ||
        vertex_table(ring, slot_index, gpu_mapping, gpu_mapping_bytes,
                     clear_vertices, sizeof(BspBundleVertex), 3u,
                     &out->clear_vertex_table) != 0)
        return -4;

    uint32_t texture_dwords = 0u;
    Ps5TransientTable texture_table;
    uint32_t written = 0u;
    if (bsp_texture_table_required_dwords(bundle, &texture_dwords) != 0 ||
        ps5_transient_table_allocate(ring, slot_index, texture_dwords,
                                     gpu_mapping, gpu_mapping_bytes,
                                     &texture_table) != 0 ||
        bsp_texture_build_tables(
            texture_table.words, texture_dwords, bundle,
            (uintptr_t)bundle->texture_pixels,
            (uintptr_t)bundle->lightmap_pixels, &written) != 0 ||
        written != texture_dwords)
        return -5;
    out->texture_tables = texture_table.words;
    out->texture_table_dwords = texture_dwords;

    Ps5TransientSlice vertex_slice;
    Ps5TransientSlice index_slice;
    if (slice(ring, slot_index,
              BSP_RESOURCE_OVERLAY_VERTICES * sizeof(BspOverlayVertex),
              16u, gpu_mapping, gpu_mapping_bytes, &vertex_slice) != 0 ||
        slice(ring, slot_index,
              BSP_RESOURCE_OVERLAY_INDICES * sizeof(uint16_t), 2u,
              gpu_mapping, gpu_mapping_bytes, &index_slice) != 0)
        return -6;
    BspOverlayVertex *overlay = vertex_slice.cpu;
    const float x0 = -0.96f, x1 = -0.62f, y0 = 0.86f, y1 = 0.94f;
    const float pulse = 0.35f + (float)(frame_index & 63u) / 128.0f;
    overlay[0] = (BspOverlayVertex){{x0, y0}, {0.04f, pulse, 0.10f, 1}};
    overlay[1] = (BspOverlayVertex){{x1, y0}, {0.04f, pulse, 0.10f, 1}};
    overlay[2] = (BspOverlayVertex){{x1, y1}, {0.04f, pulse, 0.10f, 1}};
    overlay[3] = (BspOverlayVertex){{x0, y1}, {0.04f, pulse, 0.10f, 1}};
    uint16_t *indices = index_slice.cpu;
    const uint16_t values[BSP_RESOURCE_OVERLAY_INDICES] = {0, 1, 2, 0, 2, 3};
    memcpy(indices, values, sizeof(values));
    if (vertex_table(ring, slot_index, gpu_mapping, gpu_mapping_bytes,
                     overlay, sizeof(*overlay),
                     BSP_RESOURCE_OVERLAY_VERTICES,
                     &out->overlay_vertex_table) != 0)
        return -7;
    out->overlay_indices = indices;
    out->overlay_index_count = BSP_RESOURCE_OVERLAY_INDICES;
    out->transient_bytes = ring->slots[slot_index].used;
    return 0;
}
