#ifndef PS5_AGC_GEARS_BSP_TEXTURED_DRAW_H
#define PS5_AGC_GEARS_BSP_TEXTURED_DRAW_H

#include "bsp_flat_draw.h"

#include <stddef.h>
#include <stdint.h>

enum {
    BSP_TEXTURED_DWORDS_PER_DRAW = 32,
    BSP_TEXTURED_TABLE_SH_OFFSET = 0x0d,
};

int bsp_textured_required_dwords(uint32_t draw_count, uint32_t *required);
int bsp_textured_compose(
    uint32_t **cursor, uint32_t *end, const BspFlatDraw *draws,
    const uint32_t *const *descriptor_tables, uint32_t draw_count,
    const void *gpu_mapping, size_t gpu_mapping_bytes, uint64_t modifier,
    BspSetShDirectFn set_sh_direct, BspDrawIndexedFn draw_indexed,
    BspFlatComposeResult *result);

#endif
