#ifndef PS5_AGC_GEARS_BSP_FLAT_DRAW_H
#define PS5_AGC_GEARS_BSP_FLAT_DRAW_H

#include <stddef.h>
#include <stdint.h>

enum {
    BSP_FLAT_PARAMETER_WORDS = 21,
    BSP_FLAT_DWORDS_PER_DRAW = 29,
};

typedef struct BspFlatDraw {
    uint32_t sh_words[BSP_FLAT_PARAMETER_WORDS];
    uint32_t index_count;
    const uint16_t *indices;
} BspFlatDraw;

typedef int (*BspSetShDirectFn)(uint32_t **cursor, uint32_t capacity,
                                uint32_t offset, const uint32_t *values,
                                uint32_t count);
typedef int (*BspDrawIndexedFn)(uint32_t **cursor, uint32_t capacity,
                                uint32_t index_count,
                                const uint16_t *gpu_indices,
                                const void *gpu_mapping,
                                size_t gpu_mapping_bytes,
                                uint64_t modifier);

typedef struct BspFlatComposeResult {
    uint32_t draws;
    uint32_t command_dwords;
} BspFlatComposeResult;

int bsp_flat_required_dwords(uint32_t draw_count, uint32_t *required);
int bsp_flat_compose(uint32_t **cursor, uint32_t *end,
                     const BspFlatDraw *draws, uint32_t draw_count,
                     const void *gpu_mapping, size_t gpu_mapping_bytes,
                     uint64_t modifier, BspSetShDirectFn set_sh_direct,
                     BspDrawIndexedFn draw_indexed,
                     BspFlatComposeResult *result);

#endif
