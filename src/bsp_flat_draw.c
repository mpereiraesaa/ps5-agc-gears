#include "bsp_flat_draw.h"

#include <limits.h>

enum { BSP_PARAMETER_SH_OFFSET = 0x8d, BSP_SH_PACKET_WORDS = 23,
       BSP_INDEX_PACKET_WORDS = 6 };

int bsp_flat_required_dwords(uint32_t draw_count, uint32_t *required)
{
    if (!required || draw_count == 0u ||
        draw_count > UINT32_MAX / BSP_FLAT_DWORDS_PER_DRAW)
        return -1;
    *required = draw_count * BSP_FLAT_DWORDS_PER_DRAW;
    return 0;
}

int bsp_flat_compose(uint32_t **cursor, uint32_t *end,
                     const BspFlatDraw *draws, uint32_t draw_count,
                     const void *gpu_mapping, size_t gpu_mapping_bytes,
                     uint64_t modifier, BspSetShDirectFn set_sh_direct,
                     BspDrawIndexedFn draw_indexed,
                     BspFlatComposeResult *result)
{
    uint32_t required = 0u;
    if (!cursor || !*cursor || !end || *cursor > end || !draws ||
        !gpu_mapping || gpu_mapping_bytes == 0u ||
        !modifier || !set_sh_direct || !draw_indexed || !result ||
        bsp_flat_required_dwords(draw_count, &required) != 0)
        return -1;
    result->draws = 0u;
    result->command_dwords = 0u;
    if ((size_t)(end - *cursor) < required)
        return -2;
    uint32_t *const start = *cursor;
    for (uint32_t draw = 0; draw < draw_count; ++draw) {
        if (!draws[draw].indices || draws[draw].index_count == 0u ||
            draws[draw].index_count % 3u != 0u)
            return -3;
        uint32_t *before = *cursor;
        if (set_sh_direct(cursor, (uint32_t)(end - *cursor),
                          BSP_PARAMETER_SH_OFFSET, draws[draw].sh_words,
                          BSP_FLAT_PARAMETER_WORDS) != 0 ||
            *cursor - before != BSP_SH_PACKET_WORDS)
            return -4;
        before = *cursor;
        if (draw_indexed(cursor, (uint32_t)(end - *cursor),
                         draws[draw].index_count, draws[draw].indices,
                         gpu_mapping, gpu_mapping_bytes, modifier) != 0 ||
            *cursor - before != BSP_INDEX_PACKET_WORDS)
            return -5;
        ++result->draws;
    }
    result->command_dwords = (uint32_t)(*cursor - start);
    return result->draws == draw_count && result->command_dwords == required
               ? 0 : -6;
}
