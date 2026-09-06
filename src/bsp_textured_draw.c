#include "bsp_textured_draw.h"

#include "bsp_texture_descriptor.h"
#include "ps5_gpu_span.h"

#include <limits.h>

enum {
    BSP_PARAMETER_SH_OFFSET = 0x8d,
    BSP_PARAMETER_PACKET_DWORDS = 23,
    BSP_TABLE_PACKET_DWORDS = 3,
    BSP_INDEX_PACKET_DWORDS = 6,
};

int bsp_textured_required_dwords(uint32_t draw_count, uint32_t *required)
{
    if (!required || draw_count == 0u ||
        draw_count > UINT32_MAX / BSP_TEXTURED_DWORDS_PER_DRAW)
        return -1;
    *required = draw_count * BSP_TEXTURED_DWORDS_PER_DRAW;
    return 0;
}

int bsp_textured_compose(
    uint32_t **cursor, uint32_t *end, const BspFlatDraw *draws,
    const uint32_t *const *descriptor_tables, uint32_t draw_count,
    const void *gpu_mapping, size_t gpu_mapping_bytes, uint64_t modifier,
    BspSetShDirectFn set_sh_direct, BspDrawIndexedFn draw_indexed,
    BspFlatComposeResult *result)
{
    uint32_t required = 0u;
    if (!cursor || !*cursor || !end || *cursor > end || !draws ||
        !descriptor_tables || !gpu_mapping || gpu_mapping_bytes == 0u ||
        modifier == 0u || !set_sh_direct || !draw_indexed || !result ||
        bsp_textured_required_dwords(draw_count, &required) != 0)
        return -1;
    result->draws = 0u;
    result->command_dwords = 0u;
    if ((size_t)(end - *cursor) < required)
        return -2;
    for (uint32_t draw = 0; draw < draw_count; ++draw) {
        if (!draws[draw].indices || draws[draw].index_count == 0u ||
            draws[draw].index_count % 3u != 0u ||
            !descriptor_tables[draw] ||
            ((uintptr_t)descriptor_tables[draw] & 15u) != 0u ||
            !ps5_gpu_span_visible(
                gpu_mapping, gpu_mapping_bytes, descriptor_tables[draw],
                BSP_TEXTURE_TABLE_DWORDS * sizeof(uint32_t)))
            return -3;
    }

    uint32_t *const start = *cursor;
    for (uint32_t draw = 0; draw < draw_count; ++draw) {
        uint32_t *before = *cursor;
        if (set_sh_direct(cursor, (uint32_t)(end - *cursor),
                          BSP_PARAMETER_SH_OFFSET, draws[draw].sh_words,
                          BSP_FLAT_PARAMETER_WORDS) != 0 ||
            *cursor - before != BSP_PARAMETER_PACKET_DWORDS)
            return -4;
        const uint32_t table_address =
            (uint32_t)(uintptr_t)descriptor_tables[draw];
        before = *cursor;
        if (set_sh_direct(cursor, (uint32_t)(end - *cursor),
                          BSP_TEXTURED_TABLE_SH_OFFSET, &table_address,
                          1u) != 0 ||
            *cursor - before != BSP_TABLE_PACKET_DWORDS)
            return -5;
        before = *cursor;
        if (draw_indexed(cursor, (uint32_t)(end - *cursor),
                         draws[draw].index_count, draws[draw].indices,
                         gpu_mapping, gpu_mapping_bytes, modifier) != 0 ||
            *cursor - before != BSP_INDEX_PACKET_DWORDS)
            return -6;
        ++result->draws;
    }
    result->command_dwords = (uint32_t)(*cursor - start);
    return result->draws == draw_count &&
                   result->command_dwords == required ? 0 : -7;
}
