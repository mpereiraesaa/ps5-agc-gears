#include "bsp_resource_draw.h"

#include "bsp_texture_descriptor.h"
#include "ps5_gpu_span.h"

#include <limits.h>

enum {
    SET_ONE_DWORDS = 3,
    SET_TWO_DWORDS = 4,
    DRAW_INDEX_DWORDS = 6,
    CLEAR_DWORDS = SET_TWO_DWORDS + SET_ONE_DWORDS + DRAW_INDEX_DWORDS,
    MAP_PREFIX_DWORDS = SET_TWO_DWORDS,
    MAP_DRAW_DWORDS = SET_ONE_DWORDS + DRAW_INDEX_DWORDS,
    OVERLAY_DWORDS = SET_ONE_DWORDS + DRAW_INDEX_DWORDS,
};

static int table_visible(const void *mapping, size_t mapping_bytes,
                         const uint32_t *table, uint32_t dwords)
{
    return table && ((uintptr_t)table & 15u) == 0u &&
           ps5_gpu_span_visible(mapping, mapping_bytes, table,
                                (size_t)dwords * sizeof(uint32_t));
}

int bsp_resource_compose_map(
    uint32_t **cursor, uint32_t *end, const BspResourceFrame *frame,
    const BspBundleView *bundle, const uint16_t clear_indices[3],
    const void *gpu_mapping, size_t gpu_mapping_bytes, uint64_t modifier,
    BspSetShDirectFn set_sh_direct, BspDrawIndexedFn draw_indexed,
    BspResourceComposeResult *result)
{
    if (!cursor || !*cursor || !end || *cursor > end || !frame || !bundle ||
        !bundle->draws || !bundle->indices || bundle->draw_count == 0u ||
        !clear_indices || !gpu_mapping || !gpu_mapping_bytes || !modifier ||
        !set_sh_direct || !draw_indexed || !result ||
        bundle->draw_count > (UINT32_MAX - CLEAR_DWORDS -
                              MAP_PREFIX_DWORDS) / MAP_DRAW_DWORDS)
        return -1;
    result->map_draws = result->overlay_draws = result->command_dwords = 0u;
    const uint32_t required = CLEAR_DWORDS + MAP_PREFIX_DWORDS +
                              bundle->draw_count * MAP_DRAW_DWORDS;
    if ((size_t)(end - *cursor) < required)
        return -2;
    if (!table_visible(gpu_mapping, gpu_mapping_bytes,
                       frame->clear_constant_table, 4u) ||
        !table_visible(gpu_mapping, gpu_mapping_bytes,
                       frame->clear_vertex_table, 4u) ||
        !table_visible(gpu_mapping, gpu_mapping_bytes,
                       frame->map_constant_table, 4u) ||
        !table_visible(gpu_mapping, gpu_mapping_bytes,
                       frame->map_vertex_table, 4u) ||
        !table_visible(gpu_mapping, gpu_mapping_bytes,
                       frame->texture_tables,
                       frame->texture_table_dwords) ||
        !ps5_gpu_span_visible(gpu_mapping, gpu_mapping_bytes,
                              clear_indices, 3u * sizeof(*clear_indices)))
        return -3;
    const uint32_t expected_texture_dwords =
        bundle->texture_count * BSP_TEXTURE_TABLE_DWORDS;
    if (bundle->texture_count == 0u ||
        frame->texture_table_dwords != expected_texture_dwords)
        return -3;
    for (uint32_t draw = 0; draw < bundle->draw_count; ++draw) {
        const BspBundleDraw *source = &bundle->draws[draw];
        if (source->base_texture >= bundle->texture_count ||
            source->first_index > bundle->index_count ||
            source->index_count == 0u || source->index_count % 3u != 0u ||
            source->index_count > bundle->index_count - source->first_index)
            return -3;
    }

    uint32_t *const start = *cursor;
    uint32_t gs_values[2] = {
        (uint32_t)(uintptr_t)frame->clear_constant_table,
        (uint32_t)(uintptr_t)frame->clear_vertex_table,
    };
    uint32_t texture = (uint32_t)(uintptr_t)frame->texture_tables;
    if (set_sh_direct(cursor, (uint32_t)(end - *cursor),
                      BSP_RESOURCE_GS_SH_OFFSET, gs_values, 2u) != 0 ||
        set_sh_direct(cursor, (uint32_t)(end - *cursor),
                      BSP_RESOURCE_PS_SH_OFFSET, &texture, 1u) != 0 ||
        draw_indexed(cursor, (uint32_t)(end - *cursor), 3u, clear_indices,
                     gpu_mapping, gpu_mapping_bytes, modifier) != 0)
        return -4;
    gs_values[0] = (uint32_t)(uintptr_t)frame->map_constant_table;
    gs_values[1] = (uint32_t)(uintptr_t)frame->map_vertex_table;
    if (set_sh_direct(cursor, (uint32_t)(end - *cursor),
                      BSP_RESOURCE_GS_SH_OFFSET, gs_values, 2u) != 0)
        return -4;
    for (uint32_t draw = 0; draw < bundle->draw_count; ++draw) {
        const BspBundleDraw *source = &bundle->draws[draw];
        const uint32_t *table = frame->texture_tables +
            source->base_texture * BSP_TEXTURE_TABLE_DWORDS;
        texture = (uint32_t)(uintptr_t)table;
        if (set_sh_direct(cursor, (uint32_t)(end - *cursor),
                          BSP_RESOURCE_PS_SH_OFFSET, &texture, 1u) != 0 ||
            draw_indexed(cursor, (uint32_t)(end - *cursor),
                         source->index_count,
                         bundle->indices + source->first_index,
                         gpu_mapping, gpu_mapping_bytes, modifier) != 0)
            return -5;
        ++result->map_draws;
    }
    result->command_dwords = (uint32_t)(*cursor - start);
    return result->command_dwords == required ? 0 : -6;
}

int bsp_resource_compose_overlay(
    uint32_t **cursor, uint32_t *end, const BspResourceFrame *frame,
    const void *gpu_mapping, size_t gpu_mapping_bytes, uint64_t modifier,
    BspSetShDirectFn set_sh_direct, BspDrawIndexedFn draw_indexed,
    BspResourceComposeResult *result)
{
    if (!cursor || !*cursor || !end || *cursor > end || !frame ||
        !gpu_mapping || !gpu_mapping_bytes || !modifier || !set_sh_direct ||
        !draw_indexed || !result || (size_t)(end - *cursor) < OVERLAY_DWORDS ||
        !table_visible(gpu_mapping, gpu_mapping_bytes,
                       frame->overlay_vertex_table, 4u) ||
        !frame->overlay_indices || frame->overlay_index_count != 6u ||
        !ps5_gpu_span_visible(gpu_mapping, gpu_mapping_bytes,
                              frame->overlay_indices,
                              frame->overlay_index_count * sizeof(uint16_t)))
        return -1;
    uint32_t *const start = *cursor;
    const uint32_t vertex_table =
        (uint32_t)(uintptr_t)frame->overlay_vertex_table;
    if (set_sh_direct(cursor, (uint32_t)(end - *cursor),
                      BSP_RESOURCE_GS_SH_OFFSET, &vertex_table, 1u) != 0 ||
        draw_indexed(cursor, (uint32_t)(end - *cursor),
                     frame->overlay_index_count, frame->overlay_indices,
                     gpu_mapping, gpu_mapping_bytes, modifier) != 0)
        return -2;
    result->overlay_draws = 1u;
    result->command_dwords += (uint32_t)(*cursor - start);
    return (uint32_t)(*cursor - start) == OVERLAY_DWORDS ? 0 : -3;
}
